#include <xtl.h>
#include "ppcasm.h"
#include "Detours.h"

// Detours statics (build_plugin.bat compiles this single TU)
BYTE   Detour::TrampolineBuffer[200 * 20] = {};
SIZE_T Detour::TrampolineSize = 0;

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
#define USBDISDEVICEAUTHENTICATED_1888 0x800A0A08

// patch address for 17489 devkit kernel - assertion in WgcBindToUser that's hit with some 3rd party xinput devices
#define WGCBINDTOUSER_INST_17489_DEV 0x801331F0

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

// XamUserBindDeviceCallback function addresses
#define XAMUSERBINDDEVICECALLBACK_17559 0x816D9060 // 7C 8B 23 78 7C A4 2B 78 54 CA 06 3F
#define XAMUSERBINDDEVICECALLBACK_17489_DEV 0x817A34B8

typedef int (*xam_bind_func_t)(unsigned int controllerId, unsigned int context, unsigned __int8 category, bool disconnect, unsigned __int8* userIndex);

static Detour XamBindDetour;

struct BoundDevice {
	uint32_t origId;
	uint32_t bumpedId;
	uint32_t context;
};

static BoundDevice g_bound[8];
static int g_boundCount = 0;

static bool checkBumpedIdExists(uint32_t bumped) {
	for (int j = 0; j < g_boundCount; j++) {
		if (g_bound[j].bumpedId == bumped) {
			return true;
		}
	}

	return false;
}

static int CallOriginalBind(unsigned int id, unsigned int context, unsigned __int8 category, bool disconnect, unsigned __int8* userIndex) {
	return XamBindDetour.GetOriginal<xam_bind_func_t>()(id, context, category, disconnect, userIndex);
}

static int XamUserBindDeviceCallbackHook(unsigned int controllerId, unsigned int context, unsigned __int8 category, bool disconnect, unsigned __int8* userIndex) {
	DbgPrint("UsbdSecPatch | bind cb: id=%08x ctx=%08x cat=%d disc=%d\n",
		controllerId, context, category, disconnect ? 1 : 0);

	if (disconnect) {
		// translate the id back for devices we bumped, then drop the entry
		for (int i = 0; i < g_boundCount; i++) {
			// the disconnecting controller is one of ours
			if (g_bound[i].context == context) { 
				uint32_t id = g_bound[i].bumpedId;
				g_bound[i] = g_bound[--g_boundCount];

				DbgPrint("UsbdSecPatch | disconnect ctx=%08x -> id %08x\n", context, id);
				
				return CallOriginalBind(id, context, category, disconnect, userIndex);
			}
		}

		// the disconnecting controller did not need to be hooked
		return CallOriginalBind(controllerId, context, category, disconnect, userIndex);
	} else {
		// check if a known device re-binding: keep its (possibly bumped) id
		for (int i = 0; i < g_boundCount; i++) {
			if (g_bound[i].context == context) {
				DbgPrint("UsbdSecPatch | rebind ctx=%08x -> id %08x\n", context, g_bound[i].bumpedId);

				return CallOriginalBind(g_bound[i].bumpedId, context, category, disconnect, userIndex);
			}
		}

		// new device: bump the id if it collides with a live different-context one
		uint32_t id = controllerId;
		for (int i = 0; i < g_boundCount; i++) {
			if (g_bound[i].bumpedId == id || g_bound[i].origId == id) {
				uint32_t bumped = controllerId | 0x80000000;
				
				while (true) {
					if (checkBumpedIdExists(bumped)) {
						bumped++;
					} else {
						break;
					}
				}

				DbgPrint("UsbdSecPatch | duplicate id %08x (ctx %08x) -> bumped %08x\n",
					controllerId, context, bumped);
				id = bumped;
				break;
			}
		}

		if (g_boundCount < 8) {
			g_bound[g_boundCount].origId = controllerId;
			g_bound[g_boundCount].bumpedId = id;
			g_bound[g_boundCount].context = context;
			g_boundCount++;
		}
		return CallOriginalBind(id, context, category, disconnect, userIndex);
	}
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
			POKE_B(WGCADDDEVICE_INST_1888, WGCADDDEVICE_INST_1888 + 0x4);
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

		// Hook XamUserBindDeviceCallback 
		uint32_t bindAddr = 0;
		if (isDevkit && XboxKrnlVersion->Build >= 17489) {
			bindAddr = XAMUSERBINDDEVICECALLBACK_17489_DEV;
		} else if (!isDevkit && XboxKrnlVersion->Build == 17559) {
			bindAddr = XAMUSERBINDDEVICECALLBACK_17559;
		}

		if (bindAddr == 0) {
			DbgPrint("UsbdSecPatch | no XamUserBindDeviceCallback address for this kernel, hook not installed\n");
		} else {
			XamBindDetour = Detour((void*)bindAddr, (void*)XamUserBindDeviceCallbackHook);
			XamBindDetour.Install();
			DbgPrint("UsbdSecPatch | XamUserBindDeviceCallback hook installed at %08x\n", bindAddr);
		}
	
		DbgPrint("UsbdSecPatch | resident, build %s %s\n", __DATE__, __TIME__);
	}
	end:
	// Because we need to hook a callback we have to stay resident, return TRUE
	return TRUE;
}
