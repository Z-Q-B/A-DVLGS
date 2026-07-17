#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "pair_BLS12381.h"

using namespace BLS12381;
using namespace BLS12381_BIG;

namespace {

constexpr int kDefaultRounds = 10000;
constexpr int kDefaultMaxBatchSize = 16;
constexpr int kDefaultLargeBatchRounds = 100;
constexpr int kPrecomputedVehicleCount = 256;
constexpr int kMessageBytes = 128;
constexpr int kScalarBytes = 32;
constexpr int kG1Bytes = 48;
constexpr int kG2Bytes = 96;
constexpr int kGTBytes = 288;
constexpr int kIiotWiBytes = kG2Bytes;
constexpr int kIiotAccBytes = kG2Bytes;
constexpr int kSmallIntBytes = 4;
constexpr int kStateBytes = 16;
constexpr int kTimestampBytes = 8;
constexpr int kInternalScalarBytes = MODBYTES_B384_58;
constexpr int kInternalG1Bytes = MODBYTES_B384_58 + 1;
constexpr int kInternalG2BufferBytes = 4 * MODBYTES_B384_58 + 1;
constexpr int kInternalGtBytes = 12 * MODBYTES_B384_58;
constexpr const char* kTimingOutputDirEnv = "VEINS_TIMING_DIR";
constexpr const char* kVeinsTimingCsvName = "scheme_timings.csv";
constexpr const char* kVeinsTimingDoneName = "scheme_timings.done";

struct SchemeTimingExport {
    std::string scheme;
    double signMs;
    double verifyMs;
};

struct BatchTimingExport {
    int batchSize;
    double verifyMs;
};

std::vector<SchemeTimingExport> g_schemeTimingExports;
std::vector<BatchTimingExport> g_advlgsBatchTimingExports;

std::filesystem::path timingOutputPath(const char* fileName)
{
    const char* outputDir = std::getenv(kTimingOutputDirEnv);
    if (outputDir != nullptr && outputDir[0] != '\0') {
        return std::filesystem::path(outputDir) / fileName;
    }
    return std::filesystem::path(fileName);
}

struct Scalar {
    BIG v;

    Scalar()
    {
        BIG_zero(v);
    }
};

void curveOrder(BIG q)
{
    BIG_rcopy(q, CURVE_Order);
}

bool scalarEquals(const Scalar& a, const Scalar& b)
{
    BIG av;
    BIG bv;
    BIG_copy(av, const_cast<chunk*>(a.v));
    BIG_copy(bv, const_cast<chunk*>(b.v));
    return BIG_comp(av, bv) == 0;
}

Scalar scalarHash(const std::string& input)
{
    hash512 hash;
    std::array<char, 64> digest {};
    Scalar out;
    BIG q;
    curveOrder(q);
    HASH512_init(&hash);
    for (unsigned char c : input) {
        HASH512_process(&hash, c);
    }
    HASH512_hash(&hash, digest.data());
    BIG_fromBytesLen(out.v, digest.data(), static_cast<int>(digest.size()));
    BIG_mod(out.v, q);
    if (BIG_iszilch(out.v)) {
        BIG_one(out.v);
    }
    return out;
}

Scalar randomScalar(const std::string& label)
{
    static unsigned long long counter = 1;
    std::ostringstream os;
    os << "deterministic-advlgs-random|" << label << "|" << counter++;
    return scalarHash(os.str());
}

Scalar add(const Scalar& a, const Scalar& b)
{
    Scalar out;
    BIG q;
    curveOrder(q);
    BIG_modadd(out.v, const_cast<chunk*>(a.v), const_cast<chunk*>(b.v), q);
    return out;
}

Scalar neg(const Scalar& a)
{
    Scalar out;
    BIG q;
    curveOrder(q);
    BIG_modneg(out.v, const_cast<chunk*>(a.v), q);
    return out;
}

Scalar sub(const Scalar& a, const Scalar& b)
{
    return add(a, neg(b));
}

Scalar mul(const Scalar& a, const Scalar& b)
{
    Scalar out;
    BIG q;
    curveOrder(q);
    BIG_modmul(out.v, const_cast<chunk*>(a.v), const_cast<chunk*>(b.v), q);
    return out;
}

Scalar inv(const Scalar& a)
{
    Scalar out;
    BIG q;
    curveOrder(q);
    BIG_invmodp(out.v, const_cast<chunk*>(a.v), q);
    return out;
}

std::string bytesToHex(const char* data, int len)
{
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (int i = 0; i < len; ++i) {
        os << std::setw(2) << static_cast<int>(static_cast<unsigned char>(data[i]));
    }
    return os.str();
}

ECP g1Generator()
{
    ECP p;
    ECP_generator(&p);
    return p;
}

ECP2 g2Generator()
{
    ECP2 p;
    ECP2_generator(&p);
    return p;
}

ECP g1Mul(ECP point, const Scalar& scalar)
{
    BIG s;
    BIG_copy(s, const_cast<chunk*>(scalar.v));
    PAIR_G1mul(&point, s);
    return point;
}

ECP2 g2Mul(ECP2 point, const Scalar& scalar)
{
    BIG s;
    BIG_copy(s, const_cast<chunk*>(scalar.v));
    PAIR_G2mul(&point, s);
    return point;
}

ECP g1Add(ECP a, ECP b)
{
    ECP_add(&a, &b);
    return a;
}

ECP2 g2Add(ECP2 a, ECP2 b)
{
    ECP2_add(&a, &b);
    return a;
}

ECP g1Sub(ECP a, ECP b)
{
    ECP_neg(&b);
    ECP_add(&a, &b);
    return a;
}

ECP hashToG1(const std::string& label)
{
    return g1Mul(g1Generator(), scalarHash("hash-to-g1|" + label));
}

ECP2 hashToG2(const std::string& label)
{
    return g2Mul(g2Generator(), scalarHash("hash-to-g2|" + label));
}

std::string g1Hex(ECP point)
{
    std::array<char, kInternalG1Bytes> bytes {};
    octet o {0, static_cast<int>(bytes.size()), bytes.data()};
    ECP_toOctet(&o, &point, true);
    return bytesToHex(o.val, o.len);
}

std::string g2Hex(ECP2 point)
{
    std::array<char, kInternalG2BufferBytes> bytes {};
    octet o {0, static_cast<int>(bytes.size()), bytes.data()};
    ECP2_toOctet(&o, &point, true);
    return bytesToHex(o.val, o.len);
}

std::string gtHex(FP12 value)
{
    std::array<char, kInternalGtBytes> bytes {};
    octet o {0, static_cast<int>(bytes.size()), bytes.data()};
    FP12_toOctet(&o, &value);
    return bytesToHex(o.val, o.len);
}

std::string joinParts(const std::vector<std::string>& parts)
{
    std::ostringstream os;
    for (const auto& part : parts) {
        os << part.size() << ":" << part << "|";
    }
    return os.str();
}

Scalar challenge(const std::vector<std::string>& parts)
{
    return scalarHash(joinParts(parts));
}

FP12 pairing(ECP g1, ECP2 g2)
{
    FP12 out;
    PAIR_ate(&out, &g2, &g1);
    PAIR_fexp(&out);
    return out;
}

FP12 gtPow(FP12 base, const Scalar& exponent)
{
    FP12 out;
    BIG e;
    BIG_copy(e, const_cast<chunk*>(exponent.v));
    FP12_pow(&out, &base, e);
    return out;
}

FP12 gtMul(FP12 a, FP12 b)
{
    FP12_mul(&a, &b);
    return a;
}

bool gtEquals(FP12 a, FP12 b)
{
    return FP12_equals(&a, &b) != 0;
}

std::string csvEscape(const std::string& text)
{
    bool quote = false;
    std::string out;
    for (char c : text) {
        if (c == '"' || c == ',' || c == '\n' || c == '\r') {
            quote = true;
        }
        if (c == '"') {
            out += "\"\"";
        } else {
            out += c;
        }
    }
    return quote ? "\"" + out + "\"" : out;
}

struct SigBase {
    virtual ~SigBase() = default;
};

struct Scheme {
    virtual ~Scheme() = default;
    virtual std::string name() const = 0;
    virtual int signatureBytes() const = 0;
    virtual int totalOnlineBytes() const = 0;
    virtual std::string notes() const = 0;
    virtual std::string symbolicSign() const = 0;
    virtual std::string symbolicVerify() const = 0;
    virtual std::string communicationFormula() const = 0;
    virtual std::unique_ptr<SigBase> sign(const std::string& message) = 0;
    virtual bool verify(const std::string& message, const SigBase& signature) = 0;
};

class BbsScheme final : public Scheme {
    struct Sig final : public SigBase {
        ECP t1, t2, t3;
        Scalar c, sAlpha, sBeta, sX, sDelta1, sDelta2;
    };

