# A-DVLGS Batch Benchmark for Visual Studio 2022

This project measures the online service-authentication costs of A-DVLGS and the comparison schemes BBS, CLGS, MLGS, and ERCA using MIRACL Core BLS12-381.

## Build

Open `advlgs-benchmark-vs2022.sln` in Visual Studio 2022.

Recommended settings:

- `Release`
- `x64`
- C++17

Configure `MIRACL_CORE_PROPS` to point to a local Visual Studio `.props` file with the MIRACL Core include and library paths.

To export `scheme_timings.csv` and `scheme_timings.done` directly for the Veins scripts, set `VEINS_TIMING_DIR` to `veins-simulation/advlgs-veins/scripts`. If it is not set, the files are written to the benchmark working directory.

## Default Run

Run:

```text
advlgs-benchmark.exe
```

The default run performs:

- Five-scheme single signing and single verification.
- A-DVLGS batch verification for batch sizes `1..16`.
- A-DVLGS large-batch verification for `1000,2000,3000,4000,5000`.
- Veins timing export.

Default settings:

- Single signing and single verification: `10000` rounds per scheme.
- A-DVLGS small-batch verification: `10000` rounds per batch size.
- A-DVLGS large-batch verification: `100` rounds per batch size.
- Maximum small batch size: `16`.
- Message body: `128 B`.
- Curve/library: MIRACL Core BLS12-381 Type-3 pairing.

## Output Files

The benchmark writes:

- `five_scheme_single_summary.csv`
- `five_scheme_single_rounds.csv`
- `advlgs_batch_summary.csv`
- `advlgs_batch_rounds.csv`
- `advlgs_large_batch_summary.csv`
- `advlgs_large_batch_rounds.csv`
- `scheme_timings.csv`
- `scheme_timings.done`

`scheme_timings.csv` is used by the Veins simulation scripts. It contains measured single signing and verification times for all schemes, and the A-DVLGS batch-verification table for batch sizes `2..14`.

## Multi-Message Verification Interpretation

A-DVLGS implements a real batch verifier. For a batch of `n` service requests, the timing region includes proof reconstruction and the final aggregated pairing check. Member credentials and signatures are prepared before timing so that certificate issuance and signing are not counted in batch-verification time.

BBS, CLGS, MLGS, and ERCA are comparison schemes without a native batch verifier in this codebase. For these four schemes, a queued group of `n` requests is modeled as sequential single-message verification:

```text
projected_time(n) = n * average_single_verify_time
```

This rule is used for `1..16` queued-message timings and for large message counts `1000,2000,3000,4000,5000`. It is not a cryptographic batch-verification algorithm for those schemes.

## Veins Timing Export

The Veins simulation uses fixed cryptographic delays instead of recomputing pairings inside every packet event. The benchmark exports these delays in `scheme_timings.csv`.

The RSU-side simulation uses a `20 ms` batch waiting window. During this window, an LV may receive multiple service requests. If the queue contains `n` requests, the simulation needs a verification delay for `n` messages:

- A-DVLGS uses measured `batchVerify()` timing.
- BBS, CLGS, MLGS, and ERCA use `n` sequential single-message verifications.

The benchmark measures A-DVLGS batch sizes `1..16`; the current Veins configuration uses `batchMaxSize = 14`, so the exported timing table covers the active simulation range.
