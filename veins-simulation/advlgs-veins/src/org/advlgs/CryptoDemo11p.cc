#include "org/advlgs/CryptoDemo11p.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <omnetpp/cconfiguration.h>
#include <sstream>

#include "org/advlgs/ADVLGSMessage_m.h"
#include "org/advlgs/advlgs_miracl.h"
#include "org/advlgs/bbs_miracl.h"
#include "org/advlgs/clgs_miracl.h"
#include "org/advlgs/erca_miracl.h"
#include "org/advlgs/mlgs_miracl.h"
#include "veins/base/modules/BaseMobility.h"

using namespace omnetpp;
using namespace veins;

namespace advlgs {

namespace {

constexpr short SIGNED_SEND_EVT = 120;
constexpr short VERIFY_COMPLETE_EVT = 121;

struct PendingVerification {
    ADVLGSMessage* msg = nullptr;
    bool ok = false;
    std::string tag;
    double verifySeconds = 0;
    simtime_t arrivalAt = SIMTIME_ZERO;
    simtime_t verifyStartedAt = SIMTIME_ZERO;
};

simtime_t laterOf(simtime_t a, simtime_t b)
{
    return a > b ? a : b;
}

void warmupScheme(const std::string& scheme)
{
    if (scheme == "BBS") BBSMiracl::warmup();
    else if (scheme == "CLGS") CLGSMiracl::warmup();
    else if (scheme == "ERCA") ERCAMiracl::warmup();
    else if (scheme == "MLGS") MLGSMiracl::warmup();
    else ADVLGSMiracl::warmup();
}

void prepareSchemeMember(const std::string& scheme, int memberId)
{
    if (scheme == "BBS") BBSMiracl::prepareMember(memberId);
    else if (scheme == "CLGS") CLGSMiracl::prepareMember(memberId);
    else if (scheme == "ERCA") ERCAMiracl::prepareMember(memberId);
    else if (scheme == "MLGS") MLGSMiracl::prepareMember(memberId);
    else ADVLGSMiracl::prepareMember(memberId);
}

std::size_t signatureBytesForScheme(const std::string& scheme)
{
    if (scheme == "BBS") return BBSMiracl::nominalSignatureBytes();
    if (scheme == "CLGS") return CLGSMiracl::nominalSignatureBytes();
    if (scheme == "ERCA") return ERCAMiracl::nominalSignatureBytes();
    if (scheme == "MLGS") return MLGSMiracl::nominalSignatureBytes();
    return ADVLGSMiracl::nominalSignatureBytes();
}

std::string displaySchemeName(const std::string& scheme)
{
    if (scheme == "ADVLGS" || scheme == "A-DVLGS") return "A-DVLGS";
    if (scheme == "MLGS") return "MLGS";
    if (scheme == "ERCA") return "ERCA";
    if (scheme == "CLGS") return "CLGS";
    return scheme;
}

} // namespace

Define_Module(CryptoDemo11p);

CryptoDemo11p::~CryptoDemo11p()
{
    cancelAndDelete(announcementTimer);
    for (auto* event : pendingSignedSendEvents) {
        delete static_cast<ADVLGSMessage*>(event->getContextPointer());
        event->setContextPointer(nullptr);
        cancelAndDelete(event);
    }
    pendingSignedSendEvents.clear();
    for (auto* event : pendingVerifyEvents) {
        auto* task = static_cast<PendingVerification*>(event->getContextPointer());
        if (task != nullptr) {
            delete task->msg;
            delete task;
        }
        event->setContextPointer(nullptr);
        cancelAndDelete(event);
    }
    pendingVerifyEvents.clear();
}

void CryptoDemo11p::initialize(int stage)
{
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        lastDroveAt = simTime();
        sentMessage = false;
        currentSubscribedServiceId = -1;
        announcementSerial = 0;
        serviceDomain = par("serviceDomain").stdstringValue();
        cryptoScheme = par("cryptoScheme").stdstringValue();
        warmupScheme(cryptoScheme);
        prepareSchemeMember(cryptoScheme, static_cast<int>(myId));
        experimentMode = par("experimentMode").boolValue();
        announcementProbability = par("announcementProbability").doubleValue();
        announcementInterval = par("announcementInterval");
        announcementStopTime = par("announcementStopTime");
        fixedMessageBytes = par("fixedMessageBytes");
        signedMessageBytes = par("signedMessageBytes");
        useFixedCryptoTiming = par("useFixedCryptoTiming").boolValue();
        skipCryptoComputation = par("skipCryptoComputation").boolValue();
        fixedSignDelaySeconds = par("fixedSignDelay").doubleValue();
        fixedVerifyDelaySeconds = par("fixedVerifyDelay").doubleValue();
        cModule* network = getParentModule() == nullptr ? nullptr : getParentModule()->getParentModule();
        cModule* connectionManager = network == nullptr ? nullptr : network->getSubmodule("connectionManager");
        if (connectionManager != nullptr && connectionManager->hasPar("maxInterfDist")) {
            communicationRangeMeters = connectionManager->par("maxInterfDist").doubleValue();
        }
        if (experimentMode) announcementTimer = new cMessage("advlgsAnnouncementTimer");
    }
    if (stage == 1 && experimentMode && announcementTimer != nullptr) {
        const simtime_t firstAnnouncement = simTime() + uniform(0, announcementInterval.dbl());
        if (firstAnnouncement <= announcementStopTime) scheduleAt(firstAnnouncement, announcementTimer);
    }
}