    ECP g1_, h_, u_, v_, a_;
    ECP2 g2_, w_;
    Scalar gamma_, x_;
    FP12 eH_W_, eH_G2_, eG1_G2_, eA_G2_;

public:
    BbsScheme()
    {
        g1_ = hashToG1("bbs-g1");
        g2_ = hashToG2("bbs-g2");
        h_ = hashToG1("bbs-h");
        u_ = hashToG1("bbs-u");
        v_ = hashToG1("bbs-v");
        gamma_ = scalarHash("bbs-gamma");
        x_ = scalarHash("bbs-member-x");
        w_ = g2Mul(g2_, gamma_);
        a_ = g1Mul(g1_, inv(add(gamma_, x_)));
        eH_W_ = pairing(h_, w_);
        eH_G2_ = pairing(h_, g2_);
        eG1_G2_ = pairing(g1_, g2_);
        eA_G2_ = pairing(a_, g2_);
    }

    std::string name() const override { return "BBS"; }
    int signatureBytes() const override { return 3 * kG1Bytes + 6 * kScalarBytes; }
    int totalOnlineBytes() const override { return signatureBytes(); }
    std::string notes() const override { return "BBS short group signature; setup/join/open excluded; fixed public pairings are precomputed outside online timing"; }
    std::string symbolicSign() const override { return "9T_PM1 + 3T_EXP"; }
    std::string symbolicVerify() const override { return "T_BP + 8T_PM1 + 2T_PM2 + 3T_EXP"; }
    std::string communicationFormula() const override { return "3|G1| + 6|Zq| + |M|"; }

    std::unique_ptr<SigBase> sign(const std::string& message) override
    {
        auto sig = std::make_unique<Sig>();
        Scalar alpha = randomScalar("bbs-alpha");
        Scalar beta = randomScalar("bbs-beta");
        Scalar rAlpha = randomScalar("bbs-r-alpha");
        Scalar rBeta = randomScalar("bbs-r-beta");
        Scalar rX = randomScalar("bbs-r-x");
        Scalar rDelta1 = randomScalar("bbs-r-delta1");
        Scalar rDelta2 = randomScalar("bbs-r-delta2");
        Scalar delta1 = mul(alpha, x_);
        Scalar delta2 = mul(beta, x_);

        sig->t1 = g1Mul(u_, alpha);
        sig->t2 = g1Mul(v_, beta);
        sig->t3 = g1Add(a_, g1Mul(h_, add(alpha, beta)));

        FP12 r3 = gtPow(eA_G2_, rX);
        Scalar hG2Exp = sub(mul(add(alpha, beta), rX), add(rDelta1, rDelta2));
        r3 = gtMul(r3, gtPow(eH_G2_, hG2Exp));
        r3 = gtMul(r3, gtPow(eH_W_, neg(add(rAlpha, rBeta))));

        ECP r1 = g1Mul(u_, rAlpha);
        ECP r2 = g1Mul(v_, rBeta);
        ECP r4 = g1Sub(g1Mul(sig->t1, rX), g1Mul(u_, rDelta1));
        ECP r5 = g1Sub(g1Mul(sig->t2, rX), g1Mul(v_, rDelta2));

        sig->c = challenge({"BBS", message, g1Hex(sig->t1), g1Hex(sig->t2), g1Hex(sig->t3), g1Hex(r1), g1Hex(r2), gtHex(r3), g1Hex(r4), g1Hex(r5)});
        sig->sAlpha = add(rAlpha, mul(sig->c, alpha));
        sig->sBeta = add(rBeta, mul(sig->c, beta));
        sig->sX = add(rX, mul(sig->c, x_));
        sig->sDelta1 = add(rDelta1, mul(sig->c, delta1));
        sig->sDelta2 = add(rDelta2, mul(sig->c, delta2));
        return sig;
    }

    bool verify(const std::string& message, const SigBase& signature) override
    {
        const auto& sig = dynamic_cast<const Sig&>(signature);
        ECP r1 = g1Sub(g1Mul(u_, sig.sAlpha), g1Mul(sig.t1, sig.c));
        ECP r2 = g1Sub(g1Mul(v_, sig.sBeta), g1Mul(sig.t2, sig.c));
        ECP r4 = g1Sub(g1Mul(sig.t1, sig.sX), g1Mul(u_, sig.sDelta1));
        ECP r5 = g1Sub(g1Mul(sig.t2, sig.sX), g1Mul(v_, sig.sDelta2));

        ECP2 pairBase = g2Add(g2Mul(g2_, sig.sX), g2Mul(w_, sig.c));
        FP12 r3 = pairing(sig.t3, pairBase);
        r3 = gtMul(r3, gtPow(eH_W_, neg(add(sig.sAlpha, sig.sBeta))));
        r3 = gtMul(r3, gtPow(eH_G2_, neg(add(sig.sDelta1, sig.sDelta2))));
        r3 = gtMul(r3, gtPow(eG1_G2_, neg(sig.c)));

        Scalar cPrime = challenge({"BBS", message, g1Hex(sig.t1), g1Hex(sig.t2), g1Hex(sig.t3), g1Hex(r1), g1Hex(r2), gtHex(r3), g1Hex(r4), g1Hex(r5)});
        return scalarEquals(sig.c, cPrime);
    }
};

class ErcaScheme final : public Scheme {
    struct Sig final : public SigBase {
        ECP t1, t2, t3;
        ECP2 a1, a2;
        Scalar c, su, ss;
    };

    ECP p1_, pPub_;
    ECP2 p2_, acc0_, accJ_, ci_, wi_;
    Scalar x_, si_;

public:
    ErcaScheme()
    {
        p1_ = hashToG1("erca-p1");
        p2_ = hashToG2("erca-p2");
        x_ = scalarHash("erca-manager-x");
        si_ = scalarHash("erca-member-si");
        Scalar r = scalarHash("erca-accumulator-r");
        pPub_ = g1Mul(p1_, x_);
        acc0_ = g2Mul(p2_, r);
        wi_ = acc0_;
        ci_ = g2Mul(p2_, inv(add(si_, x_)));
        accJ_ = g2Mul(wi_, add(si_, x_));
    }

