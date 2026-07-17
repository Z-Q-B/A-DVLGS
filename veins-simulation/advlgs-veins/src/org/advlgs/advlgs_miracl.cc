#include "org/advlgs/advlgs_miracl.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "org/advlgs/bls12381_utils.h"

namespace advlgs {
namespace {

using bls12381::Fr;

constexpr std::size_t kStateBytes = 16;
constexpr std::size_t kTimestampBytes = 8;

BLS12381::ECP hashToG1(const std::string& label)
{
    return bls12381::g1Mul(bls12381::g1Generator(), bls12381::hashScalar("hash-to-g1|" + label));
}

BLS12381::ECP2 hashToG2(const std::string& label)
{
    return bls12381::g2Mul(bls12381::g2Generator(), bls12381::hashScalar("hash-to-g2|" + label));
}

std::string joinChallengeParts(const std::vector<std::string>& parts)
{
    std::ostringstream os;
    for (const auto& part : parts) os << part.size() << ':' << part << '|';
    return os.str();
}

Fr challenge(const std::vector<std::string>& parts)
{
    return bls12381::hashScalar(joinChallengeParts(parts));
}

Fr randomScalar(const std::string& label)
{
    static unsigned long long counter = 1;
    std::ostringstream os;
    os << "deterministic-benchmark-random|" << label << "|" << counter++;
    return bls12381::hashScalar(os.str());
}

std::string deltaLabel()
{
    return "Delta_t-default";
}

std::string tsLabel()
{
    return "TS-default";
}

std::string spjt()
{
    return "VID_j-default|V_j-default|Scope_j-default|Delta_t-default|Cert_j-default";
}

std::string domjt()
{
    return "VID_j-default|V_j-default|Scope_j-default|Delta_t-default";
}

struct Params {
    BLS12381::ECP p1;
    BLS12381::ECP p2;
    BLS12381::ECP p3;
    BLS12381::ECP p4;
    BLS12381::ECP pkRA;
    BLS12381::ECP oPoint;
    BLS12381::ECP vj;
    BLS12381::ECP rDelta;
    BLS12381::ECP p1PlusRDelta;
    BLS12381::ECP djt;
    BLS12381::ECP2 q;
    BLS12381::ECP2 sQ;
    Fr skRA;
    Fr s;
    Fr o;
    Fr ell;

    Params()
        : p1(hashToG1("advlgs-P1"))
        , p2(hashToG1("advlgs-P2"))
        , p3(hashToG1("advlgs-P3"))
        , p4(hashToG1("advlgs-P4"))
        , q(hashToG2("advlgs-Q"))
        , skRA(bls12381::hashScalar("advlgs-sk_RA"))
        , s(bls12381::hashScalar("advlgs-issuer-s"))
        , o(bls12381::hashScalar("advlgs-opener-o"))
        , ell(bls12381::hashScalar("advlgs-ell_j"))
    {
        pkRA = bls12381::g1Mul(p1, skRA);
        sQ = bls12381::g2Mul(q, s);
        oPoint = bls12381::g1Mul(p2, o);
        vj = bls12381::g1Mul(p2, ell);
        rDelta = bls12381::g1Mul(p4, bls12381::hashScalar("advlgs-H3|" + deltaLabel()));
        p1PlusRDelta = bls12381::g1Add(p1, rDelta);
        djt = hashToG1("advlgs-H2|" + domjt());
    }
};

struct MemberCredential {
    Fr y;
    Fr x;
    Fr u;
    BLS12381::ECP yPublic;
    BLS12381::ECP a;
};

struct ParsedSignature {
    std::vector<std::string> parts;
    BLS12381::ECP barA;
    BLS12381::ECP hatA;
    BLS12381::ECP b;
    BLS12381::ECP cPoint;
    BLS12381::ECP l;
    Fr c;
    Fr zR;
    Fr zY;
    Fr zTU;
    Fr zX;
};

const Params& params()
{
    static const Params p;
    return p;
}

MemberCredential buildMemberCredential(int memberId)
{
    const auto& p = params();
    MemberCredential out;
    out.y = bls12381::hashScalar("advlgs-y_i|" + std::to_string(memberId));
    out.x = bls12381::hashScalar("advlgs-x_i_Delta_t|" + std::to_string(memberId));
    out.u = bls12381::hashScalar("advlgs-u_i_Delta_t|" + std::to_string(memberId));
    out.yPublic = bls12381::g1Mul(p.p2, out.y);
    const auto w = bls12381::g1Mul(p.p3, out.u);
    auto numerator = bls12381::g1Add(p.p1, out.yPublic);
    numerator = bls12381::g1Add(numerator, p.rDelta);
    numerator = bls12381::g1Add(numerator, w);
    out.a = bls12381::g1Mul(numerator, bls12381::inv(bls12381::add(p.s, out.x)));
    return out;
}

std::unordered_map<int, MemberCredential>& memberCache()
{
    static std::unordered_map<int, MemberCredential> cache;
    return cache;
}

const MemberCredential& memberCredential(int memberId)
{
    auto& cache = memberCache();
    const auto found = cache.find(memberId);
    if (found != cache.end()) return found->second;
    return cache.emplace(memberId, buildMemberCredential(memberId)).first->second;
}

std::vector<std::string> splitSignature(const std::string& encoded)
{
    std::vector<std::string> parts;
    std::stringstream ss(encoded);
    std::string item;
    while (std::getline(ss, item, '|')) parts.push_back(item);
    return parts;
}

std::string joinSignature(const std::vector<std::string>& parts)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) os << '|';
        os << parts[i];
    }
    return os.str();
}

