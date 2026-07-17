//
// Copyright (C) 2017 Christoph Sommer <sommer@ccs-labs.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later
//

import org.car2x.veins.nodes.RSU;
import org.car2x.veins.nodes.Scenario;
import org.car2x.veins.nodes.TrafficLight;

network RSUExampleScenario extends Scenario
{
    submodules:
        rsu[16]: RSU {
            @display("p=150,140;i=veins/sign/yellowdiamond;is=vs");
        }

        tls[0]: TrafficLight {
        }
}