    std::string name() const override { return "ERCA"; }
    int signatureBytes() const override { return 3 * kG1Bytes + 2 * kG2Bytes + 3 * kScalarBytes; }
    int totalOnlineBytes() const override { return signatureBytes() + kIiotWiBytes + kIiotAccBytes; }
    std::string notes() const override { return "Efficient Revocable Cross-Domain Anonymous Authentication Scheme for IIoT; online payload includes Wi and ACCj"; }
    std::string symbolicSign() const override { return "2T_BP + 8T_PM1 + 2T_PM2 + 2T_EXP"; }
    std::string symbolicVerify() const override { return "3T_BP + 7T_PM1 + 3T_EXP"; }
    std::string communicationFormula() const override { return "sigma=3|G1|+2|G2|+3|Zq|; total=sigma+|Wi|+|ACCj|+|M|"; }

    std::unique_ptr<SigBase> sign(const std::string& message) override
    {
        auto sig = std::make_unique<Sig>();
        Scalar u = randomScalar("erca-u");
        Scalar ru = randomScalar("erca-ru");
        Scalar rs = randomScalar("erca-rs");
        sig->t1 = g1Mul(p1_, u);
        sig->t2 = g1Mul(sig->t1, u);
        sig->t3 = g1Add(g1Mul(p1_, si_), g1Mul(pPub_, u));
        sig->a1 = g2Mul(wi_, u);
        sig->a2 = g2Mul(ci_, u);

        ECP r1 = g1Mul(p1_, ru);
        ECP r2 = g1Mul(sig->t1, ru);
        ECP r4 = g1Add(g1Mul(p1_, rs), g1Mul(pPub_, ru));
        ECP2 aSum = g2Add(sig->a1, sig->a2);
        FP12 r3 = gtPow(pairing(pPub_, aSum), ru);
        r3 = gtMul(r3, gtPow(pairing(sig->t1, aSum), rs));
        sig->c = challenge({"ERCA", message, g1Hex(sig->t1), g1Hex(sig->t2), g1Hex(sig->t3), g2Hex(sig->a1), g2Hex(sig->a2), g1Hex(r1), g1Hex(r2), gtHex(r3), g1Hex(r4)});
        sig->su = add(ru, mul(sig->c, u));
        sig->ss = add(rs, mul(sig->c, si_));
        return sig;
    }

    bool verify(const std::string& message, const SigBase& signature) override
    {
        const auto& sig = dynamic_cast<const Sig&>(signature);
        ECP r1 = g1Sub(g1Mul(p1_, sig.su), g1Mul(sig.t1, sig.c));
        ECP r2 = g1Sub(g1Mul(sig.t1, sig.su), g1Mul(sig.t2, sig.c));
        ECP r4 = g1Sub(g1Add(g1Mul(p1_, sig.ss), g1Mul(pPub_, sig.su)), g1Mul(sig.t3, sig.c));
        ECP2 aSum = g2Add(sig.a1, sig.a2);
        FP12 r3 = gtPow(pairing(pPub_, aSum), sig.su);
        r3 = gtMul(r3, gtPow(pairing(sig.t1, aSum), sig.ss));
        r3 = gtMul(r3, gtPow(pairing(sig.t2, g2Add(accJ_, p2_)), neg(sig.c)));
        Scalar cPrime = challenge({"ERCA", message, g1Hex(sig.t1), g1Hex(sig.t2), g1Hex(sig.t3), g2Hex(sig.a1), g2Hex(sig.a2), g1Hex(r1), g1Hex(r2), gtHex(r3), g1Hex(r4)});
        return scalarEquals(sig.c, cPrime);
    }
};

class MlgsScheme final : public Scheme {
    struct Sig final : public SigBase {
        ECP t1, t2, t3, token;
        Scalar c, sAlpha, sBeta, sX, sDelta1, sDelta2;
    };

    ECP g1Base_, u_, v_, h_, rState_, abi_;
    ECP2 g2Base_, w_;
    Scalar gamma_, x_;

public:
    MlgsScheme()
    {
        g1Base_ = hashToG1("mlgs-g1");
        g2Base_ = hashToG2("mlgs-g2");
        u_ = hashToG1("mlgs-u");
        v_ = hashToG1("mlgs-v");
        h_ = hashToG1("mlgs-h");
        rState_ = hashToG1("mlgs-H3|stbi-default");
        gamma_ = scalarHash("mlgs-gamma");
        x_ = scalarHash("mlgs-member-x");
        w_ = g2Mul(g2Base_, gamma_);
        abi_ = g1Mul(g1Add(g1Base_, rState_), inv(add(gamma_, x_)));
    }

    std::string name() const override { return "MLGS"; }
    int signatureBytes() const override { return 4 * kG1Bytes + 6 * kScalarBytes; }
    int totalOnlineBytes() const override { return signatureBytes() + 2 * kSmallIntBytes; }
    std::string notes() const override { return "Message linkable group signature with information binding and efficient revocation for VANETs"; }
    std::string symbolicSign() const override { return "2T_BP + 12T_PM1 + 2T_PM2 + T_HG1"; }
    std::string symbolicVerify() const override { return "2T_BP + 15T_PM1 + 2T_HG1"; }
    std::string communicationFormula() const override { return "sigma=4|G1|+6|Zq|; online=sigma+|Tbi|+|rbi|+|M|"; }

    std::unique_ptr<SigBase> sign(const std::string& message) override
    {
        auto sig = std::make_unique<Sig>();
        Scalar alpha = randomScalar("mlgs-alpha");
        Scalar beta = randomScalar("mlgs-beta");
        Scalar rAlpha = randomScalar("mlgs-r-alpha");
        Scalar rBeta = randomScalar("mlgs-r-beta");
        Scalar rX = randomScalar("mlgs-r-x");
        Scalar rDelta1 = randomScalar("mlgs-r-delta1");
        Scalar rDelta2 = randomScalar("mlgs-r-delta2");
        Scalar delta1 = mul(alpha, x_);
        Scalar delta2 = mul(beta, x_);

        sig->t1 = g1Mul(u_, alpha);
        sig->t2 = g1Mul(v_, beta);
        ECP h2Message = hashToG1("mlgs-H2|" + message);
        sig->t3 = g1Add(abi_, g1Mul(h_, add(alpha, beta)));
        sig->token = g1Mul(h2Message, x_);

        FP12 r3 = pairing(g1Mul(sig->t3, rX), g2Base_);
        ECP2 secondPairBase = g2Add(g2Mul(w_, neg(add(rAlpha, rBeta))), g2Mul(g2Base_, neg(add(rDelta1, rDelta2))));
        r3 = gtMul(r3, pairing(h_, secondPairBase));
        ECP r1 = g1Mul(u_, rAlpha);
        ECP r2 = g1Mul(v_, rBeta);
        ECP r4 = g1Sub(g1Mul(sig->t1, rX), g1Mul(u_, rDelta1));
        ECP r5 = g1Sub(g1Mul(sig->t2, rX), g1Mul(v_, rDelta2));
        ECP r6 = g1Mul(h2Message, rX);

        sig->c = challenge({"MLGS", message, g1Hex(sig->t1), g1Hex(sig->t2), g1Hex(sig->t3), g1Hex(r1), g1Hex(r2), gtHex(r3), g1Hex(r4), g1Hex(r5), g1Hex(r6), g1Hex(sig->token)});
        sig->sAlpha = add(rAlpha, mul(sig->c, alpha));
        sig->sBeta = add(rBeta, mul(sig->c, beta));
        sig->sX = add(rX, mul(sig->c, x_));
        sig->sDelta1 = add(rDelta1, mul(sig->c, delta1));
        sig->sDelta2 = add(rDelta2, mul(sig->c, delta2));
        return sig;
    }

