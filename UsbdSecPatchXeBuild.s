# UsbdSecPatch.s - the UsbdSec patch in XeBuild format
.set KERNEL_VIRT_BASE, 0x80000000

# function patch addresses as of 17559 retail
# and 17489 devkit
.ifdef devkit_17489
	.set UsbdIsDeviceAuthenticated, 0x801051b8
	.set WgcAddDevice_bne, 0x801341f4
.else
	.set UsbdIsDeviceAuthenticated, 0x800d8748
	.set WgcAddDevice_bne, 0x800f98e0
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
b 0x10
WgcIntPatchEnd:
