#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace advlgs {

struct ADVLGSSignature {
    std::string encoded;
    std::string linkTag;
    std::size_t wireBytes = 0;
};

struct ADVLGSBatchItem {
    std::string message;
    std::string serviceDomain;
    std::string encodedSignature;
};

struct ADVLGSBatchResult {
    std::vector<bool> accepted;
    std::vector<std::string> linkTags;
};

class ADVLGSMiracl {
public:
    static void warmup();
    static void prepareMember(int memberId);
    static ADVLGSSignature sign(int memberId, const std::string& message, const std::string& serviceDomain);
    static bool verify(const std::string& message, const std::string& serviceDomain, const std::string& encodedSignature, std::string* linkTag);
    static ADVLGSBatchResult batchVerify(const std::vector<ADVLGSBatchItem>& items);
    static std::size_t nominalSignatureBytes();
};

} // namespace advlgs
