# UsbdSecPatch.s - the UsbdSec patch in XeBuild format
.set KERNEL_VIRT_BASE, 0x80000000

# function patch addresses
.ifdef retail_6717
	.set UsbdIsDeviceAuthenticated, 0x800C5950
	.set WgcAddDevice_bne, 0x800E48A4
	.set WgcAddDevice_BranchLocation, 0x4
.endif

.ifdef retail_7258
	.set UsbdIsDeviceAuthenticated, 0x800C7128
	.set WgcAddDevice_bne, 0x800E618C
	.set WgcAddDevice_BranchLocation, 0x4
.endif

.ifdef retail_9199
	.set UsbdIsDeviceAuthenticated, 0x800CF280
	.set WgcAddDevice_bne, 0x800ED7DC
	.set WgcAddDevice_BranchLocation, 0x4
.endif

.ifdef retail_13604
	.set UsbdIsDeviceAuthenticated, 0x800D8A78
	.set WgcAddDevice_bne, 0x800F75D0
	.set WgcAddDevice_BranchLocation, 0x10
.endif

.ifdef retail_15574
	.set UsbdIsDeviceAuthenticated, 0x800D90D8
	.set WgcAddDevice_bne, 0x800F9BD0
	.set WgcAddDevice_BranchLocation, 0x10
.endif

.ifdef devkit_17489
	.set UsbdIsDeviceAuthenticated, 0x801051B8
	.set WgcAddDevice_bne, 0x801341F4
	.set WgcAddDevice_BranchLocation, 0x10
	.set WgcBindToUserNop, 0x801331F0
.endif

.ifdef retail_17559
	.set UsbdIsDeviceAuthenticated, 0x800D8748
	.set WgcAddDevice_bne, 0x800F98E0
	.set WgcAddDevice_BranchLocation, 0x10
.endif

# patches UsbdIsDeviceAuthenticated to report all devices as authenticated
# patch header
.long UsbdIsDeviceAuthenticated - KERNEL_VIRT_BASE # patch address
.long (UsbdAuthPatchEnd - UsbdAuthPatchStart) / 4 # patch length in number of DWORDs
# patch itself - returns true
UsbdAuthPatchStart:
li r3, 1
blr
UsbdAuthPatchEnd:

# patches inside WgcAddDevice to allow for more leniency on the interface descriptor
# patch header
.long WgcAddDevice_bne - KERNEL_VIRT_BASE # patch address
.long (WgcIntPatchEnd - WgcIntPatchStart) / 4 # patch length in number of DWORDs
# patch itself - replaces a bne cr6, 0x10 with b 0x10 after UsbdGetEndpointDescriptor(device, 0, 3, 1)
WgcIntPatchStart:
b WgcAddDevice_BranchLocation
WgcIntPatchEnd:

.ifdef devkit_17489
.long WgcBindToUserNop - KERNEL_VIRT_BASE # patch address
.long (WgcBindToUserNopEnd - WgcBindToUserNopStart) / 4 # patch length in number of DWORDs
# patch itself - NOPs out an assertion in the kernel to allow unauthorized Xinput devices
# Specifically hit this with the Mayflash Magic-NS
WgcBindToUserNopStart:
nop
WgcBindToUserNopEnd:
.endif
