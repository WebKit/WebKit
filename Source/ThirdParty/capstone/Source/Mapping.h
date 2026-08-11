/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */
/*    Rot127 <unisono@quyllur.org>, 2022-2023 */

#ifndef CS_MAPPING_H
#define CS_MAPPING_H

#if defined(CAPSTONE_HAS_OSXKERNEL)
#include <libkern/libkern.h>
#else
#include "include/capstone/capstone.h"
#include <stddef.h>
#endif
#include "cs_priv.h"
#include <assert.h>
#include <string.h>

// map instruction to its characteristics
typedef struct insn_map {
	unsigned short id;		    // The LLVM instruction id
	unsigned short mapid;		    // The Capstone instruction id
#ifndef CAPSTONE_DIET
	uint16_t regs_use[MAX_IMPL_R_REGS]; ///< list of implicit registers used by
		///< this instruction
	uint16_t regs_mod[MAX_IMPL_W_REGS]; ///< list of implicit registers modified
		///< by this instruction
	unsigned char groups
		[MAX_NUM_GROUPS]; ///< list of group this instruction belong to
	bool branch;		  // branch instruction?
	bool indirect_branch;	  // indirect branch instruction?
#endif
} insn_map;

// look for @id in @m, given its size in @max. first time call will update
// @cache. return 0 if not found
unsigned short insn_find(const insn_map *m, unsigned int max, unsigned int id,
			 unsigned short **cache);

unsigned int find_cs_id(unsigned MC_Opcode, const insn_map *imap,
			unsigned imap_size);

#define MAX_NO_DATA_TYPES 10

///< A LLVM<->CS Mapping entry of an MCOperand.
typedef struct {
	uint8_t /* cs_op_type */ type;	 ///< Operand type (e.g.: reg, imm, mem)
	uint8_t /* cs_ac_type */ access; ///< The access type (read, write)
	uint8_t				 /* cs_data_type */
		dtypes[MAX_NO_DATA_TYPES]; ///< List of op types. Terminated by
		///< CS_DATA_TYPE_LAST
} mapping_op;

#define MAX_NO_INSN_MAP_OPS 16

///< MCOperands of an instruction.
typedef struct {
	mapping_op
		ops[MAX_NO_INSN_MAP_OPS]; ///< NULL terminated array of insn_op.
} map_insn_ops;

/// Only usable by `auto-sync` archs!
const cs_op_type mapping_get_op_type(MCInst *MI, unsigned OpNum,
				     const map_insn_ops *insn_ops_map,
				     size_t map_size);

/// Only usable by `auto-sync` archs!
const cs_ac_type mapping_get_op_access(MCInst *MI, unsigned OpNum,
				       const map_insn_ops *insn_ops_map,
				       size_t map_size);

/// Macro for easier access of operand types from the map.
/// Assumes the istruction operands map is called "insn_operands"
/// Only usable by `auto-sync` archs!
#define map_get_op_type(MI, OpNum) \
	mapping_get_op_type(MI, OpNum, (const map_insn_ops *)insn_operands, \
			    sizeof(insn_operands) / sizeof(insn_operands[0]))

/// Macro for easier access of operand access flags from the map.
/// Assumes the istruction operands map is called "insn_operands"
/// Only usable by `auto-sync` archs!
#define map_get_op_access(MI, OpNum) \
	mapping_get_op_access(MI, OpNum, (const map_insn_ops *)insn_operands, \
			      sizeof(insn_operands) / \
				      sizeof(insn_operands[0]))

///< Map for ids to their string
typedef struct name_map {
	unsigned int id;
	const char *name;
} name_map;

// map a name to its ID
// return 0 if not found
int name2id(const name_map *map, int max, const char *name);

// map ID to a name
// return NULL if not found
const char *id2name(const name_map *map, int max, const unsigned int id);

void map_add_implicit_write(MCInst *MI, uint32_t Reg);

void map_implicit_reads(MCInst *MI, const insn_map *imap);

void map_implicit_writes(MCInst *MI, const insn_map *imap);

void map_groups(MCInst *MI, const insn_map *imap);

void map_cs_id(MCInst *MI, const insn_map *imap, unsigned int imap_size);

#define DECL_get_detail_op(arch, ARCH) \
	cs_##arch##_op *ARCH##_get_detail_op(MCInst *MI, int offset);

DECL_get_detail_op(arm, ARM);
DECL_get_detail_op(ppc, PPC);
DECL_get_detail_op(tricore, TriCore);

/// Increments the detail->arch.op_count by one.
#define DEFINE_inc_detail_op_count(arch, ARCH) \
	static inline void ARCH##_inc_op_count(MCInst *MI) \
	{ \
		MI->flat_insn->detail->arch.op_count++; \
	}

/// Decrements the detail->arch.op_count by one.
#define DEFINE_dec_detail_op_count(arch, ARCH) \
	static inline void ARCH##_dec_op_count(MCInst *MI) \
	{ \
		MI->flat_insn->detail->arch.op_count--; \
	}

DEFINE_inc_detail_op_count(arm, ARM);
DEFINE_dec_detail_op_count(arm, ARM);
DEFINE_inc_detail_op_count(ppc, PPC);
DEFINE_dec_detail_op_count(ppc, PPC);
DEFINE_inc_detail_op_count(tricore, TriCore);
DEFINE_dec_detail_op_count(tricore, TriCore);

/// Returns true if a memory operand is currently edited.
static inline bool doing_mem(const MCInst *MI)
{
	return MI->csh->doing_mem;
}

/// Sets the doing_mem flag to @status.
static inline void set_doing_mem(const MCInst *MI, bool status)
{
	MI->csh->doing_mem = status;
}

/// Returns detail->arch
#define DEFINE_get_arch_detail(arch, ARCH) \
	static inline cs_##arch *ARCH##_get_detail(const MCInst *MI) \
	{ \
		assert(MI && MI->flat_insn && MI->flat_insn->detail); \
		return &MI->flat_insn->detail->arch; \
	}

DEFINE_get_arch_detail(arm, ARM);
DEFINE_get_arch_detail(ppc, PPC);
DEFINE_get_arch_detail(tricore, TriCore);

static inline bool detail_is_set(const MCInst *MI)
{
	assert(MI && MI->flat_insn);
	return MI->flat_insn->detail != NULL;
}

static inline cs_detail *get_detail(const MCInst *MI)
{
	assert(MI && MI->flat_insn);
	return MI->flat_insn->detail;
}

#endif // CS_MAPPING_H