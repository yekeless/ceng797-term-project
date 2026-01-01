#ifndef __LCC_H_
#define __LCC_H_

#include <omnetpp.h>
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/applications/base/ApplicationBase.h"
// -----------------------------
#include "LCCMessage_m.h"
#include <map>
#include <vector>
#include <string>

using namespace omnetpp;

namespace inet {

class LCC : public ApplicationBase, public UdpSocket::ICallback
{
  protected:
    // --- Parametreler ---
    simtime_t beaconInterval;
    simtime_t neighborValidityInterval;
    int localPort, destPort;
    bool useMulticast;
    int numHosts;
    int numSent = 0;         // Gönderdiğim toplam veri paketi
    double totalDelay;       // Toplam gecikme süresi (saniye cinsinden)
    long totalBytesReceived; // Toplam alınan veri boyutu (Throughput için)
    long numBeaconsSent;     // Gönderdiğim Beacon sayısı (Overhead için)
    int numReceived = 0;
    int numRoleChanges;
    // --- Durum Değişkenleri ---
    int myId;
    int myRole;          // 0: Undecided, 1: Member, 2: CH
    int myClusterHeadId; // Bağlı olduğum lider
    simtime_t chStartTime;
    bool isGateway = false;

    // --- Ağ Bilgisi ---
    std::map<int, simtime_t> neighborsLastSeen;
    std::map<int, int> neighborsRoles;
    std::map<int, int> foreignNeighbors;
    std::vector<int> myMembers;

    // --- ROUTING ---
    std::map<int, std::vector<int>> clusterRoutingTable;
    std::map<std::string, simtime_t> seenPackets;
    int seqNum = 0;

    // --- Timer ve Socket ---
    cMessage *beaconTimer;
    cMessage *checkTimeoutTimer;
    cMessage *dataTimer;
    UdpSocket socket;

    // --- Sinyaller ---
    simsignal_t chChangeSignal;
    simsignal_t chLifetimeSignal;
    simsignal_t controlOverheadSignal;
    simsignal_t clusterSizeSignal;
    simsignal_t rttSignal;
    simsignal_t pdrSignal;
    simsignal_t dataSentSignal;
    simsignal_t dataReceivedSignal;

  public:
    LCC();
    virtual ~LCC();

  protected:
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;

    // Lifecycle
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    // Logic
    void sendBeacon();
    void sendDataPacket();
    void checkTimeouts();
    void runLCCLogic();
    void updateVisuals();

    // Socket & Packet Processing
    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override { delete indication; }
    virtual void socketClosed(UdpSocket *socket) override {}

    void processDataPacket(Packet *packet, const Ptr<const LccData>& dataPkt);
};

} // namespace inet

#endif
