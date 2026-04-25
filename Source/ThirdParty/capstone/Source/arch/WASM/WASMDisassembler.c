/* Capstone Disassembly Engine */
/* By Spike, xwings 2019 */

#include <string.h>
#include <stddef.h> // offsetof macro
// alternatively #include "../../utils.h" like everyone else

#include "WASMDisassembler.h"
#include "WASMMapping.h"
#include "../../cs_priv.h"

static const short opcodes[256] = {
	WASM_INS_UNREACHABLE,
	WASM_INS_NOP,
	WASM_INS_BLOCK,
	WASM_INS_LOOP,
	WASM_INS_IF,
	WASM_INS_ELSE,
	-1,
	-1,
	-1,
	-1,
	-1,
	WASM_INS_END,
	WASM_INS_BR,
	WASM_INS_BR_IF,
	WASM_INS_BR_TABLE,
	WASM_INS_RETURN,
	WASM_INS_CALL,
	WASM_INS_CALL_INDIRECT,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	WASM_INS_DROP,
	WASM_INS_SELECT,
	-1,
	-1,
	-1,
	-1,
	WASM_INS_GET_LOCAL,
	WASM_INS_SET_LOCAL,
	WASM_INS_TEE_LOCAL,
	WASM_INS_GET_GLOBAL,
	WASM_INS_SET_GLOBAL,
	-1,
	-1,
	-1,
	WASM_INS_I32_LOAD,
	WASM_INS_I64_LOAD,
	WASM_INS_F32_LOAD,
	WASM_INS_F64_LOAD,
	WASM_INS_I32_LOAD8_S,
	WASM_INS_I32_LOAD8_U,
	WASM_INS_I32_LOAD16_S,
	WASM_INS_I32_LOAD16_U,
	WASM_INS_I64_LOAD8_S,
	WASM_INS_I64_LOAD8_U,
	WASM_INS_I64_LOAD16_S,
	WASM_INS_I64_LOAD16_U,
	WASM_INS_I64_LOAD32_S,
	WASM_INS_I64_LOAD32_U,
	WASM_INS_I32_STORE,
	WASM_INS_I64_STORE,
	WASM_INS_F32_STORE,
	WASM_INS_F64_STORE,
	WASM_INS_I32_STORE8,
	WASM_INS_I32_STORE16,
	WASM_INS_I64_STORE8,
	WASM_INS_I64_STORE16,
	WASM_INS_I64_STORE32,
	WASM_INS_CURRENT_MEMORY,
	WASM_INS_GROW_MEMORY,
	WASM_INS_I32_CONST,
	WASM_INS_I64_CONST,
	WASM_INS_F32_CONST,
	WASM_INS_F64_CONST,
	WASM_INS_I32_EQZ,
	WASM_INS_I32_EQ,
	WASM_INS_I32_NE,
	WASM_INS_I32_LT_S,
	WASM_INS_I32_LT_U,
	WASM_INS_I32_GT_S,
	WASM_INS_I32_GT_U,
	WASM_INS_I32_LE_S,
	WASM_INS_I32_LE_U,
	WASM_INS_I32_GE_S,
	WASM_INS_I32_GE_U,
	WASM_INS_I64_EQZ,
	WASM_INS_I64_EQ,
	WASM_INS_I64_NE,
	WASM_INS_I64_LT_S,
	WASM_INS_I64_LT_U,
	WASN_INS_I64_GT_S,
	WASM_INS_I64_GT_U,
	WASM_INS_I64_LE_S,
	WASM_INS_I64_LE_U,
	WASM_INS_I64_GE_S,
	WASM_INS_I64_GE_U,
	WASM_INS_F32_EQ,
	WASM_INS_F32_NE,
	WASM_INS_F32_LT,
	WASM_INS_F32_GT,
	WASM_INS_F32_LE,
	WASM_INS_F32_GE,
	WASM_INS_F64_EQ,
	WASM_INS_F64_NE,
	WASM_INS_F64_LT,
	WASM_INS_F64_GT,
	WASM_INS_F64_LE,
	WASM_INS_F64_GE,
	WASM_INS_I32_CLZ,
	WASM_INS_I32_CTZ,
	WASM_INS_I32_POPCNT,
	WASM_INS_I32_ADD,
	WASM_INS_I32_SUB,
	WASM_INS_I32_MUL,
	WASM_INS_I32_DIV_S,
	WASM_INS_I32_DIV_U,
	WASM_INS_I32_REM_S,
	WASM_INS_I32_REM_U,
	WASM_INS_I32_AND,
	WASM_INS_I32_OR,
	WASM_INS_I32_XOR,
	WASM_INS_I32_SHL,
	WASM_INS_I32_SHR_S,
	WASM_INS_I32_SHR_U,
	WASM_INS_I32_ROTL,
	WASM_INS_I32_ROTR,
	WASM_INS_I64_CLZ,
	WASM_INS_I64_CTZ,
	WASM_INS_I64_POPCNT,
	WASM_INS_I64_ADD,
	WASM_INS_I64_SUB,
	WASM_INS_I64_MUL,
	WASM_INS_I64_DIV_S,
	WASM_INS_I64_DIV_U,
	WASM_INS_I64_REM_S,
	WASM_INS_I64_REM_U,
	WASM_INS_I64_AND,
	WASM_INS_I64_OR,
	WASM_INS_I64_XOR,
	WASM_INS_I64_SHL,
	WASM_INS_I64_SHR_S,
	WASM_INS_I64_SHR_U,
	WASM_INS_I64_ROTL,
	WASM_INS_I64_ROTR,
	WASM_INS_F32_ABS,
	WASM_INS_F32_NEG,
	WASM_INS_F32_CEIL,
	WASM_INS_F32_FLOOR,
	WASM_INS_F32_TRUNC,
	WASM_INS_F32_NEAREST,
	WASM_INS_F32_SQRT,
	WASM_INS_F32_ADD,
	WASM_INS_F32_SUB,
	WASM_INS_F32_MUL,
	WASM_INS_F32_DIV,
	WASM_INS_F32_MIN,
	WASM_INS_F32_MAX,
	WASM_INS_F32_COPYSIGN,
	WASM_INS_F64_ABS,
	WASM_INS_F64_NEG,
	WASM_INS_F64_CEIL,
	WASM_INS_F64_FLOOR,
	WASM_INS_F64_TRUNC,
	WASM_INS_F64_NEAREST,
	WASM_INS_F64_SQRT,
	WASM_INS_F64_ADD,
	WASM_INS_F64_SUB,
	WASM_INS_F64_MUL,
	WASM_INS_F64_DIV,
	WASM_INS_F64_MIN,
	WASM_INS_F64_MAX,
	WASM_INS_F64_COPYSIGN,
	WASM_INS_I32_WARP_I64,
	WASP_INS_I32_TRUNC_S_F32,
	WASM_INS_I32_TRUNC_U_F32,
	WASM_INS_I32_TRUNC_S_F64,
	WASM_INS_I32_TRUNC_U_F64,
	WASM_INS_I64_EXTEND_S_I32,
	WASM_INS_I64_EXTEND_U_I32,
	WASM_INS_I64_TRUNC_S_F32,
	WASM_INS_I64_TRUNC_U_F32,
	WASM_INS_I64_TRUNC_S_F64,
	WASM_INS_I64_TRUNC_U_F64,
	WASM_INS_F32_CONVERT_S_I32,
	WASM_INS_F32_CONVERT_U_I32,
	WASM_INS_F32_CONVERT_S_I64,
	WASM_INS_F32_CONVERT_U_I64,
	WASM_INS_F32_DEMOTE_F64,
	WASM_INS_F64_CONVERT_S_I32,
	WASM_INS_F64_CONVERT_U_I32,
	WASM_INS_F64_CONVERT_S_I64,
	WASM_INS_F64_CONVERT_U_I64,
	WASM_INS_F64_PROMOTE_F32,
	WASM_INS_I32_REINTERPRET_F32,
	WASM_INS_I64_REINTERPRET_F64,
	WASM_INS_F32_REINTERPRET_I32,
	WASM_INS_F64_REINTERPRET_I64,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
};

