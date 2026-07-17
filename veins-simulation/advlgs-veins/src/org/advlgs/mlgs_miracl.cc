#include "org/advlgs/mlgs_miracl.h"

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
    BLS12381::ECP g1Base;
    BLS12381::ECP u;
    BLS12381::ECP v;
    BLS12381::ECP h;
    BLS12381::ECP rState;
    BLS12381::ECP gbar1;
    BLS12381::ECP2 g2Base;
    BLS12381::ECP2 w;
    Fr gamma;

    Params()
        : g1Base(hashToG1("zhang-g1"))
        , u(hashToG1("zhang-u"))
        , v(hashToG1("zhang-v"))
        , h(hashToG1("zhang-h"))
        , rState(hashToG1("zhang-H3|stbi-default"))
        , g2Base(hashToG2("zhang-g2"))
        , gamma(bls12381::hashScalar("zhang-gamma"))
    {
        gbar1 = bls12381::g1Add(g1Base, rState);
        w = bls12381::g2Mul(g2Base, gamma);
    }
};

struct MemberKey {
    Fr x;
    BLS12381::ECP abi;
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
    out.x = bls12381::hashScalar("zhang-member-x|" + std::to_string(memberId));
    out.abi = bls12381::g1Mul(p.gbar1, bls12381::inv(bls12381::add(p.gamma, out.x)));
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
                    BLS12381::ECP* t1,
                    BLS12381::ECP* t2,
                    BLS12381::ECP* t3,
                    BLS12381::ECP* token,
                    Fr* c,
                    Fr* sAlpha,
                    Fr* sBeta,
                    Fr* sX,
                    Fr* sDelta1,
                    Fr* sDelta2)
{
    if (parts.size() != 11 || parts[0] != "MLGS") return false;
    bool ok = false;
    if (!bls12381::g1FromHex(parts[1], t1)) return false;
    if (!bls12381::g1FromHex(parts[2], t2)) return false;
    if (!bls12381::g1FromHex(parts[3], t3)) return false;
    if (!bls12381::g1FromHex(parts[4], token)) return false;
    *c = bls12381::scalarFromHex(parts[5], &ok);
    if (!ok) return false;
    *sAlpha = bls12381::scalarFromHex(parts[6], &ok);
    if (!ok) return false;
    *sBeta = bls12381::scalarFromHex(parts[7], &ok);
    if (!ok) return false;
    *sX = bls12381::scalarFromHex(parts[8], &ok);
    if (!ok) return false;
    *sDelta1 = bls12381::scalarFromHex(parts[9], &ok);
    if (!ok) return false;
    *sDelta2 = bls12381::scalarFromHex(parts[10], &ok);
    return ok;
}

} // namespace

void MLGSMiracl::warmup()
{
    (void)params();
}

void MLGSMiracl::prepareMember(int memberId)
{
    (void)memberKey(memberId);
}

std::size_t MLGSMiracl::nominalSignatureBytes()
{
    return 4 * bls12381::kG1CompressedWireBytes + 6 * bls12381::kScalarWireBytes + 2 * kSmallIntBytes;
}

