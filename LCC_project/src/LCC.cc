#include "LCC.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>

namespace inet {

Define_Module(LCC);

LCC::LCC() {
    beaconTimer = nullptr;
    checkTimeoutTimer = nullptr;
    dataTimer = nullptr;
}

LCC::~LCC() {
    cancelAndDelete(beaconTimer);
    cancelAndDelete(checkTimeoutTimer);
    cancelAndDelete(dataTimer);
}

void LCC::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        beaconInterval = par("beaconInterval");
        neighborValidityInterval = par("neighborValidityInterval");
        localPort = par("localPort");
        destPort = par("destPort");
        useMulticast = par("useMulticast");
        numHosts = par("numHosts");

        myId = getParentModule()->getIndex();
        myRole = 0;
        myClusterHeadId = -1;
        chStartTime = SIMTIME_ZERO;
        seqNum = 0;
        numSent = 0;
        numReceived = 0;
        numSent = 0;
        numReceived = 0;
        totalDelay = 0.0;
        totalBytesReceived = 0;
        numBeaconsSent = 0;
        numRoleChanges = 0;

        // Timerlar
        beaconTimer = new cMessage("beaconTimer");
        checkTimeoutTimer = new cMessage("checkTimeoutTimer");
        dataTimer = new cMessage("dataTimer");

        // Sinyaller
        chChangeSignal = registerSignal("chChangeSignal");
        chLifetimeSignal = registerSignal("chLifetimeSignal");
        controlOverheadSignal = registerSignal("controlOverheadSignal");
        clusterSizeSignal = registerSignal("clusterSizeSignal");
        rttSignal = registerSignal("rttSignal");
        pdrSignal = registerSignal("pdrSignal");
        dataSentSignal = registerSignal("dataSentSignal");
        dataReceivedSignal = registerSignal("dataReceivedSignal");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localPort);
        socket.setCallback(this);

        // Discovery (Keşif) ve Beaconlar için Multicast dinlemeye devam
        L3Address mcastAddr = L3AddressResolver().resolve("224.0.0.1");
        socket.joinMulticastGroup(mcastAddr);

        scheduleAt(simTime() + uniform(0, 2), beaconTimer);
        scheduleAt(simTime() + beaconInterval, checkTimeoutTimer);
        scheduleAt(simTime() + uniform(1, 3), dataTimer);

        updateVisuals();
    }
}

void LCC::handleStartOperation(LifecycleOperation *operation) {}
void LCC::handleStopOperation(LifecycleOperation *operation) {
    cancelEvent(beaconTimer);
    cancelEvent(checkTimeoutTimer);
    cancelEvent(dataTimer);
    socket.close();
}
void LCC::handleCrashOperation(LifecycleOperation *operation) {
    if (operation->getRootModule() != getContainingNode(this))
        socket.destroy();
}

void LCC::handleMessageWhenUp(cMessage *msg)
{
    if (msg == beaconTimer) {
        sendBeacon();
        scheduleAt(simTime() + beaconInterval, beaconTimer);
    }
    else if (msg == checkTimeoutTimer) {
        checkTimeouts();
        scheduleAt(simTime() + neighborValidityInterval/2, checkTimeoutTimer);
    }
    else if (msg == dataTimer) {
        sendDataPacket();
        scheduleAt(simTime() + uniform(2.0, 4.0), dataTimer);
    }
    else {
        socket.processMessage(msg);
    }
}

// ------------------------------------------------------------------
// BEACON MANTIĞI (Discovery & Maintenance)
// ------------------------------------------------------------------
void LCC::sendBeacon()
{
    Packet *packet = new Packet("LccBeacon");
    auto beacon = makeShared<LccBeacon>();

    beacon->setSrcId(myId);
    beacon->setRole(static_cast<LccRole>(myRole));
    beacon->setClusterHeadId(myClusterHeadId);
    beacon->setChunkLength(B(100));

    // Gateway Raporlaması: Gördüğüm yabancı kümeleri Liderime bildiriyorum
    if (isGateway && !foreignNeighbors.empty()) {
        beacon->setSeenClusterIdsArraySize(foreignNeighbors.size());
        int i = 0;
        for (auto const& [neighborId, foreignCH] : foreignNeighbors) {
            beacon->setSeenClusterIds(i, foreignCH);
            i++;
        }
    } else {
        beacon->setSeenClusterIdsArraySize(0);
    }

    packet->insertAtBack(beacon);
    emit(controlOverheadSignal, 1);
    numBeaconsSent++;
    // Beaconlar her zaman Multicast (Herkes duysun)
    L3Address destAddr = L3AddressResolver().resolve("224.0.0.1");
    socket.sendTo(packet, destAddr, destPort);
}

