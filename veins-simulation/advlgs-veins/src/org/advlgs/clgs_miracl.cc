#include "org/advlgs/clgs_miracl.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "org/advlgs/bls12381_utils.h"

namespace advlgs {
namespace {

using bls12381::Fr;

constexpr std::size_t kSmallIntBytes = 4;

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

struct Params {
    BLS12381::ECP g;
    BLS12381::ECP g1;
    BLS12381::ECP g2InG1;
    BLS12381::ECP u;
    BLS12381::ECP w;
    BLS12381::ECP d;
    BLS12381::ECP2 h1;
    BLS12381::ECP2 hTheta;
    BLS12381::FP12 eW_HTheta;
    BLS12381::FP12 eW_H1;
    BLS12381::FP12 eG2_H1;
    BLS12381::FP12 eG1_H1;
    Fr theta;
    Fr eta;
    Fr xi;

    Params()
        : g(hashToG1("hwang-g"))
        , g1(hashToG1("hwang-g1"))
        , g2InG1(hashToG1("hwang-g2-in-G1"))
        , u(hashToG1("hwang-u"))
        , h1(hashToG2("hwang-h1"))
        , theta(bls12381::hashScalar("hwang-theta"))
        , eta(bls12381::hashScalar("hwang-eta"))
        , xi(bls12381::hashScalar("hwang-xi"))
    {
        w = bls12381::g1Mul(u, eta);
        d = bls12381::g1Mul(u, xi);
        hTheta = bls12381::g2Mul(h1, theta);
        eW_HTheta = bls12381::pair(hTheta, w);
        eW_H1 = bls12381::pair(h1, w);
        eG2_H1 = bls12381::pair(h1, g2InG1);
        eG1_H1 = bls12381::pair(h1, g1);
    }
};

struct MemberKey {
    Fr x;
    Fr y;
    Fr z;
    BLS12381::ECP a;
    BLS12381::FP12 eA_H1;
};

const Params& params()
{
    static const Params p;
    return p;
}

MemberKey buildMemberKey(int memberId)
{
    const auto& p = params();
    MemberKey out;
    out.x = bls12381::hashScalar("hwang-member-x|" + std::to_string(memberId));
    out.y = bls12381::hashScalar("hwang-member-y|" + std::to_string(memberId));
    out.z = bls12381::hashScalar("hwang-member-z|" + std::to_string(memberId));
    auto numerator = bls12381::g1Sub(bls12381::g1Sub(p.g1, bls12381::g1Mul(p.g2InG1, out.y)), bls12381::g1Mul(p.w, out.z));
    out.a = bls12381::g1Mul(numerator, bls12381::inv(bls12381::add(p.theta, out.x)));
    out.eA_H1 = bls12381::pair(p.h1, out.a);
    return out;
}

std::unordered_map<int, MemberKey>& memberCache()
{
    static std::unordered_map<int, MemberKey> cache;
    return cache;
}

const MemberKey& memberKey(int memberId)
{
    auto& cache = memberCache();
    const auto found = cache.find(memberId);
    if (found != cache.end()) return found->second;
    return cache.emplace(memberId, buildMemberKey(memberId)).first->second;
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

bool parseSignature(const std::vector<std::string>& parts,
                    BLS12381::ECP* d1,
                    BLS12381::ECP* d2,
                    BLS12381::ECP* d3,
                    Fr* c,
                    Fr* sAlpha,
                    Fr* sX,
                    Fr* sGamma,
                    Fr* sY)
{
    if (parts.size() != 9 || parts[0] != "CLGS") return false;
    bool ok = false;
    if (!bls12381::g1FromHex(parts[1], d1)) return false;
    if (!bls12381::g1FromHex(parts[2], d2)) return false;
    if (!bls12381::g1FromHex(parts[3], d3)) return false;
    *c = bls12381::scalarFromHex(parts[4], &ok);
    if (!ok) return false;
    *sAlpha = bls12381::scalarFromHex(parts[5], &ok);
    if (!ok) return false;
    *sX = bls12381::scalarFromHex(parts[6], &ok);
    if (!ok) return false;
    *sGamma = bls12381::scalarFromHex(parts[7], &ok);
    if (!ok) return false;
    *sY = bls12381::scalarFromHex(parts[8], &ok);
    return ok;
}

} // namespace

void CLGSMiracl::warmup()
{
    (void)params();
}

void CLGSMiracl::prepareMember(int memberId)
{
    (void)memberKey(memberId);
}

std::size_t CLGSMiracl::nominalSignatureBytes()
{
    return kSmallIntBytes + 3 * bls12381::kG1CompressedWireBytes + 5 * bls12381::kScalarWireBytes;
}

CLGSSignature CLGSMiracl::sign(int memberId, const std::string& message, const std::string& serviceDomain)
{
    (void)serviceDomain;
    const auto& p = params();
    const auto& member = memberKey(memberId);

    const Fr alpha = randomScalar("hwang-alpha");
    const Fr gamma = bls12381::sub(bls12381::mul(member.x, alpha), member.z);
    const Fr rAlpha = randomScalar("hwang-r-alpha");
    const Fr rX = randomScalar("hwang-r-x");
    const Fr rGamma = randomScalar("hwang-r-gamma");
    const Fr rY = randomScalar("hwang-r-y");

    const auto d1 = bls12381::g1Mul(p.u, alpha);
    const auto d2 = bls12381::g1Add(member.a, bls12381::g1Mul(p.w, alpha));
    const auto d3 = bls12381::g1Add(bls12381::g1Mul(p.g, member.y), bls12381::g1Mul(p.d, alpha));
    const auto r1 = bls12381::g1Mul(p.u, rAlpha);
    auto r2 = bls12381::gtPow(member.eA_H1, rX);
    r2 = bls12381::gtMul(r2, bls12381::gtPow(p.eW_H1, bls12381::sub(bls12381::mul(alpha, rX), rGamma)));
    r2 = bls12381::gtMul(r2, bls12381::gtPow(p.eW_HTheta, bls12381::neg(rAlpha)));
    r2 = bls12381::gtMul(r2, bls12381::gtPow(p.eG2_H1, rY));
    const auto r3 = bls12381::g1Add(bls12381::g1Mul(p.g, rY), bls12381::g1Mul(p.d, rAlpha));

    const Fr c = challenge({"HWANG", message, bls12381::g1ToHex(d1), bls12381::g1ToHex(d2),
                            bls12381::g1ToHex(d3), bls12381::g1ToHex(r1), bls12381::gtToHex(r2),
                            bls12381::g1ToHex(r3)});
    const Fr sAlpha = bls12381::add(rAlpha, bls12381::mul(c, alpha));
    const Fr sX = bls12381::add(rX, bls12381::mul(c, member.x));
    const Fr sGamma = bls12381::add(rGamma, bls12381::mul(c, gamma));
    const Fr sY = bls12381::add(rY, bls12381::mul(c, member.y));

    CLGSSignature sig;
    sig.encoded = joinSignature({
        "CLGS", bls12381::g1ToHex(d1), bls12381::g1ToHex(d2), bls12381::g1ToHex(d3),
        bls12381::scalarToHex(c), bls12381::scalarToHex(sAlpha), bls12381::scalarToHex(sX),
        bls12381::scalarToHex(sGamma), bls12381::scalarToHex(sY)});
    sig.linkTag.clear();
    sig.wireBytes = nominalSignatureBytes();
    return sig;
}

bool CLGSMiracl::verify(const std::string& message, const std::string& serviceDomain, const std::string& encodedSignature, std::string* linkTag)
{
    (void)serviceDomain;
    const auto parts = splitSignature(encodedSignature);
    BLS12381::ECP d1;
    BLS12381::ECP d2;
    BLS12381::ECP d3;
    Fr c;
    Fr sAlpha;
    Fr sX;
    Fr sGamma;
    Fr sY;
    if (!parseSignature(parts, &d1, &d2, &d3, &c, &sAlpha, &sX, &sGamma, &sY)) return false;

    const auto& p = params();
    const auto r1 = bls12381::g1Sub(bls12381::g1Mul(p.u, sAlpha), bls12381::g1Mul(d1, c));
    const auto pairBase = bls12381::g2Add(bls12381::g2Mul(p.h1, sX), bls12381::g2Mul(p.hTheta, c));
    auto r2 = bls12381::pair(pairBase, d2);
    r2 = bls12381::gtMul(r2, bls12381::gtPow(p.eW_HTheta, bls12381::neg(sAlpha)));
    r2 = bls12381::gtMul(r2, bls12381::gtPow(p.eW_H1, bls12381::neg(sGamma)));
    r2 = bls12381::gtMul(r2, bls12381::gtPow(p.eG2_H1, sY));
    r2 = bls12381::gtMul(r2, bls12381::gtPow(p.eG1_H1, bls12381::neg(c)));
    const auto r3 = bls12381::g1Sub(
        bls12381::g1Add(bls12381::g1Mul(p.g, sY), bls12381::g1Mul(p.d, sAlpha)),
        bls12381::g1Mul(d3, c));

    const Fr cPrime = challenge({"HWANG", message, parts[1], parts[2], parts[3], bls12381::g1ToHex(r1),
                                 bls12381::gtToHex(r2), bls12381::g1ToHex(r3)});
    if (!bls12381::scalarEquals(c, cPrime)) return false;
    if (linkTag) linkTag->clear();
    return true;
}

} // namespace advlgs
