#ifndef CAPSTONE_SH_H
#define CAPSTONE_SH_H

/* Capstone Disassembly Engine */
/* By Yoshinori Sato, 2022 */

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"

#ifdef _MSC_VER
#pragma warning(disable:4201)
#endif

/// SH registers and special registers
typedef enum {
	SH_REG_INVALID = 0,

	SH_REG_R0,
	SH_REG_R1,
	SH_REG_R2,
	SH_REG_R3,
	SH_REG_R4,
	SH_REG_R5,
	SH_REG_R6,
	SH_REG_R7,

	SH_REG_R8,
	SH_REG_R9,
	SH_REG_R10,
	SH_REG_R11,
	SH_REG_R12,
	SH_REG_R13,
	SH_REG_R14,
	SH_REG_R15,

	SH_REG_R0_BANK,
	SH_REG_R1_BANK,
	SH_REG_R2_BANK,
	SH_REG_R3_BANK,
	SH_REG_R4_BANK,
	SH_REG_R5_BANK,
	SH_REG_R6_BANK,
	SH_REG_R7_BANK,

	SH_REG_FR0,
	SH_REG_FR1,
	SH_REG_FR2,
	SH_REG_FR3,
	SH_REG_FR4,
	SH_REG_FR5,
	SH_REG_FR6,
	SH_REG_FR7,
	SH_REG_FR8,
	SH_REG_FR9,
	SH_REG_FR10,
	SH_REG_FR11,
	SH_REG_FR12,
	SH_REG_FR13,
	SH_REG_FR14,
	SH_REG_FR15,

	SH_REG_DR0,
	SH_REG_DR2,
	SH_REG_DR4,
	SH_REG_DR6,
	SH_REG_DR8,
	SH_REG_DR10,
	SH_REG_DR12,
	SH_REG_DR14,

	SH_REG_XD0,
	SH_REG_XD2,
	SH_REG_XD4,
	SH_REG_XD6,
	SH_REG_XD8,
	SH_REG_XD10,
	SH_REG_XD12,
	SH_REG_XD14,

	SH_REG_XF0,
	SH_REG_XF1,
	SH_REG_XF2,
	SH_REG_XF3,
	SH_REG_XF4,
	SH_REG_XF5,
	SH_REG_XF6,
	SH_REG_XF7,
	SH_REG_XF8,
	SH_REG_XF9,
	SH_REG_XF10,
	SH_REG_XF11,
	SH_REG_XF12,
	SH_REG_XF13,
	SH_REG_XF14,
	SH_REG_XF15,

	SH_REG_FV0,
	SH_REG_FV4,
	SH_REG_FV8,
	SH_REG_FV12,

	SH_REG_XMATRX,

	SH_REG_PC,
	SH_REG_PR,
	SH_REG_MACH,
	SH_REG_MACL,

	SH_REG_SR,
	SH_REG_GBR,
	SH_REG_SSR,
	SH_REG_SPC,
	SH_REG_SGR,
	SH_REG_DBR,
	SH_REG_VBR,
	SH_REG_TBR,
	SH_REG_RS,
	SH_REG_RE,
	SH_REG_MOD,

	SH_REG_FPUL,
	SH_REG_FPSCR,

	SH_REG_DSP_X0,
	SH_REG_DSP_X1,
	SH_REG_DSP_Y0,
	SH_REG_DSP_Y1,
	SH_REG_DSP_A0,
	SH_REG_DSP_A1,
	SH_REG_DSP_A0G,
	SH_REG_DSP_A1G,
	SH_REG_DSP_M0,
	SH_REG_DSP_M1,
	SH_REG_DSP_DSR,

	SH_REG_DSP_RSV0,
	SH_REG_DSP_RSV1,
	SH_REG_DSP_RSV2,
	SH_REG_DSP_RSV3,
	SH_REG_DSP_RSV4,
	SH_REG_DSP_RSV5,
	SH_REG_DSP_RSV6,
	SH_REG_DSP_RSV7,
	SH_REG_DSP_RSV8,
	SH_REG_DSP_RSV9,
	SH_REG_DSP_RSVA,
	SH_REG_DSP_RSVB,
	SH_REG_DSP_RSVC,
	SH_REG_DSP_RSVD,
	SH_REG_DSP_RSVE,
	SH_REG_DSP_RSVF,

	SH_REG_ENDING,   // <-- mark the end of the list of registers
} sh_reg;

