/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */
/*    Rot127 <unisono@quyllur.org>, 2022-2023 */

#include "Mapping.h"

// create a cache for fast id lookup
static unsigned short *make_id2insn(const insn_map *insns, unsigned int size)
{
	// NOTE: assume that the max id is always put at the end of insns array
	unsigned short max_id = insns[size - 1].id;
	unsigned int i;

	unsigned short *cache =
		(unsigned short *)cs_mem_calloc(max_id + 1, sizeof(*cache));

	for (i = 1; i < size; i++)
		cache[insns[i].id] = i;

	return cache;
}

// look for @id in @insns, given its size in @max. first time call will update
// @cache. return 0 if not found
unsigned short insn_find(const insn_map *insns, unsigned int max,
			 unsigned int id, unsigned short **cache)
{
	if (id > insns[max - 1].id)
		return 0;

	if (*cache == NULL)
		*cache = make_id2insn(insns, max);

	return (*cache)[id];
}

// Gives the id for the given @name if it is saved in @map.
// Returns the id or -1 if not found.
int name2id(const name_map *map, int max, const char *name)
{
	int i;

	for (i = 0; i < max; i++) {
		if (!strcmp(map[i].name, name)) {
			return map[i].id;
		}
	}

	// nothing match
	return -1;
}

// Gives the name for the given @id if it is saved in @map.
// Returns the name or NULL if not found.
const char *id2name(const name_map *map, int max, const unsigned int id)
{
	int i;

	for (i = 0; i < max; i++) {
		if (map[i].id == id) {
			return map[i].name;
		}
	}

	// nothing match
	return NULL;
}

/// Adds a register to the implicit write register list.
/// It will not add the same register twice.
void map_add_implicit_write(MCInst *MI, uint32_t Reg)
{
	if (!MI->flat_insn->detail)
		return;

	uint16_t *regs_write = MI->flat_insn->detail->regs_write;
	for (int i = 0; i < MAX_IMPL_W_REGS; ++i) {
		if (i == MI->flat_insn->detail->regs_write_count) {
			regs_write[i] = Reg;
			MI->flat_insn->detail->regs_write_count++;
			return;
		}
		if (regs_write[i] == Reg)
			return;
	}
}

/// Copies the implicit read registers of @imap to @MI->flat_insn.
/// Already present registers will be preserved.
void map_implicit_reads(MCInst *MI, const insn_map *imap)
{
#ifndef CAPSTONE_DIET
	if (!MI->flat_insn->detail)
		return;

	cs_detail *detail = MI->flat_insn->detail;
	unsigned Opcode = MCInst_getOpcode(MI);
	unsigned i = 0;
	uint16_t reg = imap[Opcode].regs_use[i];
	while (reg != 0) {
		if (i >= MAX_IMPL_R_REGS ||
		    detail->regs_read_count >= MAX_IMPL_R_REGS) {
			printf("ERROR: Too many implicit read register defined in "
			       "instruction mapping.\n");
			return;
		}
		detail->regs_read[detail->regs_read_count++] = reg;
		reg = imap[Opcode].regs_use[++i];
	}
#endif // CAPSTONE_DIET
}

/// Copies the implicit write registers of @imap to @MI->flat_insn.
/// Already present registers will be preserved.
void map_implicit_writes(MCInst *MI, const insn_map *imap)
{
#ifndef CAPSTONE_DIET
	if (!MI->flat_insn->detail)
		return;

	cs_detail *detail = MI->flat_insn->detail;
	unsigned Opcode = MCInst_getOpcode(MI);
	unsigned i = 0;
	uint16_t reg = imap[Opcode].regs_mod[i];
	while (reg != 0) {
		if (i >= MAX_IMPL_W_REGS ||
		    detail->regs_write_count >= MAX_IMPL_W_REGS) {
			printf("ERROR: Too many implicit write register defined in "
			       "instruction mapping.\n");
			return;
		}
		detail->regs_write[detail->regs_write_count++] = reg;
		reg = imap[Opcode].regs_mod[++i];
	}
#endif // CAPSTONE_DIET
}

