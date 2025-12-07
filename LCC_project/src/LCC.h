/*
 * LCC.h
 *
 *  Created on: Nov 23, 2025
 *      Author: yekeles
 */
#ifndef __LCC_PROJECT_LCC_H
#define __LCC_PROJECT_LCC_H

#include "inet/common/INETDefs.h"
#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/common/lifecycle/LifecycleOperation.h"
#include "LCCMessage_m.h"

namespace inet {

class LCC : public ApplicationBase, public UdpSocket::ICallback
{
  protected:
    // --- Durum Değişkenleri ---
    int myId;
    int myRole; // 0: Undecided, 1: Member, 2: CH
    int myClusterHeadId;

    bool useMulticast;
    int numHosts;

    // Gateway
    bool isGateway = false;
    std::map<int, int> foreignNeighbors; // Komşu ID -> Onun CH ID

    // Komşuluk Tabloları
    std::map<int, simtime_t> neighborsLastSeen;
    std::map<int, int> neighborsRoles;

    // INET Araçları
    UdpSocket socket;
    cMessage *beaconTimer = nullptr;
    cMessage *checkTimeoutTimer = nullptr;
    cMessage *dataTimer = nullptr; // YENİ: Data göndermek için

    // Parametreler
    simtime_t beaconInterval;
    simtime_t neighborValidityInterval;
    int localPort;
    int destPort;

    // --- İstatistik Sinyalleri (YENİ) ---
    simsignal_t chChangeSignal;
    simsignal_t chLifetimeSignal;
    simsignal_t controlOverheadSignal;
    simsignal_t clusterSizeSignal;
    simsignal_t rttSignal;
    simsignal_t pdrSignal;
    simsignal_t dataSentSignal;

    simtime_t chStartTime; // Lifetime hesabı için

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;

    // UDP Callback
    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketClosed(UdpSocket *socket) override {}
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override {}

    // Lifecycle
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    // LCC ve Data Fonksiyonları
    void sendBeacon();
    void sendDataPacket();
    void processDataPacket(const Ptr<const LccData>& dataPkt);
    void checkTimeouts();
    void runLCCLogic();
    void updateVisuals();

  public:
    LCC();
    virtual ~LCC();
};

} // namespace inet

#endif
