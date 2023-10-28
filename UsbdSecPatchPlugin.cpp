#include <xtl.h>
#include "ppcasm.h"

// patch address for 17559 - the bne after UsbdGetEndpointDescriptor in WgcAddDevice
#define WGCADDDEVICE_INST 0x800F98E0

// struct for the kernel version
typedef struct _XBOX_KRNL_VERSION {
	WORD Major;
	WORD Minor;
	WORD Build;
	WORD Qfe;
} XBOX_KRNL_VERSION, *PXBOX_KRNL_VERSION;

// kernel imports that aren't normally exposed
EXTERN_C { 
	VOID DbgPrint(const char* s, ...);
	DWORD XexGetModuleHandle(PCSTR moduleName, PHANDLE hand);
	DWORD XexGetProcedureAddress(HANDLE hand, DWORD dwOrdinal, PVOID Address);
	extern PXBOX_KRNL_VERSION XboxKrnlVersion;
}
 
// dll entrypoint
BOOL APIENTRY DllMain(HANDLE hInstDLL, DWORD dwReason, LPVOID lpReserved) {
	HANDLE hKernel = NULL;
	PDWORD pdwUsbdAuthFunction = NULL;
	if (dwReason == DLL_PROCESS_ATTACH) {
		// fetch the kernel handle and find UsbdIsDeviceAuthenticated's export
		XexGetModuleHandle("xboxkrnl.exe", &hKernel);
		XexGetProcedureAddress(hKernel, 745, &pdwUsbdAuthFunction);
		DbgPrint("UsbdSecPatch | got UsbdIsDeviceAuthenticated at %p\n", pdwUsbdAuthFunction);
		// if we couldn't find the function, jump ship
		if (pdwUsbdAuthFunction == NULL)
			goto end;
		// patch the function to return true
		pdwUsbdAuthFunction[0] = LI(3, 1);
		pdwUsbdAuthFunction[1] = BLR;
		
		// only patch WgcAddDevice if 17559, due to hardcoded function address
		// we could scan kernel address space but lol. lmao.
		if (XboxKrnlVersion->Build == 17559) {
			// replace bne cr6, 0x10 to b 0x10 after UsbdGetEndpointDescriptor(device, 0, 3, 1)
			// nullifies the check to see if that returned NULL
			POKE_B(WGCADDDEVICE_INST, WGCADDDEVICE_INST + 0x10);
		} else {
			DbgPrint("UsbdSecPatch | not patching WgcAddDevice: kernel is %i\n", XboxKrnlVersion->Build);
		}
end:
		// set load count to 1
		*(WORD*)((DWORD)hInstDLL + 64) = 1;
		return FALSE;
	}
	return TRUE;
}
