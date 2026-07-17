#include "org/advlgs/CryptoRSU11p.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <omnetpp/cconfiguration.h>
#include <sstream>
#include <vector>

#include "org/advlgs/ADVLGSMessage_m.h"
#include "org/advlgs/advlgs_miracl.h"
#include "org/advlgs/bbs_miracl.h"
#include "org/advlgs/clgs_miracl.h"
#include "org/advlgs/erca_miracl.h"
#include "org/advlgs/mlgs_miracl.h"

using namespace omnetpp;
using namespace veins;

namespace advlgs {

namespace {

constexpr short VERIFY_COMPLETE_EVT = 121;
constexpr short BATCH_FLUSH_EVT = 122;

struct VerificationOutcome {
    ADVLGSMessage* msg = nullptr;
    bool ok = false;
    std::string tag;
    simtime_t arrivalAt = SIMTIME_ZERO;
    simtime_t verifyStartedAt = SIMTIME_ZERO;
};

struct BatchVerification {
    std::vector<VerificationOutcome> items;
    double verifySeconds = 0;
    simtime_t verifyStartedAt = SIMTIME_ZERO;
};

bool isAdvlgsScheme(const std::string& scheme)
{
    return scheme == "A-DVLGS" || scheme == "ADVLGS";
}

bool isMlgsScheme(const std::string& scheme)
{
    return scheme == "MLGS";
}

bool isErcaScheme(const std::string& scheme)
{
    return scheme == "ERCA";
}

std::string trim(const std::string& value)
{
    auto begin = value.begin();
    while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
    auto end = value.end();
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) --end;
    return std::string(begin, end);
}

double parseDurationSeconds(std::string value)
{
    value = trim(value);
    if (value.empty()) return 0;
    if (value.size() >= 2 && value.substr(value.size() - 2) == "ms") {
        return std::stod(trim(value.substr(0, value.size() - 2))) / 1000.0;
    }
    if (value.back() == 's') {
        return std::stod(trim(value.substr(0, value.size() - 1)));
    }
    return std::stod(value);
}

std::map<int, double> parseBatchDelayTable(const std::string& text)
{
    std::map<int, double> table;
    std::stringstream entries(text);
    std::string entry;
    while (std::getline(entries, entry, ',')) {
        const auto sep = entry.find(':');
        if (sep == std::string::npos) continue;
        const int batchSize = std::stoi(trim(entry.substr(0, sep)));
        const double seconds = parseDurationSeconds(entry.substr(sep + 1));
        if (batchSize > 0 && seconds >= 0) table[batchSize] = seconds;
    }
    return table;
}

std::string displaySchemeName(const std::string& scheme)
{
    if (isAdvlgsScheme(scheme)) return "A-DVLGS";
    if (isMlgsScheme(scheme)) return "MLGS";
    if (isErcaScheme(scheme)) return "ERCA";
    if (scheme == "CLGS") return "CLGS";
    return scheme;
}

void warmupScheme(const std::string& scheme)
{
    if (scheme == "BBS") BBSMiracl::warmup();
    else if (scheme == "CLGS") CLGSMiracl::warmup();
    else if (isErcaScheme(scheme)) ERCAMiracl::warmup();
    else if (isMlgsScheme(scheme)) MLGSMiracl::warmup();
    else ADVLGSMiracl::warmup();
}

bool verifyOne(const std::string& scheme, const std::string& payload, const ADVLGSMessage* msg, std::string* tag)
{
    if (scheme == "BBS") return BBSMiracl::verify(payload, msg->getServiceDomain(), msg->getSignature(), tag);
    if (scheme == "CLGS") return CLGSMiracl::verify(payload, msg->getServiceDomain(), msg->getSignature(), tag);
    if (isErcaScheme(scheme)) return ERCAMiracl::verify(payload, msg->getServiceDomain(), msg->getSignature(), tag);
    if (isMlgsScheme(scheme)) return MLGSMiracl::verify(std::string("st|") + msg->getServiceDomain(), payload, msg->getServiceDomain(), msg->getSignature(), tag);
    return ADVLGSMiracl::verify(payload, msg->getServiceDomain(), msg->getSignature(), tag);
}

} // namespace

