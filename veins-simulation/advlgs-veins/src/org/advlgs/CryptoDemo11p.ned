package org.advlgs;

import org.car2x.veins.modules.application.ieee80211p.DemoBaseApplLayer;

simple CryptoDemo11p extends DemoBaseApplLayer
{
    parameters:
        string serviceDomain = default("SPS:erlangen");
        string cryptoScheme = default("A-DVLGS");
        bool experimentMode = default(false);
        double announcementProbability = default(1.0);
        double announcementInterval @unit(s) = default(0.3s);
        double announcementStopTime @unit(s) = default(99.5s);
        int fixedMessageBytes @unit(B) = default(0B);
        int signedMessageBytes @unit(B) = default(128B);
        bool useFixedCryptoTiming = default(false);
        bool skipCryptoComputation = default(false);
        double fixedSignDelay @unit(s) = default(0s);
        double fixedVerifyDelay @unit(s) = default(0s);
        @class(advlgs::CryptoDemo11p);
        @display("i=block/app2");
}