// ------------------------------------------------------------------
// DATA GÖNDERME (Traffic Generation)
// ------------------------------------------------------------------
void LCC::sendDataPacket()
{
    // Komşum yoksa paket atma
    if (myRole == 0 && neighborsLastSeen.empty()) return;

    Packet *packet = new Packet("LccData");
    auto data = makeShared<LccData>();

    data->setSrcId(myId);
    data->setSendTime(simTime());
    data->setChunkLength(B(1024));

    // Paket Kimliği
    data->setSeqNo(seqNum++);
    std::string pktId = std::to_string(myId) + ":" + std::to_string(data->getSeqNo());
    seenPackets[pktId] = simTime();

    L3Address destAddr;

    if (useMulticast) {
        // Phase 3 Test Modu
        if (neighborsLastSeen.empty()) { delete packet; return; }
        auto it = neighborsLastSeen.begin();
        std::advance(it, intuniform(0, neighborsLastSeen.size() - 1));
        data->setDestId(it->first);
        destAddr = L3AddressResolver().resolve("224.0.0.1");
    }
    else {
        // Phase 4: Unicast / Hierarchical Routing
        int randomNodeIndex = intuniform(0, numHosts - 1);
        while (randomNodeIndex == myId) randomNodeIndex = intuniform(0, numHosts - 1);
        data->setDestId(randomNodeIndex);

        if (myRole == 1) {
            if (myClusterHeadId != -1) {
                std::string chName = "host[" + std::to_string(myClusterHeadId) + "]";
                destAddr = L3AddressResolver().resolve(chName.c_str());
                EV << "DATA START: Member -> CH (" << myClusterHeadId << ")" << endl;
            } else { delete packet; return; }
        }
        else {
            destAddr = L3AddressResolver().resolve("224.0.0.1");
            packet->setName("InterClusterData");
            EV << "DATA START: CH -> Flood Start." << endl;
        }
    }
    numSent++;
    emit(dataSentSignal, 1);
    packet->insertAtBack(data);
    socket.sendTo(packet, destAddr, destPort);
}

// ------------------------------------------------------------------
// PAKET ALMA (Reception)
// ------------------------------------------------------------------
void LCC::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    auto chunk = packet->peekAtFront<Chunk>();

    // 1. DATA PAKETİ (Veri Geldi)
    if (auto dataPkt = dynamicPtrCast<const LccData>(chunk)) {
        processDataPacket(packet, dataPkt);
        delete packet;
        return;
    }

    // 2. BEACON PAKETİ (Sinyal Geldi)
    if (auto beacon = dynamicPtrCast<const LccBeacon>(chunk)) {

        int senderId = beacon->getSrcId();

        // Kendi beacon'ımı görmezden gel
        if (senderId == myId) { delete packet; return; }

        // A) Gateway Tespiti (Yabancı bir cluster üyesini duydum mu?)
        if (beacon->getClusterHeadId() != myClusterHeadId && beacon->getClusterHeadId() != -1) {
            foreignNeighbors[senderId] = beacon->getClusterHeadId();
            isGateway = true;
        }

        // B) CH İŞLEMLERİ (Eğer ben Lidersem)
        if (myRole == 2) {

            // Gelen beacon BENİM ÜYEMDEN mi?
            if (beacon->getClusterHeadId() == myId) {

                bool isMemberRecorded = false;
                for (int m : myMembers) {
                    if (m == senderId) { isMemberRecorded = true; break; }
                }
                if (!isMemberRecorded) {
                    myMembers.push_back(senderId);
                }
                // ---------------------------------------------------------

                // Routing Tablosunu Güncelle (Üyem başka cluster görüyor mu?)
                for (int k = 0; k < beacon->getSeenClusterIdsArraySize(); k++) {
                    int targetCluster = beacon->getSeenClusterIds(k);

                    // Bu hedef kümeye giden listeye bu üyeyi (Gateway adayı) ekle
                    std::vector<int>& gateways = clusterRoutingTable[targetCluster];
                    bool alreadyIn = false;
                    for(int gw : gateways) if(gw == senderId) alreadyIn = true;

                    if (!alreadyIn) gateways.push_back(senderId);
                }
            }
        }

        neighborsLastSeen[senderId] = simTime();
        neighborsRoles[senderId] = beacon->getRole();

        updateVisuals();
        delete packet;
        return;
    }
    delete packet;
}

