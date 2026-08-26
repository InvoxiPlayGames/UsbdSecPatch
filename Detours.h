//
// Made by iMoD1998	
// V3.1
//

#ifndef DETOUR_H
#define DETOUR_H

#include <xtl.h>
#include <stdint.h>

#define MASK_N_BITS(N) ( ( 1 << ( N ) ) - 1 )

#define POWERPC_HI(X) ( ( X >> 16 ) & 0xFFFF )
#define POWERPC_LO(X) ( X & 0xFFFF )

//
// PowerPC most significant bit is addressed as bit 0 in documentation.
//
#define POWERPC_BIT32(N) ( 31 - N )

//
// Opcode is bits 0-5. 
// Allowing for op codes ranging from 0-63.
//
#define POWERPC_OPCODE(OP)       ( OP << 26 )
#define POWERPC_OPCODE_ADDI      POWERPC_OPCODE( 14 )
#define POWERPC_OPCODE_ADDIS     POWERPC_OPCODE( 15 )
#define POWERPC_OPCODE_BC        POWERPC_OPCODE( 16 )
#define POWERPC_OPCODE_B         POWERPC_OPCODE( 18 )
#define POWERPC_OPCODE_BCCTR     POWERPC_OPCODE( 19 )
#define POWERPC_OPCODE_ORI       POWERPC_OPCODE( 24 )
#define POWERPC_OPCODE_EXTENDED  POWERPC_OPCODE( 31 ) // Use extended opcodes.
#define POWERPC_OPCODE_STW       POWERPC_OPCODE( 36 )
#define POWERPC_OPCODE_LWZ       POWERPC_OPCODE( 32 )
#define POWERPC_OPCODE_LD        POWERPC_OPCODE( 58 )
#define POWERPC_OPCODE_STD       POWERPC_OPCODE( 62 )
#define POWERPC_OPCODE_MASK      POWERPC_OPCODE( 63 )

#define POWERPC_EXOPCODE(OP)     ( OP << 1 )
#define POWERPC_EXOPCODE_BCCTR   POWERPC_EXOPCODE( 528 )
#define POWERPC_EXOPCODE_MTSPR   POWERPC_EXOPCODE( 467 )

//
// SPR field is encoded as two 5 bit bitfields.
//
#define POWERPC_SPR(SPR) (UINT32)( ( ( SPR & 0x1F ) << 5 ) | ( ( SPR >> 5 ) & 0x1F ) )

//
// Instruction helpers.
//
// rD - Destination register.
// rS - Source register.
// rA/rB - Register inputs.
// SPR - Special purpose register.
// UIMM/SIMM - Unsigned/signed immediate.
//
#define POWERPC_ADDI(rD, rA, SIMM)  (UINT32)( POWERPC_OPCODE_ADDI | ( rD << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | SIMM )
#define POWERPC_ADDIS(rD, rA, SIMM) (UINT32)( POWERPC_OPCODE_ADDIS | ( rD << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | SIMM )
#define POWERPC_LIS(rD, SIMM)       POWERPC_ADDIS( rD, 0, SIMM ) // Mnemonic for addis %rD, 0, SIMM
#define POWERPC_LI(rD, SIMM)        POWERPC_ADDI( rD, 0, SIMM )  // Mnemonic for addi %rD, 0, SIMM
#define POWERPC_MTSPR(SPR, rS)      (UINT32)( POWERPC_OPCODE_EXTENDED | ( rS << POWERPC_BIT32( 10 ) ) | ( POWERPC_SPR( SPR ) << POWERPC_BIT32( 20 ) ) | POWERPC_EXOPCODE_MTSPR )
#define POWERPC_MTCTR(rS)           POWERPC_MTSPR( 9, rS ) // Mnemonic for mtspr 9, rS
#define POWERPC_ORI(rS, rA, UIMM)   (UINT32)( POWERPC_OPCODE_ORI | ( rS << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | UIMM )
#define POWERPC_BCCTR(BO, BI, LK)   (UINT32)( POWERPC_OPCODE_BCCTR | ( BO << POWERPC_BIT32( 10 ) ) | ( BI << POWERPC_BIT32( 15 ) ) | ( LK & 1 ) | POWERPC_EXOPCODE_BCCTR )
#define POWERPC_STD(rS, DS, rA)     (UINT32)( POWERPC_OPCODE_STD | ( rS << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | ( (INT16)DS & 0xFFFF ) )
#define POWERPC_LD(rS, DS, rA)      (UINT32)( POWERPC_OPCODE_LD | ( rS << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | ( (INT16)DS & 0xFFFF ) )

//
// Branch related fields.
//
#define POWERPC_BRANCH_LINKED    1
#define POWERPC_BRANCH_ABSOLUTE  2
#define POWERPC_BRANCH_TYPE_MASK ( POWERPC_BRANCH_LINKED | POWERPC_BRANCH_ABSOLUTE )

#define POWERPC_BRANCH_OPTIONS_ALWAYS ( 20 )

class Detour
{
public:
	Detour() {}