    bool verify(const std::string& message, const SigBase& signature) override
    {
        const auto& sig = dynamic_cast<const Sig&>(signature);
        ECP h2Message = hashToG1("mlgs-H2|" + message);
        ECP statePoint = hashToG1("mlgs-H3|stbi-default");
        ECP gbar1 = g1Add(g1Base_, statePoint);
        ECP r1 = g1Sub(g1Mul(u_, sig.sAlpha), g1Mul(sig.t1, sig.c));
        ECP r2 = g1Sub(g1Mul(v_, sig.sBeta), g1Mul(sig.t2, sig.c));
        ECP r4 = g1Sub(g1Mul(sig.t1, sig.sX), g1Mul(u_, sig.sDelta1));
        ECP r5 = g1Sub(g1Mul(sig.t2, sig.sX), g1Mul(v_, sig.sDelta2));
        ECP r6 = g1Sub(g1Mul(h2Message, sig.sX), g1Mul(sig.token, sig.c));

        ECP first = g1Sub(g1Sub(g1Mul(sig.t3, sig.sX), g1Mul(h_, add(sig.sDelta1, sig.sDelta2))), g1Mul(gbar1, sig.c));
        ECP second = g1Sub(g1Mul(sig.t3, sig.c), g1Mul(h_, add(sig.sAlpha, sig.sBeta)));
        FP12 r3 = gtMul(pairing(first, g2Base_), pairing(second, w_));
        Scalar cPrime = challenge({"MLGS", message, g1Hex(sig.t1), g1Hex(sig.t2), g1Hex(sig.t3), g1Hex(r1), g1Hex(r2), gtHex(r3), g1Hex(r4), g1Hex(r5), g1Hex(r6), g1Hex(sig.token)});
        return scalarEquals(sig.c, cPrime);
    }
};

class ClgsScheme final : public Scheme {
    struct Sig final : public SigBase {
        ECP d1, d2, d3;
        Scalar c, sAlpha, sX, sGamma, sY;
    };

    ECP g_, g1_, g2_, u_, w_, d_, a_;
    ECP2 h1_, hTheta_;
    Scalar theta_, eta_, xi_, x_, y_, z_;
    FP12 eA_H1_, eW_HTheta_, eW_H1_, eG2_H1_, eG1_H1_;

public:
    ClgsScheme()
    {
        g_ = hashToG1("clgs-g");
        g1_ = hashToG1("clgs-g1");
        g2_ = hashToG1("clgs-g2-in-G1");
        u_ = hashToG1("clgs-u");
        h1_ = hashToG2("clgs-h1");
        theta_ = scalarHash("clgs-theta");
        eta_ = scalarHash("clgs-eta");
        xi_ = scalarHash("clgs-xi");
        w_ = g1Mul(u_, eta_);
        d_ = g1Mul(u_, xi_);
        hTheta_ = g2Mul(h1_, theta_);
        x_ = scalarHash("clgs-member-x");
        y_ = scalarHash("clgs-member-y");
        z_ = scalarHash("clgs-member-z");
        ECP numerator = g1Sub(g1Sub(g1_, g1Mul(g2_, y_)), g1Mul(w_, z_));
        a_ = g1Mul(numerator, inv(add(theta_, x_)));
        eA_H1_ = pairing(a_, h1_);
        eW_HTheta_ = pairing(w_, hTheta_);
        eW_H1_ = pairing(w_, h1_);
        eG2_H1_ = pairing(g2_, h1_);
        eG1_H1_ = pairing(g1_, h1_);
    }

    std::string name() const override { return "CLGS"; }
    int signatureBytes() const override { return kSmallIntBytes + 3 * kG1Bytes + 5 * kScalarBytes; }
    int totalOnlineBytes() const override { return signatureBytes(); }
    std::string notes() const override { return "Short dynamic group signature supporting controllable linkability; named CLGS following the user's table"; }
    std::string symbolicSign() const override { return "7T_PM1 + 4T_EXP"; }
    std::string symbolicVerify() const override { return "T_BP + 5T_PM1 + 2T_PM2 + 4T_EXP"; }
    std::string communicationFormula() const override { return "|lambda| + 3|G1| + 5|Zq| + |M|"; }

    std::unique_ptr<SigBase> sign(const std::string& message) override
    {
        auto sig = std::make_unique<Sig>();
        Scalar alpha = randomScalar("clgs-alpha");
        Scalar gamma = sub(mul(x_, alpha), z_);
        Scalar rAlpha = randomScalar("clgs-r-alpha");
        Scalar rX = randomScalar("clgs-r-x");
        Scalar rGamma = randomScalar("clgs-r-gamma");
        Scalar rY = randomScalar("clgs-r-y");

        sig->d1 = g1Mul(u_, alpha);
        sig->d2 = g1Add(a_, g1Mul(w_, alpha));
        sig->d3 = g1Add(g1Mul(g_, y_), g1Mul(d_, alpha));
        ECP r1 = g1Mul(u_, rAlpha);
        FP12 r2 = gtPow(eA_H1_, rX);
        r2 = gtMul(r2, gtPow(eW_H1_, sub(mul(alpha, rX), rGamma)));
        r2 = gtMul(r2, gtPow(eW_HTheta_, neg(rAlpha)));
        r2 = gtMul(r2, gtPow(eG2_H1_, rY));
        ECP r3 = g1Add(g1Mul(g_, rY), g1Mul(d_, rAlpha));

        sig->c = challenge({"CLGS", message, g1Hex(sig->d1), g1Hex(sig->d2), g1Hex(sig->d3), g1Hex(r1), gtHex(r2), g1Hex(r3)});
        sig->sAlpha = add(rAlpha, mul(sig->c, alpha));
        sig->sX = add(rX, mul(sig->c, x_));
        sig->sGamma = add(rGamma, mul(sig->c, gamma));
        sig->sY = add(rY, mul(sig->c, y_));
        return sig;
    }

    bool verify(const std::string& message, const SigBase& signature) override
    {
        const auto& sig = dynamic_cast<const Sig&>(signature);
        ECP r1 = g1Sub(g1Mul(u_, sig.sAlpha), g1Mul(sig.d1, sig.c));
        ECP2 pairBase = g2Add(g2Mul(h1_, sig.sX), g2Mul(hTheta_, sig.c));
        FP12 r2 = pairing(sig.d2, pairBase);
        r2 = gtMul(r2, gtPow(eW_HTheta_, neg(sig.sAlpha)));
        r2 = gtMul(r2, gtPow(eW_H1_, neg(sig.sGamma)));
        r2 = gtMul(r2, gtPow(eG2_H1_, sig.sY));
        r2 = gtMul(r2, gtPow(eG1_H1_, neg(sig.c)));
        ECP r3 = g1Sub(g1Add(g1Mul(g_, sig.sY), g1Mul(d_, sig.sAlpha)), g1Mul(sig.d3, sig.c));
        Scalar cPrime = challenge({"CLGS", message, g1Hex(sig.d1), g1Hex(sig.d2), g1Hex(sig.d3), g1Hex(r1), gtHex(r2), g1Hex(r3)});
        return scalarEquals(sig.c, cPrime);
    }
};

class ADVLGSScheme {
private:
    struct MemberCredential {
        Scalar y_i;
        Scalar x_i_Delta_t;
        Scalar u_i_Delta_t;
        ECP Y_i;
        ECP A_i_Delta_t;
    };

public:
    struct Signature {
        ECP barA;
        ECP hatA;
        ECP B;
        ECP C;
        ECP L;
        Scalar c;
        Scalar z_r;
        Scalar z_y;
        Scalar z_t_u;
        Scalar z_x;
    };

