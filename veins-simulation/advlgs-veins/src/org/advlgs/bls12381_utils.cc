#include "org/advlgs/bls12381_utils.h"

#include <array>
#include <cctype>
#include <sstream>
#include <vector>

namespace advlgs::bls12381 {
namespace {

using namespace BLS12381;
using namespace BLS12381_BIG;

void order(BIG out)
{
    BIG_rcopy(out, CURVE_Order);
}

std::string bytesToHex(const char* data, int len)
{
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(static_cast<std::size_t>(len) * 2);
    for (int i = 0; i < len; ++i) {
        const auto b = static_cast<unsigned char>(data[i]);
        out.push_back(kDigits[b >> 4]);
        out.push_back(kDigits[b & 0x0f]);
    }
    return out;
}

int hexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool hexToBytes(const std::string& hex, std::vector<char>* out)
{
    if ((hex.size() % 2) != 0) return false;
    std::vector<char> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = hexValue(hex[i]);
        const int lo = hexValue(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        bytes.push_back(static_cast<char>((hi << 4) | lo));
    }
    *out = std::move(bytes);
    return true;
}

} // namespace

Fr::Fr()
{
    BIG_zero(v);
}

Fr zero()
{
    return Fr();
}

Fr one()
{
    Fr out;
    BIG_one(out.v);
    return out;
}

Fr hashScalar(const std::string& input)
{
    core::hash512 h;
    std::array<char, 64> digest {};
    core::HASH512_init(&h);
    for (unsigned char c : input) core::HASH512_process(&h, c);
    core::HASH512_hash(&h, digest.data());

    Fr out;
    BIG q;
    order(q);
    BIG_fromBytesLen(out.v, digest.data(), static_cast<int>(digest.size()));
    BIG_mod(out.v, q);
    if (BIG_iszilch(out.v)) BIG_one(out.v);
    return out;
}

Fr scalarFromHex(const std::string& hex, bool* ok)
{
    std::vector<char> bytes;
    const bool parsed = hexToBytes(hex, &bytes);
    if (ok) *ok = parsed;
    Fr out;
    if (!parsed || bytes.empty()) return out;
    BIG q;
    order(q);
    BIG_fromBytesLen(out.v, bytes.data(), static_cast<int>(bytes.size()));
    BIG_mod(out.v, q);
    return out;
}

std::string scalarToHex(const Fr& x)
{
    std::array<char, MODBYTES_B384_58> bytes {};
    BIG tmp;
    BIG_copy(tmp, const_cast<chunk*>(x.v));
    BIG_toBytes(bytes.data(), tmp);
    return bytesToHex(bytes.data(), static_cast<int>(bytes.size()));
}

bool scalarEquals(const Fr& a, const Fr& b)
{
    BIG left;
    BIG right;
    BIG_copy(left, const_cast<chunk*>(a.v));
    BIG_copy(right, const_cast<chunk*>(b.v));
    return BIG_comp(left, right) == 0;
}

Fr add(const Fr& a, const Fr& b)
{
    Fr out;
    BIG q;
    order(q);
    BIG_modadd(out.v, const_cast<chunk*>(a.v), const_cast<chunk*>(b.v), q);
    return out;
}

Fr sub(const Fr& a, const Fr& b)
{
    return add(a, neg(b));
}

Fr mul(const Fr& a, const Fr& b)
{
    Fr out;
    BIG q;
    order(q);
    BIG_modmul(out.v, const_cast<chunk*>(a.v), const_cast<chunk*>(b.v), q);
    return out;
}

Fr neg(const Fr& a)
{
    Fr out;
    BIG q;
    order(q);
    BIG_modneg(out.v, const_cast<chunk*>(a.v), q);
    return out;
}

Fr inv(const Fr& a)
{
    Fr out;
    BIG q;
    order(q);
    BIG_invmodp(out.v, const_cast<chunk*>(a.v), q);
    return out;
}

BLS12381::ECP g1Generator()
{
    ECP p;
    ECP_generator(&p);
    return p;
}

BLS12381::ECP2 g2Generator()
{
    ECP2 q;
    ECP2_generator(&q);
    return q;
}

BLS12381::ECP g1HashPoint(const std::string& label)
{
    return g1Mul(g1Generator(), hashScalar(label));
}

BLS12381::ECP g1Mul(const BLS12381::ECP& p, const Fr& x)
{
    ECP out;
    ECP_copy(&out, const_cast<ECP*>(&p));
    PAIR_G1mul(&out, const_cast<chunk*>(x.v));
    return out;
}

BLS12381::ECP g1Add(const BLS12381::ECP& a, const BLS12381::ECP& b)
{
    ECP out;
    ECP rhs;
    ECP_copy(&out, const_cast<ECP*>(&a));
    ECP_copy(&rhs, const_cast<ECP*>(&b));
    ECP_add(&out, &rhs);
    return out;
}

BLS12381::ECP g1Sub(const BLS12381::ECP& a, const BLS12381::ECP& b)
{
    ECP out;
    ECP rhs;
    ECP_copy(&out, const_cast<ECP*>(&a));
    ECP_copy(&rhs, const_cast<ECP*>(&b));
    ECP_sub(&out, &rhs);
    return out;
}

BLS12381::ECP g1Neg(const BLS12381::ECP& p)
{
    ECP out;
    ECP_copy(&out, const_cast<ECP*>(&p));
    ECP_neg(&out);
    return out;
}

std::string g1ToHex(const BLS12381::ECP& p)
{
    std::array<char, MODBYTES_B384_58 + 1> bytes {};
    core::octet o {0, static_cast<int>(bytes.size()), bytes.data()};
    ECP tmp;
    ECP_copy(&tmp, const_cast<ECP*>(&p));
    ECP_toOctet(&o, &tmp, true);
    return bytesToHex(o.val, o.len);
}

bool g1FromHex(const std::string& hex, BLS12381::ECP* out)
{
    std::vector<char> bytes;
    if (!hexToBytes(hex, &bytes) || bytes.empty()) return false;
    core::octet o {static_cast<int>(bytes.size()), static_cast<int>(bytes.size()), bytes.data()};
    if (!ECP_fromOctet(out, &o)) return false;
    return PAIR_G1member(out) != 0;
}

bool g1Equals(const BLS12381::ECP& a, const BLS12381::ECP& b)
{
    ECP left;
    ECP right;
    ECP_copy(&left, const_cast<ECP*>(&a));
    ECP_copy(&right, const_cast<ECP*>(&b));
    return ECP_equals(&left, &right) != 0;
}

BLS12381::ECP2 g2Mul(const BLS12381::ECP2& p, const Fr& x)
{
    ECP2 out;
    ECP2_copy(&out, const_cast<ECP2*>(&p));
    PAIR_G2mul(&out, const_cast<chunk*>(x.v));
    return out;
}

BLS12381::ECP2 g2Add(const BLS12381::ECP2& a, const BLS12381::ECP2& b)
{
    ECP2 out;
    ECP2 rhs;
    ECP2_copy(&out, const_cast<ECP2*>(&a));
    ECP2_copy(&rhs, const_cast<ECP2*>(&b));
    ECP2_add(&out, &rhs);
    return out;
}

BLS12381::ECP2 g2Sub(const BLS12381::ECP2& a, const BLS12381::ECP2& b)
{
    ECP2 out;
    ECP2 rhs;
    ECP2_copy(&out, const_cast<ECP2*>(&a));
    ECP2_copy(&rhs, const_cast<ECP2*>(&b));
    ECP2_sub(&out, &rhs);
    return out;
}

std::string g2ToHex(const BLS12381::ECP2& p)
{
    std::array<char, 2 * MODBYTES_B384_58 + 1> bytes {};
    core::octet o {0, static_cast<int>(bytes.size()), bytes.data()};
    ECP2 tmp;
    ECP2_copy(&tmp, const_cast<ECP2*>(&p));
    ECP2_toOctet(&o, &tmp, true);
    return bytesToHex(o.val, o.len);
}

bool g2FromHex(const std::string& hex, BLS12381::ECP2* out)
{
    std::vector<char> bytes;
    if (!hexToBytes(hex, &bytes) || bytes.empty()) return false;
    core::octet o {static_cast<int>(bytes.size()), static_cast<int>(bytes.size()), bytes.data()};
    if (!ECP2_fromOctet(out, &o)) return false;
    return PAIR_G2member(out) != 0;
}

bool g2Equals(const BLS12381::ECP2& a, const BLS12381::ECP2& b)
{
    ECP2 left;
    ECP2 right;
    ECP2_copy(&left, const_cast<ECP2*>(&a));
    ECP2_copy(&right, const_cast<ECP2*>(&b));
    return ECP2_equals(&left, &right) != 0;
}

BLS12381::FP12 pair(const BLS12381::ECP2& q, const BLS12381::ECP& p)
{
    ECP2 qCopy;
    ECP pCopy;
    FP12 out;
    ECP2_copy(&qCopy, const_cast<ECP2*>(&q));
    ECP_copy(&pCopy, const_cast<ECP*>(&p));
    PAIR_ate(&out, &qCopy, &pCopy);
    PAIR_fexp(&out);
    return out;
}

BLS12381::FP12 gtOne()
{
    FP12 out;
    FP12_one(&out);
    return out;
}

BLS12381::FP12 gtMul(const BLS12381::FP12& a, const BLS12381::FP12& b)
{
    FP12 out;
    FP12 rhs;
    FP12_copy(&out, const_cast<FP12*>(&a));
    FP12_copy(&rhs, const_cast<FP12*>(&b));
    FP12_mul(&out, &rhs);
    return out;
}

BLS12381::FP12 gtDiv(const BLS12381::FP12& a, const BLS12381::FP12& b)
{
    FP12 invB;
    FP12 rhs;
    FP12_copy(&rhs, const_cast<FP12*>(&b));
    FP12_inv(&invB, &rhs);
    return gtMul(a, invB);
}

BLS12381::FP12 gtPow(const BLS12381::FP12& a, const Fr& x)
{
    FP12 out;
    FP12_copy(&out, const_cast<FP12*>(&a));
    PAIR_GTpow(&out, const_cast<chunk*>(x.v));
    return out;
}

std::string gtToHex(const BLS12381::FP12& x)
{
    std::array<char, 12 * MODBYTES_B384_58> bytes {};
    core::octet o {0, static_cast<int>(bytes.size()), bytes.data()};
    FP12 tmp;
    FP12_copy(&tmp, const_cast<FP12*>(&x));
    FP12_reduce(&tmp);
    FP12_toOctet(&o, &tmp);
    return bytesToHex(o.val, o.len);
}

bool gtEquals(const BLS12381::FP12& a, const BLS12381::FP12& b)
{
    FP12 left;
    FP12 right;
    FP12_copy(&left, const_cast<FP12*>(&a));
    FP12_copy(&right, const_cast<FP12*>(&b));
    return FP12_equals(&left, &right) != 0;
}

} // namespace advlgs::bls12381