// ------------------------------------------------------------------
// ROUTING MANTIĞI (En Önemli Kısım)
// ------------------------------------------------------------------
void LCC::processDataPacket(Packet *packet, const Ptr<const LccData>& dataPkt)
{
    // ------------------------------------------------------------------
    // 1. LOOP VE DUPLICATE KONTROLÜ
    // ------------------------------------------------------------------
    std::string packetId = std::to_string(dataPkt->getSrcId()) + ":" + std::to_string(dataPkt->getSeqNo());

    if (seenPackets.find(packetId) != seenPackets.end()) {
        return;
    }
    seenPackets[packetId] = simTime();

    // ------------------------------------------------------------------
    // 2. HEDEF KONTROLÜ
    // ------------------------------------------------------------------
    if (dataPkt->getDestId() == myId) {
        EV << "SUCCESS: Paket bana (" << myId << ") ulasti! Kaynak: " << dataPkt->getSrcId() << endl;

        emit(pdrSignal, 1.0);
        emit(rttSignal, simTime() - dataPkt->getSendTime());
        emit(dataReceivedSignal, 1);
        numReceived++;
        simtime_t delay = simTime() - dataPkt->getSendTime();
        totalDelay += delay.dbl();

        totalBytesReceived += packet->getByteLength();
        return;
    }

    // ------------------------------------------------------------------
    // 3. MEMBER (ÜYE) DAVRANIŞI (Rol: 1)
    // ------------------------------------------------------------------
    if (myRole == 1) {
        std::string pktName = packet->getName();

        if (pktName == "InterClusterData") {
            if (foreignNeighbors.empty()) return;

            for (auto const& [neighborId, foreignCH] : foreignNeighbors) {
                 Packet *outPkt = new Packet("GatewayForward");
                 outPkt->insertAtBack(makeShared<LccData>(*dataPkt));

                 std::string gwNeighborName = "host[" + std::to_string(neighborId) + "]";
                 L3Address neighborAddr = L3AddressResolver().resolve(gwNeighborName.c_str());
                 socket.sendTo(outPkt, neighborAddr, destPort);
            }
            return;
        }

        if (myClusterHeadId != -1) {
            Packet *relayPkt = new Packet("RelayToCH");
            relayPkt->insertAtBack(makeShared<LccData>(*dataPkt));

            std::string chName = "host[" + std::to_string(myClusterHeadId) + "]";
            L3Address chAddr = L3AddressResolver().resolve(chName.c_str());
            socket.sendTo(relayPkt, chAddr, destPort);
        }
        return;
    }

    // ------------------------------------------------------------------
    // 4. CLUSTER HEAD (LİDER) DAVRANIŞI (Rol: 2)
    // ------------------------------------------------------------------
    if (myRole == 2) {

        // A) Hedef Benim Üyem mi? (Local Delivery)
        if (neighborsLastSeen.find(dataPkt->getDestId()) != neighborsLastSeen.end()) {
            Packet *finalPkt = new Packet("FinalDelivery");
            finalPkt->insertAtBack(makeShared<LccData>(*dataPkt));

            std::string destName = "host[" + std::to_string(dataPkt->getDestId()) + "]";
            L3Address destAddr = L3AddressResolver().resolve(destName.c_str());
            socket.sendTo(finalPkt, destAddr, destPort);
            return;
        }

        // B) HEDEF BAŞKA CLUSTER'DA -> AKILLI GATEWAY SEÇİMİ (SMART FLOODING)
        bool sentViaGateway = false;

        // Bu turda paket gönderdiğimiz Gateway'lerin ID'lerini tutalım.
        // Bir node'a aynı paket için sadece BİR KERE görev verilir.
        std::vector<int> usedGateways;

        for (auto const& [targetClusterId, gateways] : clusterRoutingTable) {
            if (gateways.empty()) continue;

            int selectedGw = -1;

            // Aday listesini kopyala (orijinalini bozmayalım)
            std::vector<int> candidates = gateways;

            // Adayları rastgele sırayla dene (Yük dağılımı için)
            while (!candidates.empty()) {
                // Rastgele bir aday seç
                int randIndex = intuniform(0, candidates.size() - 1);
                int candidateId = candidates[randIndex];

                bool alreadyUsed = false;
                for (int u : usedGateways) {
                    if (u == candidateId) { alreadyUsed = true; break; }
                }

                if (!alreadyUsed) {
                    selectedGw = candidateId;
                    break;
                } else {
                    candidates.erase(candidates.begin() + randIndex);
                }
            }

            if (selectedGw != -1) {
                Packet *interClusterPkt = new Packet("InterClusterData");
                interClusterPkt->insertAtBack(makeShared<LccData>(*dataPkt));

                std::string gwName = "host[" + std::to_string(selectedGw) + "]";
                L3Address gwAddr = L3AddressResolver().resolve(gwName.c_str());
                socket.sendTo(interClusterPkt, gwAddr, destPort);

                usedGateways.push_back(selectedGw);
                sentViaGateway = true;

            }
            else {

                if (!gateways.empty()) {
                    sentViaGateway = true;
                }
            }
        }

        // Eğer hiçbir Gateway bulamadıysam ve Member'ım da yoksa:
        if (!sentViaGateway) {
            if (myMembers.empty()) {

                if (!neighborsLastSeen.empty()) {

                    int luckyNeighborId = neighborsLastSeen.begin()->first;

                    Packet *rescuePkt = new Packet("EmergencyRelay");
                    rescuePkt->insertAtBack(makeShared<LccData>(*dataPkt));

                    std::string neighborName = "host[" + std::to_string(luckyNeighborId) + "]";
                    L3Address neighborAddr = L3AddressResolver().resolve(neighborName.c_str());
                    socket.sendTo(rescuePkt, neighborAddr, destPort);

                }
            }
        }
    }
}

