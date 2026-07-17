#include "org/advlgs/erca_miracl.h"

#include <sstream>
#include <string>
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
    BLS12381::ECP p1;
    BLS12381::ECP pPub;
    BLS12381::ECP2 p2;
    BLS12381::ECP2 acc0;
    BLS12381::ECP2 wi;
    BLS12381::ECP2 ci;
    BLS12381::ECP2 accJ;
    BLS12381::ECP2 accJPlusP2;
    Fr x;
    Fr si;

    Params()
        : p1(hashToG1("iiot-p1"))
        , p2(hashToG2("iiot-p2"))
        , x(bls12381::hashScalar("iiot-manager-x"))
        , si(bls12381::hashScalar("iiot-member-si"))
    {
        const Fr r = bls12381::hashScalar("iiot-accumulator-r");
        pPub = bls12381::g1Mul(p1, x);
        acc0 = bls12381::g2Mul(p2, r);
        wi = acc0;
        ci = bls12381::g2Mul(p2, bls12381::inv(bls12381::add(si, x)));
        accJ = bls12381::g2Mul(wi, bls12381::add(si, x));
        accJPlusP2 = bls12381::g2Add(accJ, p2);
    }
};

const Params& params()
{
    static const Params p;
    return p;
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
                    BLS12381::ECP2* a1,
                    BLS12381::ECP2* a2,
                    Fr* c,
                    Fr* su,
                    Fr* ss)
{
    if (parts.size() != 9 || parts[0] != "ERCA") return false;
    bool ok = false;
    if (!bls12381::g1FromHex(parts[1], t1)) return false;
    if (!bls12381::g1FromHex(parts[2], t2)) return false;
    if (!bls12381::g1FromHex(parts[3], t3)) return false;
    if (!bls12381::g2FromHex(parts[4], a1)) return false;
    if (!bls12381::g2FromHex(parts[5], a2)) return false;
    *c = bls12381::scalarFromHex(parts[6], &ok);
    if (!ok) return false;
    *su = bls12381::scalarFromHex(parts[7], &ok);
    if (!ok) return false;
    *ss = bls12381::scalarFromHex(parts[8], &ok);
    return ok;
}

} // namespace

void ERCAMiracl::warmup()
{
    (void)params();
}

void ERCAMiracl::prepareMember(int memberId)
{
    (void)memberId;
    (void)params();
}

std::size_t ERCAMiracl::nominalSignatureBytes()
{
    return 3 * bls12381::kG1CompressedWireBytes + 4 * bls12381::kG2CompressedWireBytes + 3 * bls12381::kScalarWireBytes;
}

ERCASignature ERCAMiracl::sign(int memberId, const std::string& message, const std::string& serviceDomain)
{
    (void)memberId;
    (void)serviceDomain;
    const auto& p = params();

    const Fr u = randomScalar("iiot-u");
    const Fr ru = randomScalar("iiot-ru");
    const Fr rs = randomScalar("iiot-rs");
    const auto t1 = bls12381::g1Mul(p.p1, u);
    const auto t2 = bls12381::g1Mul(t1, u);
    const auto t3 = bls12381::g1Add(bls12381::g1Mul(p.p1, p.si), bls12381::g1Mul(p.pPub, u));
    const auto a1 = bls12381::g2Mul(p.wi, u);
    const auto a2 = bls12381::g2Mul(p.ci, u);

    const auto r1 = bls12381::g1Mul(p.p1, ru);
    const auto r2 = bls12381::g1Mul(t1, ru);
    const auto aSum = bls12381::g2Add(a1, a2);
    auto r3 = bls12381::gtPow(bls12381::pair(aSum, p.pPub), ru);
    r3 = bls12381::gtMul(r3, bls12381::gtPow(bls12381::pair(aSum, t1), rs));
    const auto r4 = bls12381::g1Add(bls12381::g1Mul(p.p1, rs), bls12381::g1Mul(p.pPub, ru));

    const Fr c = challenge({"IIOT", message, bls12381::g1ToHex(t1), bls12381::g1ToHex(t2),
                            bls12381::g1ToHex(t3), bls12381::g2ToHex(a1), bls12381::g2ToHex(a2),
                            bls12381::g1ToHex(r1), bls12381::g1ToHex(r2), bls12381::gtToHex(r3),
                            bls12381::g1ToHex(r4)});
    const Fr su = bls12381::add(ru, bls12381::mul(c, u));
    const Fr ss = bls12381::add(rs, bls12381::mul(c, p.si));

    ERCASignature sig;
    sig.encoded = joinSignature({
        "ERCA", bls12381::g1ToHex(t1), bls12381::g1ToHex(t2), bls12381::g1ToHex(t3),
        bls12381::g2ToHex(a1), bls12381::g2ToHex(a2), bls12381::scalarToHex(c),
        bls12381::scalarToHex(su), bls12381::scalarToHex(ss)});
    sig.linkTag.clear();
    sig.wireBytes = nominalSignatureBytes();
    return sig;
}

bool ERCAMiracl::verify(const std::string& message, const std::string& serviceDomain, const std::string& encodedSignature, std::string* linkTag)
{
    (void)serviceDomain;
    const auto parts = splitSignature(encodedSignature);
    BLS12381::ECP t1;
    BLS12381::ECP t2;
    BLS12381::ECP t3;
    BLS12381::ECP2 a1;
    BLS12381::ECP2 a2;
    Fr c;
    Fr su;
    Fr ss;
    if (!parseSignature(parts, &t1, &t2, &t3, &a1, &a2, &c, &su, &ss)) return false;

    const auto& p = params();
    const auto r1 = bls12381::g1Sub(bls12381::g1Mul(p.p1, su), bls12381::g1Mul(t1, c));
    const auto r2 = bls12381::g1Sub(bls12381::g1Mul(t1, su), bls12381::g1Mul(t2, c));
    const auto r4 = bls12381::g1Sub(
        bls12381::g1Add(bls12381::g1Mul(p.p1, ss), bls12381::g1Mul(p.pPub, su)),
        bls12381::g1Mul(t3, c));
    const auto aSum = bls12381::g2Add(a1, a2);
    auto r3 = bls12381::gtPow(bls12381::pair(aSum, p.pPub), su);
    r3 = bls12381::gtMul(r3, bls12381::gtPow(bls12381::pair(aSum, t1), ss));
    r3 = bls12381::gtMul(r3, bls12381::gtPow(bls12381::pair(p.accJPlusP2, t2), bls12381::neg(c)));

    const Fr cPrime = challenge({"IIOT", message, parts[1], parts[2], parts[3], parts[4], parts[5],
                                 bls12381::g1ToHex(r1), bls12381::g1ToHex(r2), bls12381::gtToHex(r3),
                                 bls12381::g1ToHex(r4)});
    if (!bls12381::scalarEquals(c, cPrime)) return false;
    if (linkTag) linkTag->clear();
    return true;
}

} // namespace advlgs
