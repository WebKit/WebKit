/* Capstone Disassembly Engine */
/* BPF Backend by david942j <david942j@gmail.com>, 2019 */

#include <string.h>

#include "BPFConstants.h"
#include "BPFMapping.h"
#include "../../utils.h"

#ifndef CAPSTONE_DIET
static const name_map group_name_maps[] = {
	{ BPF_GRP_INVALID, NULL },

	{ BPF_GRP_LOAD, "load" },
	{ BPF_GRP_STORE, "store" },
	{ BPF_GRP_ALU, "alu" },
	{ BPF_GRP_JUMP, "jump" },
	{ BPF_GRP_CALL, "call" },
	{ BPF_GRP_RETURN, "return" },
	{ BPF_GRP_MISC, "misc" },
};
#endif

const char *BPF_group_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	return id2name(group_name_maps, ARR_SIZE(group_name_maps), id);
#else
	return NULL;
#endif
}

#ifndef CAPSTONE_DIET
static const name_map insn_name_maps[BPF_INS_ENDING] = {
	{ BPF_INS_INVALID, NULL },

	{ BPF_INS_ADD, "add" },
	{ BPF_INS_SUB, "sub" },
	{ BPF_INS_MUL, "mul" },
	{ BPF_INS_DIV, "div" },
	{ BPF_INS_OR, "or" },
	{ BPF_INS_AND, "and" },
	{ BPF_INS_LSH, "lsh" },
	{ BPF_INS_RSH, "rsh" },
	{ BPF_INS_NEG, "neg" },
	{ BPF_INS_MOD, "mod" },
	{ BPF_INS_XOR, "xor" },
	{ BPF_INS_MOV, "mov" },
	{ BPF_INS_ARSH, "arsh" },

	{ BPF_INS_ADD64, "add64" },
	{ BPF_INS_SUB64, "sub64" },
	{ BPF_INS_MUL64, "mul64" },
	{ BPF_INS_DIV64, "div64" },
	{ BPF_INS_OR64, "or64" },
	{ BPF_INS_AND64, "and64" },
	{ BPF_INS_LSH64, "lsh64" },
	{ BPF_INS_RSH64, "rsh64" },
	{ BPF_INS_NEG64, "neg64" },
	{ BPF_INS_MOD64, "mod64" },
	{ BPF_INS_XOR64, "xor64" },
	{ BPF_INS_MOV64, "mov64" },
	{ BPF_INS_ARSH64, "arsh64" },

	{ BPF_INS_LE16, "le16" },
	{ BPF_INS_LE32, "le32" },
	{ BPF_INS_LE64, "le64" },
	{ BPF_INS_BE16, "be16" },
	{ BPF_INS_BE32, "be32" },
	{ BPF_INS_BE64, "be64" },

	{ BPF_INS_LDW, "ldw" },
	{ BPF_INS_LDH, "ldh" },
	{ BPF_INS_LDB, "ldb" },
	{ BPF_INS_LDDW,	"lddw" },
	{ BPF_INS_LDXW, "ldxw" },
	{ BPF_INS_LDXH, "ldxh" },
	{ BPF_INS_LDXB, "ldxb" },
	{ BPF_INS_LDXDW, "ldxdw" },

	{ BPF_INS_STW, "stw" },
	{ BPF_INS_STH, "sth" },
	{ BPF_INS_STB, "stb" },
	{ BPF_INS_STDW,	"stdw" },
	{ BPF_INS_STXW, "stxw" },
	{ BPF_INS_STXH, "stxh" },
	{ BPF_INS_STXB, "stxb" },
	{ BPF_INS_STXDW, "stxdw" },
	{ BPF_INS_XADDW, "xaddw" },
	{ BPF_INS_XADDDW, "xadddw" },

	{ BPF_INS_JMP, "jmp" },
	{ BPF_INS_JEQ, "jeq" },
	{ BPF_INS_JGT, "jgt" },
	{ BPF_INS_JGE, "jge" },
	{ BPF_INS_JSET, "jset" },
	{ BPF_INS_JNE, "jne" },
	{ BPF_INS_JSGT,	"jsgt" },
	{ BPF_INS_JSGE,	"jsge" },
	{ BPF_INS_CALL,	"call" },
	{ BPF_INS_CALLX, "callx" },
	{ BPF_INS_EXIT,	"exit" },
	{ BPF_INS_JLT, "jlt" },
	{ BPF_INS_JLE, "jle" },
	{ BPF_INS_JSLT, "jslt" },
	{ BPF_INS_JSLE,	"jsle" },

	{ BPF_INS_RET, "ret" },

	{ BPF_INS_TAX, "tax" },
	{ BPF_INS_TXA, "txa" },
};
#endif

