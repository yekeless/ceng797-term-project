/*
 * LCC.cc
 *
 * Created on: Nov 23, 2025
 * Author: yekeles
 * Updated for Phase 4 (Hybrid Data Transmission)
 */

#include "LCC.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include <string>
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
        // Parametreleri oku
        beaconInterval = par("beaconInterval");
        neighborValidityInterval = par("neighborValidityInterval");
        localPort = par("localPort");
        destPort = par("destPort");

        // --- YENİ: Phase 4 Parametreleri ---
        useMulticast = par("useMulticast");
        numHosts = par("numHosts");
        // ----------------------------------

        myId = getParentModule()->getIndex();
        myRole = 0;
        myClusterHeadId = -1;
        chStartTime = SIMTIME_ZERO;

        // Timerlar
        beaconTimer = new cMessage("beaconTimer");
        checkTimeoutTimer = new cMessage("checkTimeoutTimer");
        dataTimer = new cMessage("dataTimer");

        // Sinyalleri Kaydet
        chChangeSignal = registerSignal("chChangeSignal");
        chLifetimeSignal = registerSignal("chLifetimeSignal");
        controlOverheadSignal = registerSignal("controlOverheadSignal");
        clusterSizeSignal = registerSignal("clusterSizeSignal");
        rttSignal = registerSignal("rttSignal");
        pdrSignal = registerSignal("pdrSignal");
        dataSentSignal = registerSignal("dataSentSignal");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localPort);
        socket.setCallback(this);

        // Multicast Grubuna Katıl
        L3Address mcastAddr = L3AddressResolver().resolve("224.0.0.1");
        socket.joinMulticastGroup(mcastAddr);

        // Başlat
        scheduleAt(simTime() + uniform(0, 2), beaconTimer);
        scheduleAt(simTime() + beaconInterval, checkTimeoutTimer);
        scheduleAt(simTime() + uniform(1, 3), dataTimer); // Data trafiği başlasın

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
        sendDataPacket(); // Veri gönder
        scheduleAt(simTime() + uniform(0.5, 1.5), dataTimer); // Sık sık data at
    }
    else {
        socket.processMessage(msg);
    }
}

void LCC::sendBeacon()
{
    Packet *packet = new Packet("LccBeacon");
    auto beacon = makeShared<LccBeacon>();

    beacon->setSrcId(myId);
    beacon->setRole(static_cast<LccRole>(myRole));
    beacon->setClusterHeadId(myClusterHeadId);
    beacon->setChunkLength(B(100));

    packet->insertAtBack(beacon);

    // Overhead Say
    emit(controlOverheadSignal, 1);

    L3Address destAddr = L3AddressResolver().resolve("224.0.0.1");
    socket.sendTo(packet, destAddr, destPort);
}

void LCC::sendDataPacket()
{
    // Paket Hazırlığı
    Packet *packet = new Packet("LccData");
    auto data = makeShared<LccData>();
    data->setSrcId(myId);
    data->setSendTime(simTime());
    data->setChunkLength(B(1024)); // 1 KB Veri

    L3Address destAddr;

    // --- PHASE 3 vs PHASE 4 AYRIMI ---
    if (useMulticast) {
        // --- MOD 1: PHASE 3 (Multicast Hilesi) ---
        if (neighborsLastSeen.empty()) { delete packet; return; }

        // Rastgele bir komşu seç (İstatistik hedefi olarak)
        auto it = neighborsLastSeen.begin();
        std::advance(it, intuniform(0, neighborsLastSeen.size() - 1));
        data->setDestId(it->first);

        // Adres: Multicast Grubu (Herkes duyar, sahibi alır)
        destAddr = L3AddressResolver().resolve("224.0.0.1");
    }
    else {
        // --- MOD 2: PHASE 4 (Unicast / AODV) ---
        // Rastgele HERHANGİ bir node seç (Komşu olması şart değil)
        // AODV yolu bulacaktır.
        int randomNodeIndex = intuniform(0, numHosts - 1);

        while (randomNodeIndex == myId) {
            randomNodeIndex = intuniform(0, numHosts - 1);
        }

        data->setDestId(randomNodeIndex);

        // İsimden IP Çözümleme (Örn: "host[5]" -> 10.0.0.x)
        std::string destName = "host[" + std::to_string(randomNodeIndex) + "]";
        destAddr = L3AddressResolver().resolve(destName.c_str());
    }
    emit(dataSentSignal, 1); // "Bir paket yola çıktı" diye not al
    packet->insertAtBack(data);
    socket.sendTo(packet, destAddr, destPort);
}

