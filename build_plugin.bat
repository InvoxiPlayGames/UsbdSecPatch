@echo off
setlocal enabledelayedexpansion

REM Set compilation variables.
set OUTNAME=UsbdSecPatch
set SOURCEFILE=UsbdSecPatchPlugin
set BUILD_DIR=build

REM Set the SDK paths to use.
set INCLUDE=%XEDK%\include\xbox
set LIB=%XEDK%\lib\xbox
set BINARYPATH=%XEDK%\bin\win32

REM Create folder
@mkdir %BUILD_DIR% >NUL

REM Compile source code.
"%BINARYPATH%\cl.exe" /c /Zi /nologo /W3 /WX- /Ox /Os /D NDEBUG /D _XBOX /GF /Gm- /MT /GS- /Gy /fp:fast /fp:except- /Zc:wchar_t /Zc:forScope /GR- /openmp- /Fo"%BUILD_DIR%/" /Fd"%BUILD_DIR%/" /FI"%XEDK%/include/xbox/xbox_intellisense_platform.h" /TP %SOURCEFILE%.cpp

REM Link the DLL file
echo Linking DLL...
"%BINARYPATH%\link.exe" /ERRORREPORT:PROMPT /OUT:"%BUILD_DIR%/%OUTNAME%.exe" /INCREMENTAL:NO /NOLOGO xapilib.lib xboxkrnl.lib /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /DEBUG /PDB:"%BUILD_DIR%/%OUTNAME%.pdb" /STACK:"262144","262144" /OPT:REF /OPT:ICF /TLBID:1 /RELEASE /IMPLIB:"%BUILD_DIR%/%OUTNAME%.lib" %BUILD_DIR%/%SOURCEFILE%.obj /ignore:4089 /dll /entry:"_DllMainCRTStartup" /ALIGN:128,4096 /XEX:NO

REM Create a final XEXDLL
echo Creating XEXDLL...
"%BINARYPATH%\imagexex.exe" /nologo /config:"xex.xml" /out:"%OUTNAME%.xex" "%BUILD_DIR%/%OUTNAME%.exe"

endlocal