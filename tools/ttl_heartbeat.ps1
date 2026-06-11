param(
  [Parameter(Mandatory = $true)]
  [string]$Port,

  [int]$BaudRate = 115200,
  [int]$IntervalMs = 1000
)

$ErrorActionPreference = "Stop"

$serial = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$serial.ReadTimeout = 200
$serial.WriteTimeout = 200

try {
  $serial.Open()
  Write-Host "Keepalive activo en $Port a $BaudRate bps (cada $IntervalMs ms). Ctrl+C para salir."
  Write-Host "AVISO: Si cierras esta ventana, el buck se apagara en 5s."

  while ($true) {
    # Envia "HB" texto: unico mecanismo que resetea el watchdog del MCU
    $serial.WriteLine("HB")
    Start-Sleep -Milliseconds $IntervalMs
  }
}
finally {
  if ($serial -and $serial.IsOpen) {
    $serial.Close()
  }
}
