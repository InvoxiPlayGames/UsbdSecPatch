@echo off
REM Set PPC_TOOLCHAIN to the path of your GCC PowerPC toolchain, including executable prefix - e.g. C:\gcc_ppc\bin\powerpc-eabi
REM failkit works
%PPC_TOOLCHAIN%-as -mregnames UsbdSecPatchXeBuild.s -o UsbdSecPatchXeBuild.bin
%PPC_TOOLCHAIN%-objcopy UsbdSecPatchXeBuild.bin -S -O binary