const char *BPF_insn_name(csh handle, unsigned int id)
{
#ifndef CAPSTONE_DIET
	/* We have some special cases because 'ld' in cBPF is equivalent to 'ldw'
	 * in eBPF, and we don't want to see 'ldw' appears in cBPF mode.
	 */
	if (!EBPF_MODE(handle)) {
		switch (id) {
		case BPF_INS_LD: return "ld";
		case BPF_INS_LDX: return "ldx";
		case BPF_INS_ST: return "st";
		case BPF_INS_STX: return "stx";
		}
	}
	return id2name(insn_name_maps, ARR_SIZE(insn_name_maps), id);
#else
	return NULL;
#endif
}

const char *BPF_reg_name(csh handle, unsigned int reg)
{
#ifndef CAPSTONE_DIET
	if (EBPF_MODE(handle)) {
		if (reg < BPF_REG_R0 || reg > BPF_REG_R10)
			return NULL;
		static const char reg_names[11][4] = {
			"r0", "r1", "r2", "r3", "r4",
			"r5", "r6", "r7", "r8", "r9",
			"r10"
		};
		return reg_names[reg - BPF_REG_R0];
	}

	/* cBPF mode */
	if (reg == BPF_REG_A)
		return "a";
	else if (reg == BPF_REG_X)
		return "x";
	else
		return NULL;
#else
	return NULL;
#endif
}

static bpf_insn op2insn_ld(unsigned opcode)
{
#define CASE(c) case BPF_SIZE_##c: \
		if (BPF_CLASS(opcode) == BPF_CLASS_LD) \
			return BPF_INS_LD##c; \
		else \
			return BPF_INS_LDX##c;

	switch (BPF_SIZE(opcode)) {
	CASE(W);
	CASE(H);
	CASE(B);
	CASE(DW);
	}
#undef CASE

	return BPF_INS_INVALID;
}

static bpf_insn op2insn_st(unsigned opcode)
{
	/*
	 * - BPF_STX | BPF_XADD | BPF_{W,DW}
	 * - BPF_ST* | BPF_MEM | BPF_{W,H,B,DW}
	 */

	if (opcode == (BPF_CLASS_STX | BPF_MODE_XADD | BPF_SIZE_W))
		return BPF_INS_XADDW;
	if (opcode == (BPF_CLASS_STX | BPF_MODE_XADD | BPF_SIZE_DW))
		return BPF_INS_XADDDW;

	/* should be BPF_MEM */
#define CASE(c) case BPF_SIZE_##c: \
		if (BPF_CLASS(opcode) == BPF_CLASS_ST) \
			return BPF_INS_ST##c; \
		else \
			return BPF_INS_STX##c;
	switch (BPF_SIZE(opcode)) {
	CASE(W);
	CASE(H);
	CASE(B);
	CASE(DW);
	}
#undef CASE

	return BPF_INS_INVALID;
}

static bpf_insn op2insn_alu(unsigned opcode)
{
	/* Endian is a special case */
	if (BPF_OP(opcode) == BPF_ALU_END) {
		switch (opcode ^ BPF_CLASS_ALU ^ BPF_ALU_END) {
		case BPF_SRC_LITTLE | (16 << 4):
			return BPF_INS_LE16;
		case BPF_SRC_LITTLE | (32 << 4):
			return BPF_INS_LE32;
		case BPF_SRC_LITTLE | (64 << 4):
			return BPF_INS_LE64;
		case BPF_SRC_BIG | (16 << 4):
			return BPF_INS_BE16;
		case BPF_SRC_BIG | (32 << 4):
			return BPF_INS_BE32;
		case BPF_SRC_BIG | (64 << 4):
			return BPF_INS_BE64;
		}
		return BPF_INS_INVALID;
	}

#define CASE(c) case BPF_ALU_##c: \
		if (BPF_CLASS(opcode) == BPF_CLASS_ALU) \
			return BPF_INS_##c; \
		else \
			return BPF_INS_##c##64;

	switch (BPF_OP(opcode)) {
	CASE(ADD);
	CASE(SUB);
	CASE(MUL);
	CASE(DIV);
	CASE(OR);
	CASE(AND);
	CASE(LSH);
	CASE(RSH);
	CASE(NEG);
	CASE(MOD);
	CASE(XOR);
	CASE(MOV);
	CASE(ARSH);
	}
#undef CASE

	return BPF_INS_INVALID;
}

