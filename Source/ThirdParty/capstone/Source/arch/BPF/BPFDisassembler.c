/* Capstone Disassembly Engine */
/* BPF Backend by david942j <david942j@gmail.com>, 2019 */

#ifdef CAPSTONE_HAS_BPF

#include <string.h>
#include <stddef.h> // offsetof macro

#include "BPFConstants.h"
#include "BPFDisassembler.h"
#include "BPFMapping.h"
#include "../../cs_priv.h"

static uint16_t read_u16(cs_struct *ud, const uint8_t *code)
{
	if (MODE_IS_BIG_ENDIAN(ud->mode))
		return (((uint16_t)code[0] << 8) | code[1]);
	else
		return (((uint16_t)code[1] << 8) | code[0]);
}

static uint32_t read_u32(cs_struct *ud, const uint8_t *code)
{
	if (MODE_IS_BIG_ENDIAN(ud->mode))
		return ((uint32_t)read_u16(ud, code) << 16) | read_u16(ud, code + 2);
	else
		return ((uint32_t)read_u16(ud, code + 2) << 16) | read_u16(ud, code);
}

///< Malloc bpf_internal, also checks if code_len is large enough.
static bpf_internal *alloc_bpf_internal(size_t code_len)
{
	bpf_internal *bpf;

	if (code_len < 8)
		return NULL;
	bpf = cs_mem_malloc(sizeof(bpf_internal));
	if (bpf == NULL)
		return NULL;
	/* default value */
	bpf->insn_size = 8;
	return bpf;
}

///< Fetch a cBPF structure from code
static bpf_internal* fetch_cbpf(cs_struct *ud, const uint8_t *code,
		size_t code_len)
{
	bpf_internal *bpf;

	bpf = alloc_bpf_internal(code_len);
	if (bpf == NULL)
		return NULL;

	bpf->op = read_u16(ud, code);
	bpf->jt = code[2];
	bpf->jf = code[3];
	bpf->k = read_u32(ud, code + 4);
	return bpf;
}

///< Fetch an eBPF structure from code
static bpf_internal* fetch_ebpf(cs_struct *ud, const uint8_t *code,
		size_t code_len)
{
	bpf_internal *bpf;

	bpf = alloc_bpf_internal(code_len);
	if (bpf == NULL)
		return NULL;

	bpf->op = (uint16_t)code[0];
	bpf->dst = code[1] & 0xf;
	bpf->src = (code[1] & 0xf0) >> 4;

	// eBPF has one 16-byte instruction: BPF_LD | BPF_DW | BPF_IMM,
	// in this case imm is combined with the next block's imm.
	if (bpf->op == (BPF_CLASS_LD | BPF_SIZE_DW | BPF_MODE_IMM)) {
		if (code_len < 16) {
			cs_mem_free(bpf);
			return NULL;
		}
		bpf->k = read_u32(ud, code + 4) | (((uint64_t)read_u32(ud, code + 12)) << 32);
		bpf->insn_size = 16;
	}
	else {
		bpf->offset = read_u16(ud, code + 2);
		bpf->k = read_u32(ud, code + 4);
	}
	return bpf;
}

#define CHECK_READABLE_REG(ud, reg) do { \
		if (! ((reg) >= BPF_REG_R0 && (reg) <= BPF_REG_R10)) \
			return false; \
	} while (0)

#define CHECK_WRITABLE_REG(ud, reg) do { \
		if (! ((reg) >= BPF_REG_R0 && (reg) < BPF_REG_R10)) \
			return false; \
	} while (0)

#define CHECK_READABLE_AND_PUSH(ud, MI, r) do { \
		CHECK_READABLE_REG(ud, r + BPF_REG_R0); \
		MCOperand_CreateReg0(MI, r + BPF_REG_R0); \
	} while (0)

#define CHECK_WRITABLE_AND_PUSH(ud, MI, r) do { \
		CHECK_WRITABLE_REG(ud, r + BPF_REG_R0); \
		MCOperand_CreateReg0(MI, r + BPF_REG_R0); \
	} while (0)

