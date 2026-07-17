param(
    [string]$TimingCsv = "",
    [string]$OmnetIni = ""
)

if ([string]::IsNullOrWhiteSpace($TimingCsv)) {
    $TimingCsv = Join-Path $PSScriptRoot "scheme_timings.csv"
}
if ([string]::IsNullOrWhiteSpace($OmnetIni)) {
    $OmnetIni = Join-Path (Split-Path $PSScriptRoot -Parent) "scenarios\changfeng-northwest-2km\omnetpp.ini"
}

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $TimingCsv)) {
    throw "Timing CSV not found: $TimingCsv"
}
if (-not (Test-Path -LiteralPath $OmnetIni)) {
    throw "omnetpp.ini not found: $OmnetIni"
}

$schemeToBase = @{
    "ADVLGS" = "ADVLGSBase"
    "A-DVLGS" = "ADVLGSBase"
    "BBS" = "BBSBase"
    "CLGS" = "CLGSBase"
    "MLGS" = "MLGSBase"
    "ERCA" = "ERCABase"
}

function Read-Ms {
    param([string]$Value, [string]$Name, [string]$Scheme)
    if ([string]::IsNullOrWhiteSpace($Value)) {
        throw "$Name is empty for scheme $Scheme"
    }
    $clean = $Value.Trim() -replace "ms$", ""
    return [double]::Parse($clean, [System.Globalization.CultureInfo]::InvariantCulture)
}

function To-SecondsLiteral {
    param([double]$Milliseconds)
    $seconds = $Milliseconds / 1000.0
    return $seconds.ToString("0.#########", [System.Globalization.CultureInfo]::InvariantCulture) + "s"
}

function Normalize-BatchTable {
    param($Row)
    $entries = New-Object System.Collections.Generic.List[string]

    if ($Row.PSObject.Properties.Name -contains "batch_verify_by_size_ms" -and
        -not [string]::IsNullOrWhiteSpace($Row.batch_verify_by_size_ms)) {
        foreach ($part in ($Row.batch_verify_by_size_ms -split "[;,]")) {
            if ([string]::IsNullOrWhiteSpace($part)) { continue }
            $pieces = $part.Trim() -split ":", 2
            if ($pieces.Count -ne 2) {
                throw "Bad batch entry '$part' for scheme $($Row.scheme). Use size:milliseconds"
            }
            $size = [int]$pieces[0].Trim()
            $msText = ($pieces[1].Trim() -replace "ms$", "")
            $ms = [double]::Parse($msText, [System.Globalization.CultureInfo]::InvariantCulture)
            $entries.Add(("{0}:{1}ms" -f $size, $ms.ToString("0.######", [System.Globalization.CultureInfo]::InvariantCulture)))
        }
    }
    else {
        foreach ($prop in $Row.PSObject.Properties) {
            if ($prop.Name -match "^batch_?(\d+)_ms$" -and -not [string]::IsNullOrWhiteSpace([string]$prop.Value)) {
                $size = [int]$matches[1]
                $ms = [double]::Parse(([string]$prop.Value).Trim(), [System.Globalization.CultureInfo]::InvariantCulture)
                $entries.Add(("{0}:{1}ms" -f $size, $ms.ToString("0.######", [System.Globalization.CultureInfo]::InvariantCulture)))
            }
        }
    }

    if ($entries.Count -eq 0) { return "" }
    return ($entries | Sort-Object { [int](($_ -split ":", 2)[0]) }) -join ","
}

function Set-Or-Append-Line {
    param([string]$Block, [string]$LineRegex, [string]$NewLine)
    $options = [System.Text.RegularExpressions.RegexOptions]::Multiline
    if ([regex]::IsMatch($Block, $LineRegex, $options)) {
        return [regex]::Replace($Block, $LineRegex, $NewLine, $options)
    }
    if ($Block.EndsWith("`n")) {
        return $Block + $NewLine + "`n"
    }
    return $Block + "`n" + $NewLine
}

$rows = Import-Csv -LiteralPath $TimingCsv
if (@($rows).Count -eq 0) {
    throw "Timing CSV has no rows: $TimingCsv"
}

$content = Get-Content -LiteralPath $OmnetIni -Raw
$backup = "$OmnetIni.timing-backup-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
Copy-Item -LiteralPath $OmnetIni -Destination $backup -Force

$applied = New-Object System.Collections.Generic.List[object]
foreach ($row in $rows) {
    $schemeKey = ([string]$row.scheme).Trim().ToUpperInvariant()
    if (-not $schemeToBase.ContainsKey($schemeKey)) {
        throw "Unknown scheme '$($row.scheme)' in $TimingCsv"
    }

    $baseName = $schemeToBase[$schemeKey]
    $signSeconds = To-SecondsLiteral (Read-Ms ([string]$row.sign_ms) "sign_ms" $row.scheme)
    $verifySeconds = To-SecondsLiteral (Read-Ms ([string]$row.verify_ms) "verify_ms" $row.scheme)
    $batchTable = Normalize-BatchTable $row

    $sectionPattern = "(?ms)(\[Config $([regex]::Escape($baseName))\]\r?\n)(.*?)(?=\r?\n\[Config |\z)"
    if (-not [regex]::IsMatch($content, $sectionPattern)) {
        throw "Config section [$baseName] was not found in $OmnetIni"
    }
    $content = [regex]::Replace($content, $sectionPattern, {
        param($m)
        $header = $m.Groups[1].Value
        $block = $m.Groups[2].Value
        $block = Set-Or-Append-Line $block '^\*\.node\[\*\]\.appl\.fixedSignDelay\s*=.*$' "*.node[*].appl.fixedSignDelay = $signSeconds"
        $block = Set-Or-Append-Line $block '^\*\.node\[\*\]\.appl\.fixedVerifyDelay\s*=.*$' "*.node[*].appl.fixedVerifyDelay = $verifySeconds"
        $block = Set-Or-Append-Line $block '^\*\.rsu\[\*\]\.appl\.fixedVerifyDelay\s*=.*$' "*.rsu[*].appl.fixedVerifyDelay = $verifySeconds"
        if (-not [string]::IsNullOrWhiteSpace($batchTable)) {
            $block = Set-Or-Append-Line $block '^\*\.rsu\[\*\]\.appl\.fixedBatchVerifyDelayBySize\s*=.*$' "*.rsu[*].appl.fixedBatchVerifyDelayBySize = `"$batchTable`""
        }
        return $header + $block
    })

    $applied.Add([pscustomobject]@{
        scheme = $row.scheme
        baseConfig = $baseName
        sign = $signSeconds
        verify = $verifySeconds
        batch = $batchTable
    })
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($OmnetIni, $content, $utf8NoBom)
$applied | Format-Table -AutoSize
"Backup: $backup"