void CryptoDemo11p::finish()
{
    if (announcementTimer != nullptr) {
        cancelAndDelete(announcementTimer);
        announcementTimer = nullptr;
    }
    DemoBaseApplLayer::finish();
    const std::uint64_t signBacklogDrops = pendingSignedSendEvents.size();
    const std::uint64_t verifyBacklogDrops = pendingVerifyEvents.size();
    const std::uint64_t completedChecks = cryptoReceivedPackets + cryptoRejectedPackets;
    const std::uint64_t attemptedChecks = completedChecks + verifyBacklogDrops;
    const double acceptanceRate = attemptedChecks == 0 ? 0 : cryptoReceivedPackets / static_cast<double>(attemptedChecks);
    const double packetLossRate = 1.0 - acceptanceRate;
    const double rangeDeliveryRate = expectedVehicleRangeReceipts == 0 ? 0 : cryptoReceivedPackets / static_cast<double>(expectedVehicleRangeReceipts);
    const double rangeLossRate = expectedVehicleRangeReceipts == 0 ? 0 : std::max(0.0, 1.0 - rangeDeliveryRate);
    const double throughputBps = simTime() == SIMTIME_ZERO ? 0 : cryptoReceivedBytes * 8.0 / simTime().dbl();
    const double avgMessageBytes = cryptoSentPackets == 0 ? 0 : cryptoSentBytes / static_cast<double>(cryptoSentPackets);
    const double avgSignatureBytes = cryptoSentPackets == 0 ? 0 : cryptoSignatureBytes / static_cast<double>(cryptoSentPackets);
    const double avgSignMs = cryptoSentPackets == 0 ? 0 : totalSignSeconds * 1000.0 / cryptoSentPackets;
    const double avgVerifyMs = completedChecks == 0 ? 0 : totalVerifySeconds * 1000.0 / completedChecks;
    const double avgNetworkDelayMs = cryptoReceivedPackets == 0 ? 0 : totalNetworkDelaySeconds * 1000.0 / cryptoReceivedPackets;
    const double avgSignQueueMs = cryptoSentPackets == 0 ? 0 : totalSignQueueSeconds * 1000.0 / cryptoSentPackets;
    const double avgVerifyQueueMs = completedChecks == 0 ? 0 : totalVerifyQueueSeconds * 1000.0 / completedChecks;
    const double avgValidationDelayMs = cryptoReceivedPackets == 0 ? 0 : totalValidationDelaySeconds * 1000.0 / cryptoReceivedPackets;
    const auto* config = dynamic_cast<cConfigurationEx*>(getEnvir()->getConfig());
    const char* configName = config == nullptr ? "unknown" : config->getActiveConfigName();
    const std::string schemeName = displaySchemeName(cryptoScheme);
    recordScalar("advlgs.crypto.sentPackets", cryptoSentPackets);
    recordScalar("advlgs.crypto.signBacklogDrops", signBacklogDrops);
    recordScalar("advlgs.crypto.sentBytes", cryptoSentBytes);
    recordScalar("advlgs.crypto.avgMessageBytes", avgMessageBytes);
    recordScalar("advlgs.crypto.avgSignatureBytes", avgSignatureBytes);
    recordScalar("advlgs.crypto.receivedPackets", cryptoReceivedPackets);
    recordScalar("advlgs.crypto.rejectedPackets", cryptoRejectedPackets);
    recordScalar("advlgs.crypto.verifyBacklogDrops", verifyBacklogDrops);
    recordScalar("advlgs.crypto.receivedBytes", cryptoReceivedBytes);
    recordScalar("advlgs.net.expectedRangeReceipts", expectedRangeReceipts);
    recordScalar("advlgs.net.expectedVehicleRangeReceipts", expectedVehicleRangeReceipts);
    recordScalar("advlgs.net.expectedRsuRangeReceipts", expectedRsuRangeReceipts);
    recordScalar("advlgs.net.rangeDeliveryRate", rangeDeliveryRate);
    recordScalar("advlgs.net.rangeLossRate", rangeLossRate);
    recordScalar("advlgs.crypto.packetAcceptanceRate", acceptanceRate);
    recordScalar("advlgs.metrics.Pmsg", acceptanceRate);
    recordScalar("advlgs.metrics.DmsgMs", avgValidationDelayMs);
    recordScalar("advlgs.crypto.throughputBps", throughputBps);
    recordScalar("advlgs.crypto.avgSignMs", avgSignMs);
    recordScalar("advlgs.crypto.avgVerifyMs", avgVerifyMs);
    recordScalar("advlgs.net.avgNetworkDelayMs", avgNetworkDelayMs);
    recordScalar("advlgs.crypto.avgSignQueueMs", avgSignQueueMs);
    recordScalar("advlgs.crypto.avgVerifyQueueMs", avgVerifyQueueMs);

    const char* csvPath = "results/advlgs-metrics.csv";
    std::ifstream existing(csvPath);
    const bool needHeader = !existing.good() || existing.peek() == std::ifstream::traits_type::eof();
    std::ofstream csv(csvPath, std::ios::app);
    if (needHeader) csv << "config,scheme,module,role,simTime,sentPackets,signBacklogDrops,sentBytes,avgMessageBytes,avgSignatureBytes,receivedPackets,rejectedPackets,deadlineExpiredPackets,arrivalExpiredPackets,queueTimeoutPackets,batchTimeoutPackets,verifyBacklogDrops,receivedBytes,expectedRangeReceipts,expectedVehicleRangeReceipts,expectedRsuRangeReceipts,rangeDeliveryRate,rangeLossRate,packetAcceptanceRate,packetLossRate,throughputBps,avgSignMs,avgVerifyMs,avgBatchVerifyMs,avgBatchSize,maxVerifyQueueLength,avgNetworkDelayMs,avgSignQueueMs,avgVerifyQueueMs,avgBatchWaitMs,Pmsg,DmsgMs\n";
    csv << configName << ',' << schemeName << ',' << getFullPath() << ",vehicle," << simTime() << ',' << cryptoSentPackets << ',' << signBacklogDrops << ',' << cryptoSentBytes << ','
        << avgMessageBytes << ',' << avgSignatureBytes << ',' << cryptoReceivedPackets << ',' << cryptoRejectedPackets << ",0,0,0,0," << verifyBacklogDrops << ',' << cryptoReceivedBytes << ','
        << expectedRangeReceipts << ',' << expectedVehicleRangeReceipts << ',' << expectedRsuRangeReceipts << ',' << rangeDeliveryRate << ',' << rangeLossRate << ',' << acceptanceRate << ',' << packetLossRate << ','
        << throughputBps << ',' << avgSignMs << ',' << avgVerifyMs << ",0,0,0," << avgNetworkDelayMs << ',' << avgSignQueueMs << ',' << avgVerifyQueueMs << ",0," << acceptanceRate << ',' << avgValidationDelayMs << '\n';
}