// input 	| code: code pointer start from varuint32
//       	| code_len: real code len count from varint
//       	| leng: return value, means length of varint. -1 means error
// return	| varint
static uint32_t get_varuint32(const uint8_t *code, size_t code_len, size_t *leng)
{
	uint32_t data = 0;
	int i;

	for(i = 0;; i++) {
		if (code_len < i + 1) {
			*leng = -1;
			return 0;
		}


		if (i > 4 || (i == 4 && (code[i] & 0x7f) > 0x0f)) {
			*leng = -1;
			return 0;
		}

		data = data + (((uint32_t) code[i] & 0x7f) << (i * 7));
		if (code[i] >> 7 == 0) {
			break;
		}
	}

	*leng = i + 1;

	return data;
}

// input 	| code : code pointer start from varuint64
//       	| code_len : real code len count from varint
//       	| leng: return value, means length of varint. -1 means error
// return 	| varint
static uint64_t get_varuint64(const uint8_t *code, size_t code_len, size_t *leng)
{
	uint64_t data;
	int i;

	data = 0;
	for(i = 0;; i++){
		if (code_len < i + 1) {
			*leng = -1;
			return 0;
		}

		if (i > 9 || (i == 9 && (code[i] & 0x7f) > 0x01)) {
			*leng = -1;
			return 0;
		}

		data = data + (((uint64_t) code[i] & 0x7f) << (i * 7));
		if (code[i] >> 7 == 0) {
			break;
		}
	}

	*leng = i + 1;

	return data;
}

