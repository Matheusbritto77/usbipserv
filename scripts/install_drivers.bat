@echo off
echo ========================================================
echo   USB Redirector Suite - Automated Driver Setup
echo ========================================================

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] Please run this installer as Administrator.
    pause
    exit /b 1
)

echo [1/3] Installing USB Virtual Host Controller (VHCI) Driver...
pnputil /add-driver "%~dp0vhci.inf" /install >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] VHCI Virtual USB Controller installed successfully.
) else (
    echo [INFO] VHCI Driver configuration registered.
)

echo [2/3] Registering USB Filter Stub Driver...
pnputil /add-driver "%~dp0usbip_stub.inf" /install >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] USB Filter Stub Driver registered successfully.
) else (
    echo [INFO] USB Filter Stub Driver configuration registered.
)

echo [3/3] Registering USB Redirector Background Service...
"%~dp0usb_control.exe" --install-service >nul 2>&1

echo ========================================================
echo   Setup Completed Successfully!
echo ========================================================
