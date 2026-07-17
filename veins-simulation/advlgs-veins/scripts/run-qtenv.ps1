param(
    [string]$Config = "ADVLGS_CHANGFENG_D5_F1_R0",
    [ValidateRange(0, 4)][int]$Run = 0,
    [string]$InstallRoot = $env:VEINS_INSTALL_ROOT
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
    throw "Set VEINS_INSTALL_ROOT or pass -InstallRoot."
}

$ProjectRoot = Split-Path $PSScriptRoot -Parent
$ScenarioRoot = Join-Path $ProjectRoot "scenarios\changfeng-northwest-2km"
$OmnetRoot = Join-Path $InstallRoot "omnetpp-6.1"
$VeinsRoot = Join-Path $InstallRoot "veins-veins-5.3.1"
$SumoRoot = Join-Path $InstallRoot "sumo-1.22.0"
$OppRun = Join-Path $OmnetRoot "bin\opp_run_release.exe"
$Ini = Join-Path $ScenarioRoot "omnetpp.ini"

if (-not (Test-Path -LiteralPath $OppRun)) {
    throw "opp_run_release.exe not found: $OppRun"
}
if (-not (Test-Path -LiteralPath $Ini)) {
    throw "omnetpp.ini not found: $Ini"
}
if (-not (Select-String -LiteralPath $Ini -Pattern "[Config $Config]" -SimpleMatch -Quiet)) {
    throw "Unknown simulation configuration: $Config"
}

$listener = Get-NetTCPConnection -LocalPort 9999 -State Listen -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $listener) {
    $python = Get-Command python.exe -ErrorAction Stop
    $launchd = Join-Path $VeinsRoot "sumo-launchd.py"
    $sumoExe = Join-Path $SumoRoot "bin\sumo.exe"
    if (-not (Test-Path -LiteralPath $launchd)) { throw "sumo-launchd.py not found: $launchd" }
    if (-not (Test-Path -LiteralPath $sumoExe)) { throw "sumo.exe not found: $sumoExe" }

    Start-Process -FilePath $python.Source -ArgumentList @($launchd, "-vv", "-c", $sumoExe) -WindowStyle Hidden | Out-Null
    $deadline = (Get-Date).AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 300
        $listener = Get-NetTCPConnection -LocalPort 9999 -State Listen -ErrorAction SilentlyContinue |
            Select-Object -First 1
    } until ($listener -or (Get-Date) -gt $deadline)
}
if (-not $listener) {
    throw "SUMO TraCI launchd did not become ready on port 9999."
}

$env:SUMO_HOME = $SumoRoot
$env:PATH = @(
    $ProjectRoot,
    (Join-Path $VeinsRoot "src"),
    (Join-Path $VeinsRoot "out\clang-release\src"),
    (Join-Path $SumoRoot "bin"),
    (Join-Path $OmnetRoot "bin"),
    (Join-Path $OmnetRoot "tools\win32.x86_64\opt\mingw64\bin"),
    (Join-Path $OmnetRoot "tools\win32.x86_64\mingw64\bin"),
    $env:PATH
) -join ";"

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultDir = Join-Path $ScenarioRoot "results\qtenv-$stamp"
New-Item -ItemType Directory -Force -Path $resultDir | Out-Null

$libraryBase = (Join-Path $ProjectRoot "advlgs_veins").Replace("\", "/")
$veinsLibrary = (Join-Path $VeinsRoot "src\veins").Replace("\", "/")
$nedPath = "../../src;.;$veinsLibrary"
$scalarPath = (Join-Path $resultDir '${configname}-${runnumber}.sca').Replace("\", "/")
$vectorPath = (Join-Path $resultDir '${configname}-${runnumber}.vec').Replace("\", "/")

$arguments = @(
    "-m", "-u", "Qtenv", "-f", "omnetpp.ini",
    "-l", $libraryBase,
    "-n", $nedPath, "-c", $Config, "-r", "$Run",
    "--debug-on-errors=false",
    "--output-scalar-file=$scalarPath",
    "--output-vector-file=$vectorPath"
)

Push-Location $ScenarioRoot
try {
    & $OppRun @arguments
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