typedef enum {
	SH_OP_INVALID = 0,  ///< = CS_OP_INVALID (Uninitialized).
	SH_OP_REG, ///< = CS_OP_REG (Register operand).
	SH_OP_IMM, ///< = CS_OP_IMM (Immediate operand).
	SH_OP_MEM, ///< = CS_OP_MEM (Memory operand).
} sh_op_type;	

typedef enum {
	SH_OP_MEM_INVALID = 0,   /// <= Invalid
	SH_OP_MEM_REG_IND,   /// <= Register indirect
	SH_OP_MEM_REG_POST,  /// <= Register post increment
	SH_OP_MEM_REG_PRE,   /// <= Register pre decrement
	SH_OP_MEM_REG_DISP,  /// <= displacement
	SH_OP_MEM_REG_R0,    /// <= R0 indexed
	SH_OP_MEM_GBR_DISP,  /// <= GBR based displacement
	SH_OP_MEM_GBR_R0,    /// <= GBR based R0 indexed
	SH_OP_MEM_PCR,       /// <= PC relative
	SH_OP_MEM_TBR_DISP,  /// <= TBR based displaysment
} sh_op_mem_type;

typedef struct sh_op_mem {
	sh_op_mem_type address;  /// <= memory address
	sh_reg reg;              /// <= base register
	uint32_t disp;           /// <= displacement
} sh_op_mem;

// SH-DSP instcutions define
typedef enum sh_dsp_insn_type {
	SH_INS_DSP_INVALID,
	SH_INS_DSP_DOUBLE,
	SH_INS_DSP_SINGLE,
	SH_INS_DSP_PARALLEL,
} sh_dsp_insn_type;

typedef enum sh_dsp_insn {
	SH_INS_DSP_NOP = 1,
	SH_INS_DSP_MOV,
	SH_INS_DSP_PSHL,
	SH_INS_DSP_PSHA,
	SH_INS_DSP_PMULS,
	SH_INS_DSP_PCLR_PMULS,
	SH_INS_DSP_PSUB_PMULS,
	SH_INS_DSP_PADD_PMULS,
	SH_INS_DSP_PSUBC,
	SH_INS_DSP_PADDC,
	SH_INS_DSP_PCMP,
	SH_INS_DSP_PABS,
	SH_INS_DSP_PRND,
	SH_INS_DSP_PSUB,
	SH_INS_DSP_PSUBr,
	SH_INS_DSP_PADD,
	SH_INS_DSP_PAND,
	SH_INS_DSP_PXOR,
	SH_INS_DSP_POR,
	SH_INS_DSP_PDEC,
	SH_INS_DSP_PINC,
	SH_INS_DSP_PCLR,
	SH_INS_DSP_PDMSB,
	SH_INS_DSP_PNEG, 
	SH_INS_DSP_PCOPY,
	SH_INS_DSP_PSTS,
	SH_INS_DSP_PLDS,
	SH_INS_DSP_PSWAP,
	SH_INS_DSP_PWAD,
	SH_INS_DSP_PWSB,
} sh_dsp_insn;

typedef enum sh_dsp_operand {
	SH_OP_DSP_INVALID,
	SH_OP_DSP_REG_PRE,
	SH_OP_DSP_REG_IND,
	SH_OP_DSP_REG_POST,
	SH_OP_DSP_REG_INDEX,
	SH_OP_DSP_REG,
	SH_OP_DSP_IMM,
	
} sh_dsp_operand;

