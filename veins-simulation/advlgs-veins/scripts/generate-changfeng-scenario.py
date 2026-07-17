# Build the Changfeng northwest SUMO/Veins scenario from the released map.

import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SUMO_HOME_ENV = os.environ.get("SUMO_HOME")
VEINS_INSTALL_ROOT = os.environ.get("VEINS_INSTALL_ROOT")
SUMO_HOME = (
    Path(SUMO_HOME_ENV)
    if SUMO_HOME_ENV
    else Path(VEINS_INSTALL_ROOT) / "sumo-1.22.0"
    if VEINS_INSTALL_ROOT
    else None
)

MAP_ROOT = PROJECT_ROOT / "maps"
TEMPLATE_ROOT = PROJECT_ROOT / "templates" / "veins-base"
SCENARIO = PROJECT_ROOT / "scenarios" / "changfeng-northwest-2km"

RANDOM_TRIPS = SUMO_HOME / "tools" / "randomTrips.py" if SUMO_HOME else None

MAP_SLUG = "changfeng_northwest_2km"
NET_NAME = f"{MAP_SLUG}.net.xml"
SOURCE_NET_NAME = "changfeng-northwest-2km.net.xml"

# The configured road area is 2 km x 2 km, so density is converted using 4 km^2.
DENSITIES = {
    5: 20,
    10: 40,
    15: 60,
    20: 80,
    25: 100,
    30: 120,
    35: 140,
    40: 160,
    45: 180,
    50: 200,
}
RUNS = range(5)
REQUEST_FREQUENCIES = [
    ("F1", "1 msg/s/vehicle", "1s"),
    ("F2", "2 msg/s/vehicle", "0.5s"),
    ("F5", "5 msg/s/vehicle", "0.2s"),
    ("F10", "10 msg/s/vehicle", "0.1s"),
]

RSU_GRID_POSITIONS = [
    (350, 350), (850, 350), (1350, 350), (1850, 350),
    (350, 850), (850, 850), (1350, 850), (1850, 850),
    (350, 1350), (850, 1350), (1350, 1350), (1850, 1350),
    (350, 1850), (850, 1850), (1350, 1850), (1850, 1850),
]

SCHEMES = [
    ("ADVLGS", "A-DVLGS", "0.009354059s", "0.012962006s"),
    ("BBS", "BBS", "0.010505589s", "0.015034069s"),
    ("CLGS", "CLGS", "0.011342214s", "0.015390718s"),
    ("MLGS", "MLGS", "0.015716830s", "0.015133717s"),
    ("ERCA", "ERCA", "0.017648287s", "0.019896913s"),
]

ADVLGS_BATCH_DELAYS = (
    "2:22.027156ms,3:29.600752ms,4:37.354594ms,5:45.163510ms,"
    "6:52.978918ms,7:60.440226ms,8:67.939923ms,9:77.860591ms,"
    "10:83.794047ms,11:94.519844ms,12:99.176086ms,13:106.781596ms,"
    "14:114.415620ms,15:126.985832ms,16:129.987362ms"
)

# SUMO uses metres per second; the experiment parameter remains expressed in km/h.
MAX_VEHICLE_SPEED_KMH = 56.0
MAX_VEHICLE_SPEED_MPS = MAX_VEHICLE_SPEED_KMH / 3.6

VTYPE_ATTRS = {
    "id": "veh_passenger",
    "vClass": "passenger",
    "accel": "2.6",
    "decel": "4.5",
    "sigma": "0.5",
    "length": "4.5",
    "minGap": "2.5",
    "maxSpeed": f"{MAX_VEHICLE_SPEED_MPS:.6f}",
    "speedFactor": "normc(1.00,0.10,0.70,1.30)",
    "color": "1,1,0",
}


def run(cmd, cwd=None, env=None):
    print(" ".join(str(c) for c in cmd))
    subprocess.run(cmd, check=True, cwd=cwd, env=env)


def indent(elem, level=0):
    i = "\n" + level * "    "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "    "
        for child in elem:
            indent(child, level + 1)
        if not child.tail or not child.tail.strip():
            child.tail = i
    if level and (not elem.tail or not elem.tail.strip()):
        elem.tail = i


def ensure_inputs():
    if RANDOM_TRIPS is None:
        raise RuntimeError("Set SUMO_HOME or VEINS_INSTALL_ROOT before generating the scenario")
    if not RANDOM_TRIPS.exists():
        raise FileNotFoundError(f"randomTrips.py not found: {RANDOM_TRIPS}")
    if not (MAP_ROOT / SOURCE_NET_NAME).exists():
        raise FileNotFoundError(f"released map file not found: {MAP_ROOT / SOURCE_NET_NAME}")
    for name in ["antenna.xml", "config.xml"]:
        if not (TEMPLATE_ROOT / name).exists():
            raise FileNotFoundError(f"template file not found: {TEMPLATE_ROOT / name}")


