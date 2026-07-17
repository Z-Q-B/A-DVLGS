#pragma once

#include <cstddef>
#include <string>

namespace advlgs {

struct ERCASignature {
    std::string encoded;
    std::string linkTag;
    std::size_t wireBytes = 0;
};

class ERCAMiracl {
public:
    static void warmup();
    static void prepareMember(int memberId);
    static ERCASignature sign(int memberId, const std::string& message, const std::string& serviceDomain);
    static bool verify(const std::string& message, const std::string& serviceDomain, const std::string& encodedSignature, std::string* linkTag);
    static std::size_t nominalSignatureBytes();
};

} // namespace advlgs
