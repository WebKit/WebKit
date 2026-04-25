#ifndef CAPSTONE_RISCV_H
#define CAPSTONE_RISCV_H

/* Capstone Disassembly Engine */
/* RISC-V Backend By Rodrigo Cortes Porto <porto703@gmail.com> & 
   Shawn Chang <citypw@gmail.com>, HardenedLinux@2018 */

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_MSC_VER) || !defined(_KERNEL_MODE)
#include <stdint.h>
#endif

#include "platform.h"

// GCC MIPS toolchain has a default macro called "mips" which breaks
// compilation
//#undef riscv

#ifdef _MSC_VER
#pragma warning(disable:4201)
#endif

//> Operand type for instruction's operands
typedef enum riscv_op_type {
	RISCV_OP_INVALID = 0, // = CS_OP_INVALID (Uninitialized).
	RISCV_OP_REG, // = CS_OP_REG (Register operand).
	RISCV_OP_IMM, // = CS_OP_IMM (Immediate operand).
	RISCV_OP_MEM, // = CS_OP_MEM (Memory operand).
} riscv_op_type;

// Instruction's operand referring to memory
// This is associated with RISCV_OP_MEM operand type above
typedef struct riscv_op_mem {
	unsigned int base;	// base register
	int64_t disp;	// displacement/offset value
} riscv_op_mem;

// Instruction operand
typedef struct cs_riscv_op {
	riscv_op_type type;	// operand type
	union {
		unsigned int reg;	// register value for REG operand
		int64_t imm;		// immediate value for IMM operand
		riscv_op_mem mem;	// base/disp value for MEM operand
	};
} cs_riscv_op;

// Instruction structure
typedef struct cs_riscv {
	// Does this instruction need effective address or not.
	bool need_effective_addr;
	// Number of operands of this instruction, 
	// or 0 when instruction has no operand.
	uint8_t op_count;
	cs_riscv_op operands[8]; // operands for this instruction.
} cs_riscv;

