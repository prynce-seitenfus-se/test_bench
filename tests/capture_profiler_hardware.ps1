[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $OutputDirectory -PathType Container)) {
    throw "Output directory does not exist: $OutputDirectory"
}
$serial = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout = 1000
$serial.WriteTimeout = 1000
try {
    $serial.Open()
    $serial.DiscardInBuffer()
    $serial.Write("prof-status`r`n")
    Start-Sleep -Seconds 1
    $status = $serial.ReadExisting()
    if ($status -notmatch 'Enabled:\s+YES') {
        throw "Profiler did not report enabled status: $status"
    }
    Write-Output $status
    $serial.Write("prof-top`r`n")
    Start-Sleep -Seconds 2
    $top = $serial.ReadExisting()
    if ($top -notmatch 'PROFILER RUNTIME METRICS') {
        throw 'Profiler did not return a TOP table'
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'instrumentation_top.log'), $top)
    Write-Output $top
    $serial.DiscardInBuffer()
    $serial.Write("prof-dump`r`n")
    $data = New-Object 'System.Collections.Generic.List[byte]'
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    $expected = 32
    while ([DateTime]::UtcNow -lt $deadline) {
        $available = $serial.BytesToRead
        if ($available -gt 0) {
            $chunk = New-Object byte[] $available
            $read = $serial.Read($chunk, 0, $available)
            for ($i = 0; $i -lt $read; $i++) {
                $data.Add($chunk[$i])
            }
        }
        if ($data.Count -ge 28) {
            $bytes = $data.ToArray()
            if ([BitConverter]::ToUInt32($bytes, 0) -ne 0x464F5250 -or
                [BitConverter]::ToUInt16($bytes, 4) -ne 4) {
                throw 'Serial response is not a PROF-BIN v4 packet'
            }
            $expected = 32 + 24 * [BitConverter]::ToUInt16($bytes, 6)
            if ($data.Count -ge $expected) {
                break
            }
        }
        Start-Sleep -Milliseconds 20
    }
    if ($data.Count -ne $expected) {
        throw "Incomplete profiler packet: expected $expected bytes, received $($data.Count)"
    }
    [IO.File]::WriteAllBytes((Join-Path $OutputDirectory 'instrumentation_capture.bin'), $data.ToArray())
    Write-Output "Captured $expected bytes; run parse_prof_dump.py to validate CRC and resolve symbols."
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
    $serial.Dispose()
}
