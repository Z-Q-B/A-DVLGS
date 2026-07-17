# A-DVLGS Experiment Code

This repository contains the cryptographic benchmark and Veins/SUMO simulation code used to evaluate A-DVLGS and the BBS, CLGS, MLGS, and ERCA comparison schemes. It provides the source code, deterministic SUMO route inputs, the released road network, and OMNeT++ experiment configurations.

## Repository Layout

- `crypto-benchmarks/advlgs-benchmark-vs2022/`
  - Visual Studio 2022 C++ benchmark based on MIRACL Core BLS12-381.
  - Measures single signing and verification for all five schemes.
  - Measures A-DVLGS batch verification for small and large batches.
  - Exports timing values for the Veins simulation.
- `veins-simulation/advlgs-veins/`
  - OMNeT++/Veins overlay project and application source code.
  - Contains the Changfeng SUMO scenario, deterministic routes, configuration templates, and single-run utilities.
  - Uses benchmark-derived fixed cryptographic delays to model the service-authentication workload.

## Requirements

- Windows 11 x64
- Visual Studio 2022 with C++17, the v143 toolset, and x64 Release support
- MIRACL Core with BLS12-381 Type-3 pairing support
- OMNeT++ 6.1
- Veins 5.3.1
- SUMO 1.22.0
- Python 3
- PowerShell 5.1 or later

The simulation packages should share one installation root:

```text
<simulation-root>/
  omnetpp-6.1/
  veins-veins-5.3.1/
  sumo-1.22.0/
```

Set the simulation root before using the supplied scripts:

```powershell
$env:VEINS_INSTALL_ROOT = "<simulation-root>"
```

## Cryptographic Benchmark

### Build

Open `crypto-benchmarks/advlgs-benchmark-vs2022/advlgs-benchmark-vs2022.sln` in Visual Studio 2022 and select `Release | x64`.

Set `MIRACL_CORE_PROPS` to a local Visual Studio property sheet that supplies the MIRACL Core include and library paths:

```powershell
$env:MIRACL_CORE_PROPS = "<path-to-miracl-core.props>"
```

To write the Veins timing files directly to the simulation scripts directory, set:

```powershell
$env:VEINS_TIMING_DIR = "<repository-root>\veins-simulation\advlgs-veins\scripts"
```

If `VEINS_TIMING_DIR` is not set, the timing files are written to the benchmark working directory.

### Default Run

Run the benchmark with no arguments:

```text
advlgs-benchmark.exe
```

The default run performs:

- Single signing and single verification for A-DVLGS, BBS, CLGS, MLGS, and ERCA.
- A-DVLGS batch verification for batch sizes `1..16`.
- A-DVLGS large-batch verification for batch sizes `1000,2000,3000,4000,5000`.
- Veins timing export.

Default settings:

| Setting | Value |
|---|---|
| Single signing and verification | `10000` rounds per scheme |
| A-DVLGS small-batch verification | `10000` rounds per batch size |
| A-DVLGS large-batch verification | `100` rounds per batch size |
| Message body | `128 B` |
| Curve and library | MIRACL Core BLS12-381 Type-3 pairing |

### Benchmark Outputs

- `five_scheme_single_summary.csv`
- `five_scheme_single_rounds.csv`
- `advlgs_batch_summary.csv`
- `advlgs_batch_rounds.csv`
- `advlgs_large_batch_summary.csv`
- `advlgs_large_batch_rounds.csv`
- `scheme_timings.csv`
- `scheme_timings.done`

`scheme_timings.csv` supplies the measured single-signing and single-verification delays and the A-DVLGS batch-verification timing table used by the simulation.

## Multi-Message Verification Model

A-DVLGS implements an explicit batch verifier. For a batch of `n` service requests, the verifier reconstructs the per-signature transcript data and aggregates the final pairing equation. This reduces the pairing portion from `2n` pairings for independent verification to two pairings for one batch.

BBS, CLGS, MLGS, and ERCA do not have a native batch verifier in this codebase. When a queued group of `n` requests is modeled for these schemes, its verification cost is calculated as sequential verification of `n` independent signatures:

```text
projected_time(n) = n * average_single_verify_time
```

This calculation is a workload model for the comparison schemes, not a new cryptographic batch-verification algorithm.

## Veins Simulation

### Build

