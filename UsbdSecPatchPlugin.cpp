#include <xtl.h>
#include "ppcasm.h"

// patch address for the bne (or beq for 6717 - 9199) after UsbdGetEndpointDescriptor in WgcAddDevice
//
// Patches modify the check after UsbdGetEndpointDescriptor(device, 0, 3, 1)
//
// Patch for 6717, 7258, and 9199 replace a beq cr6, 0x64 with b 0x4 (NOP would work as well)
// Patch for 13604, 15574, 17489, 17559 replace bne cr6, 0x10 with b 0x10
//
#define WGCADDDEVICE_INST_17559 0x800F98E0
#define WGCADDDEVICE_INST_17489_DEV 0x801341F4
#define WGCADDDEVICE_INST_15574 0x800F9BD0
#define WGCADDDEVICE_INST_13604 0x800F75D0
#define WGCADDDEVICE_INST_9199 0x800ED7DC
#define WGCADDDEVICE_INST_7258 0x800E618C
#define WGCADDDEVICE_INST_6717 0x800E48A4
#define WGCADDDEVICE_INST_1888 0x800B862C

// Addresses of the UsbdIsDeviceAuthenticated function. 9199 and earlier do not
// have UsbdIsDeviceAuthenticated as ordinal 745 and must be patched manually
#define USBDISDEVICEAUTHENTICATED_9199 0x800CF280
#define USBDISDEVICEAUTHENTICATED_7258 0x800C7128
#define USBDISDEVICEAUTHENTICATED_6717 0x800C5950
#define USBDISDEVICEAUTHENTICATED_6717 0x800A0A08

// patch address for 17489 devkit kernel - assertion in WgcBindToUser that's hit with some 3rd party xinput devices
#define WGCBINDTOUSER_INST_17489_DEV 0x801331F0 

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
	bool isDevkit = *(uint32_t*)(0x8010D334) == 0x00000000;

	if (dwReason == DLL_PROCESS_ATTACH) {

		// UsbdIsDeviceAuthenticated is ordinal 745 on 13604 and later
		// 9199 and earlier do not have an export for UsbdIsDeviceAuthenticated
		if(XboxKrnlVersion->Build >= 13604)
		{
			// fetch the kernel handle and find UsbdIsDeviceAuthenticated's export
			XexGetModuleHandle("xboxkrnl.exe", &hKernel);
			XexGetProcedureAddress(hKernel, 745, &pdwUsbdAuthFunction);
		} else if(XboxKrnlVersion->Build == 9199) {
			pdwUsbdAuthFunction = (PDWORD)USBDISDEVICEAUTHENTICATED_9199;
		} else if(XboxKrnlVersion->Build == 7258) {
			pdwUsbdAuthFunction = (PDWORD)USBDISDEVICEAUTHENTICATED_7258;
		} else if(XboxKrnlVersion->Build == 6717) {
			pdwUsbdAuthFunction = (PDWORD)USBDISDEVICEAUTHENTICATED_6717;
		} else if(XboxKrnlVersion->Build == 1888) {
			pdwUsbdAuthFunction = (PDWORD)USBDISDEVICEAUTHENTICATED_1888;
		}

		DbgPrint("UsbdSecPatch | got UsbdIsDeviceAuthenticated at %p\n", pdwUsbdAuthFunction);
		// if we couldn't find the function, jump ship
		if (pdwUsbdAuthFunction == NULL)
			goto end;
		// patch the function to return true
		pdwUsbdAuthFunction[0] = LI(3, 1);
		pdwUsbdAuthFunction[1] = BLR;

		// Only patch WgcAddDevice for certain kernel versions due to hardcoded function address
		// we could scan kernel address space but lol. lmao.
		// 17489 kernel is used for XDKBuild and RGLoader, the
		// latter optionally spoofs the kernel version to 17559
		if (isDevkit && XboxKrnlVersion->Build >= 17489){
			POKE_B(WGCADDDEVICE_INST_17489_DEV, WGCADDDEVICE_INST_17489_DEV + 0x10);

			// Replaces twui r0, 0x19 to avoid a kernel assertion in WgcBindToUser when certain 3rd party
			// XInput devices like the Mayflash NS are inserted
			POKE_NOP(WGCBINDTOUSER_INST_17489_DEV);
		} else if (XboxKrnlVersion->Build == 1888) {
			POKE_B(WGCADDDEVICE_INST_6717, WGCADDDEVICE_INST_1888 + 0x4);
		} else if (XboxKrnlVersion->Build == 6717) {
			POKE_B(WGCADDDEVICE_INST_6717, WGCADDDEVICE_INST_6717 + 0x4);
		} else if (XboxKrnlVersion->Build == 7258) {
			POKE_B(WGCADDDEVICE_INST_7258, WGCADDDEVICE_INST_7258 + 0x4);
		} else if (XboxKrnlVersion->Build == 9199) {
			POKE_B(WGCADDDEVICE_INST_9199, WGCADDDEVICE_INST_9199 + 0x4);
		} else if (XboxKrnlVersion->Build == 13604) {
			POKE_B(WGCADDDEVICE_INST_13604, WGCADDDEVICE_INST_13604 + 0x10);
		} else if (XboxKrnlVersion->Build == 15574) {
			POKE_B(WGCADDDEVICE_INST_15574, WGCADDDEVICE_INST_15574 + 0x10);
		} else if (XboxKrnlVersion->Build == 17559) {
			POKE_B(WGCADDDEVICE_INST_17559, WGCADDDEVICE_INST_17559 + 0x10);
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