bool parseSignature(const std::string& encoded, ParsedSignature* sig)
{
    sig->parts = splitSignature(encoded);
    if (sig->parts.size() != 11 || sig->parts[0] != "ADVLGS") return false;
    bool ok = false;
    if (!bls12381::g1FromHex(sig->parts[1], &sig->barA)) return false;
    if (!bls12381::g1FromHex(sig->parts[2], &sig->hatA)) return false;
    if (!bls12381::g1FromHex(sig->parts[3], &sig->b)) return false;
    if (!bls12381::g1FromHex(sig->parts[4], &sig->cPoint)) return false;
    if (!bls12381::g1FromHex(sig->parts[5], &sig->l)) return false;
    sig->c = bls12381::scalarFromHex(sig->parts[6], &ok);
    if (!ok) return false;
    sig->zR = bls12381::scalarFromHex(sig->parts[7], &ok);
    if (!ok) return false;
    sig->zY = bls12381::scalarFromHex(sig->parts[8], &ok);
    if (!ok) return false;
    sig->zTU = bls12381::scalarFromHex(sig->parts[9], &ok);
    if (!ok) return false;
    sig->zX = bls12381::scalarFromHex(sig->parts[10], &ok);
    return ok;
}

bool verifyPairingEquation(const ParsedSignature& sig)
{
    const auto& p = params();
    return bls12381::gtEquals(bls12381::pair(p.sQ, sig.barA), bls12381::pair(p.q, sig.hatA));
}

bool verifyProofWithoutPairing(const std::string& message, const ParsedSignature& sig)
{
    const auto& p = params();
    const auto rb = bls12381::g1Sub(bls12381::g1Mul(p.p2, sig.zR), bls12381::g1Mul(sig.b, sig.c));
    const auto rc = bls12381::g1Sub(
        bls12381::g1Add(bls12381::g1Mul(p.p2, sig.zY), bls12381::g1Mul(p.oPoint, sig.zR)),
        bls12381::g1Mul(sig.cPoint, sig.c));
    const auto rl = bls12381::g1Sub(
        bls12381::g1Add(bls12381::g1Mul(p.djt, sig.zY), bls12381::g1Mul(p.vj, sig.zR)),
        bls12381::g1Mul(sig.l, sig.c));
    auto ra = bls12381::g1Add(bls12381::g1Mul(p.p1PlusRDelta, sig.zR), bls12381::g1Mul(sig.b, sig.zY));
    ra = bls12381::g1Add(ra, bls12381::g1Mul(p.p3, sig.zTU));
    ra = bls12381::g1Sub(ra, bls12381::g1Mul(sig.barA, sig.zX));
    ra = bls12381::g1Sub(ra, bls12381::g1Mul(sig.hatA, sig.c));

    const Fr cPrime = challenge({
        "ADVLGS", message, bls12381::g1ToHex(p.pkRA), spjt(), tsLabel(), sig.parts[1], sig.parts[2],
        sig.parts[3], sig.parts[4], sig.parts[5], bls12381::g1ToHex(rb), bls12381::g1ToHex(rc),
        bls12381::g1ToHex(rl), bls12381::g1ToHex(ra)});
    if (!bls12381::scalarEquals(sig.c, cPrime)) return false;
    return true;
}

} // namespace

void ADVLGSMiracl::warmup()
{
    (void)params();
}

void ADVLGSMiracl::prepareMember(int memberId)
{
    (void)memberCredential(memberId);
}

std::size_t ADVLGSMiracl::nominalSignatureBytes()
{
    return 5 * bls12381::kG1CompressedWireBytes + 5 * bls12381::kScalarWireBytes + kStateBytes + kTimestampBytes;
}

