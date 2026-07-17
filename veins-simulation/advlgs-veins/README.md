# A-DVLGS Veins Simulation

This directory contains the OMNeT++/Veins overlay and the deterministic Changfeng scenario used by the A-DVLGS experiment.

## Layout

- `src/org/advlgs/`: application modules, messages, and cryptographic scheme adapters.
- `scenarios/changfeng-northwest-2km/`: SUMO routes, launch configurations, and `omnetpp.ini`.
- `maps/`: released SUMO road network used by the scenario generator.
- `templates/veins-base/`: Veins antenna and physical-layer configuration templates.
- `scripts/generate-changfeng-scenario.py`: rebuilds deterministic route and configuration inputs.
- `scripts/apply-benchmark-timings.ps1`: imports benchmark timing values into `omnetpp.ini`.
- `scripts/run-qtenv.ps1`: runs one selected Qtenv configuration.

## Build

Use an OMNeT++ 6.1 command environment. Set `VEINS_INSTALL_ROOT` and either `MIRACL_CORE_ROOT` or `MIRACL_CORE_CPP_ROOT`, then run:

```text
make MODE=release
```

The expected library name is `libadvlgs_veins` with the platform-specific shared-library suffix.

## Scenario Naming

```text
<SCHEME>_CHANGFENG_D<DENSITY>_F<RATE>_R<SEED>
```

- `SCHEME`: `ADVLGS`, `BBS`, `CLGS`, `MLGS`, or `ERCA`.
- `DENSITY`: 5 through 50 vehicles/km² in steps of 5.
- `RATE`: 1, 2, 5, or 10 messages/s/vehicle.
- `SEED`: 0 through 4.

The scenario uses the route file with the matching density and seed. OMNeT++ uses the run number as its seed set.

## Timing Import

The benchmark generates `scheme_timings.csv`. Place it in `scripts/` and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\apply-benchmark-timings.ps1
```

The script creates a timestamped local backup before updating `omnetpp.ini`; backup files are ignored by Git.
