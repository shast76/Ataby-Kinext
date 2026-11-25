@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
rem ============================================================================
rem  Kinext 1.1 - kurulum sihirbazi olusturucu
rem  1) Visual Studio'da kinext_dashboard.exe'yi Release olarak derleyin
rem  2) Inno Setup 6 ya da 7'yi kurun: https://jrsoftware.org/isdl.php
rem  3) Bu dosyaya cift tiklayin. Sonuc: installer\Output\Kinext-1.1-Setup.exe
rem ============================================================================

set "ROOT=%~dp0.."
set "BUILD=%ROOT%\out\build"
set "DIST=%ROOT%\dist"
set "HERE=%~dp0"

echo.
echo  Kinext kurulum sihirbazi hazirlaniyor...
echo.

rem --- 1. Derlenmis program var mi? ---
if not exist "%BUILD%\kinext_dashboard.exe" (
    echo [HATA] "%BUILD%\kinext_dashboard.exe" bulunamadi.
    echo        Once Visual Studio'da kinext_dashboard.exe'yi derleyin ^(Ctrl+Shift+B^).
    goto :fail
)

rem --- 2. dist klasorunu topla: sadece dashboard, DLL'ler, model ve gorseller ---
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%\models" "%DIST%\assets"
copy /y "%BUILD%\kinext_dashboard.exe" "%DIST%\" >nul
copy /y "%BUILD%\*.dll" "%DIST%\" >nul
copy /y "%ROOT%\models\*.onnx" "%DIST%\models\" >nul
xcopy /e /i /y /q "%ROOT%\assets" "%DIST%\assets" >nul
copy /y "%HERE%THIRD_PARTY_NOTICES.txt" "%DIST%\" >nul
if not exist "%DIST%\models\movenet_lightning.onnx" if not exist "%DIST%\models\movenet_thunder.onnx" (
    echo [HATA] models klasorunde MoveNet modeli yok.
    goto :fail
)
for /f %%c in ('dir /b "%DIST%\*.dll" ^| find /c /v ""') do echo  - %%c DLL, model ve gorseller "dist" klasorune toplandi

rem --- 3. Visual C++ calisma zamani (vc_redist.x64.exe) Visual Studio'nun icinden bulunur ---
if not exist "%HERE%redist" mkdir "%HERE%redist"
if not exist "%HERE%redist\vc_redist.x64.exe" (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -property installationPath`) do set "VSDIR=%%i"
        if exist "!VSDIR!\VC\Redist\MSVC" (
            pushd "!VSDIR!\VC\Redist\MSVC"
            for /r %%f in (vc_redist.x64.exe) do if exist "%%f" set "REDIST=%%f"
            popd
        )
    )
    if defined REDIST (
        copy /y "!REDIST!" "%HERE%redist\" >nul
        echo  - Visual C++ calisma zamani eklendi
    ) else (
        echo  [UYARI] vc_redist.x64.exe bulunamadi. Kurulum onsuz hazirlanacak;
        echo          Visual C++ kurulu olmayan bilgisayarlarda program acilmayabilir.
    )
) else (
    echo  - Visual C++ calisma zamani hazir
)

rem --- 4. Inno Setup derleyicisini bul ---
set "ISCC="
for %%p in ("%ProgramFiles%\Inno Setup 7\ISCC.exe" "%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe" "%ProgramFiles%\Inno Setup 6\ISCC.exe" "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" "%LocalAppData%\Programs\Inno Setup 7\ISCC.exe" "%LocalAppData%\Programs\Inno Setup 6\ISCC.exe") do (
    if not defined ISCC if exist %%p set "ISCC=%%~p"
)
if not defined ISCC (
    echo [HATA] Inno Setup bulunamadi. https://jrsoftware.org/isdl.php adresinden kurun ve tekrar deneyin.
    goto :fail
)
echo  - Inno Setup: !ISCC!
echo.

rem --- 5. Derle ---
pushd "%HERE%"
"!ISCC!" /Q kinext.iss
set "ERR=!errorlevel!"
popd
if not "!ERR!"=="0" (
    echo [HATA] Kurulum derlenemedi ^(hata kodu !ERR!^). Ayrintilar icin kinext.iss'i Inno Setup'ta acip derleyin.
    goto :fail
)

echo.
echo  TAMAM: %HERE%Output\Kinext-1.1-Setup.exe
echo.
explorer "%HERE%Output"
pause
exit /b 0

:fail
echo.
pause
exit /b 1
