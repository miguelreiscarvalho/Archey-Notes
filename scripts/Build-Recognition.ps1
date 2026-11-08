param([string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build\recognition'))
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '..\recognition'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$ink = Join-Path $env:ProgramFiles 'Common Files\microsoft shared\ink\Microsoft.Ink.dll'
foreach ($item in @($compiler,$ink)) { if (!(Test-Path -LiteralPath $item)) { throw "Dependência ausente: $item" } }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$references = @('System.dll','System.Core.dll','System.Drawing.dll','System.Runtime.Serialization.dll',$ink) | ForEach-Object { '/r:' + $_ }
$sources = @('RecognitionHost.cs','Model.cs','WindowsInkEngine.cs','CharacterCoverage.cs') | ForEach-Object { Join-Path $source $_ }
& $compiler /nologo /target:exe /platform:x64 ("/out:" + (Join-Path $OutputDirectory 'TracosRecognitionHost.exe')) $references $sources
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o adaptador Traços.' }
