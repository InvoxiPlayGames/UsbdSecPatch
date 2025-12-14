#!/bin/bash
# Set PPC_TOOLCHAIN to the path of your GCC PowerPC toolchain, including executable prefix - e.g. C:\gcc_ppc\bin\powerpc-eabi
# failkit works

# Create an output directory to store the patch files
mkdir -p build

for i in retail_17559 devkit_17489 retail_15574 retail_13604 retail_9199 retail_7258 retail_6717; do
   $PPC_TOOLCHAIN-as -mregnames --defsym $i=1 UsbdSecPatchXeBuild.s -o build/UsbdSecPatchXeBuild_$i.bin
   $PPC_TOOLCHAIN-objcopy build/UsbdSecPatchXeBuild_$i.bin -S -O binary
done