std::string CryptoDemo11p::messagePayload(const ADVLGSMessage* msg) const
{
    std::ostringstream os;
    os << "data=" << msg->getDemoData()
       << "|sender=" << msg->getSenderAddress()
       << "|serial=" << msg->getSerial()
       << "|time=" << msg->getGeneratedAt();

    std::string payload = os.str();
    if (signedMessageBytes <= 0) return payload;

    const auto targetBytes = static_cast<std::size_t>(signedMessageBytes);
    if (payload.size() > targetBytes) {
        payload.resize(targetBytes);
    }
    else {
        payload.append(targetBytes - payload.size(), '#');
    }
    return payload;
}

void CryptoDemo11p::signAndSend(ADVLGSMessage* msg)
{
    const simtime_t requestedAt = simTime();
    msg->setServiceDomain(serviceDomain.c_str());
    msg->setGeneratedAt(requestedAt);
    const auto start = std::chrono::steady_clock::now();
    ADVLGSSignature sig;
    if (skipCryptoComputation) {
        sig.encoded = "SIM|" + cryptoScheme + "|" + std::to_string(msg->getSerial());
        sig.linkTag.clear();
        sig.wireBytes = signatureBytesForScheme(cryptoScheme);
    }
    else if (cryptoScheme == "BBS") {
        auto bbsSig = BBSMiracl::sign(static_cast<int>(myId), messagePayload(msg), serviceDomain);
        sig.encoded = bbsSig.encoded;
        sig.linkTag = bbsSig.linkTag;
        sig.wireBytes = bbsSig.wireBytes;
    }
    else if (cryptoScheme == "CLGS") {
        auto clgsSig = CLGSMiracl::sign(static_cast<int>(myId), messagePayload(msg), serviceDomain);
        sig.encoded = clgsSig.encoded;
        sig.linkTag = clgsSig.linkTag;
        sig.wireBytes = clgsSig.wireBytes;
    }
    else if (cryptoScheme == "ERCA") {
        auto ercaSig = ERCAMiracl::sign(static_cast<int>(myId), messagePayload(msg), serviceDomain);
        sig.encoded = ercaSig.encoded;
        sig.linkTag = ercaSig.linkTag;
        sig.wireBytes = ercaSig.wireBytes;
    }
    else if (cryptoScheme == "MLGS") {
        auto mlgsSig = MLGSMiracl::sign(static_cast<int>(myId), "st|" + serviceDomain, messagePayload(msg), serviceDomain);
        sig.encoded = mlgsSig.encoded;
        sig.linkTag = mlgsSig.linkTag;
        sig.wireBytes = mlgsSig.wireBytes;
    }
    else {
        sig = ADVLGSMiracl::sign(static_cast<int>(myId), messagePayload(msg), serviceDomain);
    }
    const auto end = std::chrono::steady_clock::now();
    const double measuredSignSeconds = std::chrono::duration<double>(end - start).count();
    const double signSeconds = useFixedCryptoTiming ? fixedSignDelaySeconds : measuredSignSeconds;
    msg->setSignDurationSeconds(signSeconds);
    msg->setSignature(sig.encoded.c_str());
    msg->setLinkTag(sig.linkTag.c_str());
    if (fixedMessageBytes > 0) msg->setByteLength(fixedMessageBytes);
    else {
        const int64_t payloadBytes = std::max<int64_t>(0, signedMessageBytes);
        const int64_t unsignedMessageBytes = (static_cast<int64_t>(headerLength) + 7) / 8 + payloadBytes;
        msg->setByteLength(unsignedMessageBytes);
        msg->addByteLength(static_cast<int64_t>(sig.wireBytes));
    }
    const simtime_t signStart = laterOf(requestedAt, signerBusyUntil);
    const simtime_t signFinish = signStart + SimTime(signSeconds);
    signerBusyUntil = signFinish;
    msg->setSignedAt(signFinish);

    auto* event = new cMessage("advlgsSignedSend", SIGNED_SEND_EVT);
    event->setContextPointer(msg);
    pendingSignedSendEvents.insert(event);
    scheduleAt(signFinish, event);
}