	//
	// HookSource - The function that will be hooked.
	// HookTarget - The function that the hook will be redirected to.
	//	
	Detour(
		_Inout_ void*       HookSource,
		_In_    const void* HookTarget
	) :
		HookSource(HookSource),
		HookTarget(HookTarget),
		TrampolineAddress(NULL),
		OriginalLength(0)
	{

	}

	~Detour()
	{
		this->Remove();
	}

	//
	// Writes an unconditional branch to the destination address that will branch to the target address.
	//
	// Destination - Where the branch will be written to.
	// BranchTarget - The address the branch will jump to.
	// Linked - Branch is a call or a jump? aka bl or b
	// PreserveRegister - Preserve the register clobbered after loading the branch address.
	//
	static SIZE_T WriteFarBranch(
		_Out_   void*       Destination,
		_In_    const void* BranchTarget,
		_In_    bool        Linked = true,
		_In_    bool        PreserveRegister = false
	)
	{
		return Detour::WriteFarBranchEx(Destination, BranchTarget, Linked, PreserveRegister);
	}

	//
	// Writes both conditional and unconditional branches using the count register to the destination address that will branch to the target address.
	//
	// Destination - Where the branch will be written to.
	// BranchTarget - The address the branch will jump to.
	// Linked - Branch is a call or a jump? aka bl or b
	// PreserveRegister - Preserve the register clobbered after loading the branch address.
	// BranchOptions - Options for determining when a branch to be followed.
	// ConditionRegisterBit - The bit of the condition register to compare.
	// RegisterIndex - Register to use when loading the destination address into the count register.
	//
	static SIZE_T WriteFarBranchEx(
		_Out_ void*       Destination,
		_In_  const void* BranchTarget,
		_In_  bool        Linked = false,
		_In_  bool        PreserveRegister = false,
		_In_  UINT32      BranchOptions = POWERPC_BRANCH_OPTIONS_ALWAYS,
		_In_  BYTE        ConditionRegisterBit = 0,
		_In_  BYTE        RegisterIndex = 0
	)
	{
		const UINT32 BranchFarAsm[] = {
			POWERPC_LIS(RegisterIndex, POWERPC_HI((UINT32)BranchTarget)),                     // lis   %rX, BranchTarget@hi
			POWERPC_ORI(RegisterIndex, RegisterIndex, POWERPC_LO((UINT32)BranchTarget)),      // ori   %rX, %rX, BranchTarget@lo
			POWERPC_MTCTR(RegisterIndex),                                                       // mtctr %rX
			POWERPC_BCCTR(BranchOptions, ConditionRegisterBit, Linked)                          // bcctr (bcctr 20, 0 == bctr)
		};

		const UINT32 BranchFarAsmPreserve[] = {
			POWERPC_STD(RegisterIndex, -0x30, 1),                                               // std   %rX, -0x30(%r1)
			POWERPC_LIS(RegisterIndex, POWERPC_HI((UINT32)BranchTarget)),                     // lis   %rX, BranchTarget@hi
			POWERPC_ORI(RegisterIndex, RegisterIndex, POWERPC_LO((UINT32)BranchTarget)),      // ori   %rX, %rX, BranchTarget@lo
			POWERPC_MTCTR(RegisterIndex),                                                       // mtctr %rX
			POWERPC_LD(RegisterIndex, -0x30, 1),                                                // lwz   %rX, -0x30(%r1)
			POWERPC_BCCTR(BranchOptions, ConditionRegisterBit, Linked)                          // bcctr (bcctr 20, 0 == bctr)
		};

		const auto BranchAsm = PreserveRegister ? BranchFarAsmPreserve : BranchFarAsm;
		const auto BranchAsmSize = PreserveRegister ? sizeof(BranchFarAsmPreserve) : sizeof(BranchFarAsm);

		if (Destination)
			memcpy(Destination, BranchAsm, BranchAsmSize);

		return BranchAsmSize;
	}

