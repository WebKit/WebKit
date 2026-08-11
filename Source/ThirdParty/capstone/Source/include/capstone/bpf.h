/* Capstone Disassembly Engine */
/* BPF Backend by david942j <david942j@gmail.com>, 2019 */

#ifndef CAPSTONE_BPF_H
#define CAPSTONE_BPF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"

#ifdef _MSC_VER
#pragma warning(disable:4201)
#endif

/// Operand type for instruction's operands
typedef enum bpf_op_type {
	BPF_OP_INVALID = 0,

	BPF_OP_REG,
	BPF_OP_IMM,
	BPF_OP_OFF,
	BPF_OP_MEM,
	BPF_OP_MMEM,	///< M[k] in cBPF
	BPF_OP_MSH,	///< corresponds to cBPF's BPF_MSH mode
	BPF_OP_EXT,	///< cBPF's extension (not eBPF)
} bpf_op_type;

/// BPF registers
typedef enum bpf_reg {
	BPF_REG_INVALID = 0,

	///< cBPF
	BPF_REG_A,
	BPF_REG_X,

	///< eBPF
	BPF_REG_R0,
	BPF_REG_R1,
	BPF_REG_R2,
	BPF_REG_R3,
	BPF_REG_R4,
	BPF_REG_R5,
	BPF_REG_R6,
	BPF_REG_R7,
	BPF_REG_R8,
	BPF_REG_R9,
	BPF_REG_R10,

	BPF_REG_ENDING,
} bpf_reg;

/// Instruction's operand referring to memory
/// This is associated with BPF_OP_MEM operand type above
typedef struct bpf_op_mem {
	bpf_reg base;	///< base register
	uint32_t disp;	///< offset value
} bpf_op_mem;

typedef enum bpf_ext_type {
	BPF_EXT_INVALID = 0,

	BPF_EXT_LEN,
} bpf_ext_type;

/// Instruction operand
typedef struct cs_bpf_op {
	bpf_op_type type;
	union {
		uint8_t reg;	///< register value for REG operand
		uint64_t imm;	///< immediate value IMM operand
		uint32_t off;	///< offset value, used in jump & call
		bpf_op_mem mem;	///< base/disp value for MEM operand
		/* cBPF only */
		uint32_t mmem;	///< M[k] in cBPF
		uint32_t msh;	///< corresponds to cBPF's BPF_MSH mode
		uint32_t ext;	///< cBPF's extension (not eBPF)
	};

	/// How is this operand accessed? (READ, WRITE or READ|WRITE)
	/// This field is combined of cs_ac_type.
	/// NOTE: this field is irrelevant if engine is compiled in DIET mode.
	uint8_t access;
} cs_bpf_op;

/// Instruction structure
typedef struct cs_bpf {
	uint8_t op_count;
	cs_bpf_op operands[4];
} cs_bpf;

/// BPF instruction
typedef enum bpf_insn {
	BPF_INS_INVALID = 0,

	///< ALU
	BPF_INS_ADD,
	BPF_INS_SUB,
	BPF_INS_MUL,
	BPF_INS_DIV,
	BPF_INS_OR,
	BPF_INS_AND,
	BPF_INS_LSH,
	BPF_INS_RSH,
	BPF_INS_NEG,
	BPF_INS_MOD,
	BPF_INS_XOR,
	BPF_INS_MOV,	///< eBPF only
	BPF_INS_ARSH,	///< eBPF only

	///< ALU64, eBPF only
	BPF_INS_ADD64,
	BPF_INS_SUB64,
	BPF_INS_MUL64,
	BPF_INS_DIV64,
	BPF_INS_OR64,
	BPF_INS_AND64,
	BPF_INS_LSH64,
	BPF_INS_RSH64,
	BPF_INS_NEG64,
	BPF_INS_MOD64,
	BPF_INS_XOR64,
	BPF_INS_MOV64,
	BPF_INS_ARSH64,

	///< Byteswap, eBPF only
	BPF_INS_LE16,
	BPF_INS_LE32,
	BPF_INS_LE64,
	BPF_INS_BE16,
	BPF_INS_BE32,
	BPF_INS_BE64,

	///< Load
	BPF_INS_LDW,	///< eBPF only
	BPF_INS_LDH,
	BPF_INS_LDB,
	BPF_INS_LDDW,	///< eBPF only: load 64-bit imm
	BPF_INS_LDXW,	///< eBPF only
	BPF_INS_LDXH,	///< eBPF only
	BPF_INS_LDXB,	///< eBPF only
	BPF_INS_LDXDW,	///< eBPF only

	///< Store
	BPF_INS_STW,	///< eBPF only
	BPF_INS_STH,	///< eBPF only
	BPF_INS_STB,	///< eBPF only
	BPF_INS_STDW,	///< eBPF only
	BPF_INS_STXW,	///< eBPF only
	BPF_INS_STXH,	///< eBPF only
	BPF_INS_STXB,	///< eBPF only
	BPF_INS_STXDW,	///< eBPF only
	BPF_INS_XADDW,	///< eBPF only
	BPF_INS_XADDDW,	///< eBPF only

	///< Jump
	BPF_INS_JMP,
	BPF_INS_JEQ,
	BPF_INS_JGT,
	BPF_INS_JGE,
	BPF_INS_JSET,
	BPF_INS_JNE,	///< eBPF only
	BPF_INS_JSGT,	///< eBPF only
	BPF_INS_JSGE,	///< eBPF only
	BPF_INS_CALL,	///< eBPF only
	BPF_INS_CALLX,	///< eBPF only
	BPF_INS_EXIT,	///< eBPF only
	BPF_INS_JLT,	///< eBPF only
	BPF_INS_JLE,	///< eBPF only
	BPF_INS_JSLT,	///< eBPF only
	BPF_INS_JSLE,	///< eBPF only

	///< Return, cBPF only
	BPF_INS_RET,

	///< Misc, cBPF only
	BPF_INS_TAX,
	BPF_INS_TXA,

	BPF_INS_ENDING,

	// alias instructions
	BPF_INS_LD = BPF_INS_LDW,	///< cBPF only
	BPF_INS_LDX = BPF_INS_LDXW,	///< cBPF only
	BPF_INS_ST = BPF_INS_STW,	///< cBPF only
	BPF_INS_STX = BPF_INS_STXW,	///< cBPF only
} bpf_insn;

/// Group of BPF instructions
typedef enum bpf_insn_group {
	BPF_GRP_INVALID = 0, ///< = CS_GRP_INVALID

	BPF_GRP_LOAD,
	BPF_GRP_STORE,
	BPF_GRP_ALU,
	BPF_GRP_JUMP,
	BPF_GRP_CALL, ///< eBPF only
	BPF_GRP_RETURN,
	BPF_GRP_MISC, ///< cBPF only

	BPF_GRP_ENDING,
} bpf_insn_group;

#ifdef __cplusplus
}
#endif

#endif
