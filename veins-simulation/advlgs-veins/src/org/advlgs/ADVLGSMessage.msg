import veins.modules.messages.BaseFrame1609_4;
import veins.base.utils.SimpleAddress;

namespace advlgs;

packet ADVLGSMessage extends veins::BaseFrame1609_4 {
    string demoData;
    veins::LAddress::L2Type senderAddress = -1;
    int serial = 0;
    simtime_t generatedAt;
    simtime_t signedAt;
    simtime_t sentAt;
    double signDurationSeconds = 0;
    string serviceDomain;
    string signature;
    string linkTag;
}
