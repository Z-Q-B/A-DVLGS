#include "org/advlgs/bbs_miracl.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "org/advlgs/bls12381_utils.h"

namespace advlgs {
namespace {

using bls12381::Fr;

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
    BLS12381::ECP g1;
    BLS12381::ECP h;
    BLS12381::ECP u;
    BLS12381::ECP v;
    BLS12381::ECP2 g2;
    BLS12381::ECP2 w;
    BLS12381::FP12 eH_W;
    BLS12381::FP12 eH_G2;
    BLS12381::FP12 eG1_G2;
    Fr gamma;

    Params()
        : g1(hashToG1("bbs-g1"))
        , h(hashToG1("bbs-h"))
        , u(hashToG1("bbs-u"))
        , v(hashToG1("bbs-v"))
        , g2(hashToG2("bbs-g2"))
        , gamma(bls12381::hashScalar("bbs-gamma"))
    {
        w = bls12381::g2Mul(g2, gamma);
        eH_W = bls12381::pair(w, h);
        eH_G2 = bls12381::pair(g2, h);
        eG1_G2 = bls12381::pair(g2, g1);
    }
};

struct MemberKey {
    Fr x;
    BLS12381::ECP a;
    BLS12381::FP12 eA_G2;
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
    out.x = bls12381::hashScalar("bbs-member-x|" + std::to_string(memberId));
    out.a = bls12381::g1Mul(p.g1, bls12381::inv(bls12381::add(p.gamma, out.x)));
    out.eA_G2 = bls12381::pair(p.g2, out.a);
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
                    Fr* c,
                    Fr* sAlpha,
                    Fr* sBeta,
                    Fr* sX,
                    Fr* sDelta1,
                    Fr* sDelta2)
{
    if (parts.size() != 10 || parts[0] != "BBS") return false;
    bool ok = false;
    if (!bls12381::g1FromHex(parts[1], t1)) return false;
    if (!bls12381::g1FromHex(parts[2], t2)) return false;
    if (!bls12381::g1FromHex(parts[3], t3)) return false;
    *c = bls12381::scalarFromHex(parts[4], &ok);
    if (!ok) return false;
    *sAlpha = bls12381::scalarFromHex(parts[5], &ok);
    if (!ok) return false;
    *sBeta = bls12381::scalarFromHex(parts[6], &ok);
    if (!ok) return false;
    *sX = bls12381::scalarFromHex(parts[7], &ok);
    if (!ok) return false;
    *sDelta1 = bls12381::scalarFromHex(parts[8], &ok);
    if (!ok) return false;
    *sDelta2 = bls12381::scalarFromHex(parts[9], &ok);
    return ok;
}

} // namespace

void BBSMiracl::warmup()
{
    (void)params();
}

void BBSMiracl::prepareMember(int memberId)
{
    (void)memberKey(memberId);
}

std::size_t BBSMiracl::nominalSignatureBytes()
{
    return 3 * bls12381::kG1CompressedWireBytes + 6 * bls12381::kScalarWireBytes;
}