def copy_static_files():
    SCENARIO.mkdir(parents=True, exist_ok=True)
    (SCENARIO / "results").mkdir(exist_ok=True)
    shutil.copy2(MAP_ROOT / SOURCE_NET_NAME, SCENARIO / NET_NAME)
    for name in ["antenna.xml", "config.xml"]:
        shutil.copy2(TEMPLATE_ROOT / name, SCENARIO / name)
    (SCENARIO / "RSUExampleScenario.ned").write_text("""//
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
""", encoding="utf-8")


def write_routes():
    env = os.environ.copy()
    env["SUMO_HOME"] = str(SUMO_HOME)

    for density, target in DENSITIES.items():
        for run_no in RUNS:
            tmp = SCENARIO / f"_tmp_changfeng_d{density}_r{run_no}.rou.xml"
            route_file = SCENARIO / f"changfeng_d{density}_r{run_no}.rou.xml"
            seed = 300000 + density * 100 + run_no
            period = 1.0 / (target * 3.0)
            run([
                sys.executable, RANDOM_TRIPS,
                "-n", SCENARIO / NET_NAME,
                "-r", tmp,
                "-b", "0", "-e", "1", "-p", f"{period:.12f}",
                "--seed", str(seed),
                "--min-distance", "1000",
                "--intermediate", "2",
                "--random-departpos", "--random-arrivalpos",
                "--vehicle-class", "passenger",
                "--prefix", f"changfeng{density}_{run_no}_",
                "--validate",
            ], cwd=SCENARIO, env=env)

            tree = ET.parse(tmp)
            root = tree.getroot()
            vehicles = [child for child in list(root) if child.tag == "vehicle"]
            if len(vehicles) < target:
                raise RuntimeError(f"{route_file.name}: generated {len(vehicles)} routes, need {target}")

            new_root = ET.Element("routes", {
                "xmlns:xsi": "http://www.w3.org/2001/XMLSchema-instance",
                "xsi:noNamespaceSchemaLocation": "http://sumo.dlr.de/xsd/routes_file.xsd",
            })
            ET.SubElement(new_root, "vType", VTYPE_ATTRS)
            for idx, vehicle in enumerate(vehicles[:target]):
                vehicle.set("id", f"changfeng{density}_{run_no}_{idx}")
                vehicle.set("type", "veh_passenger")
                vehicle.set("depart", f"{idx * 5.0 / target:.3f}")
                vehicle.set("departPos", "random_free")
                vehicle.set("departLane", "best")
                vehicle.set("departSpeed", "max")
                new_root.append(vehicle)
            indent(new_root)
            ET.ElementTree(new_root).write(route_file, encoding="UTF-8", xml_declaration=True)
            tmp.unlink(missing_ok=True)
            (SCENARIO / "trips.trips.xml").unlink(missing_ok=True)

            (SCENARIO / f"changfeng_d{density}_r{run_no}.sumo.cfg").write_text(
                f"""<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<configuration xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"http://sumo.dlr.de/xsd/sumoConfiguration.xsd\">
    <input>
        <net-file value=\"{NET_NAME}\"/>
        <route-files value=\"changfeng_d{density}_r{run_no}.rou.xml\"/>
    </input>
    <time>
        <begin value=\"0\"/>
        <end value=\"100\"/>
        <step-length value=\"0.1\"/>
    </time>
    <report>
        <xml-validation value=\"never\"/>
        <xml-validation.net value=\"never\"/>
        <no-step-log value=\"true\"/>
    </report>
    <gui_only>
        <start value=\"true\"/>
    </gui_only>
</configuration>
""",
                encoding="utf-8",
            )

            (SCENARIO / f"changfeng_d{density}_r{run_no}.launchd.xml").write_text(
                f"""<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<launch>
    <copy file=\"{NET_NAME}\"/>
    <copy file=\"changfeng_d{density}_r{run_no}.rou.xml\"/>
    <copy file=\"changfeng_d{density}_r{run_no}.sumo.cfg\" type=\"config\"/>
</launch>
""",
                encoding="utf-8",
            )
            print(f"changfeng_d{density}_r{run_no}: {target} vehicles")


