#pragma once

#include <cstddef>
#include <string>

#include "pair_BLS12381.h"

namespace advlgs::bls12381 {

constexpr std::size_t kG1CompressedWireBytes = 48;
constexpr std::size_t kG2CompressedWireBytes = 96;
constexpr std::size_t kScalarWireBytes = 32;

struct Fr {
    BLS12381_BIG::BIG v;
    Fr();
};

Fr zero();
Fr one();
Fr hashScalar(const std::string& input);
Fr scalarFromHex(const std::string& hex, bool* ok = nullptr);
std::string scalarToHex(const Fr& x);
bool scalarEquals(const Fr& a, const Fr& b);

Fr add(const Fr& a, const Fr& b);
Fr sub(const Fr& a, const Fr& b);
Fr mul(const Fr& a, const Fr& b);
Fr neg(const Fr& a);
Fr inv(const Fr& a);

BLS12381::ECP g1Generator();
BLS12381::ECP2 g2Generator();
BLS12381::ECP g1HashPoint(const std::string& label);
BLS12381::ECP g1Mul(const BLS12381::ECP& p, const Fr& x);
BLS12381::ECP g1Add(const BLS12381::ECP& a, const BLS12381::ECP& b);
BLS12381::ECP g1Sub(const BLS12381::ECP& a, const BLS12381::ECP& b);
BLS12381::ECP g1Neg(const BLS12381::ECP& p);
std::string g1ToHex(const BLS12381::ECP& p);
bool g1FromHex(const std::string& hex, BLS12381::ECP* out);
bool g1Equals(const BLS12381::ECP& a, const BLS12381::ECP& b);

BLS12381::ECP2 g2Mul(const BLS12381::ECP2& p, const Fr& x);
BLS12381::ECP2 g2Add(const BLS12381::ECP2& a, const BLS12381::ECP2& b);
BLS12381::ECP2 g2Sub(const BLS12381::ECP2& a, const BLS12381::ECP2& b);
std::string g2ToHex(const BLS12381::ECP2& p);
bool g2FromHex(const std::string& hex, BLS12381::ECP2* out);
bool g2Equals(const BLS12381::ECP2& a, const BLS12381::ECP2& b);

BLS12381::FP12 pair(const BLS12381::ECP2& q, const BLS12381::ECP& p);
BLS12381::FP12 gtOne();
BLS12381::FP12 gtMul(const BLS12381::FP12& a, const BLS12381::FP12& b);
BLS12381::FP12 gtDiv(const BLS12381::FP12& a, const BLS12381::FP12& b);
BLS12381::FP12 gtPow(const BLS12381::FP12& a, const Fr& x);
std::string gtToHex(const BLS12381::FP12& x);
bool gtEquals(const BLS12381::FP12& a, const BLS12381::FP12& b);

} // namespace advlgs::bls12381