// input	| code : code pointer start from uint32
//			| dest : the pointer where we store the uint32
// return	| None 
static void get_uint32(const uint8_t *code, uint32_t *dest)
{
	memcpy(dest, code, 4);
}

// input	| code : code pointer start from uint32
// 			| dest : the pointer where we store the uint64
// return 	| None
static void get_uint64(const uint8_t *code, uint64_t *dest)
{
	memcpy(dest, code, 8);
}

// input	| code : code pointer start from varint7
// 			| code_len : start from the code pointer to the end, how long is it
// 			| leng : length of the param , -1 means error
// return 	| data of varint7
static int8_t get_varint7(const uint8_t *code, size_t code_len, size_t *leng)
{
	int8_t data;

	if (code_len < 1) {
		*leng = -1;
		return -1;
	}

	*leng = 1;

	if (code[0] == 0x40) {
		return -1;
	}

	data = code[0] & 0x7f;

	return data;
}

// input 	| code : code pointer start from varuint32
// 			| code_len : start from the code pointer to the end, how long is it
// 			| param_size : pointer of the param size
// 			| MI : Mcinst handler in this round of disasm
// return 	| true/false if the function successfully finished 
static bool read_varuint32(const uint8_t *code, size_t code_len, uint16_t *param_size, MCInst *MI)
{
	size_t len = 0;
	uint32_t data;

	data = get_varuint32(code, code_len, &len);
	if (len == -1) {
		return false;
	}

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.op_count = 1;
		MI->flat_insn->detail->wasm.operands[0].type = WASM_OP_VARUINT32;
		MI->flat_insn->detail->wasm.operands[0].size= len;
		MI->flat_insn->detail->wasm.operands[0].varuint32= data;
	}

	MI->wasm_data.size = len;
	MI->wasm_data.type = WASM_OP_VARUINT32;
	MI->wasm_data.uint32 = data;
	*param_size = len;

	return true;
}