//> RISCV registers
typedef enum riscv_reg {
	RISCV_REG_INVALID = 0,
	//> General purpose registers
	RISCV_REG_X0,			// "zero" 
	RISCV_REG_ZERO = RISCV_REG_X0, 	// "zero" 
	RISCV_REG_X1, 			// "ra"
	RISCV_REG_RA   = RISCV_REG_X1, 	// "ra"
	RISCV_REG_X2, 			// "sp"
	RISCV_REG_SP   = RISCV_REG_X2, 	// "sp"
	RISCV_REG_X3, 			// "gp"
	RISCV_REG_GP   = RISCV_REG_X3, 	// "gp"
	RISCV_REG_X4, 			// "tp"
	RISCV_REG_TP   = RISCV_REG_X4,	// "tp"
	RISCV_REG_X5, 			// "t0"
	RISCV_REG_T0   = RISCV_REG_X5, 	// "t0"
	RISCV_REG_X6, 			// "t1"
	RISCV_REG_T1   = RISCV_REG_X6, 	// "t1"
	RISCV_REG_X7, 			// "t2"
	RISCV_REG_T2   = RISCV_REG_X7, 	// "t2"
	RISCV_REG_X8, 			// "s0/fp"
	RISCV_REG_S0   = RISCV_REG_X8,	// "s0"
	RISCV_REG_FP   = RISCV_REG_X8,	// "fp"
	RISCV_REG_X9, 			// "s1"
	RISCV_REG_S1   = RISCV_REG_X9, 	// "s1"
	RISCV_REG_X10,			// "a0"
	RISCV_REG_A0   = RISCV_REG_X10,	// "a0"
	RISCV_REG_X11,			// "a1"
	RISCV_REG_A1   = RISCV_REG_X11,	// "a1"
	RISCV_REG_X12,			// "a2"
	RISCV_REG_A2   = RISCV_REG_X12,	// "a2"
	RISCV_REG_X13,			// "a3"
	RISCV_REG_A3   = RISCV_REG_X13,	// "a3"
	RISCV_REG_X14,			// "a4"
	RISCV_REG_A4   = RISCV_REG_X14,	// "a4"
	RISCV_REG_X15,			// "a5"
	RISCV_REG_A5   = RISCV_REG_X15, // "a5"
	RISCV_REG_X16,			// "a6"
	RISCV_REG_A6   = RISCV_REG_X16,	// "a6"
	RISCV_REG_X17,			// "a7"
	RISCV_REG_A7   = RISCV_REG_X17,	// "a7"
	RISCV_REG_X18,			// "s2"
	RISCV_REG_S2   = RISCV_REG_X18,	// "s2"
	RISCV_REG_X19,			// "s3"
	RISCV_REG_S3   = RISCV_REG_X19, // "s3"
	RISCV_REG_X20,			// "s4"
	RISCV_REG_S4   = RISCV_REG_X20,	// "s4"
	RISCV_REG_X21,			// "s5"
	RISCV_REG_S5   = RISCV_REG_X21,	// "s5"
	RISCV_REG_X22,			// "s6"
	RISCV_REG_S6   = RISCV_REG_X22,	// "s6"
	RISCV_REG_X23,			// "s7"
	RISCV_REG_S7   = RISCV_REG_X23,	// "s7"
	RISCV_REG_X24,			// "s8"
	RISCV_REG_S8   = RISCV_REG_X24,	// "s8"
	RISCV_REG_X25,			// "s9"
	RISCV_REG_S9   = RISCV_REG_X25,	// "s9"
	RISCV_REG_X26,			// "s10"
	RISCV_REG_S10  = RISCV_REG_X26,	// "s10"
	RISCV_REG_X27,			// "s11"
	RISCV_REG_S11  = RISCV_REG_X27, // "s11"
	RISCV_REG_X28,			// "t3"
	RISCV_REG_T3   = RISCV_REG_X28,	// "t3"
	RISCV_REG_X29,			// "t4"
	RISCV_REG_T4   = RISCV_REG_X29, // "t4"
	RISCV_REG_X30,			// "t5"
	RISCV_REG_T5   = RISCV_REG_X30,	// "t5"
	RISCV_REG_X31,			// "t6"
	RISCV_REG_T6   = RISCV_REG_X31,	// "t6"
	
	//> Floating-point registers
	RISCV_REG_F0_32,		// "ft0"
	RISCV_REG_F0_64,		// "ft0"
	RISCV_REG_F1_32,		// "ft1"
	RISCV_REG_F1_64,		// "ft1"
	RISCV_REG_F2_32,		// "ft2"
	RISCV_REG_F2_64,		// "ft2"
	RISCV_REG_F3_32,		// "ft3"
	RISCV_REG_F3_64,		// "ft3"
	RISCV_REG_F4_32,		// "ft4"
	RISCV_REG_F4_64,		// "ft4"
	RISCV_REG_F5_32,		// "ft5"
	RISCV_REG_F5_64,		// "ft5"
	RISCV_REG_F6_32,		// "ft6"
	RISCV_REG_F6_64,		// "ft6"
	RISCV_REG_F7_32,		// "ft7"
	RISCV_REG_F7_64,		// "ft7"
	RISCV_REG_F8_32,		// "fs0"
	RISCV_REG_F8_64,		// "fs0"
	RISCV_REG_F9_32,		// "fs1"
	RISCV_REG_F9_64,		// "fs1"
	RISCV_REG_F10_32,		// "fa0"
	RISCV_REG_F10_64,		// "fa0"
	RISCV_REG_F11_32,		// "fa1"
	RISCV_REG_F11_64,		// "fa1"
	RISCV_REG_F12_32,		// "fa2"
	RISCV_REG_F12_64,		// "fa2"
	RISCV_REG_F13_32,		// "fa3"
	RISCV_REG_F13_64,		// "fa3"
	RISCV_REG_F14_32,		// "fa4"
	RISCV_REG_F14_64,		// "fa4"
	RISCV_REG_F15_32,		// "fa5"
	RISCV_REG_F15_64,		// "fa5"
	RISCV_REG_F16_32,		// "fa6"
	RISCV_REG_F16_64,		// "fa6"
	RISCV_REG_F17_32,		// "fa7"
	RISCV_REG_F17_64,		// "fa7"
	RISCV_REG_F18_32,		// "fs2"
	RISCV_REG_F18_64,		// "fs2"
	RISCV_REG_F19_32,		// "fs3"
	RISCV_REG_F19_64,		// "fs3"
	RISCV_REG_F20_32,		// "fs4"
	RISCV_REG_F20_64,		// "fs4"
	RISCV_REG_F21_32,		// "fs5"
	RISCV_REG_F21_64,		// "fs5"
	RISCV_REG_F22_32,		// "fs6"
	RISCV_REG_F22_64,		// "fs6"
	RISCV_REG_F23_32,		// "fs7"
	RISCV_REG_F23_64,		// "fs7"
	RISCV_REG_F24_32,		// "fs8"
	RISCV_REG_F24_64,		// "fs8"
	RISCV_REG_F25_32,		// "fs9"
	RISCV_REG_F25_64,		// "fs9"
	RISCV_REG_F26_32,		// "fs10"
	RISCV_REG_F26_64,		// "fs10"
	RISCV_REG_F27_32,		// "fs11"
	RISCV_REG_F27_64,		// "fs11"
	RISCV_REG_F28_32,		// "ft8"
	RISCV_REG_F28_64,		// "ft8"
	RISCV_REG_F29_32,		// "ft9"
	RISCV_REG_F29_64,		// "ft9"
	RISCV_REG_F30_32,		// "ft10"
	RISCV_REG_F30_64,		// "ft10"
	RISCV_REG_F31_32,		// "ft11"
	RISCV_REG_F31_64,		// "ft11"
	
	RISCV_REG_ENDING,		// <-- mark the end of the list or registers
} riscv_reg;