    explicit ADVLGSScheme(int precomputedVehicleCount = kPrecomputedVehicleCount)
    {
        P1_ = hashToG1("advlgs-P1");
        P2_ = hashToG1("advlgs-P2");
        P3_ = hashToG1("advlgs-P3");
        P4_ = hashToG1("advlgs-P4");
        pk_RA_ = g1Mul(P1_, scalarHash("advlgs-sk_RA"));
        R_Delta_t_ = computeR_Delta_t();
        P1_plus_R_Delta_t_ = g1Add(P1_, R_Delta_t_);
        D_j_t_ = computeD_j_t();
        Q_ = hashToG2("advlgs-Q");
        s_ = scalarHash("advlgs-issuer-s");
        S_ = g2Mul(Q_, s_);
        O_ = g1Mul(P2_, scalarHash("advlgs-opener-o"));
        ell_j_ = scalarHash("advlgs-ell_j");
        V_j_ = g1Mul(P2_, ell_j_);

        const int precomputeCount = precomputedVehicleCount > 0 ? precomputedVehicleCount : kPrecomputedVehicleCount;
        memberCredentials_.reserve(static_cast<std::size_t>(precomputeCount));
        for (int i = 0; i < precomputeCount; ++i) {
            memberCredentials_.push_back(issueMemberCredentialUnchecked(i));
        }
    }

    Signature sign(const std::string& message) const
    {
        return sign(message, 0);
    }

    Signature sign(const std::string& message, int vehicleIndex) const
    {
        Signature sig {};
        const MemberCredential& member = memberCredential(vehicleIndex);
        Scalar r = randomScalar("advlgs-r");
        Scalar t_u = mul(r, member.u_i_Delta_t);
        Scalar alpha_r = randomScalar("advlgs-alpha_r");
        Scalar alpha_y = randomScalar("advlgs-alpha_y");
        Scalar alpha_t_u = randomScalar("advlgs-alpha_t_u");
        Scalar alpha_x = randomScalar("advlgs-alpha_x");
        ECP D_j_t = computeD_j_t();

        sig.barA = g1Mul(member.A_i_Delta_t, r);
        sig.B = g1Mul(P2_, r);
        sig.C = g1Add(member.Y_i, g1Mul(O_, r));
        sig.L = g1Add(g1Mul(D_j_t, member.y_i), g1Mul(V_j_, r));
        sig.hatA = g1Add(g1Mul(P1_plus_R_Delta_t_, r), g1Mul(sig.B, member.y_i));
        sig.hatA = g1Add(sig.hatA, g1Mul(P3_, t_u));
        sig.hatA = g1Sub(sig.hatA, g1Mul(sig.barA, member.x_i_Delta_t));

        ECP RB = g1Mul(P2_, alpha_r);
        ECP RC = g1Add(g1Mul(P2_, alpha_y), g1Mul(O_, alpha_r));
        ECP RL = g1Add(g1Mul(D_j_t, alpha_y), g1Mul(V_j_, alpha_r));
        ECP RA = g1Add(g1Mul(P1_plus_R_Delta_t_, alpha_r), g1Mul(sig.B, alpha_y));
        RA = g1Add(RA, g1Mul(P3_, alpha_t_u));
        RA = g1Sub(RA, g1Mul(sig.barA, alpha_x));
        sig.c = challenge({"ADVLGS", message, g1Hex(pk_RA_), spLabel(), tsLabel(), g1Hex(sig.barA), g1Hex(sig.hatA), g1Hex(sig.B), g1Hex(sig.C), g1Hex(sig.L), g1Hex(RB), g1Hex(RC), g1Hex(RL), g1Hex(RA)});
        sig.z_r = add(alpha_r, mul(sig.c, r));
        sig.z_y = add(alpha_y, mul(sig.c, member.y_i));
        sig.z_t_u = add(alpha_t_u, mul(sig.c, t_u));
        sig.z_x = add(alpha_x, mul(sig.c, member.x_i_Delta_t));
        return sig;
    }

    bool verifySingle(const std::string& message, const Signature& sig) const
    {
        if (!gtEquals(pairing(sig.barA, S_), pairing(sig.hatA, Q_))) {
            return false;
        }
        return verifyProofOnly(message, sig);
    }

    bool batchVerify(const std::vector<std::string>& messages, const std::vector<Signature>& signatures) const
    {
        if (messages.empty() || messages.size() != signatures.size()) {
            return false;
        }

        ECP sumBarA {};
        ECP sumHatA {};
        bool hasFirst = false;

        for (std::size_t i = 0; i < signatures.size(); ++i) {
            const Signature& sig = signatures[i];
            if (!verifyProofOnly(messages[i], sig)) {
                return false;
            }

            Scalar rho = scalarHash("ADVLGS-batch-rho|" + std::to_string(i) + "|" + messages[i] + "|" + g1Hex(sig.barA) + "|" + g1Hex(sig.hatA));
            ECP weightedBarA = g1Mul(sig.barA, rho);
            ECP weightedHatA = g1Mul(sig.hatA, rho);
            if (!hasFirst) {
                sumBarA = weightedBarA;
                sumHatA = weightedHatA;
                hasFirst = true;
            } else {
                sumBarA = g1Add(sumBarA, weightedBarA);
                sumHatA = g1Add(sumHatA, weightedHatA);
            }
        }

        return gtEquals(pairing(sumBarA, S_), pairing(sumHatA, Q_));
    }

    int signatureBytes() const
    {
        return 5 * kG1Bytes + 5 * kScalarBytes;
    }

private:
    ECP P1_;
    ECP P2_;
    ECP P3_;
    ECP P4_;
    ECP pk_RA_;
    ECP O_;
    ECP V_j_;
    ECP R_Delta_t_;
    ECP P1_plus_R_Delta_t_;
    ECP D_j_t_;
    ECP2 Q_;
    ECP2 S_;
    Scalar s_;
    Scalar ell_j_;
    std::vector<MemberCredential> memberCredentials_;

    std::string deltaLabel() const
    {
        return "Delta_t-default";
    }

    std::string tsLabel() const
    {
        return "TS-default";
    }

    std::string spLabel() const
    {
        return "VID_j-default|V_j-default|Scope_j-default|Delta_t-default|Cert_j-default";
    }

    std::string domLabel() const
    {
        return "VID_j-default|V_j-default|Scope_j-default|Delta_t-default";
    }

    ECP computeR_Delta_t() const
    {
        return g1Mul(P4_, scalarHash("advlgs-H3|" + deltaLabel()));
    }

    ECP computeD_j_t() const
    {
        return hashToG1("advlgs-H2|" + domLabel());
    }

    const MemberCredential& memberCredential(int vehicleIndex) const
    {
        if (memberCredentials_.empty()) {
            throw std::runtime_error("A-DVLGS member credentials were not precomputed");
        }
        const std::size_t index = static_cast<std::size_t>(vehicleIndex >= 0 ? vehicleIndex : 0) % memberCredentials_.size();
        return memberCredentials_[index];
    }

    MemberCredential issueMemberCredentialUnchecked(int vehicleIndex) const
    {
        const std::string label = "vehicle-" + std::to_string(vehicleIndex);
        MemberCredential member {};
        member.y_i = scalarHash("advlgs-y_i|" + label);
        member.x_i_Delta_t = scalarHash("advlgs-x_i_Delta_t|" + label);
        member.u_i_Delta_t = scalarHash("advlgs-u_i_Delta_t|" + label);
        member.Y_i = g1Mul(P2_, member.y_i);
        ECP W_i_Delta_t = g1Mul(P3_, member.u_i_Delta_t);
        ECP N_i_Delta_t = g1Add(g1Add(g1Add(P1_, member.Y_i), R_Delta_t_), W_i_Delta_t);
        member.A_i_Delta_t = g1Mul(N_i_Delta_t, inv(add(s_, member.x_i_Delta_t)));
        return member;
    }