def write_omnet_ini():
    rsu_position_lines = "\n".join(
        f"*.rsu[{idx}].mobility.x = {x}\n*.rsu[{idx}].mobility.y = {y}"
        for idx, (x, y) in enumerate(RSU_GRID_POSITIONS)
    )
    parts = [f"""[General]
cmdenv-express-mode = true
cmdenv-autoflush = true
cmdenv-status-frequency = 1s
**.cmdenv-log-level = info
repeat = 5

network = RSUExampleScenario

[Config ChangfengBase]
description = "A-DVLGS simulation on the Shanghai Changfeng northwest 2.0x2.0 km map"

sim-time-limit = 100s
seed-set = ${{runnumber}}

**.scalar-recording = true
**.vector-recording = true
output-scalar-file = results/${{configname}}-${{runnumber}}.sca
output-vector-file = results/${{configname}}-${{runnumber}}.vec

*.playgroundSizeX = 2300m
*.playgroundSizeY = 2300m
*.playgroundSizeZ = 50m

*.annotations.draw = true

*.manager.updateInterval = 0.1s
*.manager.host = "localhost"
*.manager.port = 9999
*.manager.autoShutdown = true
*.manager.trafficLightModuleType = ""

{rsu_position_lines}
*.rsu[*].mobility.z = 3
*.rsu[*].applType = "org.advlgs.CryptoRSU11p"
*.rsu[*].appl.serviceDomain = "SPS:changfeng-northwest"
*.rsu[*].appl.repeatMessages = false
*.rsu[*].appl.headerLength = 80 bit
*.rsu[*].appl.dataLengthBits = 1024 bit
*.rsu[*].appl.signedMessageBytes = 128B
*.rsu[*].appl.serviceRequestTtl = 1s
*.rsu[*].appl.enableBatchVerification = true
*.rsu[*].appl.batchMaxSize = 0  # unlimited inside each 20 ms arrival window
*.rsu[*].appl.batchWindow = 0.02s
*.rsu[*].appl.useFixedCryptoTiming = true
*.rsu[*].appl.skipCryptoComputation = true
*.rsu[*].appl.sendBeacons = false
*.rsu[*].appl.dataOnSch = false
*.rsu[*].appl.beaconInterval = 1s
*.rsu[*].appl.beaconUserPriority = 7
*.rsu[*].appl.dataUserPriority = 5
*.rsu[*].nic.phy80211p.antennaOffsetZ = 0 m

*.connectionManager.sendDirect = true
*.connectionManager.maxInterfDist = 300m
*.connectionManager.drawMaxIntfDist = false
*.**.nic.mac1609_4.useServiceChannel = false
*.**.nic.mac1609_4.txPower = 20mW
*.**.nic.mac1609_4.bitrate = 6Mbps
*.**.nic.phy80211p.minPowerLevel = -110dBm
*.**.nic.phy80211p.useNoiseFloor = true
*.**.nic.phy80211p.noiseFloor = -98dBm
*.**.nic.phy80211p.decider = xmldoc("config.xml")
*.**.nic.phy80211p.analogueModels = xmldoc("config.xml")
*.**.nic.phy80211p.usePropagationDelay = true
*.**.nic.phy80211p.antenna = xmldoc("antenna.xml", "/root/Antenna[@id='monopole']")
*.node[*].nic.phy80211p.antennaOffsetY = 0 m
*.node[*].nic.phy80211p.antennaOffsetZ = 1.895 m

*.node[*].applType = "org.advlgs.CryptoDemo11p"
*.node[*].appl.serviceDomain = "SPS:changfeng-northwest"
*.node[*].appl.experimentMode = true
*.node[*].appl.announcementProbability = 1.0
*.node[*].appl.announcementInterval = 0.3s
*.node[*].appl.announcementStopTime = 99.5s
*.node[*].appl.fixedMessageBytes = 0B
*.node[*].appl.headerLength = 80 bit
*.node[*].appl.dataLengthBits = 1024 bit
*.node[*].appl.signedMessageBytes = 128B
*.node[*].appl.useFixedCryptoTiming = true
*.node[*].appl.skipCryptoComputation = true
*.node[*].appl.sendBeacons = false
*.node[*].appl.dataOnSch = false
*.node[*].appl.beaconInterval = 1s

*.node[*].veinsmobility.x = 0
*.node[*].veinsmobility.y = 0
*.node[*].veinsmobility.z = 0
*.node[*].veinsmobility.setHostSpeed = false
"""]

    for config_scheme, display_scheme, sign_delay, verify_delay in SCHEMES:
        parts.append(f"""
[Config {config_scheme}Base]
extends = ChangfengBase
*.node[*].appl.cryptoScheme = "{display_scheme}"
*.rsu[*].appl.cryptoScheme = "{display_scheme}"
*.node[*].appl.fixedSignDelay = {sign_delay}
*.node[*].appl.fixedVerifyDelay = {verify_delay}
*.rsu[*].appl.fixedVerifyDelay = {verify_delay}
""")
        if config_scheme == "ADVLGS":
            parts.append(f"""*.rsu[*].appl.fixedBatchVerifyDelayPerItem = 0s
*.rsu[*].appl.fixedBatchVerifyDelayBySize = "{ADVLGS_BATCH_DELAYS}"
""")

    for config_scheme, display_scheme, _sign_delay, _verify_delay in SCHEMES:
        for freq_code, freq_label, interval in REQUEST_FREQUENCIES:
            for density in DENSITIES:
                for run_no in RUNS:
                    parts.append(f"""
[Config {config_scheme}_CHANGFENG_D{density}_{freq_code}_R{run_no}]
extends = {config_scheme}Base
description = "{display_scheme} on Changfeng northwest density {density} vehicles/km2, request frequency {freq_label}, independent run {run_no}"
*.node[*].appl.announcementInterval = {interval}
*.manager.launchConfig = xmldoc("changfeng_d{density}_r{run_no}.launchd.xml")
""")

    (SCENARIO / "omnetpp.ini").write_text("\n".join(parts), encoding="utf-8")


def main():
    ensure_inputs()
    copy_static_files()
    write_routes()
    write_omnet_ini()
    print(f"Done: {SCENARIO}")


if __name__ == "__main__":
    main()
