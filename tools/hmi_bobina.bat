@echo off
rem Lanza el HMI del controlador de bobina. Uso: hmi_bobina.bat [PUERTO]  (por defecto COM3)
python "%~dp0hmi_bobina.py" %*
if errorlevel 1 pause