static bpf_insn op2insn_jmp(unsigned opcode)
{
	if (opcode == (BPF_CLASS_JMP | BPF_JUMP_CALL | BPF_SRC_X)) {
		return BPF_INS_CALLX;
	}

#define CASE(c) case BPF_JUMP_##c: return BPF_INS_##c
	switch (BPF_OP(opcode)) {
	case BPF_JUMP_JA:
		return BPF_INS_JMP;
	CASE(JEQ);
	CASE(JGT);
	CASE(JGE);
	CASE(JSET);
	CASE(JNE);
	CASE(JSGT);
	CASE(JSGE);
	CASE(CALL);
	CASE(EXIT);
	CASE(JLT);
	CASE(JLE);
	CASE(JSLT);
	CASE(JSLE);
	}
#undef CASE

	return BPF_INS_INVALID;
}

static void update_regs_access(cs_struct *ud, cs_detail *detail,
		bpf_insn insn_id, unsigned int opcode)
{
	if (insn_id == BPF_INS_INVALID)
		return;
#define PUSH_READ(r) do { \
		detail->regs_read[detail->regs_read_count] = r; \
		detail->regs_read_count++; \
	} while (0)
#define PUSH_WRITE(r) do { \
		detail->regs_write[detail->regs_write_count] = r; \
		detail->regs_write_count++; \
	} while (0)
	/*
	 * In eBPF mode, only these instructions have implicit registers access:
	 * - legacy ld{w,h,b,dw} * // w: r0
	 * - exit // r: r0
	 */
	if (EBPF_MODE(ud)) {
		switch (insn_id) {
		default:
			break;
		case BPF_INS_LDW:
		case BPF_INS_LDH:
		case BPF_INS_LDB:
		case BPF_INS_LDDW:
			if (BPF_MODE(opcode) == BPF_MODE_ABS || BPF_MODE(opcode) == BPF_MODE_IND) {
				PUSH_WRITE(BPF_REG_R0);
			}
			break;
		case BPF_INS_EXIT:
			PUSH_READ(BPF_REG_R0);
			break;
		}
		return;
	}

	/* cBPF mode */
	switch (BPF_CLASS(opcode)) {
	default:
		break;
	case BPF_CLASS_LD:
		PUSH_WRITE(BPF_REG_A);
		break;
	case BPF_CLASS_LDX:
		PUSH_WRITE(BPF_REG_X);
		break;
	case BPF_CLASS_ST:
		PUSH_READ(BPF_REG_A);
		break;
	case BPF_CLASS_STX:
		PUSH_READ(BPF_REG_X);
		break;
	case BPF_CLASS_ALU:
		PUSH_READ(BPF_REG_A);
		PUSH_WRITE(BPF_REG_A);
		break;
	case BPF_CLASS_JMP:
		if (insn_id != BPF_INS_JMP) // except the unconditional jump
			PUSH_READ(BPF_REG_A);
		break;
	/* case BPF_CLASS_RET: */
	case BPF_CLASS_MISC:
		if (insn_id == BPF_INS_TAX) {
			PUSH_READ(BPF_REG_A);
			PUSH_WRITE(BPF_REG_X);
		}
		else {
			PUSH_READ(BPF_REG_X);
			PUSH_WRITE(BPF_REG_A);
		}
		break;
	}
}

/*
 * 1. Convert opcode(id) to BPF_INS_*
 * 2. Set regs_read/regs_write/groups
 */
void BPF_get_insn_id(cs_struct *ud, cs_insn *insn, unsigned int opcode)
{
	// No need to care the mode (cBPF or eBPF) since all checks has be done in
	// BPF_getInstruction, we can simply map opcode to BPF_INS_*.
	cs_detail *detail;
	bpf_insn id = BPF_INS_INVALID;
	bpf_insn_group grp;

	detail = insn->detail;
#ifndef CAPSTONE_DIET
 #define PUSH_GROUP(grp) do { \
		if (detail) { \
			detail->groups[detail->groups_count] = grp; \
			detail->groups_count++; \
		} \
	} while(0)