//> RISCV instruction
typedef enum riscv_insn {
  	RISCV_INS_INVALID = 0,

  	RISCV_INS_ADD,
  	RISCV_INS_ADDI,
  	RISCV_INS_ADDIW,
  	RISCV_INS_ADDW,
  	RISCV_INS_AMOADD_D,
  	RISCV_INS_AMOADD_D_AQ,
  	RISCV_INS_AMOADD_D_AQ_RL,
  	RISCV_INS_AMOADD_D_RL,
  	RISCV_INS_AMOADD_W,
  	RISCV_INS_AMOADD_W_AQ,
  	RISCV_INS_AMOADD_W_AQ_RL,
  	RISCV_INS_AMOADD_W_RL,
  	RISCV_INS_AMOAND_D,
  	RISCV_INS_AMOAND_D_AQ,
  	RISCV_INS_AMOAND_D_AQ_RL,
  	RISCV_INS_AMOAND_D_RL,
  	RISCV_INS_AMOAND_W,
  	RISCV_INS_AMOAND_W_AQ,
  	RISCV_INS_AMOAND_W_AQ_RL,
  	RISCV_INS_AMOAND_W_RL,
  	RISCV_INS_AMOMAXU_D,
  	RISCV_INS_AMOMAXU_D_AQ,
  	RISCV_INS_AMOMAXU_D_AQ_RL,
  	RISCV_INS_AMOMAXU_D_RL,
  	RISCV_INS_AMOMAXU_W,
  	RISCV_INS_AMOMAXU_W_AQ,
  	RISCV_INS_AMOMAXU_W_AQ_RL,
  	RISCV_INS_AMOMAXU_W_RL,
  	RISCV_INS_AMOMAX_D,
  	RISCV_INS_AMOMAX_D_AQ,
  	RISCV_INS_AMOMAX_D_AQ_RL,
  	RISCV_INS_AMOMAX_D_RL,
  	RISCV_INS_AMOMAX_W,
  	RISCV_INS_AMOMAX_W_AQ,
  	RISCV_INS_AMOMAX_W_AQ_RL,
  	RISCV_INS_AMOMAX_W_RL,
  	RISCV_INS_AMOMINU_D,
  	RISCV_INS_AMOMINU_D_AQ,
  	RISCV_INS_AMOMINU_D_AQ_RL,
  	RISCV_INS_AMOMINU_D_RL,
  	RISCV_INS_AMOMINU_W,
  	RISCV_INS_AMOMINU_W_AQ,
  	RISCV_INS_AMOMINU_W_AQ_RL,
  	RISCV_INS_AMOMINU_W_RL,
  	RISCV_INS_AMOMIN_D,
  	RISCV_INS_AMOMIN_D_AQ,
  	RISCV_INS_AMOMIN_D_AQ_RL,
  	RISCV_INS_AMOMIN_D_RL,
  	RISCV_INS_AMOMIN_W,
  	RISCV_INS_AMOMIN_W_AQ,
  	RISCV_INS_AMOMIN_W_AQ_RL,
  	RISCV_INS_AMOMIN_W_RL,
  	RISCV_INS_AMOOR_D,
  	RISCV_INS_AMOOR_D_AQ,
  	RISCV_INS_AMOOR_D_AQ_RL,
  	RISCV_INS_AMOOR_D_RL,
  	RISCV_INS_AMOOR_W,
  	RISCV_INS_AMOOR_W_AQ,
  	RISCV_INS_AMOOR_W_AQ_RL,
  	RISCV_INS_AMOOR_W_RL,
  	RISCV_INS_AMOSWAP_D,
  	RISCV_INS_AMOSWAP_D_AQ,
  	RISCV_INS_AMOSWAP_D_AQ_RL,
  	RISCV_INS_AMOSWAP_D_RL,
  	RISCV_INS_AMOSWAP_W,
  	RISCV_INS_AMOSWAP_W_AQ,
  	RISCV_INS_AMOSWAP_W_AQ_RL,
  	RISCV_INS_AMOSWAP_W_RL,
  	RISCV_INS_AMOXOR_D,
  	RISCV_INS_AMOXOR_D_AQ,
  	RISCV_INS_AMOXOR_D_AQ_RL,
  	RISCV_INS_AMOXOR_D_RL,
  	RISCV_INS_AMOXOR_W,
  	RISCV_INS_AMOXOR_W_AQ,
  	RISCV_INS_AMOXOR_W_AQ_RL,
  	RISCV_INS_AMOXOR_W_RL,
  	RISCV_INS_AND,
  	RISCV_INS_ANDI,
  	RISCV_INS_AUIPC,
  	RISCV_INS_BEQ,
  	RISCV_INS_BGE,
  	RISCV_INS_BGEU,
  	RISCV_INS_BLT,
  	RISCV_INS_BLTU,
  	RISCV_INS_BNE,
  	RISCV_INS_CSRRC,
  	RISCV_INS_CSRRCI,
  	RISCV_INS_CSRRS,
  	RISCV_INS_CSRRSI,
 	RISCV_INS_CSRRW,
  	RISCV_INS_CSRRWI,
  	RISCV_INS_C_ADD,
  	RISCV_INS_C_ADDI,
  	RISCV_INS_C_ADDI16SP,
  	RISCV_INS_C_ADDI4SPN,
  	RISCV_INS_C_ADDIW,
  	RISCV_INS_C_ADDW,
  	RISCV_INS_C_AND,
  	RISCV_INS_C_ANDI,
  	RISCV_INS_C_BEQZ,
  	RISCV_INS_C_BNEZ,
  	RISCV_INS_C_EBREAK,
  	RISCV_INS_C_FLD,
  	RISCV_INS_C_FLDSP,
  	RISCV_INS_C_FLW,
  	RISCV_INS_C_FLWSP,
  	RISCV_INS_C_FSD,
  	RISCV_INS_C_FSDSP,
  	RISCV_INS_C_FSW,
  	RISCV_INS_C_FSWSP,
  	RISCV_INS_C_J,
  	RISCV_INS_C_JAL,
  	RISCV_INS_C_JALR,
  	RISCV_INS_C_JR,
  	RISCV_INS_C_LD,
  	RISCV_INS_C_LDSP,
  	RISCV_INS_C_LI,
  	RISCV_INS_C_LUI,
  	RISCV_INS_C_LW,
  	RISCV_INS_C_LWSP,
  	RISCV_INS_C_MV,
  	RISCV_INS_C_NOP,
  	RISCV_INS_C_OR,
  	RISCV_INS_C_SD,
	RISCV_INS_C_SDSP,
  	RISCV_INS_C_SLLI,
  	RISCV_INS_C_SRAI,
  	RISCV_INS_C_SRLI,
  	RISCV_INS_C_SUB,
  	RISCV_INS_C_SUBW,
  	RISCV_INS_C_SW,
  	RISCV_INS_C_SWSP,
  	RISCV_INS_C_UNIMP,
  	RISCV_INS_C_XOR,
  	RISCV_INS_DIV,
  	RISCV_INS_DIVU,
  	RISCV_INS_DIVUW,
  	RISCV_INS_DIVW,
  	RISCV_INS_EBREAK,
  	RISCV_INS_ECALL,
  	RISCV_INS_FADD_D,
  	RISCV_INS_FADD_S,
  	RISCV_INS_FCLASS_D,
  	RISCV_INS_FCLASS_S,
  	RISCV_INS_FCVT_D_L,
  	RISCV_INS_FCVT_D_LU,
  	RISCV_INS_FCVT_D_S,
  	RISCV_INS_FCVT_D_W,
  	RISCV_INS_FCVT_D_WU,
  	RISCV_INS_FCVT_LU_D,
  	RISCV_INS_FCVT_LU_S,
  	RISCV_INS_FCVT_L_D,
  	RISCV_INS_FCVT_L_S,
  	RISCV_INS_FCVT_S_D,
  	RISCV_INS_FCVT_S_L,
  	RISCV_INS_FCVT_S_LU,
  	RISCV_INS_FCVT_S_W,
  	RISCV_INS_FCVT_S_WU,
  	RISCV_INS_FCVT_WU_D,
  	RISCV_INS_FCVT_WU_S,
  	RISCV_INS_FCVT_W_D,
  	RISCV_INS_FCVT_W_S,
  	RISCV_INS_FDIV_D,
  	RISCV_INS_FDIV_S,
  	RISCV_INS_FENCE,
  	RISCV_INS_FENCE_I,
  	RISCV_INS_FENCE_TSO,
  	RISCV_INS_FEQ_D,
  	RISCV_INS_FEQ_S,
  	RISCV_INS_FLD,
  	RISCV_INS_FLE_D,
  	RISCV_INS_FLE_S,
  	RISCV_INS_FLT_D,
  	RISCV_INS_FLT_S,
  	RISCV_INS_FLW,
  	RISCV_INS_FMADD_D,
  	RISCV_INS_FMADD_S,
  	RISCV_INS_FMAX_D,
  	RISCV_INS_FMAX_S,
  	RISCV_INS_FMIN_D,
  	RISCV_INS_FMIN_S,
  	RISCV_INS_FMSUB_D,
  	RISCV_INS_FMSUB_S,
  	RISCV_INS_FMUL_D,
  	RISCV_INS_FMUL_S,
  	RISCV_INS_FMV_D_X,
  	RISCV_INS_FMV_W_X,
  	RISCV_INS_FMV_X_D,
  	RISCV_INS_FMV_X_W,
  	RISCV_INS_FNMADD_D,
  	RISCV_INS_FNMADD_S,
  	RISCV_INS_FNMSUB_D,
  	RISCV_INS_FNMSUB_S,
  	RISCV_INS_FSD,
  	RISCV_INS_FSGNJN_D,
  	RISCV_INS_FSGNJN_S,
  	RISCV_INS_FSGNJX_D,
  	RISCV_INS_FSGNJX_S,
  	RISCV_INS_FSGNJ_D,
  	RISCV_INS_FSGNJ_S,
  	RISCV_INS_FSQRT_D,
  	RISCV_INS_FSQRT_S,
  	RISCV_INS_FSUB_D,
  	RISCV_INS_FSUB_S,
  	RISCV_INS_FSW,
  	RISCV_INS_JAL,
  	RISCV_INS_JALR,
  	RISCV_INS_LB,
  	RISCV_INS_LBU,
  	RISCV_INS_LD,
  	RISCV_INS_LH,
  	RISCV_INS_LHU,
  	RISCV_INS_LR_D,
  	RISCV_INS_LR_D_AQ,
  	RISCV_INS_LR_D_AQ_RL,
  	RISCV_INS_LR_D_RL,
  	RISCV_INS_LR_W,
  	RISCV_INS_LR_W_AQ,
  	RISCV_INS_LR_W_AQ_RL,
  	RISCV_INS_LR_W_RL,
  	RISCV_INS_LUI,
  	RISCV_INS_LW,
  	RISCV_INS_LWU,
  	RISCV_INS_MRET,
  	RISCV_INS_MUL,
  	RISCV_INS_MULH,
  	RISCV_INS_MULHSU,
  	RISCV_INS_MULHU,
  	RISCV_INS_MULW,
  	RISCV_INS_OR,
  	RISCV_INS_ORI,
  	RISCV_INS_REM,
  	RISCV_INS_REMU,
  	RISCV_INS_REMUW,
  	RISCV_INS_REMW,
  	RISCV_INS_SB,
  	RISCV_INS_SC_D,
  	RISCV_INS_SC_D_AQ,
  	RISCV_INS_SC_D_AQ_RL,
  	RISCV_INS_SC_D_RL,
  	RISCV_INS_SC_W,
  	RISCV_INS_SC_W_AQ,
  	RISCV_INS_SC_W_AQ_RL,
  	RISCV_INS_SC_W_RL,
  	RISCV_INS_SD,
  	RISCV_INS_SFENCE_VMA,
  	RISCV_INS_SH,
  	RISCV_INS_SLL,
  	RISCV_INS_SLLI,
  	RISCV_INS_SLLIW,
  	RISCV_INS_SLLW,
  	RISCV_INS_SLT,
  	RISCV_INS_SLTI,
  	RISCV_INS_SLTIU,
  	RISCV_INS_SLTU,
  	RISCV_INS_SRA,
  	RISCV_INS_SRAI,
  	RISCV_INS_SRAIW,
  	RISCV_INS_SRAW,
  	RISCV_INS_SRET,
  	RISCV_INS_SRL,
  	RISCV_INS_SRLI,
  	RISCV_INS_SRLIW,
  	RISCV_INS_SRLW,
  	RISCV_INS_SUB,
  	RISCV_INS_SUBW,
  	RISCV_INS_SW,
  	RISCV_INS_UNIMP,
  	RISCV_INS_URET,
  	RISCV_INS_WFI,
  	RISCV_INS_XOR,
  	RISCV_INS_XORI,	
	
  	RISCV_INS_ENDING,
} riscv_insn;

