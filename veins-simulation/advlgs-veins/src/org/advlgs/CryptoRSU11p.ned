package org.advlgs;

import org.car2x.veins.modules.application.ieee80211p.DemoBaseApplLayer;

simple CryptoRSU11p extends DemoBaseApplLayer
{
    parameters:
        string serviceDomain = default("SPS:erlangen");
        string cryptoScheme = default("A-DVLGS");
        int signedMessageBytes @unit(B) = default(128B);
        bool repeatMessages = default(true);
        bool enableBatchVerification = default(true);
        // 0 = unlimited inside one batch window; positive = optional hard cap.
        int batchMaxSize = default(0);
        double batchWindow @unit(s) = default(0.02s);
        double serviceRequestTtl @unit(s) = default(1s);
        bool useFixedCryptoTiming = default(false);
        bool skipCryptoComputation = default(false);
        double fixedVerifyDelay @unit(s) = default(0s);
        double fixedBatchVerifyDelayPerItem @unit(s) = default(0s);
        string fixedBatchVerifyDelayBySize = default("");
        @class(advlgs::CryptoRSU11p);
        @display("i=veins/sign/yellowdiamond");
}