static bool decodeLoad(cs_struct *ud, MCInst *MI, bpf_internal *bpf)
{
	if (!EBPF_MODE(ud)) {
		/*
		 *  +-----+-----------+--------------------+
		 *  | ldb |    [k]    |       [x+k]        |
		 *  | ldh |    [k]    |       [x+k]        |
		 *  +-----+-----------+--------------------+
		 */
		if (BPF_SIZE(bpf->op) == BPF_SIZE_DW)
			return false;
		if (BPF_SIZE(bpf->op) == BPF_SIZE_B || BPF_SIZE(bpf->op) == BPF_SIZE_H) {
			/* no ldx */
			if (BPF_CLASS(bpf->op) != BPF_CLASS_LD)
				return false;
			/* can only be BPF_ABS and BPF_IND */
			if (BPF_MODE(bpf->op) == BPF_MODE_ABS) {
				MCOperand_CreateImm0(MI, bpf->k);
				return true;
			}
			else if (BPF_MODE(bpf->op) == BPF_MODE_IND) {
				MCOperand_CreateReg0(MI, BPF_REG_X);
				MCOperand_CreateImm0(MI, bpf->k);
				return true;
			}
			return false;
		}
		/*
		 *  +-----+----+------+------+-----+-------+
		 *  | ld  | #k | #len | M[k] | [k] | [x+k] |
		 *  +-----+----+------+------+-----+-------+
		 *  | ldx | #k | #len | M[k] | 4*([k]&0xf) |
		 *  +-----+----+------+------+-------------+
		 */
		switch (BPF_MODE(bpf->op)) {
		default:
			break;
		case BPF_MODE_IMM:
			MCOperand_CreateImm0(MI, bpf->k);
			return true;
		case BPF_MODE_LEN:
			return true;
		case BPF_MODE_MEM:
			MCOperand_CreateImm0(MI, bpf->k);
			return true;
		}
		if (BPF_CLASS(bpf->op) == BPF_CLASS_LD) {
			if (BPF_MODE(bpf->op) == BPF_MODE_ABS) {
				MCOperand_CreateImm0(MI, bpf->k);
				return true;
			}
			else if (BPF_MODE(bpf->op) == BPF_MODE_IND) {
				MCOperand_CreateReg0(MI, BPF_REG_X);
				MCOperand_CreateImm0(MI, bpf->k);
				return true;
			}
		}
		else { /* LDX */
			if (BPF_MODE(bpf->op) == BPF_MODE_MSH) {
				MCOperand_CreateImm0(MI, bpf->k);
				return true;
			}
		}
		return false;
	}

	/* eBPF mode */
	/*
	 * - IMM: lddw dst, imm64
	 * - ABS: ld{w,h,b,dw} [k]
	 * - IND: ld{w,h,b,dw} [src+k]
	 * - MEM: ldx{w,h,b,dw} dst, [src+off]
	 */
	if (BPF_CLASS(bpf->op) == BPF_CLASS_LD) {
		switch (BPF_MODE(bpf->op)) {
		case BPF_MODE_IMM:
			if (bpf->op != (BPF_CLASS_LD | BPF_SIZE_DW | BPF_MODE_IMM))
				return false;
			CHECK_WRITABLE_AND_PUSH(ud, MI, bpf->dst);
			MCOperand_CreateImm0(MI, bpf->k);
			return true;
		case BPF_MODE_ABS:
			MCOperand_CreateImm0(MI, bpf->k);
			return true;
		case BPF_MODE_IND:
			CHECK_READABLE_AND_PUSH(ud, MI, bpf->src);
			MCOperand_CreateImm0(MI, bpf->k);
			return true;
		}
		return false;

	}
	/* LDX */
	if (BPF_MODE(bpf->op) == BPF_MODE_MEM) {
		CHECK_WRITABLE_AND_PUSH(ud, MI, bpf->dst);
		CHECK_READABLE_AND_PUSH(ud, MI, bpf->src);
		MCOperand_CreateImm0(MI, bpf->offset);
		return true;
	}
	return false;
}

