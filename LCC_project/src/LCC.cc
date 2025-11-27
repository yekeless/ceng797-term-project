/*
 * LCC.cc
 *
 *  Created on: Nov 23, 2025
 *      Author: yekeles
 */

#include "LCC.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"

namespace inet {

Define_Module(LCC);

LCC::LCC() {
    beaconTimer = nullptr;
    checkTimeoutTimer = nullptr;
}

LCC::~LCC() {
    cancelAndDelete(beaconTimer);
    cancelAndDelete(checkTimeoutTimer);
}

void LCC::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        beaconInterval = par("beaconInterval");
        neighborValidityInterval = par("neighborValidityInterval");
        localPort = par("localPort");
        destPort = par("destPort");

        myId = getParentModule()->getIndex();
        myRole = 0;
        myClusterHeadId = -1;

        beaconTimer = new cMessage("beaconTimer");
        checkTimeoutTimer = new cMessage("checkTimeoutTimer");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
            socket.setOutputGate(gate("socketOut"));
            socket.bind(localPort); // Port 5000'i açtık
            socket.setCallback(this);

            // --- BU KISMI EKLE (Multicast Grubuna Katıl) ---
            // Node'a diyoruz ki: "224.0.0.1 adresine gelen mektupları da kabul et"
            L3Address mcastAddr = L3AddressResolver().resolve("224.0.0.1");
            socket.joinMulticastGroup(mcastAddr);
            // -----------------------------------------------

            scheduleAt(simTime() + uniform(0, 2), beaconTimer);
            scheduleAt(simTime() + beaconInterval, checkTimeoutTimer);

            updateVisuals();
        }
}

// --- Lifecycle Fonksiyonları (Compiler Hatası İçin Eklendi) ---
void LCC::handleStartOperation(LifecycleOperation *operation) {
    // Başlangıçta özel bir şey yapmamıza gerek yok, initialize hallediyor
}
void LCC::handleStopOperation(LifecycleOperation *operation) {
    // Durdurulunca timerları iptal et
    cancelEvent(beaconTimer);
    cancelEvent(checkTimeoutTimer);
    socket.close();
}
void LCC::handleCrashOperation(LifecycleOperation *operation) {
    // Çökme durumunda yapılacaklar
    if (operation->getRootModule() != getContainingNode(this))
        socket.destroy(); // Sadece biz değilsek socketi yok et
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
    beacon->setChunkLength(B(100)); // Pakete sembolik bir boyut verelim

    packet->insertAtBack(beacon);

    L3Address destAddr = L3AddressResolver().resolve("224.0.0.1");
    socket.sendTo(packet, destAddr, destPort);
}

// İsim düzeltildi: onDataArrived -> socketDataArrived
void LCC::socketDataArrived(UdpSocket *socket, Packet *packet)
{

    // ----------------------
    EV << "Paket ALINDI! Kimden: " << packet->peekAtFront<LccBeacon>()->getSrcId() << endl;
    // ----------------------
    auto beacon = packet->peekAtFront<LccBeacon>();
    if (!beacon) {
        delete packet;
        return;
    }

    int senderId = beacon->getSrcId();
    if (senderId == myId) {
        delete packet;
        return;
    }

    neighborsLastSeen[senderId] = simTime();
    neighborsRoles[senderId] = beacon->getRole();

    delete packet;
}

void LCC::checkTimeouts()
{
    simtime_t now = simTime();
    bool myClusterHeadLost = false;

    for (auto it = neighborsLastSeen.begin(); it != neighborsLastSeen.end(); ) {
        if (now - it->second > neighborValidityInterval) {
            if (myRole == 1 && it->first == myClusterHeadId) {
                myClusterHeadLost = true;
            }
            neighborsRoles.erase(it->first);
            it = neighborsLastSeen.erase(it);
        } else {
            ++it;
        }
    }

    if (myClusterHeadLost) {
        myRole = 0;
        myClusterHeadId = -1;
    }

    runLCCLogic();
    updateVisuals();
}

void LCC::runLCCLogic()
{
    // --- DEBUG LOG BAŞLANGICI ---
    EV << "LCC Mantığı Çalışıyor. ID: " << myId << " Rol: " << myRole << " Komşu Sayısı: " << neighborsRoles.size() << endl;
    for (auto const& [id, role] : neighborsRoles) {
        EV << "  -- Komşu ID: " << id << " Rolü: " << role << endl;
    }
    // ----------------------------

    // 1. UNDECIDED (Kararsız) Durumu
    if (myRole == 0) {
        int lowestId = myId;
        for (auto const& [neighborId, lastSeen] : neighborsLastSeen) {
            if (neighborId < lowestId) lowestId = neighborId;
        }

        if (lowestId == myId) {
            myRole = 2; // CH
            myClusterHeadId = myId;
            EV << "  -> Kendimi CH ilan ettim!" << endl; // Log
        } else {
            myRole = 1; // Member
            myClusterHeadId = lowestId;
            EV << "  -> " << lowestId << " numaralı node'a üye oldum." << endl; // Log
        }
    }

    // 2. CLUSTER HEAD Durumu
    else if (myRole == 2) {
        for (auto const& [neighborId, role] : neighborsRoles) {
            // Eğer komşu da CH ise (Role == 2)
            if (role == 2) {
                EV << "  -> Rakip CH gördüm! ID: " << neighborId << endl; // Log

                if (neighborId < myId) {
                    myRole = 1; // İstifa et
                    myClusterHeadId = neighborId;
                    EV << "  -> O benden daha kıdemli. İstifa edip Member oluyorum." << endl; // Log
                    break;
                }
            }
        }
    }
    // 3. MEMBER Durumu -> Değişim yok
}

void LCC::updateVisuals()
{
    if (!getParentModule()->getCanvas()) return;

    const char* color = (myRole == 2) ? "red" : (myRole == 1 ? "green" : "gray");
    getParentModule()->getDisplayString().setTagArg("t", 0, (myRole == 2 ? "CH" : "M"));
    getParentModule()->getDisplayString().setTagArg("i", 1, color);
}

void LCC::finish() {
    ApplicationBase::finish();
}

} // namespace