Open an OMNeT++ 6.1 command environment and enter `veins-simulation/advlgs-veins`. Set either `MIRACL_CORE_CPP_ROOT` directly or set `MIRACL_CORE_ROOT` so that `<MIRACL_CORE_ROOT>/cpp-bls12381/core.a` exists. Then build the release library:

```text
make MODE=release
```

The Makefile reads `VEINS_INSTALL_ROOT`, `VEINS_ROOT`, `MIRACL_CORE_ROOT`, and `MIRACL_CORE_CPP_ROOT`; it contains no machine-specific paths.

### Import Benchmark Timings

Place the generated `scheme_timings.csv` in `veins-simulation/advlgs-veins/scripts`, or provide its path explicitly. Apply the timing values with:

```powershell
powershell -ExecutionPolicy Bypass -File .\veins-simulation\advlgs-veins\scripts\apply-benchmark-timings.ps1
```

The script updates the relevant base configurations in `omnetpp.ini` and creates a timestamped local backup.

### Quick Run

After building the overlay, run one Qtenv configuration from PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\veins-simulation\advlgs-veins\scripts\run-qtenv.ps1 `
  -Config ADVLGS_CHANGFENG_D20_F5_R0 -Run 0
```

The script starts SUMO TraCI launchd when port 9999 is not already in use. Simulation outputs are written below `scenarios/changfeng-northwest-2km/results/`.

### Rebuild Scenario Inputs

The deterministic routes and scenario configurations can be regenerated with:

```powershell
python .\veins-simulation\advlgs-veins\scripts\generate-changfeng-scenario.py
```

Set `SUMO_HOME` explicitly or use `VEINS_INSTALL_ROOT` before running the generator.

## Experiment Matrix

Configuration names follow this pattern:

```text
<SCHEME>_CHANGFENG_D<DENSITY>_F<RATE>_R<SEED>
```

The released configuration grid contains five schemes, ten vehicle densities, four message rates, and five independent random seeds:

```text
5 schemes * 10 densities * 4 message rates * 5 seeds = 1000 configurations
```

For example, `MLGS_CHANGFENG_D30_F2_R3` selects MLGS, 30 vehicles/km², 2 messages/s/vehicle, and the R3 route and OMNeT++ seed.

| Parameter | Value |
|---|---|
| Schemes | A-DVLGS, BBS, CLGS, MLGS, ERCA |
| Road area | 2 km × 2 km |
| Vehicle density | 5, 10, ..., 50 vehicles/km² |
| Vehicles per run | 20, 40, ..., 200 |
| Message rate | 1, 2, 5, 10 messages/s/vehicle |
| Route and OMNeT++ seeds | R0--R4 |
| Simulation duration | 100 s |
| Maximum vehicle speed | 56 km/h |
| RSUs | 16 |
| Service-request TTL | 1 s |
| A-DVLGS batch window | 20 ms |
| A-DVLGS batch item cap | Unlimited within the window (`0`) |

## Reproduction Workflow

1. Build and run the cryptographic benchmark in `Release | x64`.
2. Generate `scheme_timings.csv` from the benchmark.
3. Import the timing values into the Veins scenario configuration.
4. Build the OMNeT++/Veins overlay.
5. Run the required configurations for the selected schemes, densities, message rates, and seeds.
6. Aggregate the generated OMNeT++ scalar, vector, and per-module CSV outputs.

## Simulation Outputs

- OMNeT++ scalars: `.sca` files.
- OMNeT++ vectors: `.vec` and `.vci` files.
- Per-module metrics: `results/advlgs-metrics.csv`.

The recorded metrics include accepted and rejected packets, packet loss rate, validation delay, verification queue delay, batch wait, batch size, and throughput.

## Licensing

A repository-wide license has not yet been selected. The repository owner must add a license compatible with the included and dependent components before public distribution.

This repository depends on MIRACL Core, OMNeT++, Veins, and SUMO. These external packages are not redistributed here and remain subject to their respective licenses.

The files `antenna.xml`, `config.xml`, and `RSUExampleScenario.ned` retain their embedded Veins copyright and SPDX notices. Depending on the file, the stated terms are GPL-2.0-or-later or the documented GPL-2.0-or-later/CC-BY-SA-4.0 choice. These notices must not be removed.

The released Changfeng road network was produced from OpenStreetMap data. OpenStreetMap data is available under the Open Database License (ODbL) 1.0 and requires OpenStreetMap contributor attribution. See <https://www.openstreetmap.org/copyright>.

No repository-wide license is granted by this section until the repository owner selects and adds one.