static bool decodeStore(cs_struct *ud, MCInst *MI, bpf_internal *bpf)
{
	/* in cBPF, only BPF_ST* | BPF_MEM | BPF_W is valid
	 * while in eBPF:
	 * - BPF_STX | BPF_XADD | BPF_{W,DW}
	 * - BPF_ST* | BPF_MEM | BPF_{W,H,B,DW}
	 * are valid
	 */
	if (!EBPF_MODE(ud)) {
		/* can only store to M[] */
		if (bpf->op != (BPF_CLASS(bpf->op) | BPF_MODE_MEM | BPF_SIZE_W))
			return false;
		MCOperand_CreateImm0(MI, bpf->k);
		return true;
	}

	/* eBPF */

	if (BPF_MODE(bpf->op) == BPF_MODE_XADD) {
		if (BPF_CLASS(bpf->op) != BPF_CLASS_STX)
			return false;
		if (BPF_SIZE(bpf->op) != BPF_SIZE_W && BPF_SIZE(bpf->op) != BPF_SIZE_DW)
			return false;
		/* xadd [dst + off], src */
		CHECK_READABLE_AND_PUSH(ud, MI, bpf->dst);
		MCOperand_CreateImm0(MI, bpf->offset);
		CHECK_READABLE_AND_PUSH(ud, MI, bpf->src);
		return true;
	}

	if (BPF_MODE(bpf->op) != BPF_MODE_MEM)
		return false;

	/* st [dst + off], src */
	CHECK_READABLE_AND_PUSH(ud, MI, bpf->dst);
	MCOperand_CreateImm0(MI, bpf->offset);
	if (BPF_CLASS(bpf->op) == BPF_CLASS_ST)
		MCOperand_CreateImm0(MI, bpf->k);
	else
		CHECK_READABLE_AND_PUSH(ud, MI, bpf->src);
	return true;
}

static bool decodeALU(cs_struct *ud, MCInst *MI, bpf_internal *bpf)
{
	/* Set MI->Operands */

	/* cBPF */
	if (!EBPF_MODE(ud)) {
		if (BPF_OP(bpf->op) > BPF_ALU_XOR)
			return false;
		/* cBPF's NEG has no operands */
		if (BPF_OP(bpf->op) == BPF_ALU_NEG)
			return true;
		if (BPF_SRC(bpf->op) == BPF_SRC_K)
			MCOperand_CreateImm0(MI, bpf->k);
		else /* BPF_SRC_X */
			MCOperand_CreateReg0(MI, BPF_REG_X);
		return true;
	}

	/* eBPF */

	if (BPF_OP(bpf->op) > BPF_ALU_END)
		return false;
	/* ALU64 class doesn't have ENDian */
	/* ENDian's imm must be one of 16, 32, 64 */
	if (BPF_OP(bpf->op) == BPF_ALU_END) {
		if (BPF_CLASS(bpf->op) == BPF_CLASS_ALU64)
			return false;
		if (bpf->k != 16 && bpf->k != 32 && bpf->k != 64)
			return false;
	}

	/* - op dst, imm
	 * - op dst, src
	 * - neg dst
	 * - le<imm> dst
	 */
	/* every ALU instructions have dst op */
	CHECK_WRITABLE_AND_PUSH(ud, MI, bpf->dst);

	/* special cases */
	if (BPF_OP(bpf->op) == BPF_ALU_NEG)
		return true;
	if (BPF_OP(bpf->op) == BPF_ALU_END) {
		/* bpf->k must be one of 16, 32, 64 */
		MCInst_setOpcode(MI, MCInst_getOpcode(MI) | ((uint32_t)bpf->k << 4));
		return true;
	}

	/* normal cases */
	if (BPF_SRC(bpf->op) == BPF_SRC_K) {
		MCOperand_CreateImm0(MI, bpf->k);
	}
	else { /* BPF_SRC_X */
		CHECK_READABLE_AND_PUSH(ud, MI, bpf->src);
	}
	return true;
}