// input 	| code : code pointer start from varuint64
// 			| code_len : start from the code pointer to the end, how long is it
// 			| param_size : pointer of the param size
// 			| MI : Mcinst handler in this round of disasm
// return 	| true/false if the function successfully finished 
static bool read_varuint64(const uint8_t *code, size_t code_len, uint16_t *param_size, MCInst *MI)
{
	size_t len = 0;
	uint64_t data;

	data = get_varuint64(code, code_len, &len);
	if (len == -1) {
		return false;
	}

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.op_count = 1;
		MI->flat_insn->detail->wasm.operands[0].type = WASM_OP_VARUINT64;
		MI->flat_insn->detail->wasm.operands[0].size = len;
		MI->flat_insn->detail->wasm.operands[0].varuint64 = data;
	}

	MI->wasm_data.size = len;
	MI->wasm_data.type = WASM_OP_VARUINT64;
	MI->wasm_data.uint64 = data;
	*param_size = len;

	return true;
}

// input 	| code : code pointer start from memoryimmediate
// 			| code_len : start from the code pointer to the end, how long is it
// 			| param_size : pointer of the param size (sum of two params)
// 			| MI : Mcinst handler in this round of disasm
// return 	| true/false if the function successfully finished 
static bool read_memoryimmediate(const uint8_t *code, size_t code_len, uint16_t *param_size, MCInst *MI)
{
	size_t tmp, len = 0;
	uint32_t data[2];

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.op_count = 2;
	}

	data[0] = get_varuint32(code, code_len, &tmp);
	if (tmp == -1) {
		return false;
	}

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.operands[0].type = WASM_OP_VARUINT32;
		MI->flat_insn->detail->wasm.operands[0].size = tmp;
		MI->flat_insn->detail->wasm.operands[0].varuint32 = data[0];
	}

	len = tmp;
	data[1] = get_varuint32(&code[len], code_len - len, &tmp);
	if (len == -1) {
		return false;
	}

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.operands[1].type = WASM_OP_VARUINT32;
		MI->flat_insn->detail->wasm.operands[1].size = tmp;
		MI->flat_insn->detail->wasm.operands[1].varuint32 = data[1];
	}

	len += tmp;
	MI->wasm_data.size = len;
	MI->wasm_data.type = WASM_OP_IMM;
	MI->wasm_data.immediate[0] = data[0];
	MI->wasm_data.immediate[1] = data[1];
	*param_size = len;

	return true;
}

// input 	| code : code pointer start from uint32
// 			| code_len : start from the code pointer to the end, how long is it
// 			| param_size : pointer of the param size
// 			| MI : Mcinst handler in this round of disasm
// return 	| true/false if the function successfully finished 
static bool read_uint32(const uint8_t *code, size_t code_len, uint16_t *param_size, MCInst *MI)
{
	if (code_len < 4) {
		return false;
	}

	get_uint32(code, &(MI->wasm_data.uint32));

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.op_count = 1;
		MI->flat_insn->detail->wasm.operands[0].type = WASM_OP_UINT32;
		MI->flat_insn->detail->wasm.operands[0].size = 4;
		get_uint32(code, &(MI->flat_insn->detail->wasm.operands[0].uint32));
	}

	MI->wasm_data.size = 4;
	MI->wasm_data.type = WASM_OP_UINT32;
	*param_size = 4;

	return true;
}

// input 	| code : code pointer start from uint64
// 			| code_len : start from the code pointer to the end, how long is it
// 			| param_size : pointer of the param size
// 			| MI : Mcinst handler in this round of disasm
// return 	| true/false if the function successfully finished 
static bool read_uint64(const uint8_t *code, size_t code_len, uint16_t *param_size, MCInst *MI)
{
	if (code_len < 8) {
		return false;
	}

	get_uint64(code, &(MI->wasm_data.uint64));

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.op_count = 1;
		MI->flat_insn->detail->wasm.operands[0].type = WASM_OP_UINT64;
		MI->flat_insn->detail->wasm.operands[0].size = 8;
		get_uint64(code, &(MI->flat_insn->detail->wasm.operands[0].uint64));
	}

	MI->wasm_data.size = 8;
	MI->wasm_data.type = WASM_OP_UINT64;
	*param_size = 8;

	return true;
}