Define_Module(CryptoRSU11p);

CryptoRSU11p::~CryptoRSU11p()
{
    if (batchFlushEvent != nullptr) {
        cancelAndDelete(batchFlushEvent);
        batchFlushEvent = nullptr;
    }
    for (auto* queued : verifyQueue) {
        delete queued->msg;
        delete queued;
    }
    verifyQueue.clear();
    for (auto* event : pendingVerifyEvents) {
        auto* task = static_cast<BatchVerification*>(event->getContextPointer());
        if (task != nullptr) {
            for (auto& item : task->items) delete item.msg;
            delete task;
        }
        event->setContextPointer(nullptr);
        cancelAndDelete(event);
    }
    pendingVerifyEvents.clear();
}

void CryptoRSU11p::initialize(int stage)
{
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        serviceDomain = par("serviceDomain").stdstringValue();
        cryptoScheme = par("cryptoScheme").stdstringValue();
        warmupScheme(cryptoScheme);
        signedMessageBytes = par("signedMessageBytes");
        repeatMessages = par("repeatMessages").boolValue();
        enableBatchVerification = par("enableBatchVerification").boolValue();
        batchMaxSize = std::max(0, static_cast<int>(par("batchMaxSize")));
        batchWindow = par("batchWindow");
        serviceRequestTtl = par("serviceRequestTtl");
        useFixedCryptoTiming = par("useFixedCryptoTiming").boolValue();
        skipCryptoComputation = par("skipCryptoComputation").boolValue();
        fixedVerifyDelaySeconds = par("fixedVerifyDelay").doubleValue();
        fixedBatchVerifyDelayPerItemSeconds = par("fixedBatchVerifyDelayPerItem").doubleValue();
        fixedBatchVerifyDelayBySizeSeconds = parseBatchDelayTable(par("fixedBatchVerifyDelayBySize").stdstringValue());
        batchFlushEvent = new cMessage("asdRsuBatchFlush", BATCH_FLUSH_EVT);
    }
}

bool CryptoRSU11p::isExpired(const ADVLGSMessage* msg) const
{
    return serviceRequestTtl.dbl() > 0 && (simTime() - msg->getGeneratedAt()) > serviceRequestTtl;
}

void CryptoRSU11p::scheduleVerificationFlush(simtime_t when, bool forceImmediate)
{
    if (when < simTime()) when = simTime();
    forceImmediateFlush = forceImmediateFlush || forceImmediate;
    if (!batchFlushEvent->isScheduled()) {
        scheduleAt(when, batchFlushEvent);
    }
    else if (when < batchFlushEvent->getArrivalTime()) {
        cancelEvent(batchFlushEvent);
        scheduleAt(when, batchFlushEvent);
    }
}

simtime_t CryptoRSU11p::latestSingletonStart(const QueuedVerification* queued) const
{
    simtime_t readyAt = queued->arrivalAt + batchWindow;
    if (serviceRequestTtl <= SIMTIME_ZERO) return readyAt;

    const simtime_t expiresAt = queued->msg->getGeneratedAt() + serviceRequestTtl;
    const double estimatedVerifySeconds = useFixedCryptoTiming ? std::max(0.0, fixedVerifyDelaySeconds) : 0.0;
    const simtime_t deadlineStart = expiresAt - SimTime(estimatedVerifySeconds);
    return deadlineStart < readyAt ? deadlineStart : readyAt;
}

