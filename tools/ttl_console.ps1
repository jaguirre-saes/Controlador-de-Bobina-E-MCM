param(
  [Parameter(Mandatory = $true)]
  [string]$Port,

  [int]$BaudRate = 115200,
  [int]$HeartbeatMs = 1000
)

$ErrorActionPreference = "Stop"

$serial = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$serial.NewLine = "`r`n"
$serial.ReadTimeout = 50
$serial.WriteTimeout = 200

try {
  $serial.Open()

  Write-Host "Consola TTL en $Port ($BaudRate bps)."
  Write-Host "Keepalive automatico cada $HeartbeatMs ms (silencioso)."
  Write-Host "Escribe comandos (ej: I 0.50, ON, OFF, READ)."
  Write-Host "Comandos locales: /exit, /help"
  Write-Host "AVISO: Si cierras esta ventana, el buck se apagara en 5s."
  Write-Host -NoNewline "> "

  $lineBuffer = New-Object System.Text.StringBuilder
  $lastHeartbeat = [System.DateTime]::UtcNow

  while ($true) {
    $now = [System.DateTime]::UtcNow
    if (($now - $lastHeartbeat).TotalMilliseconds -ge $HeartbeatMs) {
      try {
        # Envia "HB" texto: unico mecanismo que resetea el watchdog del MCU
        $serial.WriteLine("HB")
      }
      catch {
      }
      $lastHeartbeat = $now
    }

    try {
      if ($serial.BytesToRead -gt 0) {
        $incoming = $serial.ReadExisting()
        if (-not [string]::IsNullOrEmpty($incoming)) {
          $lines = $incoming -split '\r?\n'
          $filtered = ($lines | Where-Object { $_ -notmatch '^ALIVE$' }) -join "`r`n"
          if (-not [string]::IsNullOrEmpty($filtered.Trim())) {
            Write-Host -NoNewline "`r`n$filtered"
            Write-Host -NoNewline "> $($lineBuffer.ToString())"
          }
        }
      }
    }
    catch {
    }

    if ([System.Console]::KeyAvailable) {
      $key = [System.Console]::ReadKey($true)

      if ($key.Key -eq [System.ConsoleKey]::Enter) {
        Write-Host ""
        $cmd = $lineBuffer.ToString().Trim()
        $lineBuffer.Clear() | Out-Null

        if ($cmd.Length -eq 0) {
          Write-Host -NoNewline "> "
          continue
        }

        if ($cmd.Equals("/exit", [System.StringComparison]::OrdinalIgnoreCase)) {
          break
        }

        if ($cmd.Equals("/help", [System.StringComparison]::OrdinalIgnoreCase)) {
          Write-Host "Locales: /exit, /help"
          Write-Host "Dispositivo: I <A>, ON, OFF, READ, HELP, HB"
          Write-Host -NoNewline "> "
          continue
        }

        try {
          $serial.WriteLine($cmd)
          $lastHeartbeat = [System.DateTime]::UtcNow
        }
        catch {
          Write-Host "ERROR TX: $($_.Exception.Message)"
          break
        }

        Write-Host -NoNewline "> "
      }
      elseif ($key.Key -eq [System.ConsoleKey]::Backspace) {
        if ($lineBuffer.Length -gt 0) {
          $lineBuffer.Remove($lineBuffer.Length - 1, 1) | Out-Null
          Write-Host -NoNewline "`b `b"
        }
      }
      else {
        $ch = $key.KeyChar
        if (-not [char]::IsControl($ch)) {
          $lineBuffer.Append($ch) | Out-Null
          Write-Host -NoNewline $ch
        }
      }
    }
    else {
      Start-Sleep -Milliseconds 20
    }
  }
}
finally {
  if ($serial -and $serial.IsOpen) {
    $serial.Close()
  }
}