static bool decodeJump(cs_struct *ud, MCInst *MI, bpf_internal *bpf)
{
	/* cBPF and eBPF are very different in class jump */
	if (!EBPF_MODE(ud)) {
		if (BPF_OP(bpf->op) > BPF_JUMP_JSET)
			return false;

		/* ja is a special case of jumps */
		if (BPF_OP(bpf->op) == BPF_JUMP_JA) {
			MCOperand_CreateImm0(MI, bpf->k);
			return true;
		}

		if (BPF_SRC(bpf->op) == BPF_SRC_K)
			MCOperand_CreateImm0(MI, bpf->k);
		else /* BPF_SRC_X */
			MCOperand_CreateReg0(MI, BPF_REG_X);
		MCOperand_CreateImm0(MI, bpf->jt);
		MCOperand_CreateImm0(MI, bpf->jf);
	}
	else {
		if (BPF_OP(bpf->op) > BPF_JUMP_JSLE)
			return false;

		/* No operands for exit */
		if (BPF_OP(bpf->op) == BPF_JUMP_EXIT)
			return bpf->op == (BPF_CLASS_JMP | BPF_JUMP_EXIT);
		if (BPF_OP(bpf->op) == BPF_JUMP_CALL) {
			if (bpf->op == (BPF_CLASS_JMP | BPF_JUMP_CALL)) {
				MCOperand_CreateImm0(MI, bpf->k);
				return true;
			}
			if (bpf->op == (BPF_CLASS_JMP | BPF_JUMP_CALL | BPF_SRC_X)) {
				CHECK_READABLE_AND_PUSH(ud, MI, bpf->k);
				return true;
			}
			return false;
		}

		/* ja is a special case of jumps */
		if (BPF_OP(bpf->op) == BPF_JUMP_JA) {
			if (BPF_SRC(bpf->op) != BPF_SRC_K)
				return false;
			MCOperand_CreateImm0(MI, bpf->offset);
			return true;
		}

		/* <j>  dst, src, +off */
		CHECK_READABLE_AND_PUSH(ud, MI, bpf->dst);
		if (BPF_SRC(bpf->op) == BPF_SRC_K)
			MCOperand_CreateImm0(MI, bpf->k);
		else
			CHECK_READABLE_AND_PUSH(ud, MI, bpf->src);
		MCOperand_CreateImm0(MI, bpf->offset);
	}
	return true;
}

static bool decodeReturn(cs_struct *ud, MCInst *MI, bpf_internal *bpf)
{
	/* Here only handles the BPF_RET class in cBPF */
	switch (BPF_RVAL(bpf->op)) {
	case BPF_SRC_K:
		MCOperand_CreateImm0(MI, bpf->k);
		return true;
	case BPF_SRC_X:
		MCOperand_CreateReg0(MI, BPF_REG_X);
		return true;
	case BPF_SRC_A:
		MCOperand_CreateReg0(MI, BPF_REG_A);
		return true;
	}
	return false;
}

static bool decodeMISC(cs_struct *ud, MCInst *MI, bpf_internal *bpf)
{
	uint16_t op = bpf->op ^ BPF_CLASS_MISC;
	return op == BPF_MISCOP_TAX || op == BPF_MISCOP_TXA;
}

///< 1. Check if the instruction is valid
///< 2. Set MI->opcode
///< 3. Set MI->Operands
static bool getInstruction(cs_struct *ud, MCInst *MI, bpf_internal *bpf)
{
	cs_detail *detail;

	detail = MI->flat_insn->detail;
	// initialize detail
	if (detail) {
		memset(detail, 0, offsetof(cs_detail, bpf) + sizeof(cs_bpf));
	}

	MCInst_clear(MI);
	MCInst_setOpcode(MI, bpf->op);

	switch (BPF_CLASS(bpf->op)) {
	default: /* should never happen */
		return false;
	case BPF_CLASS_LD:
	case BPF_CLASS_LDX:
		return decodeLoad(ud, MI, bpf);
	case BPF_CLASS_ST:
	case BPF_CLASS_STX:
		return decodeStore(ud, MI, bpf);
	case BPF_CLASS_ALU:
		return decodeALU(ud, MI, bpf);
	case BPF_CLASS_JMP:
		return decodeJump(ud, MI, bpf);
	case BPF_CLASS_RET:
		/* eBPF doesn't have this class */
		if (EBPF_MODE(ud))
			return false;
		return decodeReturn(ud, MI, bpf);
	case BPF_CLASS_MISC:
	/* case BPF_CLASS_ALU64: */
		if (EBPF_MODE(ud))
			return decodeALU(ud, MI, bpf);
		else
			return decodeMISC(ud, MI, bpf);
	}
}

bool BPF_getInstruction(csh ud, const uint8_t *code, size_t code_len,
		MCInst *instr, uint16_t *size, uint64_t address, void *info)
{
	cs_struct *cs;
	bpf_internal *bpf;

	cs = (cs_struct*)ud;
	if (EBPF_MODE(cs))
		bpf = fetch_ebpf(cs, code, code_len);
	else
		bpf = fetch_cbpf(cs, code, code_len);
	if (bpf == NULL)
		return false;
	if (!getInstruction(cs, instr, bpf)) {
		cs_mem_free(bpf);
		return false;
	}

	*size = bpf->insn_size;
	cs_mem_free(bpf);

	return true;
}

#endif