void CryptoRSU11p::finish()
{
    DemoBaseApplLayer::finish();
    std::uint64_t verifyBacklogDrops = verifyQueue.size();
    for (auto* event : pendingVerifyEvents) {
        auto* task = static_cast<BatchVerification*>(event->getContextPointer());
        verifyBacklogDrops += task == nullptr ? 1 : task->items.size();
    }
    const std::uint64_t completedChecks = cryptoReceivedPackets + cryptoRejectedPackets;
    const std::uint64_t failedChecks = cryptoRejectedPackets + cryptoDeadlineExpiredPackets + verifyBacklogDrops;
    const std::uint64_t attemptedChecks = cryptoReceivedPackets + failedChecks;
    const std::uint64_t verifierProcessedChecks = completedChecks + cryptoBatchTimeoutPackets;
    const double acceptanceRate = attemptedChecks == 0 ? 0 : cryptoReceivedPackets / static_cast<double>(attemptedChecks);
    const double packetLossRate = 1.0 - acceptanceRate;
    const double throughputBps = simTime() == SIMTIME_ZERO ? 0 : cryptoReceivedBytes * 8.0 / simTime().dbl();
    const double avgVerifyMs = verifierProcessedChecks == 0 ? 0 : totalVerifySeconds * 1000.0 / verifierProcessedChecks;
    const double avgNetworkDelayMs = cryptoReceivedPackets == 0 ? 0 : totalNetworkDelaySeconds * 1000.0 / cryptoReceivedPackets;
    const double queueMeasured = completedChecks + cryptoQueueTimeoutPackets + cryptoBatchTimeoutPackets;
    const double avgVerifyQueueMs = queueMeasured == 0 ? 0 : totalVerifyQueueSeconds * 1000.0 / queueMeasured;
    const double avgBatchWaitMs = queueMeasured == 0 ? 0 : totalBatchWaitSeconds * 1000.0 / queueMeasured;
    const double avgValidationDelayMs = cryptoReceivedPackets == 0 ? 0 : totalValidationDelaySeconds * 1000.0 / cryptoReceivedPackets;
    const double avgBatchSize = cryptoBatches == 0 ? 0 : cryptoBatchVerifiedItems / static_cast<double>(cryptoBatches);
    const double avgBatchVerifyMs = cryptoBatches == 0 ? 0 : totalVerifySeconds * 1000.0 / cryptoBatches;
    const auto* config = dynamic_cast<cConfigurationEx*>(getEnvir()->getConfig());
    const char* configName = config == nullptr ? "unknown" : config->getActiveConfigName();
    const std::string schemeName = displaySchemeName(cryptoScheme);

    recordScalar("advlgs.crypto.receivedPackets", cryptoReceivedPackets);
    recordScalar("advlgs.crypto.rejectedPackets", cryptoRejectedPackets);
    recordScalar("advlgs.crypto.deadlineExpiredPackets", cryptoDeadlineExpiredPackets);
    recordScalar("advlgs.crypto.arrivalExpiredPackets", cryptoArrivalExpiredPackets);
    recordScalar("advlgs.crypto.queueTimeoutPackets", cryptoQueueTimeoutPackets);
    recordScalar("advlgs.crypto.batchTimeoutPackets", cryptoBatchTimeoutPackets);
    recordScalar("advlgs.crypto.verifyBacklogDrops", verifyBacklogDrops);
    recordScalar("advlgs.crypto.receivedBytes", cryptoReceivedBytes);
    recordScalar("advlgs.crypto.packetAcceptanceRate", acceptanceRate);
    recordScalar("advlgs.metrics.Pmsg", acceptanceRate);
    recordScalar("advlgs.metrics.DmsgMs", avgValidationDelayMs);
    recordScalar("advlgs.crypto.throughputBps", throughputBps);
    recordScalar("advlgs.crypto.avgVerifyMs", avgVerifyMs);
    recordScalar("advlgs.crypto.avgBatchVerifyMs", avgBatchVerifyMs);
    recordScalar("advlgs.crypto.avgBatchSize", avgBatchSize);
    recordScalar("advlgs.crypto.batchCount", cryptoBatches);
    recordScalar("advlgs.crypto.batchVerifiedItems", cryptoBatchVerifiedItems);
    recordScalar("advlgs.crypto.maxBatchSizeObserved", maxBatchSizeObserved);
    recordScalar("advlgs.crypto.maxVerifyQueueLength", maxVerifyQueueLength);
    recordScalar("advlgs.net.avgNetworkDelayMs", avgNetworkDelayMs);
    recordScalar("advlgs.crypto.avgVerifyQueueMs", avgVerifyQueueMs);
    recordScalar("advlgs.crypto.avgBatchWaitMs", avgBatchWaitMs);

    const char* csvPath = "results/advlgs-metrics.csv";
    std::ifstream existing(csvPath);
    const bool needHeader = !existing.good() || existing.peek() == std::ifstream::traits_type::eof();
    std::ofstream csv(csvPath, std::ios::app);
    if (needHeader) {
        csv << "config,scheme,module,role,simTime,sentPackets,signBacklogDrops,sentBytes,avgMessageBytes,avgSignatureBytes,receivedPackets,rejectedPackets,deadlineExpiredPackets,arrivalExpiredPackets,queueTimeoutPackets,batchTimeoutPackets,verifyBacklogDrops,receivedBytes,expectedRangeReceipts,expectedVehicleRangeReceipts,expectedRsuRangeReceipts,rangeDeliveryRate,rangeLossRate,packetAcceptanceRate,packetLossRate,throughputBps,avgSignMs,avgVerifyMs,avgBatchVerifyMs,avgBatchSize,maxVerifyQueueLength,avgNetworkDelayMs,avgSignQueueMs,avgVerifyQueueMs,avgBatchWaitMs,Pmsg,DmsgMs\n";
    }
    csv << configName << ',' << schemeName << ',' << getFullPath() << ",rsu," << simTime() << ",0,0,0,0,0," << cryptoReceivedPackets << ','
        << cryptoRejectedPackets << ',' << cryptoDeadlineExpiredPackets << ',' << cryptoArrivalExpiredPackets << ',' << cryptoQueueTimeoutPackets << ','
        << cryptoBatchTimeoutPackets << ',' << verifyBacklogDrops << ',' << cryptoReceivedBytes << ",0,0,0,0,0," << acceptanceRate << ','
        << packetLossRate << ',' << throughputBps << ",0," << avgVerifyMs << ',' << avgBatchVerifyMs << ',' << avgBatchSize << ','
        << maxVerifyQueueLength << ',' << avgNetworkDelayMs << ",0," << avgVerifyQueueMs << ',' << avgBatchWaitMs << ','
        << acceptanceRate << ',' << avgValidationDelayMs << '\n';

    for (const auto& [batchSize, count] : batchSizeHistogram) {
        const std::string scalarName = "advlgs.crypto.batchSizeHistogram." + std::to_string(batchSize);
        recordScalar(scalarName.c_str(), count);
    }
}

