param(
    [string]$LabRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$Version = '1.3.8',
    [string]$Document,
    [switch]$Stock
)
$ErrorActionPreference = 'Stop'
$appRoot = Join-Path $LabRoot "app\$Version\xournalpp-$Version-windows-portable-AMD64"
$executable = Join-Path $appRoot 'bin\xournalpp.exe'
$runtimeFile = Join-Path $LabRoot 'profile\editor-runtime.json'
if (!$Stock -and (Test-Path -LiteralPath $runtimeFile)) {
    $runtime = Get-Content -LiteralPath $runtimeFile -Raw | ConvertFrom-Json
    if ($runtime.version -ne 1 -or !$runtime.executable) { throw 'Configuracao do editor invalida.' }
    $resolvedBinary = [IO.Path]::GetFullPath((Join-Path $LabRoot $runtime.executable))
    $allowedRoot = [IO.Path]::GetFullPath($LabRoot).TrimEnd('\') + '\'
    if (!$resolvedBinary.StartsWith($allowedRoot,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($resolvedBinary) -ne 'xournalpp.exe') { throw 'O executavel deve pertencer ao laboratorio.' }
    $executable = $resolvedBinary
}
if (!(Test-Path -LiteralPath $executable)) { throw "Executavel ausente: $executable" }
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $executable
$startInfo.UseShellExecute = $false
$startInfo.WorkingDirectory = Join-Path $LabRoot 'documents'
foreach ($entry in @{
    XDG_CONFIG_HOME = 'profile\config'
    XDG_CACHE_HOME = 'profile\cache'
    XDG_DATA_HOME = 'profile\data'
    XDG_STATE_HOME = 'profile\state'
    TEMP = 'profile\temp'
    TMP = 'profile\temp'
}.GetEnumerator()) {
    $directory = Join-Path $LabRoot $entry.Value
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $startInfo.EnvironmentVariables[$entry.Key] = $directory
}
New-Item -ItemType Directory -Force -Path $startInfo.WorkingDirectory | Out-Null
if ($Document) {
    $resolvedDocument = (Resolve-Path -LiteralPath $Document).Path
    $startInfo.Arguments = '"' + $resolvedDocument + '"'
}
$startInfo.EnvironmentVariables['PATH'] = (Join-Path $LabRoot 'tools\TinyTeX\bin\windows') + ';' + $env:PATH
$startInfo.EnvironmentVariables['XOURNAL_LAB_ROOT'] = $LabRoot
$startInfo.EnvironmentVariables['XOURNAL_TRACOS_WORKER'] = Join-Path $LabRoot 'tools\tracos-engine\TracosRecognitionHost.exe'
$process = [System.Diagnostics.Process]::Start($startInfo)
Write-Output "Xournal++ Lab $Version iniciado (PID $($process.Id))."