MLGSSignature MLGSMiracl::sign(int memberId, const std::string& status, const std::string& message, const std::string& serviceDomain)
{
    (void)status;
    (void)serviceDomain;
    const auto& p = params();
    const auto& member = memberKey(memberId);

    const Fr alpha = randomScalar("zhang-alpha");
    const Fr beta = randomScalar("zhang-beta");
    const Fr rAlpha = randomScalar("zhang-r-alpha");
    const Fr rBeta = randomScalar("zhang-r-beta");
    const Fr rX = randomScalar("zhang-r-x");
    const Fr rDelta1 = randomScalar("zhang-r-delta1");
    const Fr rDelta2 = randomScalar("zhang-r-delta2");
    const Fr delta1 = bls12381::mul(alpha, member.x);
    const Fr delta2 = bls12381::mul(beta, member.x);

    const auto t1 = bls12381::g1Mul(p.u, alpha);
    const auto t2 = bls12381::g1Mul(p.v, beta);
    const auto h2Message = hashToG1("zhang-H2|" + message);
    const auto t3 = bls12381::g1Add(member.abi, bls12381::g1Mul(p.h, bls12381::add(alpha, beta)));
    const auto token = bls12381::g1Mul(h2Message, member.x);

    auto r3 = bls12381::pair(p.g2Base, bls12381::g1Mul(t3, rX));
    auto secondPairBase = bls12381::g2Add(
        bls12381::g2Mul(p.w, bls12381::neg(bls12381::add(rAlpha, rBeta))),
        bls12381::g2Mul(p.g2Base, bls12381::neg(bls12381::add(rDelta1, rDelta2))));
    r3 = bls12381::gtMul(r3, bls12381::pair(secondPairBase, p.h));
    const auto r1 = bls12381::g1Mul(p.u, rAlpha);
    const auto r2 = bls12381::g1Mul(p.v, rBeta);
    const auto r4 = bls12381::g1Sub(bls12381::g1Mul(t1, rX), bls12381::g1Mul(p.u, rDelta1));
    const auto r5 = bls12381::g1Sub(bls12381::g1Mul(t2, rX), bls12381::g1Mul(p.v, rDelta2));
    const auto r6 = bls12381::g1Mul(h2Message, rX);

    const Fr c = challenge({"ZHANG", message, bls12381::g1ToHex(t1), bls12381::g1ToHex(t2),
                            bls12381::g1ToHex(t3), bls12381::g1ToHex(r1), bls12381::g1ToHex(r2),
                            bls12381::gtToHex(r3), bls12381::g1ToHex(r4), bls12381::g1ToHex(r5),
                            bls12381::g1ToHex(r6), bls12381::g1ToHex(token)});
    const Fr sAlpha = bls12381::add(rAlpha, bls12381::mul(c, alpha));
    const Fr sBeta = bls12381::add(rBeta, bls12381::mul(c, beta));
    const Fr sX = bls12381::add(rX, bls12381::mul(c, member.x));
    const Fr sDelta1 = bls12381::add(rDelta1, bls12381::mul(c, delta1));
    const Fr sDelta2 = bls12381::add(rDelta2, bls12381::mul(c, delta2));

    MLGSSignature sig;
    sig.encoded = joinSignature({
        "MLGS", bls12381::g1ToHex(t1), bls12381::g1ToHex(t2), bls12381::g1ToHex(t3),
        bls12381::g1ToHex(token), bls12381::scalarToHex(c), bls12381::scalarToHex(sAlpha),
        bls12381::scalarToHex(sBeta), bls12381::scalarToHex(sX), bls12381::scalarToHex(sDelta1),
        bls12381::scalarToHex(sDelta2)});
    sig.linkTag = bls12381::g1ToHex(token);
    sig.wireBytes = nominalSignatureBytes();
    return sig;
}

bool MLGSMiracl::verify(const std::string& status, const std::string& message, const std::string& serviceDomain, const std::string& encodedSignature, std::string* linkTagOut)
{
    (void)status;
    (void)serviceDomain;
    const auto parts = splitSignature(encodedSignature);
    BLS12381::ECP t1;
    BLS12381::ECP t2;
    BLS12381::ECP t3;
    BLS12381::ECP token;
    Fr c;
    Fr sAlpha;
    Fr sBeta;
    Fr sX;
    Fr sDelta1;
    Fr sDelta2;
    if (!parseSignature(parts, &t1, &t2, &t3, &token, &c, &sAlpha, &sBeta, &sX, &sDelta1, &sDelta2)) return false;

    const auto& p = params();
    const auto h2Message = hashToG1("zhang-H2|" + message);
    const auto r1 = bls12381::g1Sub(bls12381::g1Mul(p.u, sAlpha), bls12381::g1Mul(t1, c));
    const auto r2 = bls12381::g1Sub(bls12381::g1Mul(p.v, sBeta), bls12381::g1Mul(t2, c));
    const auto r4 = bls12381::g1Sub(bls12381::g1Mul(t1, sX), bls12381::g1Mul(p.u, sDelta1));
    const auto r5 = bls12381::g1Sub(bls12381::g1Mul(t2, sX), bls12381::g1Mul(p.v, sDelta2));
    const auto r6 = bls12381::g1Sub(bls12381::g1Mul(h2Message, sX), bls12381::g1Mul(token, c));

    auto first = bls12381::g1Sub(
        bls12381::g1Sub(bls12381::g1Mul(t3, sX), bls12381::g1Mul(p.h, bls12381::add(sDelta1, sDelta2))),
        bls12381::g1Mul(p.gbar1, c));
    auto second = bls12381::g1Sub(bls12381::g1Mul(t3, c), bls12381::g1Mul(p.h, bls12381::add(sAlpha, sBeta)));
    auto r3 = bls12381::gtMul(bls12381::pair(p.g2Base, first), bls12381::pair(p.w, second));

    const Fr cPrime = challenge({"ZHANG", message, parts[1], parts[2], parts[3], bls12381::g1ToHex(r1),
                                 bls12381::g1ToHex(r2), bls12381::gtToHex(r3), bls12381::g1ToHex(r4),
                                 bls12381::g1ToHex(r5), bls12381::g1ToHex(r6), parts[4]});
    if (!bls12381::scalarEquals(c, cPrime)) return false;
    if (linkTagOut) *linkTagOut = parts[4];
    return true;
}

} // namespace advlgs
