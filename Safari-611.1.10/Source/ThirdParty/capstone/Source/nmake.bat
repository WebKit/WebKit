:: Capstone disassembler engine (www.capstone-engine.org)
:: Build Capstone libs (capstone.dll & capstone.lib) on Windows with CMake & Nmake
:: By Nguyen Anh Quynh, Jorn Vernee, 2017, 2019

@echo off

set flags="-DCMAKE_BUILD_TYPE=Release -DCAPSTONE_BUILD_STATIC_RUNTIME=ON"

if "%1"=="ARM" set %arch%=ARM
if "%1"=="ARM64" set %arch%=ARM64
if "%1"=="M68K" set %arch%=M68K
if "%1"=="MIPS" set %arch%=MIPS
if "%1"=="PowerPC" set %arch%=PPC
if "%1"=="Sparc" set %arch%=SPARC
if "%1"=="SystemZ" set %arch%=SYSZ
if "%1"=="XCore" set %arch%=XCORE
if "%1"=="x86" set %arch%=X86
if "%1"=="TMS320C64x" set %arch%=TMS320C64X
if "%1"=="M680x" set %arch%=M680X
if "%1"=="EVM" set %arch%=EVM
if "%1"=="MOS65XX" set %arch%=MOS65XX

if not "%arch%"=="" set flags=%flags% and " -DCAPSTONE_ARCHITECTURE_DEFAULT=OFF -DCAPSTONE_%arch%_SUPPORT=ON"

cmake %flags% -G "NMake Makefiles" ..
nmake