void CryptoDemo11p::completeSignedSend(cMessage* event)
{
    pendingSignedSendEvents.erase(event);
    auto* msg = static_cast<ADVLGSMessage*>(event->getContextPointer());
    event->setContextPointer(nullptr);
    delete event;

    msg->setSentAt(simTime());
    totalSignSeconds += msg->getSignDurationSeconds();
    totalSignQueueSeconds += std::max(0.0, (msg->getSignedAt() - msg->getGeneratedAt()).dbl() - msg->getSignDurationSeconds());
    cryptoSentBytes += msg->getByteLength();
    cryptoSignatureBytes += signatureBytesForScheme(cryptoScheme);
    std::uint64_t vehicleRecipients = 0;
    std::uint64_t rsuRecipients = 0;
    countPotentialRecipientsInRange(vehicleRecipients, rsuRecipients);
    expectedVehicleRangeReceipts += vehicleRecipients;
    expectedRsuRangeReceipts += rsuRecipients;
    expectedRangeReceipts += vehicleRecipients + rsuRecipients;
    cryptoSentPackets++;
    sendDown(msg);
}

void CryptoDemo11p::countPotentialRecipientsInRange(std::uint64_t& vehicleRecipients, std::uint64_t& rsuRecipients) const
{
    vehicleRecipients = 0;
    rsuRecipients = 0;
    cModule* host = getParentModule();
    cModule* network = host == nullptr ? nullptr : host->getParentModule();
    if (host == nullptr || network == nullptr || mobility == nullptr) return;

    const Coord senderPos = mobility->getPositionAt(simTime());
    for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
        cModule* candidate = *it;
        if (candidate == nullptr || candidate == host) continue;
        const char* name = candidate->getName();
        const bool isVehicle = std::strcmp(name, "node") == 0;
        const bool isRsu = std::strcmp(name, "rsu") == 0;
        if (!isVehicle && !isRsu) continue;
        if (candidate->getSubmodule("appl") == nullptr) continue;

        auto* candidateMobility = dynamic_cast<BaseMobility*>(candidate->getSubmodule("veinsmobility"));
        if (candidateMobility == nullptr) {
            candidateMobility = dynamic_cast<BaseMobility*>(candidate->getSubmodule("mobility"));
        }
        if (candidateMobility == nullptr) continue;

        if (senderPos.distance(candidateMobility->getPositionAt(simTime())) <= communicationRangeMeters) {
            if (isVehicle) vehicleRecipients++;
            else rsuRecipients++;
        }
    }
}

