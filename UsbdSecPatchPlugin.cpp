#include <xtl.h>
#include "ppcasm.h"

// patch address for Retail Kernel 17559 - the bne after UsbdGetEndpointDescriptor in WgcAddDevice
#define WGCADDDEVICE_INST 0x800F98E0
// patch address for Devkit Kernel 17489 (including spoofed 17559 etc)
#define WGCADDDEVICE_INST_DEV 0x801341F4
// patch address for Devkit Kernel 17489 - patch bne to b so we don't hit a breakpoint in WgcBindToUser
// (could also just nop the breakpoint instead)
#define WgGCBINDTOUSER_BREAKPOINT_DEV 0x801331EC

// struct for the kernel version
typedef struct _XBOX_KRNL_VERSION {
	WORD Major;
	WORD Minor;
	WORD Build;
	WORD Qfe;
} XBOX_KRNL_VERSION, *PXBOX_KRNL_VERSION;

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING, *PSTRING;

typedef struct _LDR_DATA_TABLE_ENTRY { 
	LIST_ENTRY InLoadOrderLinks;  // 0x0 sz:0x8
	LIST_ENTRY InClosureOrderLinks;  // 0x8 sz:0x8
	LIST_ENTRY InInitializationOrderLinks; // 0x10 sz:0x8
	PVOID NtHeadersBase; // 0x18 sz:0x4
	PVOID ImageBase; // 0x1C sz:0x4
	DWORD SizeOfNtImage; // 0x20 sz:0x4
	UNICODE_STRING FullDllName; // 0x24 sz:0x8
	UNICODE_STRING BaseDllName; // 0x2C sz:0x8
	DWORD Flags; // 0x34 sz:0x4
	DWORD SizeOfFullImage; // 0x38 sz:0x4
	PVOID EntryPoint; // 0x3C sz:0x4
	WORD LoadCount; // 0x40 sz:0x2
	WORD ModuleIndex; // 0x42 sz:0x2
	PVOID DllBaseOriginal; // 0x44 sz:0x4
	DWORD CheckSum; // 0x48 sz:0x4
	DWORD ModuleLoadFlags; // 0x4C sz:0x4
	DWORD TimeDateStamp; // 0x50 sz:0x4
	PVOID LoadedImports; // 0x54 sz:0x4
	PVOID XexHeaderBase; // 0x58 sz:0x4
	union{
		STRING LoadFileName; // 0x5C sz:0x8
		struct {
			PVOID ClosureRoot; // 0x5C sz:0x4 LDR_DATA_TABLE_ENTRY
			PVOID TraversalParent; // 0x60 sz:0x4 LDR_DATA_TABLE_ENTRY
		} asEntry;
	} inf;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY; // size 100

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
		// check if we're running on a devkit kernel
		BOOL bIsDevkit = *(DWORD*)0x8E038610 & 0x8000 ? FALSE : TRUE;
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
		
		// only patch WgcAddDevice if 17559(Retail) / 17489(Devkit), due to hardcoded function address
		// we could scan kernel address space but lol. lmao.
		if (bIsDevkit && XboxKrnlVersion->Build >= 17489)
		{
			DbgPrint("UsbdSecPatch | patching devkit kernel\n");
			// replace bne cr6, 0x10 to b 0x10 after UsbdGetEndpointDescriptor(device, 0, 3, 1)
			// nullifies the check to see if that returned NULL
			POKE_B(WGCADDDEVICE_INST_DEV, WGCADDDEVICE_INST_DEV + 0x10);
			// patch bne to b cr6, 0x8 to b 0x8 so we don't hit a breakpoint in WgcBindToUser
			POKE_B(WgGCBINDTOUSER_BREAKPOINT_DEV, WgGCBINDTOUSER_BREAKPOINT_DEV + 0x8);
		}
		else if (XboxKrnlVersion->Build == 17559) {
			// replace bne cr6, 0x10 to b 0x10 after UsbdGetEndpointDescriptor(device, 0, 3, 1)
			// nullifies the check to see if that returned NULL
			POKE_B(WGCADDDEVICE_INST, WGCADDDEVICE_INST + 0x10);
		} else {
			DbgPrint("UsbdSecPatch | not patching WgcAddDevice: kernel is %i\n", XboxKrnlVersion->Build);
		}
end:
		// set load count to 1
		((LDR_DATA_TABLE_ENTRY *)hInstDLL)->LoadCount = 1;
		return FALSE;
	}
	return TRUE;
}