//> Group of RISCV instructions
typedef enum riscv_insn_group {
  	RISCV_GRP_INVALID = 0, ///< = CS_GRP_INVALID

  	// Generic groups
  	// all jump instructions (conditional+direct+indirect jumps)
  	RISCV_GRP_JUMP,	///< = CS_GRP_JUMP
  	// all call instructions
  	RISCV_GRP_CALL,	///< = CS_GRP_CALL
  	// all return instructions
  	RISCV_GRP_RET,	///< = CS_GRP_RET
  	// all interrupt instructions (int+syscall)
  	RISCV_GRP_INT,	///< = CS_GRP_INT
  	// all interrupt return instructions
  	RISCV_GRP_IRET,	///< = CS_GRP_IRET
  	// all privileged instructions
  	RISCV_GRP_PRIVILEGE,	///< = CS_GRP_PRIVILEGE
  	// all relative branching instructions
  	RISCV_GRP_BRANCH_RELATIVE, ///< = CS_GRP_BRANCH_RELATIVE
  
  	// Architecture-specific groups
  	RISCV_GRP_ISRV32 = 128,
  	RISCV_GRP_ISRV64,
  	RISCV_GRP_HASSTDEXTA,
  	RISCV_GRP_HASSTDEXTC,
  	RISCV_GRP_HASSTDEXTD,
  	RISCV_GRP_HASSTDEXTF,
  	RISCV_GRP_HASSTDEXTM,
  	/*
  	RISCV_GRP_ISRVA,
  	RISCV_GRP_ISRVC,
  	RISCV_GRP_ISRVD,
  	RISCV_GRP_ISRVCD,
  	RISCV_GRP_ISRVF,
  	RISCV_GRP_ISRV32C,
  	RISCV_GRP_ISRV32CF,
  	RISCV_GRP_ISRVM,
  	RISCV_GRP_ISRV64A,
  	RISCV_GRP_ISRV64C,
  	RISCV_GRP_ISRV64D,
  	RISCV_GRP_ISRV64F,
  	RISCV_GRP_ISRV64M,
  	*/
  	RISCV_GRP_ENDING,
} riscv_insn_group;

#ifdef __cplusplus
}
#endif

#endif