std::string CryptoRSU11p::messagePayload(const ADVLGSMessage* msg) const
{
    std::ostringstream os;
    os << "data=" << msg->getDemoData()
       << "|sender=" << msg->getSenderAddress()
       << "|serial=" << msg->getSerial()
       << "|time=" << msg->getGeneratedAt();

    std::string payload = os.str();
    if (signedMessageBytes <= 0) return payload;

    const auto targetBytes = static_cast<std::size_t>(signedMessageBytes);
    if (payload.size() > targetBytes) payload.resize(targetBytes);
    else payload.append(targetBytes - payload.size(), '#');
    return payload;
}

void CryptoRSU11p::queueVerification(ADVLGSMessage* msg)
{
    if (isExpired(msg)) {
        cryptoArrivalExpiredPackets++;
        cryptoDeadlineExpiredPackets++;
        delete msg;
        return;
    }

    const bool queueWasEmpty = verifyQueue.empty();
    const bool verifierIdle = verifierBusyUntil <= simTime();

    auto* queued = new QueuedVerification();
    queued->msg = msg;
    queued->arrivalAt = simTime();
    verifyQueue.push_back(queued);
    maxVerifyQueueLength = std::max<std::uint64_t>(maxVerifyQueueLength, verifyQueue.size());

    const bool batchEnabled = isAdvlgsScheme(cryptoScheme) && enableBatchVerification;
    if (!batchEnabled) {
        // Baselines are strict FCFS single verification.
        scheduleVerificationFlush(verifierIdle ? simTime() : verifierBusyUntil, verifierIdle);
        return;
    }

    if (!verifierIdle) {
        // Requests arriving during service coalesce naturally in the FCFS queue.
        scheduleVerificationFlush(verifierBusyUntil, false);
        return;
    }

    if (queueWasEmpty || verifyQueue.size() > 1) {
        // A newly idle singleton is verified immediately. If a second request
        // joins a singleton that was waiting, batch the queue immediately.
        scheduleVerificationFlush(simTime(), true);
    }
}