typedef enum sh_dsp_cc {
	SH_DSP_CC_INVALID,
	SH_DSP_CC_NONE,
	SH_DSP_CC_DCT,
	SH_DSP_CC_DCF,
} sh_dsp_cc;

typedef struct sh_op_dsp {
	sh_dsp_insn insn;
	sh_dsp_operand operand[2];
	sh_reg r[6];
	sh_dsp_cc cc;
	uint8_t imm;
	int size;
} sh_op_dsp;
	
/// Instruction operand
typedef struct cs_sh_op {
	sh_op_type type;
	union {
		uint64_t imm;       ///< immediate value for IMM operand
		sh_reg reg;	    ///< register value for REG operand
		sh_op_mem mem; 	    ///< data when operand is targeting memory
		sh_op_dsp dsp;	    ///< dsp instruction
	};
} cs_sh_op;

/// SH instruction
typedef enum sh_insn {
	SH_INS_INVALID,
	SH_INS_ADD_r,
	SH_INS_ADD,
	SH_INS_ADDC,
	SH_INS_ADDV,
	SH_INS_AND,
	SH_INS_BAND,
	SH_INS_BANDNOT,
	SH_INS_BCLR,
	SH_INS_BF,
	SH_INS_BF_S,
	SH_INS_BLD,
	SH_INS_BLDNOT,
	SH_INS_BOR,
	SH_INS_BORNOT,
	SH_INS_BRA,
	SH_INS_BRAF,
	SH_INS_BSET,
	SH_INS_BSR,
	SH_INS_BSRF,
	SH_INS_BST,
	SH_INS_BT,
	SH_INS_BT_S,
	SH_INS_BXOR,
	SH_INS_CLIPS,
	SH_INS_CLIPU,
	SH_INS_CLRDMXY,
	SH_INS_CLRMAC,
	SH_INS_CLRS,
	SH_INS_CLRT,
	SH_INS_CMP_EQ,
	SH_INS_CMP_GE,
	SH_INS_CMP_GT,
	SH_INS_CMP_HI,
	SH_INS_CMP_HS,
	SH_INS_CMP_PL,
	SH_INS_CMP_PZ,
	SH_INS_CMP_STR,
	SH_INS_DIV0S,
	SH_INS_DIV0U,
	SH_INS_DIV1,
	SH_INS_DIVS,
	SH_INS_DIVU,
	SH_INS_DMULS_L,
	SH_INS_DMULU_L,
	SH_INS_DT,
	SH_INS_EXTS_B,
	SH_INS_EXTS_W,
	SH_INS_EXTU_B,
	SH_INS_EXTU_W,
	SH_INS_FABS,
	SH_INS_FADD,
	SH_INS_FCMP_EQ,
	SH_INS_FCMP_GT,
	SH_INS_FCNVDS,
	SH_INS_FCNVSD,
	SH_INS_FDIV,
	SH_INS_FIPR,
	SH_INS_FLDI0,
	SH_INS_FLDI1,
	SH_INS_FLDS,
	SH_INS_FLOAT,
	SH_INS_FMAC,
	SH_INS_FMOV,
	SH_INS_FMUL,
	SH_INS_FNEG,
	SH_INS_FPCHG,
	SH_INS_FRCHG,
	SH_INS_FSCA,
	SH_INS_FSCHG,
	SH_INS_FSQRT,
	SH_INS_FSRRA,
	SH_INS_FSTS,
	SH_INS_FSUB,
	SH_INS_FTRC,
	SH_INS_FTRV,
	SH_INS_ICBI,
	SH_INS_JMP,
	SH_INS_JSR,
	SH_INS_JSR_N,
	SH_INS_LDBANK,
	SH_INS_LDC,
	SH_INS_LDRC,
	SH_INS_LDRE,
	SH_INS_LDRS,
	SH_INS_LDS,
	SH_INS_LDTLB,
	SH_INS_MAC_L,
	SH_INS_MAC_W,
	SH_INS_MOV,
	SH_INS_MOVA,
	SH_INS_MOVCA,
	SH_INS_MOVCO,
	SH_INS_MOVI20,
	SH_INS_MOVI20S,
	SH_INS_MOVLI,
	SH_INS_MOVML,
	SH_INS_MOVMU,
	SH_INS_MOVRT,
	SH_INS_MOVT,
	SH_INS_MOVU,
	SH_INS_MOVUA,
	SH_INS_MUL_L,
	SH_INS_MULR,
	SH_INS_MULS_W,
	SH_INS_MULU_W,
	SH_INS_NEG,
	SH_INS_NEGC,
	SH_INS_NOP,
	SH_INS_NOT,
	SH_INS_NOTT,
	SH_INS_OCBI,
	SH_INS_OCBP,
	SH_INS_OCBWB,
	SH_INS_OR,
	SH_INS_PREF,
	SH_INS_PREFI,
	SH_INS_RESBANK,
	SH_INS_ROTCL,
	SH_INS_ROTCR,
	SH_INS_ROTL,
	SH_INS_ROTR,
	SH_INS_RTE,
	SH_INS_RTS,
	SH_INS_RTS_N,
	SH_INS_RTV_N,
	SH_INS_SETDMX,
	SH_INS_SETDMY,
	SH_INS_SETRC,
	SH_INS_SETS,
	SH_INS_SETT,
	SH_INS_SHAD,
	SH_INS_SHAL,
	SH_INS_SHAR,
	SH_INS_SHLD,
	SH_INS_SHLL,
	SH_INS_SHLL16,
	SH_INS_SHLL2,
	SH_INS_SHLL8,
	SH_INS_SHLR,
	SH_INS_SHLR16,
	SH_INS_SHLR2,
	SH_INS_SHLR8,
	SH_INS_SLEEP,
	SH_INS_STBANK,
	SH_INS_STC,
	SH_INS_STS,
	SH_INS_SUB,
	SH_INS_SUBC,
	SH_INS_SUBV,
	SH_INS_SWAP_B,
	SH_INS_SWAP_W,
	SH_INS_SYNCO,
	SH_INS_TAS,
	SH_INS_TRAPA,
	SH_INS_TST,
	SH_INS_XOR,
	SH_INS_XTRCT,
	SH_INS_DSP,
	SH_INS_ENDING,   // <-- mark the end of the list of instructions
} sh_insn;