    bool verifyProofOnly(const std::string& message, const Signature& sig) const
    {
        ECP RB = g1Sub(g1Mul(P2_, sig.z_r), g1Mul(sig.B, sig.c));
        ECP RC = g1Sub(g1Add(g1Mul(P2_, sig.z_y), g1Mul(O_, sig.z_r)), g1Mul(sig.C, sig.c));
        ECP RL = g1Sub(g1Add(g1Mul(D_j_t_, sig.z_y), g1Mul(V_j_, sig.z_r)), g1Mul(sig.L, sig.c));
        ECP RA = g1Add(g1Mul(P1_plus_R_Delta_t_, sig.z_r), g1Mul(sig.B, sig.z_y));
        RA = g1Add(RA, g1Mul(P3_, sig.z_t_u));
        RA = g1Sub(RA, g1Mul(sig.barA, sig.z_x));
        RA = g1Sub(RA, g1Mul(sig.hatA, sig.c));
        Scalar cPrime = challenge({"ADVLGS", message, g1Hex(pk_RA_), spLabel(), tsLabel(), g1Hex(sig.barA), g1Hex(sig.hatA), g1Hex(sig.B), g1Hex(sig.C), g1Hex(sig.L), g1Hex(RB), g1Hex(RC), g1Hex(RL), g1Hex(RA)});
        return scalarEquals(sig.c, cPrime);
    }
};

class ADVLGSSingleScheme final : public Scheme {
    struct Sig final : public SigBase {
        ADVLGSScheme::Signature signature;
    };

    ADVLGSScheme scheme_;
    int nextVehicleIndex_ = 0;

public:
    explicit ADVLGSSingleScheme(int precomputedVehicleCount = kPrecomputedVehicleCount)
        : scheme_(precomputedVehicleCount)
    {
    }

    std::string name() const override { return "A-DVLGS"; }
    int signatureBytes() const override { return scheme_.signatureBytes(); }
    int totalOnlineBytes() const override { return signatureBytes() + kStateBytes + kTimestampBytes; }
    std::string notes() const override { return "A-DVLGS implementation; member credentials are issued before signing; each benchmark signature uses an existing member key/certificate"; }
    std::string symbolicSign() const override { return "18T_PM1 + T_H2G"; }
    std::string symbolicVerify() const override { return "2T_BP + 13T_PM1"; }
    std::string communicationFormula() const override { return "sigma=5|G1|+5|Zq|; online=sigma+|Delta_t|+|TS|+|M|"; }

    std::unique_ptr<SigBase> sign(const std::string& message) override
    {
        auto sig = std::make_unique<Sig>();
        sig->signature = scheme_.sign(message, nextVehicleIndex_++);
        return sig;
    }

    bool verify(const std::string& message, const SigBase& signature) override
    {
        const auto& sig = dynamic_cast<const Sig&>(signature);
        return scheme_.verifySingle(message, sig.signature);
    }
};

struct TimingStats {
    double averageMs;
    double minMs;
    double maxMs;
    double stddevMs;
};

TimingStats summarize(const std::vector<double>& values)
{
    if (values.empty()) {
        throw std::runtime_error("no timing samples");
    }
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    const double average = sum / static_cast<double>(values.size());
    double minValue = std::numeric_limits<double>::max();
    double maxValue = 0.0;
    double squareSum = 0.0;
    for (double value : values) {
        minValue = value < minValue ? value : minValue;
        maxValue = value > maxValue ? value : maxValue;
        const double diff = value - average;
        squareSum += diff * diff;
    }
    return {average, minValue, maxValue, std::sqrt(squareSum / static_cast<double>(values.size()))};
}

std::string makeMessage(int batchSize, int index)
{
    std::ostringstream os;
    os << "ADVLGS|message-body|batch=" << batchSize << "|index=" << index << "|";
    std::string message = os.str();
    if (message.size() < static_cast<std::size_t>(kMessageBytes)) {
        message.append(static_cast<std::size_t>(kMessageBytes) - message.size(), 'M');
    } else if (message.size() > static_cast<std::size_t>(kMessageBytes)) {
        message.resize(static_cast<std::size_t>(kMessageBytes));
    }
    return message;
}

int parsePositiveIntArgument(int argc, char** argv, int index, int fallback)
{
    if (argc <= index) {
        return fallback;
    }
    const int parsed = std::atoi(argv[index]);
    return parsed > 0 ? parsed : fallback;
}

int parseRounds(int argc, char** argv)
{
    return parsePositiveIntArgument(argc, argv, 1, kDefaultRounds);
}

int parseMaxBatchSize(int argc, char** argv)
{
    return parsePositiveIntArgument(argc, argv, 2, kDefaultMaxBatchSize);
}

std::vector<int> makeBatchSizes(int maxBatchSize)
{
    std::vector<int> batchSizes;
    batchSizes.reserve(static_cast<std::size_t>(maxBatchSize));
    for (int batchSize = 1; batchSize <= maxBatchSize; ++batchSize) {
        batchSizes.push_back(batchSize);
    }
    return batchSizes;
}

std::vector<int> makeDefaultLargeBatchSizes()
{
    return {1000, 2000, 3000, 4000, 5000};
}

std::vector<int> parseBatchSizeList(const std::string& text)
{
    std::vector<int> batchSizes;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const int value = std::atoi(item.c_str());
        if (value > 0) {
            batchSizes.push_back(value);
        }
    }
    if (batchSizes.empty()) {
        throw std::runtime_error("batch size list is empty");
    }
    return batchSizes;
}

int maxBatchSizeIn(const std::vector<int>& batchSizes)
{
    int maxValue = 0;
    for (int batchSize : batchSizes) {
        if (batchSize > maxValue) {
            maxValue = batchSize;
        }
    }
    return maxValue;
}

std::string makeSchemeMessage(const std::string& schemeName, int index)
{
    std::ostringstream os;
    os << schemeName << "|message-body|index=" << index << "|";
    std::string message = os.str();
    if (message.size() < static_cast<std::size_t>(kMessageBytes)) {
        message.append(static_cast<std::size_t>(kMessageBytes) - message.size(), 'M');
    } else if (message.size() > static_cast<std::size_t>(kMessageBytes)) {
        message.resize(static_cast<std::size_t>(kMessageBytes));
    }
    return message;
}

bool selfTest(Scheme& scheme)
{
    auto sig = scheme.sign("self-test-message");
    if (!scheme.verify("self-test-message", *sig)) {
        std::cerr << "Self-test failed for " << scheme.name() << ": valid message rejected\n";
        return false;
    }
    if (scheme.verify("self-test-message|tampered", *sig)) {
        std::cerr << "Self-test failed for " << scheme.name() << ": tampered message accepted\n";
        return false;
    }
    return true;
}