void CryptoDemo11p::sendAnnouncement()
{
    if (uniform(0, 1) > announcementProbability) return;

    auto* wsm = new ADVLGSMessage();
    populateWSM(wsm);
    std::ostringstream payload;
    payload << "announcement|road=" << mobility->getRoadId() << "|sender=" << myId << "|serial=" << announcementSerial;
    wsm->setDemoData(payload.str().c_str());
    wsm->setSenderAddress(myId);
    wsm->setSerial(announcementSerial++);
    signAndSend(wsm);
}

void CryptoDemo11p::queueVerification(ADVLGSMessage* msg)
{
    std::string tag;
    const auto start = std::chrono::steady_clock::now();
    bool ok = false;
    if (skipCryptoComputation) {
        ok = true;
    }
    else if (cryptoScheme == "BBS") {
        ok = BBSMiracl::verify(messagePayload(msg), msg->getServiceDomain(), msg->getSignature(), &tag);
    }
    else if (cryptoScheme == "CLGS") {
        ok = CLGSMiracl::verify(messagePayload(msg), msg->getServiceDomain(), msg->getSignature(), &tag);
    }
    else if (cryptoScheme == "ERCA") {
        ok = ERCAMiracl::verify(messagePayload(msg), msg->getServiceDomain(), msg->getSignature(), &tag);
    }
    else if (cryptoScheme == "MLGS") {
        ok = MLGSMiracl::verify(std::string("st|") + msg->getServiceDomain(), messagePayload(msg), msg->getServiceDomain(), msg->getSignature(), &tag);
    }
    else {
        ok = ADVLGSMiracl::verify(messagePayload(msg), msg->getServiceDomain(), msg->getSignature(), &tag);
    }
    const auto end = std::chrono::steady_clock::now();
    const double measuredVerifySeconds = std::chrono::duration<double>(end - start).count();
    const double verifySeconds = useFixedCryptoTiming ? fixedVerifyDelaySeconds : measuredVerifySeconds;
    const simtime_t arrivalAt = simTime();
    const simtime_t verifyStartedAt = laterOf(arrivalAt, verifierBusyUntil);
    const simtime_t verifyFinishedAt = verifyStartedAt + SimTime(verifySeconds);
    verifierBusyUntil = verifyFinishedAt;

    auto* task = new PendingVerification();
    task->msg = msg;
    task->ok = ok;
    task->tag = tag;
    task->verifySeconds = verifySeconds;
    task->arrivalAt = arrivalAt;
    task->verifyStartedAt = verifyStartedAt;

    auto* event = new cMessage("advlgsVerifyComplete", VERIFY_COMPLETE_EVT);
    event->setContextPointer(task);
    pendingVerifyEvents.insert(event);
    scheduleAt(verifyFinishedAt, event);
}