BBSSignature BBSMiracl::sign(int memberId, const std::string& message, const std::string& serviceDomain)
{
    (void)serviceDomain;
    const auto& p = params();
    const auto& member = memberKey(memberId);

    const Fr alpha = randomScalar("bbs-alpha");
    const Fr beta = randomScalar("bbs-beta");
    const Fr rAlpha = randomScalar("bbs-r-alpha");
    const Fr rBeta = randomScalar("bbs-r-beta");
    const Fr rX = randomScalar("bbs-r-x");
    const Fr rDelta1 = randomScalar("bbs-r-delta1");
    const Fr rDelta2 = randomScalar("bbs-r-delta2");
    const Fr delta1 = bls12381::mul(alpha, member.x);
    const Fr delta2 = bls12381::mul(beta, member.x);

    const auto t1 = bls12381::g1Mul(p.u, alpha);
    const auto t2 = bls12381::g1Mul(p.v, beta);
    const auto t3 = bls12381::g1Add(member.a, bls12381::g1Mul(p.h, bls12381::add(alpha, beta)));

    auto r3 = bls12381::gtPow(member.eA_G2, rX);
    const Fr hG2Exp = bls12381::sub(bls12381::mul(bls12381::add(alpha, beta), rX), bls12381::add(rDelta1, rDelta2));
    r3 = bls12381::gtMul(r3, bls12381::gtPow(p.eH_G2, hG2Exp));
    r3 = bls12381::gtMul(r3, bls12381::gtPow(p.eH_W, bls12381::neg(bls12381::add(rAlpha, rBeta))));

    const auto r1 = bls12381::g1Mul(p.u, rAlpha);
    const auto r2 = bls12381::g1Mul(p.v, rBeta);
    const auto r4 = bls12381::g1Sub(bls12381::g1Mul(t1, rX), bls12381::g1Mul(p.u, rDelta1));
    const auto r5 = bls12381::g1Sub(bls12381::g1Mul(t2, rX), bls12381::g1Mul(p.v, rDelta2));

    const Fr c = challenge({"BBS", message, bls12381::g1ToHex(t1), bls12381::g1ToHex(t2), bls12381::g1ToHex(t3),
                            bls12381::g1ToHex(r1), bls12381::g1ToHex(r2), bls12381::gtToHex(r3),
                            bls12381::g1ToHex(r4), bls12381::g1ToHex(r5)});
    const Fr sAlpha = bls12381::add(rAlpha, bls12381::mul(c, alpha));
    const Fr sBeta = bls12381::add(rBeta, bls12381::mul(c, beta));
    const Fr sX = bls12381::add(rX, bls12381::mul(c, member.x));
    const Fr sDelta1 = bls12381::add(rDelta1, bls12381::mul(c, delta1));
    const Fr sDelta2 = bls12381::add(rDelta2, bls12381::mul(c, delta2));

    BBSSignature sig;
    sig.encoded = joinSignature({
        "BBS", bls12381::g1ToHex(t1), bls12381::g1ToHex(t2), bls12381::g1ToHex(t3),
        bls12381::scalarToHex(c), bls12381::scalarToHex(sAlpha), bls12381::scalarToHex(sBeta),
        bls12381::scalarToHex(sX), bls12381::scalarToHex(sDelta1), bls12381::scalarToHex(sDelta2)});
    sig.linkTag.clear();
    sig.wireBytes = nominalSignatureBytes();
    return sig;
}

bool BBSMiracl::verify(const std::string& message, const std::string& serviceDomain, const std::string& encodedSignature, std::string* linkTag)
{
    (void)serviceDomain;
    const auto parts = splitSignature(encodedSignature);
    BLS12381::ECP t1;
    BLS12381::ECP t2;
    BLS12381::ECP t3;
    Fr c;
    Fr sAlpha;
    Fr sBeta;
    Fr sX;
    Fr sDelta1;
    Fr sDelta2;
    if (!parseSignature(parts, &t1, &t2, &t3, &c, &sAlpha, &sBeta, &sX, &sDelta1, &sDelta2)) return false;

    const auto& p = params();
    const auto r1 = bls12381::g1Sub(bls12381::g1Mul(p.u, sAlpha), bls12381::g1Mul(t1, c));
    const auto r2 = bls12381::g1Sub(bls12381::g1Mul(p.v, sBeta), bls12381::g1Mul(t2, c));
    const auto r4 = bls12381::g1Sub(bls12381::g1Mul(t1, sX), bls12381::g1Mul(p.u, sDelta1));
    const auto r5 = bls12381::g1Sub(bls12381::g1Mul(t2, sX), bls12381::g1Mul(p.v, sDelta2));

    auto pairBase = bls12381::g2Add(bls12381::g2Mul(p.g2, sX), bls12381::g2Mul(p.w, c));
    auto r3 = bls12381::pair(pairBase, t3);
    r3 = bls12381::gtMul(r3, bls12381::gtPow(p.eH_W, bls12381::neg(bls12381::add(sAlpha, sBeta))));
    r3 = bls12381::gtMul(r3, bls12381::gtPow(p.eH_G2, bls12381::neg(bls12381::add(sDelta1, sDelta2))));
    r3 = bls12381::gtMul(r3, bls12381::gtPow(p.eG1_G2, bls12381::neg(c)));

    const Fr cPrime = challenge({"BBS", message, parts[1], parts[2], parts[3], bls12381::g1ToHex(r1),
                                 bls12381::g1ToHex(r2), bls12381::gtToHex(r3), bls12381::g1ToHex(r4),
                                 bls12381::g1ToHex(r5)});
    if (!bls12381::scalarEquals(c, cPrime)) return false;
    if (linkTag) linkTag->clear();
    return true;
}

} // namespace advlgs