// ------------------------------------------------------------------
// STANDART LCC FONKSİYONLARI
// ------------------------------------------------------------------
void LCC::checkTimeouts()
{
    simtime_t now = simTime();
    bool myClusterHeadLost = false;

    // Normal Komşular
    for (auto it = neighborsLastSeen.begin(); it != neighborsLastSeen.end(); ) {
        if (now - it->second > neighborValidityInterval) {
            if (myRole == 1 && it->first == myClusterHeadId) myClusterHeadLost = true;
            neighborsRoles.erase(it->first);
            it = neighborsLastSeen.erase(it);
        } else { ++it; }
    }

    // Yabancı Komşular
    for (auto it = foreignNeighbors.begin(); it != foreignNeighbors.end(); ) {
        if (neighborsLastSeen.find(it->first) == neighborsLastSeen.end()) {
            it = foreignNeighbors.erase(it);
        } else { ++it; }
    }

    if (foreignNeighbors.empty()) isGateway = false;

    if (myClusterHeadLost) {
        if (myRole == 2) emit(chLifetimeSignal, simTime() - chStartTime);
        if (myRole != 0) emit(chChangeSignal, 1);
        myRole = 0;
        myClusterHeadId = -1;
    }
    runLCCLogic();
    updateVisuals();
}

void LCC::runLCCLogic()
{
    int oldRole = myRole;
    if (myRole == 0) {
        int lowestId = myId;
        for (auto const& [neighborId, lastSeen] : neighborsLastSeen) {
            if (neighborId < lowestId) lowestId = neighborId;
        }
        if (lowestId == myId) {
            myRole = 2; myClusterHeadId = myId; chStartTime = simTime();
        } else {
            myRole = 1; myClusterHeadId = lowestId;
        }
    }
    else if (myRole == 2) {
        int memberCount = 0;
        for (auto const& [neighborId, role] : neighborsRoles) {
            if (role == 1) memberCount++;
            if (role == 2 && neighborId < myId) {
                myRole = 1; myClusterHeadId = neighborId;
                emit(chLifetimeSignal, simTime() - chStartTime);
                break;
            }
        }
        emit(clusterSizeSignal, memberCount);
    }
    if (oldRole != myRole)
        numRoleChanges++;
}

void LCC::updateVisuals()
{
    if (!getParentModule()->getCanvas()) return;
    const char* color = "gray";
    if (myRole == 2) color = "red";
    else if (myRole == 1) {
        if (isGateway) color = "blue"; else color = "green";
    }
    getParentModule()->getDisplayString().setTagArg("i", 1, color);
}

void LCC::finish()
{
    // 1. PDR Hesabı
    double pdr = (numSent > 0) ? (double)numReceived / numSent * 100.0 : 0.0;

    // 2. Ortalama Gecikme (Saniye)
    double avgDelay = (numReceived > 0) ? totalDelay / numReceived : 0.0;

    // 3. Throughput (Bits per Second - bps)
    double throughput = (simTime().dbl() > 0) ? (totalBytesReceived * 8.0) / simTime().dbl() : 0.0;

    // 4. Dosyaya Yazma (Append Modu)
    std::ofstream resultFile;
    resultFile.open("manual_results.csv", std::ios::out | std::ios::app);

    // NodeID, Sent, Received, PDR, AvgDelay, Throughput, BeaconsSent, RoleChanges
    if (resultFile.is_open()) {
        resultFile << myId << ","
                   << numSent << ","
                   << numReceived << ","
                   << pdr << ","
                   << avgDelay << ","
                   << throughput << ","
                   << numBeaconsSent << ","
                   << numRoleChanges << "\n";
        resultFile.close();
    }
}

} // namespace
