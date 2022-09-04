# UsbdSecPatch.s - the UsbdSec patch in XeBuild format
.set KERNEL_VIRT_BASE, 0x80000000

# function patch addresses as of 17559 retail
.set UsbdIsDeviceAuthenticated, 0x800d8748
.set WgcAddDevice_bne, 0x800f98e0

# patches UsbdIsDeviceAuthenticated to report all devices as authenticated
# patch header
.long UsbdIsDeviceAuthenticated - KERNEL_VIRT_BASE # patch address
.long UsbdAuthPatchEnd - UsbdAuthPatchStart # patch length
# patch itself - returns true
UsbdAuthPatchStart:
li r3, 1
blr
UsbdAuthPatchEnd:

# patches inside WgcAddDevice to allow for more leniency on the interface descriptor
# patch header
.long WgcAddDevice_bne - KERNEL_VIRT_BASE # patch address
.long WgcIntPatchEnd - WgcIntPatchStart # patch length
# patch itself - replaces a bne cr6, 0x10 with b 0x10 after UsbdGetEndpointDescriptor(device, 0, 3, 1)
WgcIntPatchStart:
b 0x10
WgcIntPatchEnd:
