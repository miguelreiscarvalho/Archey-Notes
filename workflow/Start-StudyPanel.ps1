$ErrorActionPreference = 'Stop'
$labRoot = Split-Path -Parent $PSScriptRoot
$uiRoot = Join-Path $labRoot 'tools\study-ui'
$executable = Join-Path $uiRoot 'node_modules\electron\dist\electron.exe'
if (!(Test-Path -LiteralPath $executable)) { throw 'Electron não encontrado. Execute Deploy-StudyUI.ps1.' }
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $executable
$startInfo.Arguments = '"' + (Join-Path $uiRoot 'app') + '"'
$startInfo.WorkingDirectory = $labRoot
$startInfo.UseShellExecute = $false
$startInfo.EnvironmentVariables.Remove('ELECTRON_RUN_AS_NODE')
$startInfo.EnvironmentVariables['PYTHONUTF8'] = '1'
$startInfo.EnvironmentVariables['XOURNAL_LAB_ROOT'] = $labRoot
[System.Diagnostics.Process]::Start($startInfo) | Out-Null
