# Regenerates IntelMSR_blob.h from an IntelMSR.bin module.
#
# The blob is the official signed PawnIO IntelMSR module, embedded in the DLL
# so there is no side file to lose. To update it after a module release:
#
#   1. Download release_<ver>.zip from
#      https://github.com/namazso/PawnIO.Modules/releases
#   2. Extract IntelMSR.bin next to this script
#   3. Run: pwsh -NoProfile -File gen_blob.ps1
#   4. Rebuild with build.bat
#
# Only the official signed module works: the PawnIO driver signature-checks
# blobs, so a modified one will fail pawnio_load.

$ErrorActionPreference = 'Stop'

$dir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$bin   = Join-Path $dir 'IntelMSR.bin'
$out   = Join-Path $dir 'IntelMSR_blob.h'

if (-not (Test-Path $bin)) {
    Write-Host "IntelMSR.bin not found next to this script."
    Write-Host "Download it from https://github.com/namazso/PawnIO.Modules/releases"
    exit 1
}

$bytes = [System.IO.File]::ReadAllBytes($bin)
Write-Host ("IntelMSR.bin: " + $bytes.Length + " bytes")

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("/* Auto-generated from IntelMSR.bin - do not edit by hand.")
[void]$sb.AppendLine(" * Regenerate with gen_blob.ps1.")
[void]$sb.AppendLine(" *")
[void]$sb.AppendLine(" * Module: PawnIO IntelMSR")
[void]$sb.AppendLine(" * Source: https://github.com/namazso/PawnIO.Modules/releases")
[void]$sb.AppendLine(" * License: LGPL-2.1-or-later")
[void]$sb.AppendLine(" */")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("#ifndef SIMPLIC_INTELMSR_BLOB_H")
[void]$sb.AppendLine("#define SIMPLIC_INTELMSR_BLOB_H")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("static const unsigned char gIntelMsrBlob[] = {")

for ($i = 0; $i -lt $bytes.Length; $i += 12) {
    $end = [Math]::Min($i + 12, $bytes.Length)
    $chunk = @()
    for ($j = $i; $j -lt $end; $j++) {
        $chunk += ("0x{0:X2}," -f $bytes[$j])
    }
    [void]$sb.AppendLine("    " + ($chunk -join ' '))
}

[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine(("static const unsigned int gIntelMsrBlobSize = {0};" -f $bytes.Length))
[void]$sb.AppendLine("")
[void]$sb.AppendLine("#endif /* SIMPLIC_INTELMSR_BLOB_H */")
[void]$sb.AppendLine("")

[System.IO.File]::WriteAllText($out, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("wrote " + $out)

# Verify the element count round-trips
$txt = [System.IO.File]::ReadAllText($out)
$n = ([regex]::Matches($txt, '0x[0-9A-F]{2}')).Count
Write-Host ("array elements: " + $n + " (expected " + $bytes.Length + ")")
if ($n -ne $bytes.Length) { Write-Host "MISMATCH"; exit 1 }
Write-Host "ok"