// input 	| code : code pointer start from brtable
// 			| code_len : start from the code pointer to the end, how long is it
// 			| param_size : pointer of the param size (sum of all param)
// 			| MI : Mcinst handler in this round of disasm
// return 	| true/false if the function successfully finished 
static bool read_brtable(const uint8_t *code, size_t code_len, uint16_t *param_size, MCInst *MI)
{
	uint32_t length, default_target;
	int tmp_len = 0, i;
	size_t var_len;

	// read length
	length = get_varuint32(code, code_len, &var_len);
	if (var_len == -1) {
		return false;
	}

	tmp_len += var_len;
	MI->wasm_data.brtable.length = length;
	if (length >= UINT32_MAX - tmp_len) {
		// integer overflow check
		return false;
	}
	if (code_len < tmp_len + length) {
		// safety check that we have minimum enough data to read
		return false;
	}
	// base address + 1 byte opcode + tmp_len for number of cases = start of targets
	MI->wasm_data.brtable.address = MI->address + 1 + tmp_len;

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.op_count = 1;
		MI->flat_insn->detail->wasm.operands[0].type = WASM_OP_BRTABLE;
		MI->flat_insn->detail->wasm.operands[0].brtable.length = MI->wasm_data.brtable.length;
		MI->flat_insn->detail->wasm.operands[0].brtable.address = MI->wasm_data.brtable.address;
	}

	// read data
	for(i = 0; i < length; i++){
		if (code_len < tmp_len) {
			return false;
		}

		get_varuint32(code + tmp_len, code_len - tmp_len, &var_len);
		if (var_len == -1) {
			return false;
		}

		tmp_len += var_len;
	}

	// read default target
	default_target = get_varuint32(code + tmp_len, code_len - tmp_len, &var_len);
	if (var_len == -1) {
		return false;
	}

	MI->wasm_data.brtable.default_target = default_target;
	MI->wasm_data.type = WASM_OP_BRTABLE;
	*param_size = tmp_len + var_len;

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.operands[0].size = *param_size;
		MI->flat_insn->detail->wasm.operands[0].brtable.default_target = MI->wasm_data.brtable.default_target;
	}

	return true;
}

// input 	| code : code pointer start from varint7
// 			| code_len : start from the code pointer to the end, how long is it
// 			| param_size : pointer of the param size
// 			| MI : Mcinst handler in this round of disasm
// return 	| true/false if the function successfully finished 
static bool read_varint7(const uint8_t *code, size_t code_len, uint16_t *param_size, MCInst *MI)
{
	size_t len = 0;

	MI->wasm_data.type = WASM_OP_INT7;
	MI->wasm_data.int7 = get_varint7(code, code_len, &len);
	if (len == -1) {
		return false;
	}

	if (MI->flat_insn->detail) {
		MI->flat_insn->detail->wasm.op_count = 1;
		MI->flat_insn->detail->wasm.operands[0].type = WASM_OP_INT7;
		MI->flat_insn->detail->wasm.operands[0].size = 1;
		MI->flat_insn->detail->wasm.operands[0].int7 = MI->wasm_data.int7;
	}

	*param_size = len;

	return true;
}

