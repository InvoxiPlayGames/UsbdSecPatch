@echo off
REM Set PPC_TOOLCHAIN to the path of your GCC PowerPC toolchain, including executable prefix - e.g. C:\gcc_ppc\bin\powerpc-eabi
REM failkit works
%PPC_TOOLCHAIN%-as -mregnames UsbdSecPatchXeBuild.s -o UsbdSecPatchXeBuild.bin
%PPC_TOOLCHAIN%-objcopy UsbdSecPatchXeBuild.bin -S -O binary
%PPC_TOOLCHAIN%-as -mregnames --defsym devkit_17489=1 UsbdSecPatchXeBuild.s -o UsbdSecPatchXeBuild17489.bin
%PPC_TOOLCHAIN%-objcopy UsbdSecPatchXeBuild17489.bin -S -O binary