void LCC::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    // 1. Önce paketin içini "Genel Parça" (Chunk) olarak al
    auto chunk = packet->peekAtFront<Chunk>();

    // ---------------------------------------------------------
    // SENARYO A: Gelen paket bir DATA paketi mi?
    // ---------------------------------------------------------
    if (auto dataPkt = dynamicPtrCast<const LccData>(chunk)) {
        processDataPacket(dataPkt);
        delete packet;
        return;
    }

    // ---------------------------------------------------------
    // SENARYO B: Gelen paket bir BEACON paketi mi?
    // ---------------------------------------------------------
    if (auto beacon = dynamicPtrCast<const LccBeacon>(chunk)) {

        // Kendi yankımı duyduysam yoksay
        if (beacon->getSrcId() == myId) {
            delete packet;
            return;
        }

        // Gateway Kontrolü (Başka kümeyi mi duydum?)
        if (beacon->getClusterHeadId() != myClusterHeadId && beacon->getClusterHeadId() != -1) {
            foreignNeighbors[beacon->getSrcId()] = beacon->getClusterHeadId();
            isGateway = true;
        }

        // Hafızayı güncelle
        neighborsLastSeen[beacon->getSrcId()] = simTime();
        neighborsRoles[beacon->getSrcId()] = beacon->getRole();

        updateVisuals();
        delete packet;
        return;
    }

    // Tanınmayan paket
    delete packet;
}

void LCC::processDataPacket(const Ptr<const LccData>& dataPkt)
{
    // Paket bana mı gelmiş?
    if (dataPkt->getDestId() == myId) {
        // PDR Başarılı (1.0)
        emit(pdrSignal, 1.0);

        // Delay Hesapla
        simtime_t delay = simTime() - dataPkt->getSendTime();
        emit(rttSignal, delay);
    }
}

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
        } else {
            ++it;
        }
    }

    // Yabancı Komşular (Gateway için)
    for (auto it = foreignNeighbors.begin(); it != foreignNeighbors.end(); ) {
        if (neighborsLastSeen.find(it->first) == neighborsLastSeen.end()) {
            it = foreignNeighbors.erase(it);
        } else {
            ++it;
        }
    }

    if (foreignNeighbors.empty()) isGateway = false;

    if (myClusterHeadLost) {
        if (myRole == 2) emit(chLifetimeSignal, simTime() - chStartTime); // Lifetime kaydet
        if (myRole != 0) emit(chChangeSignal, 1); // Değişim kaydet

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
            myRole = 2; // CH
            myClusterHeadId = myId;
            chStartTime = simTime(); // Başla
        } else {
            myRole = 1; // Member
            myClusterHeadId = lowestId;
        }
    }
    else if (myRole == 2) {
        int memberCount = 0;
        for (auto const& [neighborId, role] : neighborsRoles) {
            if (role == 1) memberCount++; // Basit Member sayımı

            if (role == 2 && neighborId < myId) {
                myRole = 1;
                myClusterHeadId = neighborId;
                emit(chLifetimeSignal, simTime() - chStartTime); // Bitir
                break;
            }
        }
        emit(clusterSizeSignal, memberCount); // Küme boyutu kaydet
    }

    if (oldRole != myRole) emit(chChangeSignal, 1);
}

void LCC::updateVisuals()
{
    if (!getParentModule()->getCanvas()) return;
    const char* color = "gray";
    if (myRole == 2) color = "red";
    else if (myRole == 1) {
        if (isGateway) color = "blue";
        else color = "green";
    }
    getParentModule()->getDisplayString().setTagArg("i", 1, color);
}

void LCC::finish() {
    ApplicationBase::finish();
}

} // namespace
