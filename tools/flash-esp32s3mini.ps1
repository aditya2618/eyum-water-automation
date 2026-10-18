param(
  [Parameter(Mandatory = $true)]
  [string]$Port
)

& "$PSScriptRoot\flash-esp32c3mini.ps1" -Port $Port
exit $LASTEXITCODE