/// Instruction structure
typedef struct cs_sh {
	sh_insn insn;
	uint8_t size;
	uint8_t op_count;
	cs_sh_op operands[3];
} cs_sh;

/// Group of SH instructions
typedef enum sh_insn_group {
	SH_GRP_INVALID = 0,  ///< CS_GRUP_INVALID
	SH_GRP_JUMP,  ///< = CS_GRP_JUMP
	SH_GRP_CALL,  ///< = CS_GRP_CALL
	SH_GRP_INT,  ///< = CS_GRP_INT
	SH_GRP_RET,  ///< = CS_GRP_RET
	SH_GRP_IRET, ///< = CS_GRP_IRET
        SH_GRP_PRIVILEGE,     ///< = CS_GRP_PRIVILEGE
	SH_GRP_BRANCH_RELATIVE, ///< = CS_GRP_BRANCH_RELATIVE

	SH_GRP_SH1,
	SH_GRP_SH2,
	SH_GRP_SH2E,
	SH_GRP_SH2DSP,
	SH_GRP_SH2A,
	SH_GRP_SH2AFPU,
	SH_GRP_SH3,
	SH_GRP_SH3DSP,
	SH_GRP_SH4,
	SH_GRP_SH4A,
	
	SH_GRP_ENDING,// <-- mark the end of the list of groups
} sh_insn_group;

#ifdef __cplusplus
}
#endif

#endif
