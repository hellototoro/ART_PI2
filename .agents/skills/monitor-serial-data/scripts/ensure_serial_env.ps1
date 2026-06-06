param(
    [string]$VenvPath = ".venv",
    [string]$ExamplePort = "COMx",
    [int]$Baud = 115200
)

$ErrorActionPreference = "Stop"

function Get-ProjectRoot {
    $scriptPath = $PSCommandPath
    if (-not $scriptPath) {
        return (Get-Location).Path
    }

    $scriptDir = Split-Path -Parent $scriptPath
    return (Resolve-Path (Join-Path $scriptDir "..\..\..\..")).Path
}

$projectRoot = Get-ProjectRoot
Set-Location $projectRoot

$venvPython = Join-Path $VenvPath "Scripts\python.exe"
$uvCommand = Get-Command uv -ErrorAction SilentlyContinue

if (-not (Test-Path -LiteralPath $venvPython)) {
    if ($uvCommand) {
        Write-Host "Creating virtual environment with uv: $VenvPath"
        uv venv $VenvPath
    } else {
        $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
        if (-not $pythonCommand) {
            throw "No project virtual environment found, uv is unavailable, and python is not on PATH."
        }

        Write-Host "Creating virtual environment with python: $VenvPath"
        python -m venv $VenvPath
    }
} else {
    Write-Host "Using existing virtual environment: $VenvPath"
}

if (-not (Test-Path -LiteralPath $venvPython)) {
    throw "Virtual environment Python was not found at $venvPython"
}

& $venvPython -c "import serial" 2>$null
if ($LASTEXITCODE -ne 0) {
    if ($uvCommand) {
        Write-Host "Installing pyserial with uv"
        uv pip install --python $venvPython pyserial
    } else {
        Write-Host "Installing pyserial with pip"
        & $venvPython -m pip install pyserial
    }
} else {
    $version = & $venvPython -c "import serial; print(serial.VERSION)"
    Write-Host "pyserial is already installed: $version"
}

$monitorScript = ".\.agents\skills\monitor-serial-data\scripts\monitor_serial.py"
Write-Host ""
Write-Host "Ready. Example command:"
Write-Host "$venvPython $monitorScript --port $ExamplePort --baud $Baud"