void CryptoDemo11p::completeVerification(cMessage* event)
{
    pendingVerifyEvents.erase(event);
    auto* task = static_cast<PendingVerification*>(event->getContextPointer());
    event->setContextPointer(nullptr);
    delete event;

    totalVerifySeconds += task->verifySeconds;
    totalVerifyQueueSeconds += std::max(0.0, (task->verifyStartedAt - task->arrivalAt).dbl());
    if (!task->ok) {
        cryptoRejectedPackets++;
        delete task->msg;
        delete task;
        return;
    }

    ADVLGSMessage* msg = task->msg;
    cryptoReceivedPackets++;
    cryptoReceivedBytes += msg->getByteLength();
    totalNetworkDelaySeconds += std::max(0.0, (task->arrivalAt - msg->getSentAt()).dbl());
    totalValidationDelaySeconds += std::max(0.0, (simTime() - msg->getGeneratedAt()).dbl());
    msg->setLinkTag(task->tag.c_str());
    handleAcceptedMessage(msg);
    delete msg;
    delete task;
}

void CryptoDemo11p::onWSA(DemoServiceAdvertisment* wsa)
{
    if (currentSubscribedServiceId == -1) {
        mac->changeServiceChannel(static_cast<Channel>(wsa->getTargetChannel()));
        currentSubscribedServiceId = wsa->getPsid();
        if (currentOfferedServiceId != wsa->getPsid()) {
            stopService();
            startService(static_cast<Channel>(wsa->getTargetChannel()), wsa->getPsid(), "A-DVLGS Traffic Service");
        }
    }
}

void CryptoDemo11p::onWSM(BaseFrame1609_4* frame)
{
    ADVLGSMessage* wsm = check_and_cast<ADVLGSMessage*>(frame);
    queueVerification(wsm->dup());
}

void CryptoDemo11p::handleAcceptedMessage(ADVLGSMessage* wsm)
{
    if (experimentMode) {
        findHost()->getDisplayString().setTagArg("i", 1, "green");
        return;
    }

    findHost()->getDisplayString().setTagArg("i", 1, "green");
    if (mobility->getRoadId()[0] != ':') traciVehicle->changeRoute(wsm->getDemoData(), 9999);
    if (!sentMessage) {
        sentMessage = true;
        ADVLGSMessage* copy = wsm->dup();
        copy->setSenderAddress(myId);
        copy->setSerial(3);
        scheduleAt(simTime() + 2 + uniform(0.01, 0.2), copy);
    }
}

void CryptoDemo11p::handleSelfMsg(cMessage* msg)
{
    if (msg->getKind() == SIGNED_SEND_EVT) {
        completeSignedSend(msg);
        return;
    }
    if (msg->getKind() == VERIFY_COMPLETE_EVT) {
        completeVerification(msg);
        return;
    }

    if (msg == announcementTimer) {
        if (simTime() <= announcementStopTime) sendAnnouncement();
        const simtime_t nextAnnouncement = simTime() + announcementInterval;
        if (nextAnnouncement <= announcementStopTime) scheduleAt(nextAnnouncement, announcementTimer);
        return;
    }

    if (ADVLGSMessage* wsm = dynamic_cast<ADVLGSMessage*>(msg)) {
        ADVLGSMessage* copy = wsm->dup();
        copy->setSerial(wsm->getSerial());
        signAndSend(copy);
        wsm->setSerial(wsm->getSerial() + 1);
        if (wsm->getSerial() >= 3) {
            stopService();
            delete wsm;
        }
        else {
            scheduleAt(simTime() + 1, wsm);
        }
    }
    else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void CryptoDemo11p::handlePositionUpdate(cObject* obj)
{
    DemoBaseApplLayer::handlePositionUpdate(obj);
    if (experimentMode) return;

    if (mobility->getSpeed() < 1) {
        if (simTime() - lastDroveAt >= 10 && !sentMessage) {
            findHost()->getDisplayString().setTagArg("i", 1, "red");
            sentMessage = true;

            ADVLGSMessage* wsm = new ADVLGSMessage();
            populateWSM(wsm);
            wsm->setDemoData(mobility->getRoadId().c_str());
            wsm->setSenderAddress(myId);
            wsm->setSerial(0);

            if (dataOnSch) {
                startService(Channel::sch2, 42, "A-DVLGS Traffic Information Service");
                scheduleAt(computeAsynchronousSendingTime(1, ChannelType::service), wsm);
            }
            else {
                signAndSend(wsm);
            }
        }
    }
    else {
        lastDroveAt = simTime();
    }
}

} // namespace advlgs




