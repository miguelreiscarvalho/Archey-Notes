param([string[]]$AppArguments)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$env:XOURNAL_LAB_ROOT = $root
$env:PYTHONUTF8 = '1'
Remove-Item Env:ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue
$inkTarget = Join-Path $root 'tools\tracos-engine\Microsoft.Ink.dll'
if (!(Test-Path -LiteralPath $inkTarget)) {
    foreach ($programRoot in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if (!$programRoot) { continue }
        $ink = Join-Path $programRoot 'Common Files\microsoft shared\ink\Microsoft.Ink.dll'
        if (Test-Path -LiteralPath $ink) { Copy-Item -LiteralPath $ink -Destination $inkTarget; break }
    }
}
$binary = Join-Path $root 'tools\study-ui\node_modules\electron\dist\electron.exe'
if (!(Test-Path -LiteralPath $binary)) { throw 'Extraia o ZIP completo do Archey Notes antes de abrir.' }
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = $binary
$start.Arguments = '"' + (Join-Path $root 'tools\study-ui\app') + '"'
if ($AppArguments) { $start.Arguments += ' ' + ($AppArguments -join ' ') }
$start.WorkingDirectory = $root
$start.UseShellExecute = $false
[System.Diagnostics.Process]::Start($start) | Out-Null
