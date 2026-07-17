#pragma once

#include <cstddef>
#include <string>

namespace advlgs {

struct CLGSSignature {
    std::string encoded;
    std::string linkTag;
    std::size_t wireBytes = 0;
};

class CLGSMiracl {
public:
    static void warmup();
    static void prepareMember(int memberId);
    static CLGSSignature sign(int memberId, const std::string& message, const std::string& serviceDomain);
    static bool verify(const std::string& message, const std::string& serviceDomain, const std::string& encodedSignature, std::string* linkTag);
    static std::size_t nominalSignatureBytes();
};

} // namespace advlgs
