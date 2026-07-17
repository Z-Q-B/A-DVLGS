#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>

#include "veins/modules/application/ieee80211p/DemoBaseApplLayer.h"

namespace advlgs {

class ADVLGSMessage;

class CryptoRSU11p : public veins::DemoBaseApplLayer {
public:
    ~CryptoRSU11p() override;
    void initialize(int stage) override;
    void finish() override;

protected:
    struct QueuedVerification {
        ADVLGSMessage* msg = nullptr;
        omnetpp::simtime_t arrivalAt = omnetpp::simtime_t(0);
    };

    std::string serviceDomain;
    std::string cryptoScheme;
    int signedMessageBytes = 128;
    bool enableBatchVerification = true;
    // Zero means no item-count cap inside one batch window. Positive values
    // enable an optional hard cap for a batch window.
    int batchMaxSize = 0;
    omnetpp::simtime_t batchWindow = omnetpp::simtime_t(0.02);
    omnetpp::simtime_t serviceRequestTtl = omnetpp::simtime_t(1);
    bool useFixedCryptoTiming = false;
    bool skipCryptoComputation = false;
    double fixedVerifyDelaySeconds = 0;
    double fixedBatchVerifyDelayPerItemSeconds = 0;
    std::map<int, double> fixedBatchVerifyDelayBySizeSeconds;
    std::uint64_t cryptoReceivedPackets = 0;
    std::uint64_t cryptoRejectedPackets = 0;
    std::uint64_t cryptoDeadlineExpiredPackets = 0;
    std::uint64_t cryptoArrivalExpiredPackets = 0;
    std::uint64_t cryptoQueueTimeoutPackets = 0;
    std::uint64_t cryptoBatchTimeoutPackets = 0;
    std::uint64_t cryptoReceivedBytes = 0;
    std::uint64_t cryptoBatches = 0;
    std::uint64_t cryptoBatchVerifiedItems = 0;
    std::uint64_t maxVerifyQueueLength = 0;
    std::uint64_t maxBatchSizeObserved = 0;
    std::map<int, std::uint64_t> batchSizeHistogram;
    omnetpp::simtime_t verifierBusyUntil = omnetpp::simtime_t(0);
    omnetpp::cMessage* batchFlushEvent = nullptr;
    bool forceImmediateFlush = false;
    std::deque<QueuedVerification*> verifyQueue;
    std::set<omnetpp::cMessage*> pendingVerifyEvents;
    double totalVerifySeconds = 0;
    double totalVerifyQueueSeconds = 0;
    double totalBatchWaitSeconds = 0;
    double totalNetworkDelaySeconds = 0;
    double totalValidationDelaySeconds = 0;
    bool repeatMessages = true;

    void onWSA(veins::DemoServiceAdvertisment* wsa) override;
    void onWSM(veins::BaseFrame1609_4* frame) override;
    void handleSelfMsg(omnetpp::cMessage* msg) override;
    void queueVerification(class ADVLGSMessage* msg);
    void flushVerificationBatch();
    void startNextVerificationBatch();
    void completeVerificationBatch(omnetpp::cMessage* event);
    void dropExpiredQueuedMessages();
    void scheduleVerificationFlush(omnetpp::simtime_t when, bool forceImmediate);
    omnetpp::simtime_t latestSingletonStart(const QueuedVerification* queued) const;
    bool isExpired(const class ADVLGSMessage* msg) const;
    std::string messagePayload(const class ADVLGSMessage* msg) const;
};

} // namespace advlgs
