param(
  [ValidateSet("esp32", "esp32_controller", "esp32c3mini", "esp01_relay", "all")]
  [string]$Target = "all"
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

if ($Target -eq "all") {
  pio run -e esp32_controller
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  pio run -e esp32c3mini
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  pio run -e esp01_relay
  exit $LASTEXITCODE
}

pio run -e $Target
exit $LASTEXITCODE