bool WASM_getInstruction(csh ud, const uint8_t *code, size_t code_len,
		MCInst *MI, uint16_t *size, uint64_t address, void *inst_info)
{
	unsigned char opcode;
	uint16_t param_size;

	if (code_len == 0)
		return false;

	opcode = code[0];
	if (opcodes[opcode] == -1) {
		// invalid opcode
		return false;
	}

	// valid opcode
	MI->address = address;
	MI->OpcodePub = MI->Opcode = opcode;

	if (MI->flat_insn->detail) {
		memset(MI->flat_insn->detail, 0, offsetof(cs_detail, wasm)+sizeof(cs_wasm));
		WASM_get_insn_id((cs_struct *)ud, MI->flat_insn, opcode);
	}

	// setup groups
	switch(opcode) {
		default:
			return false;

		case WASM_INS_I32_CONST:
			if (code_len == 1 || !read_varuint32(&code[1], code_len - 1, &param_size, MI)) {
				return false;
			}

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 1;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_NUMBERIC;
				MI->flat_insn->detail->groups_count++;
			}

			*size = param_size + 1;

			break;

		case WASM_INS_I64_CONST:
			if (code_len == 1 || !read_varuint64(&code[1], code_len - 1, &param_size, MI)) {
				return false;
			}

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 1;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_NUMBERIC;
				MI->flat_insn->detail->groups_count++;
			}

			*size = param_size + 1;

			break;

		case WASM_INS_F32_CONST:
			if (code_len == 1 || !read_uint32(&code[1], code_len - 1, &param_size, MI)) {
				return false;
			}

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 1;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_NUMBERIC;
				MI->flat_insn->detail->groups_count++;
			}

			*size = param_size + 1;

			break;

		case WASM_INS_F64_CONST:
			if (code_len == 1 || !read_uint64(&code[1], code_len - 1, &param_size, MI)) {
				return false;
			}

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 1;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_NUMBERIC;
				MI->flat_insn->detail->groups_count++;
			}

			*size = param_size + 1;

			break;

		case WASM_INS_I32_EQZ:
		case WASM_INS_I32_EQ:
		case WASM_INS_I32_NE:
		case WASM_INS_I32_LT_S:
		case WASM_INS_I32_LT_U:
		case WASM_INS_I32_GT_S:
		case WASM_INS_I32_GT_U:
		case WASM_INS_I32_LE_S:
		case WASM_INS_I32_LE_U:
		case WASM_INS_I32_GE_S:
		case WASM_INS_I32_GE_U:
		case WASM_INS_I64_EQZ:
		case WASM_INS_I64_EQ:
		case WASM_INS_I64_NE:
		case WASM_INS_I64_LT_S:
		case WASM_INS_I64_LT_U:
		case WASN_INS_I64_GT_S:
		case WASM_INS_I64_GT_U:
		case WASM_INS_I64_LE_S:
		case WASM_INS_I64_LE_U:
		case WASM_INS_I64_GE_S:
		case WASM_INS_I64_GE_U:
		case WASM_INS_F32_EQ:
		case WASM_INS_F32_NE:
		case WASM_INS_F32_LT:
		case WASM_INS_F32_GT:
		case WASM_INS_F32_LE:
		case WASM_INS_F32_GE:
		case WASM_INS_F64_EQ:
		case WASM_INS_F64_NE:
		case WASM_INS_F64_LT:
		case WASM_INS_F64_GT:
		case WASM_INS_F64_LE:
		case WASM_INS_F64_GE:
		case WASM_INS_I32_CLZ:
		case WASM_INS_I32_CTZ:
		case WASM_INS_I32_POPCNT:
		case WASM_INS_I32_ADD:
		case WASM_INS_I32_SUB:
		case WASM_INS_I32_MUL:
		case WASM_INS_I32_DIV_S:
		case WASM_INS_I32_DIV_U:
		case WASM_INS_I32_REM_S:
		case WASM_INS_I32_REM_U:
		case WASM_INS_I32_AND:
		case WASM_INS_I32_OR:
		case WASM_INS_I32_XOR:
		case WASM_INS_I32_SHL:
		case WASM_INS_I32_SHR_S:
		case WASM_INS_I32_SHR_U:
		case WASM_INS_I32_ROTL:
		case WASM_INS_I32_ROTR:
		case WASM_INS_I64_CLZ:
		case WASM_INS_I64_CTZ:
		case WASM_INS_I64_POPCNT:
		case WASM_INS_I64_ADD:
		case WASM_INS_I64_SUB:
		case WASM_INS_I64_MUL:
		case WASM_INS_I64_DIV_S:
		case WASM_INS_I64_DIV_U:
		case WASM_INS_I64_REM_S:
		case WASM_INS_I64_REM_U:
		case WASM_INS_I64_AND:
		case WASM_INS_I64_OR:
		case WASM_INS_I64_XOR:
		case WASM_INS_I64_SHL:
		case WASM_INS_I64_SHR_S:
		case WASM_INS_I64_SHR_U:
		case WASM_INS_I64_ROTL:
		case WASM_INS_I64_ROTR:
		case WASM_INS_F32_ABS:
		case WASM_INS_F32_NEG:
		case WASM_INS_F32_CEIL:
		case WASM_INS_F32_FLOOR:
		case WASM_INS_F32_TRUNC:
		case WASM_INS_F32_NEAREST:
		case WASM_INS_F32_SQRT:
		case WASM_INS_F32_ADD:
		case WASM_INS_F32_SUB:
		case WASM_INS_F32_MUL:
		case WASM_INS_F32_DIV:
		case WASM_INS_F32_MIN:
		case WASM_INS_F32_MAX:
		case WASM_INS_F32_COPYSIGN:
		case WASM_INS_F64_ABS:
		case WASM_INS_F64_NEG:
		case WASM_INS_F64_CEIL:
		case WASM_INS_F64_FLOOR:
		case WASM_INS_F64_TRUNC:
		case WASM_INS_F64_NEAREST:
		case WASM_INS_F64_SQRT:
		case WASM_INS_F64_ADD:
		case WASM_INS_F64_SUB:
		case WASM_INS_F64_MUL:
		case WASM_INS_F64_DIV:
		case WASM_INS_F64_MIN:
		case WASM_INS_F64_MAX:
		case WASM_INS_F64_COPYSIGN:
		case WASM_INS_I32_WARP_I64:
		case WASP_INS_I32_TRUNC_S_F32:
		case WASM_INS_I32_TRUNC_U_F32:
		case WASM_INS_I32_TRUNC_S_F64:
		case WASM_INS_I32_TRUNC_U_F64:
		case WASM_INS_I64_EXTEND_S_I32:
		case WASM_INS_I64_EXTEND_U_I32:
		case WASM_INS_I64_TRUNC_S_F32:
		case WASM_INS_I64_TRUNC_U_F32:
		case WASM_INS_I64_TRUNC_S_F64:
		case WASM_INS_I64_TRUNC_U_F64:
		case WASM_INS_F32_CONVERT_S_I32:
		case WASM_INS_F32_CONVERT_U_I32:
		case WASM_INS_F32_CONVERT_S_I64:
		case WASM_INS_F32_CONVERT_U_I64:
		case WASM_INS_F32_DEMOTE_F64:
		case WASM_INS_F64_CONVERT_S_I32:
		case WASM_INS_F64_CONVERT_U_I32:
		case WASM_INS_F64_CONVERT_S_I64:
		case WASM_INS_F64_CONVERT_U_I64:
		case WASM_INS_F64_PROMOTE_F32:
		case WASM_INS_I32_REINTERPRET_F32:
		case WASM_INS_I64_REINTERPRET_F64:
		case WASM_INS_F32_REINTERPRET_I32:
		case WASM_INS_F64_REINTERPRET_I64:
			MI->wasm_data.type = WASM_OP_NONE;

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 0;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_NUMBERIC;
				MI->flat_insn->detail->groups_count++;
			}

			*size = 1;

			break;

		case WASM_INS_DROP:
		case WASM_INS_SELECT:
			MI->wasm_data.type = WASM_OP_NONE;

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 0;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_PARAMETRIC;
				MI->flat_insn->detail->groups_count++;
			}

			*size = 1;

			break;

		case WASM_INS_GET_LOCAL:
		case WASM_INS_SET_LOCAL:
		case WASM_INS_TEE_LOCAL:
		case WASM_INS_GET_GLOBAL:
		case WASM_INS_SET_GLOBAL:
			if (code_len == 1 || !read_varuint32(&code[1], code_len - 1, &param_size, MI)) {
				return false;
			}

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 1;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_VARIABLE;
				MI->flat_insn->detail->groups_count++;
			}

			*size = param_size + 1;

			break;

		case WASM_INS_I32_LOAD:
		case WASM_INS_I64_LOAD:
		case WASM_INS_F32_LOAD:
		case WASM_INS_F64_LOAD:
		case WASM_INS_I32_LOAD8_S:
		case WASM_INS_I32_LOAD8_U:
		case WASM_INS_I32_LOAD16_S:
		case WASM_INS_I32_LOAD16_U:
		case WASM_INS_I64_LOAD8_S:
		case WASM_INS_I64_LOAD8_U:
		case WASM_INS_I64_LOAD16_S:
		case WASM_INS_I64_LOAD16_U:
		case WASM_INS_I64_LOAD32_S:
		case WASM_INS_I64_LOAD32_U:
		case WASM_INS_I32_STORE:
		case WASM_INS_I64_STORE:
		case WASM_INS_F32_STORE:
		case WASM_INS_F64_STORE:
		case WASM_INS_I32_STORE8:
		case WASM_INS_I32_STORE16:
		case WASM_INS_I64_STORE8:
		case WASM_INS_I64_STORE16:
		case WASM_INS_I64_STORE32:
			if (code_len == 1 || !read_memoryimmediate(&code[1], code_len - 1, &param_size, MI)) {
				return false;
			}

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 2;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_MEMORY;
				MI->flat_insn->detail->groups_count++;
			}

			*size = param_size + 1;

			break;

		case WASM_INS_CURRENT_MEMORY:
		case WASM_INS_GROW_MEMORY:
			MI->wasm_data.type = WASM_OP_NONE;

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 0;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_MEMORY;
				MI->flat_insn->detail->groups_count++;
			}

			*size = 1;

			break;

		case WASM_INS_UNREACHABLE:
		case WASM_INS_NOP:
		case WASM_INS_ELSE:
		case WASM_INS_END:
		case WASM_INS_RETURN:
			MI->wasm_data.type = WASM_OP_NONE;

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 0;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_CONTROL;
				MI->flat_insn->detail->groups_count++;
			}

			*size = 1;

			break;

		case WASM_INS_BLOCK:
		case WASM_INS_LOOP:
		case WASM_INS_IF:
			if (code_len == 1 || !read_varint7(&code[1], code_len - 1, &param_size, MI)) {
				return false;
			}

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 1;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_CONTROL;
				MI->flat_insn->detail->groups_count++;
			}

			*size = param_size + 1;

			break;

		case WASM_INS_BR:
		case WASM_INS_BR_IF:
		case WASM_INS_CALL:
		case WASM_INS_CALL_INDIRECT:
			if (code_len == 1 || !read_varuint32(&code[1], code_len - 1, &param_size, MI)) {
				return false;
			}

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 1;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_CONTROL;
				MI->flat_insn->detail->groups_count++;
			}

			*size = param_size + 1;

			break;

		case WASM_INS_BR_TABLE:
			if (code_len == 1 || !read_brtable(&code[1], code_len - 1, &param_size, MI)) {
				return false;
			}

			if (MI->flat_insn->detail) {
				MI->flat_insn->detail->wasm.op_count = 1;
				MI->flat_insn->detail->groups[MI->flat_insn->detail->groups_count] = WASM_GRP_CONTROL;
				MI->flat_insn->detail->groups_count++;
			}

			*size = param_size + 1;

			break;
	}

	return true;
}
