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

    // --- Komşuluk Tabloları ---
    std::map<int, simtime_t> neighborsLastSeen;
    std::map<int, int> neighborsRoles;

    // --- INET Araçları ---
    UdpSocket socket;
    cMessage *beaconTimer = nullptr;
    cMessage *checkTimeoutTimer = nullptr;

    // --- Parametreler ---
    simtime_t beaconInterval;
    simtime_t neighborValidityInterval;
    int localPort;
    int destPort;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;

    // --- INET 4.5.4 Uyumlu UDP Callback Fonksiyonları ---
    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketClosed(UdpSocket *socket) override {}
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override {}

    // --- Lifecycle (Yaşam Döngüsü) Fonksiyonları ---
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    // --- LCC Fonksiyonları ---
    void sendBeacon();
    void checkTimeouts();
    void runLCCLogic();
    void updateVisuals();

  public:
    LCC();
    virtual ~LCC();
};

} // namespace inet

#endif