/// Copies the groups from @imap to @MI->flat_insn.
/// Already present groups will be preserved.
void map_groups(MCInst *MI, const insn_map *imap)
{
#ifndef CAPSTONE_DIET
	if (!MI->flat_insn->detail)
		return;

	cs_detail *detail = MI->flat_insn->detail;
	unsigned Opcode = MCInst_getOpcode(MI);
	unsigned i = 0;
	uint16_t group = imap[Opcode].groups[i];
	while (group != 0) {
		if (detail->groups_count >= MAX_NUM_GROUPS) {
			printf("ERROR: Too many groups defined in instruction mapping.\n");
			return;
		}
		detail->groups[detail->groups_count++] = group;
		group = imap[Opcode].groups[++i];
	}
#endif // CAPSTONE_DIET
}

// Search for the CS instruction id for the given @MC_Opcode in @imap.
// return -1 if none is found.
unsigned int find_cs_id(unsigned MC_Opcode, const insn_map *imap,
			unsigned imap_size)
{
	// binary searching since the IDs are sorted in order
	unsigned int left, right, m;
	unsigned int max = imap_size;

	right = max - 1;

	if (MC_Opcode < imap[0].id || MC_Opcode > imap[right].id)
		// not found
		return -1;

	left = 0;

	while (left <= right) {
		m = (left + right) / 2;
		if (MC_Opcode == imap[m].id) {
			return m;
		}

		if (MC_Opcode < imap[m].id)
			right = m - 1;
		else
			left = m + 1;
	}

	return -1;
}

/// Sets the Capstone instruction id which maps to the @MI opcode.
/// If no mapping is found the function returns and prints an error.
void map_cs_id(MCInst *MI, const insn_map *imap, unsigned int imap_size)
{
	unsigned int i = find_cs_id(MCInst_getOpcode(MI), imap, imap_size);
	if (i != -1) {
		MI->flat_insn->id = imap[i].mapid;
		return;
	}
	printf("ERROR: Could not find CS id for MCInst opcode: %d\n",
	       MCInst_getOpcode(MI));
	return;
}

/// Returns the operand type information from the
/// mapping table for instruction operands.
/// Only usable by `auto-sync` archs!
const cs_op_type mapping_get_op_type(MCInst *MI, unsigned OpNum,
				     const map_insn_ops *insn_ops_map,
				     size_t map_size)
{
	assert(MI);
	assert(MI->Opcode < map_size);
	assert(OpNum < sizeof(insn_ops_map[MI->Opcode].ops) /
			       sizeof(insn_ops_map[MI->Opcode].ops[0]));

	return insn_ops_map[MI->Opcode].ops[OpNum].type;
}

/// Returns the operand access flags from the
/// mapping table for instruction operands.
/// Only usable by `auto-sync` archs!
const cs_ac_type mapping_get_op_access(MCInst *MI, unsigned OpNum,
				       const map_insn_ops *insn_ops_map,
				       size_t map_size)
{
	assert(MI);
	assert(MI->Opcode < map_size);
	assert(OpNum < sizeof(insn_ops_map[MI->Opcode].ops) /
			       sizeof(insn_ops_map[MI->Opcode].ops[0]));

	cs_ac_type access = insn_ops_map[MI->Opcode].ops[OpNum].access;
	if (MCInst_opIsTied(MI, OpNum) || MCInst_opIsTying(MI, OpNum))
		access |= (access == CS_AC_READ) ? CS_AC_WRITE : CS_AC_READ;
	return access;
}

/// Returns the operand at detail->arch.operands[op_count + offset]
/// Or NULL if detail is not set.
#define DEFINE_get_detail_op(arch, ARCH) \
	cs_##arch##_op *ARCH##_get_detail_op(MCInst *MI, int offset) \
	{ \
		if (!MI->flat_insn->detail) \
			return NULL; \
		int OpIdx = MI->flat_insn->detail->arch.op_count + offset; \
		assert(OpIdx >= 0 && OpIdx < MAX_MC_OPS); \
		return &MI->flat_insn->detail->arch.operands[OpIdx]; \
	}

DEFINE_get_detail_op(arm, ARM);
DEFINE_get_detail_op(ppc, PPC);
DEFINE_get_detail_op(tricore, TriCore);
