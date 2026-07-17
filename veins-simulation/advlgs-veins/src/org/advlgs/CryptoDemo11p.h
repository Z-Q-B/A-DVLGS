#pragma once

#include <cstdint>
#include <set>
#include <string>

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"

namespace advlgs {

class CryptoDemo11p : public veins::DemoBaseApplLayer {
public:
    ~CryptoDemo11p() override;
    void initialize(int stage) override;
    void finish() override;

protected:
    omnetpp::simtime_t lastDroveAt;
    omnetpp::cMessage* announcementTimer = nullptr;
    bool sentMessage = false;
    bool experimentMode = false;
    double announcementProbability = 1.0;
    omnetpp::simtime_t announcementInterval;
    omnetpp::simtime_t announcementStopTime;
    int fixedMessageBytes = 0;
    int signedMessageBytes = 128;
    bool useFixedCryptoTiming = false;
    bool skipCryptoComputation = false;
    double fixedSignDelaySeconds = 0;
    double fixedVerifyDelaySeconds = 0;
    int currentSubscribedServiceId = -1;
    int announcementSerial = 0;
    std::string serviceDomain;
    std::string cryptoScheme;

    std::uint64_t cryptoSentPackets = 0;
    std::uint64_t cryptoSentBytes = 0;
    std::uint64_t cryptoSignatureBytes = 0;
    std::uint64_t cryptoReceivedPackets = 0;
    std::uint64_t cryptoRejectedPackets = 0;
    std::uint64_t cryptoReceivedBytes = 0;
    std::uint64_t expectedRangeReceipts = 0;
    std::uint64_t expectedVehicleRangeReceipts = 0;
    std::uint64_t expectedRsuRangeReceipts = 0;
    double communicationRangeMeters = 300.0;
    omnetpp::simtime_t signerBusyUntil = omnetpp::simtime_t(0);
    omnetpp::simtime_t verifierBusyUntil = omnetpp::simtime_t(0);
    std::set<omnetpp::cMessage*> pendingSignedSendEvents;
    std::set<omnetpp::cMessage*> pendingVerifyEvents;
    double totalSignSeconds = 0;
    double totalVerifySeconds = 0;
    double totalSignQueueSeconds = 0;
    double totalVerifyQueueSeconds = 0;
    double totalNetworkDelaySeconds = 0;
    double totalValidationDelaySeconds = 0;

    void onWSM(veins::BaseFrame1609_4* frame) override;
    void onWSA(veins::DemoServiceAdvertisment* wsa) override;
    void handleSelfMsg(omnetpp::cMessage* msg) override;
    void handlePositionUpdate(omnetpp::cObject* obj) override;

    void signAndSend(class ADVLGSMessage* msg);
    void completeSignedSend(omnetpp::cMessage* event);
    void sendAnnouncement();
    void queueVerification(class ADVLGSMessage* msg);
    void completeVerification(omnetpp::cMessage* event);
    void handleAcceptedMessage(class ADVLGSMessage* msg);
    void countPotentialRecipientsInRange(std::uint64_t& vehicleRecipients, std::uint64_t& rsuRecipients) const;
    std::string messagePayload(const class ADVLGSMessage* msg) const;
};

} // namespace advlgs