ADVLGSSignature ADVLGSMiracl::sign(int memberId, const std::string& message, const std::string& serviceDomain)
{
    (void)serviceDomain;
    const auto& p = params();
    const auto& member = memberCredential(memberId);
    const Fr r = randomScalar("advlgs-r");
    const Fr tU = bls12381::mul(r, member.u);
    const Fr alphaR = randomScalar("advlgs-alpha_r");
    const Fr alphaY = randomScalar("advlgs-alpha_y");
    const Fr alphaTU = randomScalar("advlgs-alpha_t_u");
    const Fr alphaX = randomScalar("advlgs-alpha_x");

    const auto barA = bls12381::g1Mul(member.a, r);
    const auto b = bls12381::g1Mul(p.p2, r);
    const auto cPoint = bls12381::g1Add(member.yPublic, bls12381::g1Mul(p.oPoint, r));
    const auto linkTagPoint = bls12381::g1Mul(p.djt, member.y);
    const auto l = bls12381::g1Add(linkTagPoint, bls12381::g1Mul(p.vj, r));
    auto hatA = bls12381::g1Add(bls12381::g1Mul(p.p1PlusRDelta, r), bls12381::g1Mul(b, member.y));
    hatA = bls12381::g1Add(hatA, bls12381::g1Mul(p.p3, tU));
    hatA = bls12381::g1Sub(hatA, bls12381::g1Mul(barA, member.x));

    const auto rb = bls12381::g1Mul(p.p2, alphaR);
    const auto rc = bls12381::g1Add(bls12381::g1Mul(p.p2, alphaY), bls12381::g1Mul(p.oPoint, alphaR));
    const auto rl = bls12381::g1Add(bls12381::g1Mul(p.djt, alphaY), bls12381::g1Mul(p.vj, alphaR));
    auto ra = bls12381::g1Add(bls12381::g1Mul(p.p1PlusRDelta, alphaR), bls12381::g1Mul(b, alphaY));
    ra = bls12381::g1Add(ra, bls12381::g1Mul(p.p3, alphaTU));
    ra = bls12381::g1Sub(ra, bls12381::g1Mul(barA, alphaX));

    const Fr c = challenge({
        "ADVLGS", message, bls12381::g1ToHex(p.pkRA), spjt(), tsLabel(), bls12381::g1ToHex(barA),
        bls12381::g1ToHex(hatA), bls12381::g1ToHex(b), bls12381::g1ToHex(cPoint), bls12381::g1ToHex(l),
        bls12381::g1ToHex(rb), bls12381::g1ToHex(rc), bls12381::g1ToHex(rl), bls12381::g1ToHex(ra)});
    const Fr zR = bls12381::add(alphaR, bls12381::mul(c, r));
    const Fr zY = bls12381::add(alphaY, bls12381::mul(c, member.y));
    const Fr zTU = bls12381::add(alphaTU, bls12381::mul(c, tU));
    const Fr zX = bls12381::add(alphaX, bls12381::mul(c, member.x));

    ADVLGSSignature sig;
    sig.encoded = joinSignature({
        "ADVLGS", bls12381::g1ToHex(barA), bls12381::g1ToHex(hatA), bls12381::g1ToHex(b),
        bls12381::g1ToHex(cPoint), bls12381::g1ToHex(l), bls12381::scalarToHex(c),
        bls12381::scalarToHex(zR), bls12381::scalarToHex(zY), bls12381::scalarToHex(zTU),
        bls12381::scalarToHex(zX)});
    sig.linkTag = bls12381::g1ToHex(linkTagPoint);
    sig.wireBytes = nominalSignatureBytes();
    return sig;
}

bool ADVLGSMiracl::verify(const std::string& message, const std::string& serviceDomain, const std::string& encodedSignature, std::string* linkTagOut)
{
    (void)serviceDomain;
    if (linkTagOut) linkTagOut->clear();
    ParsedSignature sig;
    if (!parseSignature(encodedSignature, &sig)) return false;
    if (!verifyPairingEquation(sig)) return false;
    return verifyProofWithoutPairing(message, sig);
}

ADVLGSBatchResult ADVLGSMiracl::batchVerify(const std::vector<ADVLGSBatchItem>& items)
{
    ADVLGSBatchResult result;
    result.accepted.assign(items.size(), false);
    result.linkTags.assign(items.size(), "");
    if (items.empty()) return result;

    std::vector<ParsedSignature> parsed(items.size());
    std::vector<std::size_t> candidates;
    candidates.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (!parseSignature(items[i].encodedSignature, &parsed[i])) continue;
        if (!verifyProofWithoutPairing(items[i].message, parsed[i])) continue;
        candidates.push_back(i);
    }
    if (candidates.empty()) return result;

    const auto& p = params();
    BLS12381::ECP sumBarA;
    BLS12381::ECP sumHatA;
    bool first = true;
    for (std::size_t pos = 0; pos < candidates.size(); ++pos) {
        const std::size_t i = candidates[pos];
        const Fr gamma = bls12381::hashScalar("ASD-DVLGS-batch-gamma|" + std::to_string(pos) + "|" + items[i].message + "|" + items[i].encodedSignature);
        const auto weightedBarA = bls12381::g1Mul(parsed[i].barA, gamma);
        const auto weightedHatA = bls12381::g1Mul(parsed[i].hatA, gamma);
        if (first) {
            sumBarA = weightedBarA;
            sumHatA = weightedHatA;
            first = false;
        }
        else {
            sumBarA = bls12381::g1Add(sumBarA, weightedBarA);
            sumHatA = bls12381::g1Add(sumHatA, weightedHatA);
        }
    }

    const bool batchPairingOk = bls12381::gtEquals(bls12381::pair(p.sQ, sumBarA), bls12381::pair(p.q, sumHatA));
    if (batchPairingOk) {
        for (const auto i : candidates) result.accepted[i] = true;
        return result;
    }

    for (const auto i : candidates) {
        result.accepted[i] = verifyPairingEquation(parsed[i]);
        if (!result.accepted[i]) result.linkTags[i].clear();
    }
    return result;
}

} // namespace advlgs