#else
 #define PUSH_GROUP
#endif

	switch (BPF_CLASS(opcode)) {
	default:	// will never happen
		break;
	case BPF_CLASS_LD:
	case BPF_CLASS_LDX:
		id = op2insn_ld(opcode);
		PUSH_GROUP(BPF_GRP_LOAD);
		break;
	case BPF_CLASS_ST:
	case BPF_CLASS_STX:
		id = op2insn_st(opcode);
		PUSH_GROUP(BPF_GRP_STORE);
		break;
	case BPF_CLASS_ALU:
		id = op2insn_alu(opcode);
		PUSH_GROUP(BPF_GRP_ALU);
		break;
	case BPF_CLASS_JMP:
		grp = BPF_GRP_JUMP;
		id = op2insn_jmp(opcode);
		if (id == BPF_INS_CALL || id == BPF_INS_CALLX)
			grp = BPF_GRP_CALL;
		else if (id == BPF_INS_EXIT)
			grp = BPF_GRP_RETURN;
		PUSH_GROUP(grp);
		break;
	case BPF_CLASS_RET:
		id = BPF_INS_RET;
		PUSH_GROUP(BPF_GRP_RETURN);
		break;
	// BPF_CLASS_MISC and BPF_CLASS_ALU64 have exactly same value
	case BPF_CLASS_MISC:
	/* case BPF_CLASS_ALU64: */
		if (EBPF_MODE(ud)) {
			// ALU64 in eBPF
			id = op2insn_alu(opcode);
			PUSH_GROUP(BPF_GRP_ALU);
		}
		else {
			if (BPF_MISCOP(opcode) == BPF_MISCOP_TXA)
				id = BPF_INS_TXA;
			else
				id = BPF_INS_TAX;
			PUSH_GROUP(BPF_GRP_MISC);
		}
		break;
	}

	insn->id = id;
#undef PUSH_GROUP

#ifndef CAPSTONE_DIET
	if (detail) {
		update_regs_access(ud, detail, id, opcode);
	}
#endif
}

static void sort_and_uniq(cs_regs arr, uint8_t n, uint8_t *new_n)
{
	/* arr is always a tiny (usually n < 3) array,
	 * a simple O(n^2) sort is efficient enough. */
	int i;
	int j;
	int iMin;
	int tmp;

	/* a modified selection sort for sorting and making unique */
	for (j = 0; j < n; j++) {
		/* arr[iMin] will be min(arr[j .. n-1]) */
		iMin = j;
		for (i = j + 1; i < n; i++) {
			if (arr[i] < arr[iMin])
				iMin = i;
		}
		if (j != 0 && arr[iMin] == arr[j - 1]) { // duplicate ele found
			arr[iMin] = arr[n - 1];
			--n;
		}
		else {
			tmp = arr[iMin];
			arr[iMin] = arr[j];
			arr[j] = tmp;
		}
	}

	*new_n = n;
}
void BPF_reg_access(const cs_insn *insn,
		cs_regs regs_read, uint8_t *regs_read_count,
		cs_regs regs_write, uint8_t *regs_write_count)
{
	unsigned i;
	uint8_t read_count, write_count;
	const cs_bpf *bpf = &(insn->detail->bpf);

	read_count = insn->detail->regs_read_count;
	write_count = insn->detail->regs_write_count;

	// implicit registers
	memcpy(regs_read, insn->detail->regs_read, read_count * sizeof(insn->detail->regs_read[0]));
	memcpy(regs_write, insn->detail->regs_write, write_count * sizeof(insn->detail->regs_write[0]));

	for (i = 0; i < bpf->op_count; i++) {
		const cs_bpf_op *op = &(bpf->operands[i]);
		switch (op->type) {
		default:
			break;
		case BPF_OP_REG:
			if (op->access & CS_AC_READ) {
				regs_read[read_count] = op->reg;
				read_count++;
			}
			if (op->access & CS_AC_WRITE) {
				regs_write[write_count] = op->reg;
				write_count++;
			}
			break;
		case BPF_OP_MEM:
			if (op->mem.base != BPF_REG_INVALID) {
				regs_read[read_count] = op->mem.base;
				read_count++;
			}
			break;
		}
	}

	sort_and_uniq(regs_read, read_count, regs_read_count);
	sort_and_uniq(regs_write, write_count, regs_write_count);
}