void runFiveSchemeSingleBenchmark(int rounds)
{
    g_schemeTimingExports.clear();

    std::vector<std::unique_ptr<Scheme>> schemes;
    schemes.push_back(std::make_unique<BbsScheme>());
    schemes.push_back(std::make_unique<ClgsScheme>());
    schemes.push_back(std::make_unique<MlgsScheme>());
    schemes.push_back(std::make_unique<ErcaScheme>());
    schemes.push_back(std::make_unique<ADVLGSSingleScheme>(rounds));

    for (auto& scheme : schemes) {
        if (!selfTest(*scheme)) {
            throw std::runtime_error("self-test failed");
        }
    }

    std::ofstream summary("five_scheme_single_summary.csv");
    std::ofstream detail("five_scheme_single_rounds.csv");
    summary << "scheme,rounds,verifiedCount,avgSignMs,minSignMs,maxSignMs,stddevSignMs,avgVerifyMs,minVerifyMs,maxVerifyMs,stddevVerifyMs,messageBytes,signatureBytes,totalOnlineBytesWithoutMessage,totalPayloadBytesWithMessage,symbolicSign,symbolicVerify,communicationFormula,notes\n";
    detail << "scheme,roundIndex,verified,signMs,verifyMs\n";

    std::cout << "Five-scheme single-message benchmark, MIRACL Core BLS12-381, VS2022\n";
    std::cout << "Rounds per scheme: " << rounds << "\n";
    std::cout << "Message body m: " << kMessageBytes << " bytes\n\n";

    for (auto& scheme : schemes) {
        std::vector<std::string> messages;
        std::vector<std::unique_ptr<SigBase>> signatures;
        std::vector<double> signTimes;
        std::vector<double> verifyTimes;
        messages.reserve(static_cast<std::size_t>(rounds));
        signatures.reserve(static_cast<std::size_t>(rounds));
        signTimes.reserve(static_cast<std::size_t>(rounds));
        verifyTimes.reserve(static_cast<std::size_t>(rounds));

        for (int round = 0; round < rounds; ++round) {
            messages.push_back(makeSchemeMessage(scheme->name(), round));
            const auto start = std::chrono::steady_clock::now();
            auto sig = scheme->sign(messages.back());
            const auto end = std::chrono::steady_clock::now();
            signTimes.push_back(std::chrono::duration<double, std::milli>(end - start).count());
            signatures.push_back(std::move(sig));
        }

        int verifiedCount = 0;
        for (int round = 0; round < rounds; ++round) {
            const auto start = std::chrono::steady_clock::now();
            const bool ok = scheme->verify(messages[round], *signatures[round]);
            const auto end = std::chrono::steady_clock::now();
            verifyTimes.push_back(std::chrono::duration<double, std::milli>(end - start).count());
            if (ok) {
                ++verifiedCount;
            }
            detail << csvEscape(scheme->name()) << "," << (round + 1) << "," << (ok ? 1 : 0) << ","
                   << std::fixed << std::setprecision(6) << signTimes[round] << "," << verifyTimes[round] << "\n";
        }

        const TimingStats signStats = summarize(signTimes);
        const TimingStats verifyStats = summarize(verifyTimes);
        const std::string exportName = scheme->name() == "A-DVLGS" ? "ADVLGS" : scheme->name();
        g_schemeTimingExports.push_back({exportName, signStats.averageMs, verifyStats.averageMs});
        const int totalPayloadBytes = kMessageBytes + scheme->totalOnlineBytes();
        summary << csvEscape(scheme->name()) << "," << rounds << "," << verifiedCount << ","
                << std::fixed << std::setprecision(6)
                << signStats.averageMs << "," << signStats.minMs << "," << signStats.maxMs << "," << signStats.stddevMs << ","
                << verifyStats.averageMs << "," << verifyStats.minMs << "," << verifyStats.maxMs << "," << verifyStats.stddevMs << ","
                << kMessageBytes << "," << scheme->signatureBytes() << "," << scheme->totalOnlineBytes() << "," << totalPayloadBytes << ","
                << csvEscape(scheme->symbolicSign()) << ","
                << csvEscape(scheme->symbolicVerify()) << ","
                << csvEscape(scheme->communicationFormula()) << ","
                << csvEscape(scheme->notes()) << "\n";

        std::cout << std::setw(8) << scheme->name()
                  << ": " << verifiedCount << "/" << rounds
                  << " verified, avg sign " << std::fixed << std::setprecision(6) << signStats.averageMs
                  << " ms, avg verify " << verifyStats.averageMs
                  << " ms, payload " << totalPayloadBytes << " B\n";

        if (verifiedCount != rounds) {
            throw std::runtime_error("verification failure in " + scheme->name());
        }
    }

    std::cout << "\nWrote five_scheme_single_summary.csv and five_scheme_single_rounds.csv\n\n";
}

void runAdvlgsBatchBenchmark(int rounds, int maxBatchSize)
{
    g_advlgsBatchTimingExports.clear();

    const std::vector<int> batchSizes = makeBatchSizes(maxBatchSize);
    ADVLGSScheme scheme(maxBatchSize);

    std::ofstream summary("advlgs_batch_summary.csv");
    std::ofstream detail("advlgs_batch_rounds.csv");
    summary << "batchSize,distinctSigners,rounds,verifiedBatches,avgBatchVerifyMs,avgPerMessageMs,minBatchVerifyMs,maxBatchVerifyMs,stddevBatchVerifyMs,messageBytes,signatureBytes,pairingsIfSingleVerify,pairingsInBatchVerify,pairingsSaved\n";
    detail << "batchSize,roundIndex,verified,batchVerifyMs\n";

    std::cout << "A-DVLGS batch verification benchmark\n";
    std::cout << "Rounds per batch size: " << rounds << "\n";
    std::cout << "Batch sizes: 1.." << maxBatchSize << "\n";
    std::cout << "Batch size 1 is included as the single-message baseline; larger sizes model all queued messages in one LV batch window.\n\n";

    for (int batchSize : batchSizes) {
        std::vector<std::string> messages;
        std::vector<ADVLGSScheme::Signature> signatures;
        messages.reserve(static_cast<std::size_t>(batchSize));
        signatures.reserve(static_cast<std::size_t>(batchSize));
        for (int i = 0; i < batchSize; ++i) {
            messages.push_back(makeMessage(batchSize, i));
            signatures.push_back(scheme.sign(messages.back(), i));
        }

        if (!scheme.batchVerify(messages, signatures)) {
            throw std::runtime_error("initial batch verification failed");
        }

        for (int i = 0; i < 5; ++i) {
            (void)scheme.batchVerify(messages, signatures);
        }

        int verifiedBatches = 0;
        std::vector<double> timings;
        timings.reserve(static_cast<std::size_t>(rounds));
        for (int round = 0; round < rounds; ++round) {
            const auto start = std::chrono::steady_clock::now();
            const bool ok = scheme.batchVerify(messages, signatures);
            const auto end = std::chrono::steady_clock::now();
            const double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
            timings.push_back(elapsedMs);
            if (ok) {
                ++verifiedBatches;
            }
            detail << batchSize << "," << (round + 1) << "," << (ok ? 1 : 0) << "," << std::fixed << std::setprecision(6) << elapsedMs << "\n";
        }

        const TimingStats stats = summarize(timings);
        if (batchSize >= 2 && batchSize <= 14) {
            g_advlgsBatchTimingExports.push_back({batchSize, stats.averageMs});
        }
        const int pairingsIfSingleVerify = 2 * batchSize;
        const int pairingsInBatchVerify = 2;
        const int pairingsSaved = pairingsIfSingleVerify - pairingsInBatchVerify;
        summary << batchSize << "," << batchSize << "," << rounds << "," << verifiedBatches << ","
                << std::fixed << std::setprecision(6)
                << stats.averageMs << "," << (stats.averageMs / static_cast<double>(batchSize)) << ","
                << stats.minMs << "," << stats.maxMs << "," << stats.stddevMs << ","
                << kMessageBytes << "," << scheme.signatureBytes() << ","
                << pairingsIfSingleVerify << "," << pairingsInBatchVerify << "," << pairingsSaved << "\n";

        std::cout << "batch " << std::setw(2) << batchSize
                  << " (" << batchSize << " distinct signers)"
                  << ": " << verifiedBatches << "/" << rounds
                  << " verified, avg batch " << std::fixed << std::setprecision(6) << stats.averageMs
                  << " ms, avg per message " << (stats.averageMs / static_cast<double>(batchSize))
                  << " ms, pairings " << pairingsIfSingleVerify << " -> " << pairingsInBatchVerify << "\n";
    }

    std::cout << "\nWrote advlgs_batch_summary.csv and advlgs_batch_rounds.csv\n";
}