void CryptoRSU11p::dropExpiredQueuedMessages()
{
    std::deque<QueuedVerification*> kept;
    while (!verifyQueue.empty()) {
        auto* queued = verifyQueue.front();
        verifyQueue.pop_front();
        if (isExpired(queued->msg)) {
            cryptoQueueTimeoutPackets++;
            cryptoDeadlineExpiredPackets++;
            totalVerifyQueueSeconds += std::max(0.0, (simTime() - queued->arrivalAt).dbl());
            totalBatchWaitSeconds += std::max(0.0, (simTime() - queued->arrivalAt).dbl());
            delete queued->msg;
            delete queued;
        }
        else {
            kept.push_back(queued);
        }
    }
    verifyQueue.swap(kept);
}

void CryptoRSU11p::flushVerificationBatch()
{
    const bool forceImmediate = forceImmediateFlush;
    forceImmediateFlush = false;

    if (verifierBusyUntil > simTime()) {
        scheduleVerificationFlush(verifierBusyUntil, forceImmediate);
        return;
    }

    dropExpiredQueuedMessages();
    if (verifyQueue.empty()) return;

    const bool batchEnabled = isAdvlgsScheme(cryptoScheme) && enableBatchVerification;
    if (batchEnabled && verifyQueue.size() == 1 && !forceImmediate) {
        // A singleton that arrived while the verifier was busy may wait only
        // for the remainder of its 20 ms window. The TTL-safe start time takes
        // precedence so waiting can never cause an otherwise avoidable expiry.
        simtime_t triggerAt = latestSingletonStart(verifyQueue.front());
        if (triggerAt < simTime()) triggerAt = simTime();
        if (simTime() < triggerAt) {
            scheduleVerificationFlush(triggerAt, false);
            return;
        }
    }

    startNextVerificationBatch();
}

void CryptoRSU11p::startNextVerificationBatch()
{
    dropExpiredQueuedMessages();
    if (verifyQueue.empty()) return;

    const bool batchEnabled = isAdvlgsScheme(cryptoScheme) && enableBatchVerification;
    std::size_t itemCount = 1;
    if (batchEnabled) {
        // A batch contains every queued request that arrived in the same fixed
        // window as the oldest request. This is an arrival-time boundary, not
        // a hidden item-count cap, and prevents backlog from multiple windows
        // being merged into an ever-growing batch while the verifier is busy.
        const simtime_t windowEnd = verifyQueue.front()->arrivalAt + batchWindow;
        itemCount = 0;
        while (itemCount < verifyQueue.size() && verifyQueue[itemCount]->arrivalAt <= windowEnd) {
            ++itemCount;
        }
        if (itemCount == 0) itemCount = 1;
        if (batchMaxSize > 0) itemCount = std::min(itemCount, static_cast<std::size_t>(batchMaxSize));
    }

    auto* task = new BatchVerification();
    task->verifyStartedAt = simTime();
    task->items.reserve(itemCount);
    for (std::size_t i = 0; i < itemCount; ++i) {
        auto* queued = verifyQueue.front();
        verifyQueue.pop_front();
        VerificationOutcome outcome;
        outcome.msg = queued->msg;
        outcome.arrivalAt = queued->arrivalAt;
        outcome.verifyStartedAt = task->verifyStartedAt;
        task->items.push_back(std::move(outcome));
        delete queued;
    }

    const auto start = std::chrono::steady_clock::now();
    if (skipCryptoComputation) {
        for (auto& item : task->items) {
            item.ok = true;
            item.tag.clear();
        }
    }
    else if (batchEnabled && task->items.size() > 1) {
        std::vector<ADVLGSBatchItem> batchItems;
        batchItems.reserve(task->items.size());
        for (const auto& item : task->items) {
            batchItems.push_back({messagePayload(item.msg), item.msg->getServiceDomain(), item.msg->getSignature()});
        }
        const auto batchResult = ADVLGSMiracl::batchVerify(batchItems);
        for (std::size_t i = 0; i < task->items.size(); ++i) {
            task->items[i].ok = i < batchResult.accepted.size() && batchResult.accepted[i];
            if (i < batchResult.linkTags.size()) task->items[i].tag = batchResult.linkTags[i];
        }
    }
    else {
        for (auto& item : task->items) {
            item.ok = verifyOne(cryptoScheme, messagePayload(item.msg), item.msg, &item.tag);
        }
    }
    const auto end = std::chrono::steady_clock::now();
    const double measuredVerifySeconds = std::chrono::duration<double>(end - start).count();
    if (useFixedCryptoTiming) {
        const auto batchDelay = fixedBatchVerifyDelayBySizeSeconds.find(static_cast<int>(task->items.size()));
        if (batchEnabled && task->items.size() > 1 && batchDelay != fixedBatchVerifyDelayBySizeSeconds.end()) {
            task->verifySeconds = batchDelay->second;
        }
        else {
            const double perItem = batchEnabled && task->items.size() > 1 && fixedBatchVerifyDelayPerItemSeconds > 0
                ? fixedBatchVerifyDelayPerItemSeconds
                : fixedVerifyDelaySeconds;
            task->verifySeconds = perItem * static_cast<double>(task->items.size());
        }
    }
    else {
        task->verifySeconds = measuredVerifySeconds;
    }

    const simtime_t verifyFinishedAt = task->verifyStartedAt + SimTime(task->verifySeconds);
    verifierBusyUntil = verifyFinishedAt;
    cryptoBatches++;
    cryptoBatchVerifiedItems += task->items.size();
    maxBatchSizeObserved = std::max<std::uint64_t>(maxBatchSizeObserved, task->items.size());
    batchSizeHistogram[static_cast<int>(task->items.size())]++;

    auto* event = new cMessage("asdRsuVerifyComplete", VERIFY_COMPLETE_EVT);
    event->setContextPointer(task);
    pendingVerifyEvents.insert(event);
    scheduleAt(verifyFinishedAt, event);
}

