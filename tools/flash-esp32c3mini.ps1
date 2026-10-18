param(
  [Parameter(Mandatory = $true)]
  [string]$Port
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
  $candidate = Get-ChildItem "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe", "$env:APPDATA\Python\Python*\Scripts\pio.exe", "$env:LOCALAPPDATA\Programs\Python\Python*\Scripts\pio.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($candidate) {
    $env:Path = "$($candidate.DirectoryName);" + $env:Path
  }
}

if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
  throw "PlatformIO Core command 'pio' was not found. Install PlatformIO Core, reopen PowerShell, then run this script again."
}

Write-Host "Flashing iYum Water Automation to ESP32-C3 Super Mini on $Port..." -ForegroundColor Cyan
pio run -e esp32c3mini -t upload --upload-port $Port
exit $LASTEXITCODE