void runAdvlgsLargeBatchBenchmark(int rounds, const std::vector<int>& batchSizes)
{
    const int maxBatchSize = maxBatchSizeIn(batchSizes);
    std::cout << "A-DVLGS large-batch scalability benchmark\n";
    std::cout << "Rounds per large batch size: " << rounds << "\n";
    std::cout << "Batch sizes:";
    for (int batchSize : batchSizes) {
        std::cout << " " << batchSize;
    }
    std::cout << "\n";
    std::cout << "Precomputing " << maxBatchSize << " member credentials before timing.\n";

    ADVLGSScheme scheme(maxBatchSize);

    std::ofstream summary("advlgs_large_batch_summary.csv");
    std::ofstream detail("advlgs_large_batch_rounds.csv");
    summary << "batchSize,distinctSigners,rounds,verifiedBatches,avgBatchVerifyMs,avgPerMessageMs,minBatchVerifyMs,maxBatchVerifyMs,stddevBatchVerifyMs,messageBytes,signatureBytes,pairingsIfSingleVerify,pairingsInBatchVerify,pairingsSaved\n";
    detail << "batchSize,roundIndex,verified,batchVerifyMs\n";

    for (int batchSize : batchSizes) {
        std::cout << "\nPreparing batch size " << batchSize << " signatures before timing...\n";
        std::vector<std::string> messages;
        std::vector<ADVLGSScheme::Signature> signatures;
        messages.reserve(static_cast<std::size_t>(batchSize));
        signatures.reserve(static_cast<std::size_t>(batchSize));
        for (int i = 0; i < batchSize; ++i) {
            messages.push_back(makeMessage(batchSize, i));
            signatures.push_back(scheme.sign(messages.back(), i));
        }

        if (!scheme.batchVerify(messages, signatures)) {
            throw std::runtime_error("initial large-batch verification failed");
        }

        int verifiedBatches = 0;
        std::vector<double> timings;
        timings.reserve(static_cast<std::size_t>(rounds));
        std::cout << "Timing batch size " << batchSize << " for " << rounds << " rounds...\n";
        for (int round = 0; round < rounds; ++round) {
            const auto start = std::chrono::steady_clock::now();
            const bool ok = scheme.batchVerify(messages, signatures);
            const auto end = std::chrono::steady_clock::now();
            const double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
            timings.push_back(elapsedMs);
            if (ok) {
                ++verifiedBatches;
            }
            detail << batchSize << "," << (round + 1) << "," << (ok ? 1 : 0) << "," << std::fixed << std::setprecision(6) << elapsedMs << "\n";
        }

        const TimingStats stats = summarize(timings);
        const int pairingsIfSingleVerify = 2 * batchSize;
        const int pairingsInBatchVerify = 2;
        const int pairingsSaved = pairingsIfSingleVerify - pairingsInBatchVerify;
        summary << batchSize << "," << batchSize << "," << rounds << "," << verifiedBatches << ","
                << std::fixed << std::setprecision(6)
                << stats.averageMs << "," << (stats.averageMs / static_cast<double>(batchSize)) << ","
                << stats.minMs << "," << stats.maxMs << "," << stats.stddevMs << ","
                << kMessageBytes << "," << scheme.signatureBytes() << ","
                << pairingsIfSingleVerify << "," << pairingsInBatchVerify << "," << pairingsSaved << "\n";

        std::cout << "batch " << batchSize
                  << ": " << verifiedBatches << "/" << rounds
                  << " verified, avg batch " << std::fixed << std::setprecision(6) << stats.averageMs
                  << " ms, avg per message " << (stats.averageMs / static_cast<double>(batchSize))
                  << " ms, pairings " << pairingsIfSingleVerify << " -> " << pairingsInBatchVerify << "\n";
    }

    std::cout << "\nWrote advlgs_large_batch_summary.csv and advlgs_large_batch_rounds.csv\n";
}

const SchemeTimingExport& findTimingExport(const std::string& scheme)
{
    for (const auto& item : g_schemeTimingExports) {
        if (item.scheme == scheme) {
            return item;
        }
    }
    throw std::runtime_error("missing timing export for " + scheme);
}

std::string buildAdvlgsBatchTimingTable()
{
    if (g_advlgsBatchTimingExports.empty()) {
        throw std::runtime_error("missing A-DVLGS batch timing export");
    }

    std::ostringstream os;
    for (std::size_t i = 0; i < g_advlgsBatchTimingExports.size(); ++i) {
        if (i != 0) {
            os << ";";
        }
        os << g_advlgsBatchTimingExports[i].batchSize << ":"
           << std::fixed << std::setprecision(6) << g_advlgsBatchTimingExports[i].verifyMs;
    }
    return os.str();
}

void removeDoneMarker()
{
    const auto donePath = timingOutputPath(kVeinsTimingDoneName);
    std::remove(donePath.string().c_str());
}

void writeVeinsTimingTriggerFiles()
{
    const std::vector<std::string> schemes = {"ADVLGS", "BBS", "CLGS", "MLGS", "ERCA"};
    const std::string batchTable = buildAdvlgsBatchTimingTable();
    const auto csvPath = timingOutputPath(kVeinsTimingCsvName);
    const auto donePath = timingOutputPath(kVeinsTimingDoneName);

    std::ofstream csv(csvPath, std::ios::trunc);
    if (!csv) {
        throw std::runtime_error(std::string("cannot write ") + csvPath.string());
    }
    csv << "scheme,sign_ms,verify_ms,batch_verify_by_size_ms\n";
    for (const auto& scheme : schemes) {
        const auto& item = findTimingExport(scheme);
        csv << scheme << ","
            << std::fixed << std::setprecision(6) << item.signMs << ","
            << std::fixed << std::setprecision(6) << item.verifyMs << ",";
        if (scheme == "ADVLGS") {
            csv << csvEscape(batchTable);
        }
        csv << "\n";
    }
    csv.close();

    std::ofstream done(donePath, std::ios::trunc);
    if (!done) {
        throw std::runtime_error(std::string("cannot write ") + donePath.string());
    }
    done << "complete\n";

    std::cout << "\nWrote Veins timing trigger files:\n"
              << "  " << csvPath.string() << "\n"
              << "  " << donePath.string() << "\n";
}

} // namespace

int main(int argc, char** argv)
{
    try {
        removeDoneMarker();

        if (argc > 1 && std::string(argv[1]) == "--large-batch") {
            const int rounds = parsePositiveIntArgument(argc, argv, 2, kDefaultLargeBatchRounds);
            const std::vector<int> batchSizes = argc > 3 ? parseBatchSizeList(argv[3]) : makeDefaultLargeBatchSizes();
            runAdvlgsLargeBatchBenchmark(rounds, batchSizes);
            return 0;
        }

        const int rounds = parseRounds(argc, argv);
        const int maxBatchSize = parseMaxBatchSize(argc, argv);
        runFiveSchemeSingleBenchmark(rounds);
        runAdvlgsBatchBenchmark(rounds, maxBatchSize);
        runAdvlgsLargeBatchBenchmark(kDefaultLargeBatchRounds, makeDefaultLargeBatchSizes());
        writeVeinsTimingTriggerFiles();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