void CryptoRSU11p::completeVerificationBatch(cMessage* event)
{
    pendingVerifyEvents.erase(event);
    auto* task = static_cast<BatchVerification*>(event->getContextPointer());
    event->setContextPointer(nullptr);
    delete event;

    totalVerifySeconds += task->verifySeconds;
    for (auto& item : task->items) {
        totalVerifyQueueSeconds += std::max(0.0, (item.verifyStartedAt - item.arrivalAt).dbl());
        totalBatchWaitSeconds += std::max(0.0, (item.verifyStartedAt - item.arrivalAt).dbl());

        if (isExpired(item.msg)) {
            cryptoBatchTimeoutPackets++;
            cryptoDeadlineExpiredPackets++;
            delete item.msg;
            continue;
        }
        if (!item.ok) {
            cryptoRejectedPackets++;
            delete item.msg;
            continue;
        }

        ADVLGSMessage* msg = item.msg;
        cryptoReceivedPackets++;
        cryptoReceivedBytes += msg->getByteLength();
        totalNetworkDelaySeconds += std::max(0.0, (item.arrivalAt - msg->getSentAt()).dbl());
        totalValidationDelaySeconds += std::max(0.0, (simTime() - msg->getGeneratedAt()).dbl());
        msg->setLinkTag(item.tag.c_str());
        if (repeatMessages) sendDelayedDown(msg->dup(), 2 + uniform(0.01, 0.2));
        delete msg;
    }
    delete task;

    if (!verifyQueue.empty() && batchFlushEvent != nullptr && !batchFlushEvent->isScheduled()) {
        scheduleVerificationFlush(simTime(), false);
    }
}

void CryptoRSU11p::onWSA(DemoServiceAdvertisment* wsa)
{
    if (wsa->getPsid() == 42) mac->changeServiceChannel(static_cast<Channel>(wsa->getTargetChannel()));
}

void CryptoRSU11p::onWSM(BaseFrame1609_4* frame)
{
    ADVLGSMessage* wsm = check_and_cast<ADVLGSMessage*>(frame);
    queueVerification(wsm->dup());
}

void CryptoRSU11p::handleSelfMsg(cMessage* msg)
{
    if (msg->getKind() == BATCH_FLUSH_EVT) {
        flushVerificationBatch();
        return;
    }
    if (msg->getKind() == VERIFY_COMPLETE_EVT) {
        completeVerificationBatch(msg);
        return;
    }
    DemoBaseApplLayer::handleSelfMsg(msg);
}

} // namespace advlgs