	//
	// Copies and fixes relative branch instructions to a new location.
	//
	// Destination - Where to write the new branch.
	// Source - Address to the instruction that is being relocated.
	//
	static SIZE_T RelocateBranch(
		_Out_ UINT32*       Destination,
		_In_  const UINT32* Source
	)
	{
		const auto Instruction = *Source;
		const auto InstructionAddress = (UINT32)Source;

		//
		// Absolute branches dont need to be handled.
		//
		if (Instruction & POWERPC_BRANCH_ABSOLUTE)
		{
			*Destination = Instruction;

			return 4;
		}

		INT32  BranchOffsetBitSize;
		INT32  BranchOffsetBitBase;
		UINT32 BranchOptions;
		BYTE   ConditionRegisterBit;

		switch (Instruction & POWERPC_OPCODE_MASK)
		{
			//
			// B - Branch
			// [Opcode]            [Address]           [Absolute] [Linked]
			//   0-5                 6-29                  30        31
			//
			// Example
			//  010010   0000 0000 0000 0000 0000 0001      0         0
			//
		case POWERPC_OPCODE_B:
			BranchOffsetBitSize = 24;
			BranchOffsetBitBase = 2;
			BranchOptions = POWERPC_BRANCH_OPTIONS_ALWAYS;
			ConditionRegisterBit = 0;
			break;

			//
			// BC - Branch Conditional
			// [Opcode]   [Branch Options]     [Condition Register]         [Address]      [Absolute] [Linked]
			//   0-5           6-10                    11-15                  16-29            30        31
			//
			// Example
			//  010000        00100                    00001             00 0000 0000 0001      0         0
			//
		case POWERPC_OPCODE_BC:
			BranchOffsetBitSize = 14;
			BranchOffsetBitBase = 2;
			BranchOptions = (Instruction >> POWERPC_BIT32(10)) & MASK_N_BITS(5);
			ConditionRegisterBit = (Instruction >> POWERPC_BIT32(15)) & MASK_N_BITS(5);
			break;
		}

		//
		// Even though the address part of the instruction begins from bit 29 in the case of bc and b.
		// The value of the first bit is 4 as all addresses are aligned to for 4 for code therefore,
		// the branch offset can be caluclated by anding in place and removing any suffix bits such as the 
		// link register or absolute flags.
		//
		INT32 BranchOffset = Instruction & (MASK_N_BITS(BranchOffsetBitSize) << BranchOffsetBitBase);

		//
		// Check if the MSB of the offset is set.
		//
		if (BranchOffset >> ((BranchOffsetBitSize + BranchOffsetBitBase) - 1))
		{
			//
			// Add the nessasary bits to our integer to make it negative.
			//
			BranchOffset |= ~MASK_N_BITS(BranchOffsetBitSize + BranchOffsetBitBase);
		}

		const auto BranchAddress = (void*)(INT32)(InstructionAddress + BranchOffset);

		return Detour::WriteFarBranchEx(Destination, BranchAddress, Instruction & POWERPC_BRANCH_LINKED, true, BranchOptions, ConditionRegisterBit);
	}

	//
	// Copies an instruction enusuring things such as PC relative offsets are fixed.
	//
	// Destination - Where to write the new instruction(s).
	// Source - Address to the instruction that is being copied.
	//
	static SIZE_T CopyInstruction(
		_Out_ UINT32*       Destination,
		_In_  const UINT32* Source
	)
	{
		const auto Instruction = *Source;
		const auto InstructionAddress = (UINT32)Source;

		switch (Instruction & POWERPC_OPCODE_MASK)
		{
		case POWERPC_OPCODE_B:  // B BL BA BLA
		case POWERPC_OPCODE_BC: // BEQ BNE BLT BGE
			return Detour::RelocateBranch(Destination, Source);
		default:
			*Destination = Instruction;
			return 4;
		}
	}

	bool Install()
	{
		//
		// Check if we are already hooked.
		//
		if (this->OriginalLength != 0)
			return false;

		const auto HookSize = Detour::WriteFarBranch(NULL, this->HookTarget, false, false);

		//
		// Save the original instructions for unhooking later on.
		//
		memcpy(this->OriginalInstructions, this->HookSource, HookSize);

		this->OriginalLength = HookSize;

		//
		// Create trampoline and copy and fix instructions to the trampoline.
		//
		this->TrampolineAddress = &Detour::TrampolineBuffer[Detour::TrampolineSize];

		for (SIZE_T i = 0; i < (HookSize / 4); i++)
		{
			const auto InstructionPtr = (UINT32*)((UINT32)this->HookSource + (i * 4));

			Detour::TrampolineSize += Detour::CopyInstruction((UINT32*)&Detour::TrampolineBuffer[Detour::TrampolineSize], InstructionPtr);
		}

		//
		// Trampoline branches back to the original function after the branch we used to hook.
		//
		const auto AfterBranchAddress = (void*)((UINT32)this->HookSource + HookSize);

		Detour::TrampolineSize += Detour::WriteFarBranch(&Detour::TrampolineBuffer[Detour::TrampolineSize], AfterBranchAddress, false, true);

		//
		// Finally write the branch to the function that we are hooking.
		//
		Detour::WriteFarBranch(this->HookSource, this->HookTarget, false, false);

		return true;
	}

	bool Remove()
	{
		if (this->HookSource && this->OriginalLength)
		{
			memcpy(this->HookSource, this->OriginalInstructions, this->OriginalLength);

			this->OriginalLength = 0;
			this->HookSource = NULL;

			return true;
		}

		return false;
	}

	template<typename T>
	T GetOriginal() const
	{
		return T(this->TrampolineAddress);
	}

private:
	const void* HookTarget;                 // The funtion we are pointing the hook to.
	void*       HookSource;                 // The function we are hooking.
	BYTE*       TrampolineAddress;          // Pointer to the trampoline for this detour.
	BYTE        OriginalInstructions[30]; // Any bytes overwritten by the hook.
	SIZE_T      OriginalLength;             // The amount of bytes overwritten by the hook.

	//
	// Shared
	//
	// The secret to getting the hooks to work inside the OG xbox emulator is by placing the trampolines and all dynamically allocated code inside a section thats trusted by the hypervisor
	#pragma section(".text", read, execute)
	__declspec(allocate(".text")) static BYTE TrampolineBuffer[200 * 20];
	static SIZE_T TrampolineSize;
};

#endif // !DETOUR_H