#ifndef CAPSTONE_MOS65XX_H
#define CAPSTONE_MOS65XX_H

/* Capstone Disassembly Engine */
/* By Sebastian Macke <sebastian@macke.de, 2018 */

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"

/// MOS65XX registers and special registers
typedef enum mos65xx_reg {
	MOS65XX_REG_INVALID = 0,
	MOS65XX_REG_ACC, ///< accumulator
	MOS65XX_REG_X, ///< X index register
	MOS65XX_REG_Y, ///< Y index register
	MOS65XX_REG_P, ///< status register
	MOS65XX_REG_SP, ///< stack pointer register
	MOS65XX_REG_DP, ///< direct page register
	MOS65XX_REG_B, ///< data bank register
	MOS65XX_REG_K, ///< program bank register
	MOS65XX_REG_ENDING,   // <-- mark the end of the list of registers
} mos65xx_reg;

/// MOS65XX Addressing Modes
typedef enum mos65xx_address_mode {
	MOS65XX_AM_NONE = 0, ///< No address mode.
	MOS65XX_AM_IMP, ///< implied addressing (no addressing mode)
	MOS65XX_AM_ACC, ///< accumulator addressing
	MOS65XX_AM_IMM, ///< 8/16 Bit immediate value
	MOS65XX_AM_REL, ///< relative addressing used by branches
	MOS65XX_AM_INT, ///< interrupt addressing
	MOS65XX_AM_BLOCK, ///< memory block addressing
	MOS65XX_AM_ZP,  ///< zeropage addressing
	MOS65XX_AM_ZP_X, ///< indexed zeropage addressing by the X index register
	MOS65XX_AM_ZP_Y, ///< indexed zeropage addressing by the Y index register
	MOS65XX_AM_ZP_REL, ///< zero page address, branch relative address
	MOS65XX_AM_ZP_IND, ///< indirect zeropage addressing
	MOS65XX_AM_ZP_X_IND, ///< indexed zeropage indirect addressing by the X index register
	MOS65XX_AM_ZP_IND_Y, ///< indirect zeropage indexed addressing by the Y index register
	MOS65XX_AM_ZP_IND_LONG, ///< zeropage indirect long addressing
	MOS65XX_AM_ZP_IND_LONG_Y, ///< zeropage indirect long addressing indexed by Y register
	MOS65XX_AM_ABS, ///< absolute addressing
	MOS65XX_AM_ABS_X, ///< indexed absolute addressing by the X index register
	MOS65XX_AM_ABS_Y, ///< indexed absolute addressing by the Y index register
	MOS65XX_AM_ABS_IND, ///< absolute indirect addressing
	MOS65XX_AM_ABS_X_IND, ///< indexed absolute indirect addressing by the X index register
	MOS65XX_AM_ABS_IND_LONG, ///< absolute indirect long addressing
	MOS65XX_AM_ABS_LONG, ///< absolute long address mode
	MOS65XX_AM_ABS_LONG_X, ///< absolute long address mode, indexed by X register
	MOS65XX_AM_SR, ///< stack relative addressing
	MOS65XX_AM_SR_IND_Y, ///< indirect stack relative addressing indexed by the Y index register
} mos65xx_address_mode;

/// MOS65XX instruction
typedef enum mos65xx_insn {
	MOS65XX_INS_INVALID = 0,
	MOS65XX_INS_ADC,
	MOS65XX_INS_AND,
	MOS65XX_INS_ASL,
	MOS65XX_INS_BBR,
	MOS65XX_INS_BBS,
	MOS65XX_INS_BCC,
	MOS65XX_INS_BCS,
	MOS65XX_INS_BEQ,
	MOS65XX_INS_BIT,
	MOS65XX_INS_BMI,
	MOS65XX_INS_BNE,
	MOS65XX_INS_BPL,
	MOS65XX_INS_BRA,
	MOS65XX_INS_BRK,
	MOS65XX_INS_BRL,
	MOS65XX_INS_BVC,
	MOS65XX_INS_BVS,
	MOS65XX_INS_CLC,
	MOS65XX_INS_CLD,
	MOS65XX_INS_CLI,
	MOS65XX_INS_CLV,
	MOS65XX_INS_CMP,
	MOS65XX_INS_COP,
	MOS65XX_INS_CPX,
	MOS65XX_INS_CPY,
	MOS65XX_INS_DEC,
	MOS65XX_INS_DEX,
	MOS65XX_INS_DEY,
	MOS65XX_INS_EOR,
	MOS65XX_INS_INC,
	MOS65XX_INS_INX,
	MOS65XX_INS_INY,
	MOS65XX_INS_JML,
	MOS65XX_INS_JMP,
	MOS65XX_INS_JSL,
	MOS65XX_INS_JSR,
	MOS65XX_INS_LDA,
	MOS65XX_INS_LDX,
	MOS65XX_INS_LDY,
	MOS65XX_INS_LSR,
	MOS65XX_INS_MVN,
	MOS65XX_INS_MVP,
	MOS65XX_INS_NOP,
	MOS65XX_INS_ORA,
	MOS65XX_INS_PEA,
	MOS65XX_INS_PEI,
	MOS65XX_INS_PER,
	MOS65XX_INS_PHA,
	MOS65XX_INS_PHB,
	MOS65XX_INS_PHD,
	MOS65XX_INS_PHK,
	MOS65XX_INS_PHP,
	MOS65XX_INS_PHX,
	MOS65XX_INS_PHY,
	MOS65XX_INS_PLA,
	MOS65XX_INS_PLB,
	MOS65XX_INS_PLD,
	MOS65XX_INS_PLP,
	MOS65XX_INS_PLX,
	MOS65XX_INS_PLY,
	MOS65XX_INS_REP,
	MOS65XX_INS_RMB,
	MOS65XX_INS_ROL,
	MOS65XX_INS_ROR,
	MOS65XX_INS_RTI,
	MOS65XX_INS_RTL,
	MOS65XX_INS_RTS,
	MOS65XX_INS_SBC,
	MOS65XX_INS_SEC,
	MOS65XX_INS_SED,
	MOS65XX_INS_SEI,
	MOS65XX_INS_SEP,
	MOS65XX_INS_SMB,
	MOS65XX_INS_STA,
	MOS65XX_INS_STP,
	MOS65XX_INS_STX,
	MOS65XX_INS_STY,
	MOS65XX_INS_STZ,
	MOS65XX_INS_TAX,
	MOS65XX_INS_TAY,
	MOS65XX_INS_TCD,
	MOS65XX_INS_TCS,
	MOS65XX_INS_TDC,
	MOS65XX_INS_TRB,
	MOS65XX_INS_TSB,
	MOS65XX_INS_TSC,
	MOS65XX_INS_TSX,
	MOS65XX_INS_TXA,
	MOS65XX_INS_TXS,
	MOS65XX_INS_TXY,
	MOS65XX_INS_TYA,
	MOS65XX_INS_TYX,
	MOS65XX_INS_WAI,
	MOS65XX_INS_WDM,
	MOS65XX_INS_XBA,
	MOS65XX_INS_XCE,
	MOS65XX_INS_ENDING,   // <-- mark the end of the list of instructions
} mos65xx_insn;

/// Group of MOS65XX instructions
typedef enum mos65xx_group_type {
	MOS65XX_GRP_INVALID = 0,  ///< CS_GRP_INVALID
	MOS65XX_GRP_JUMP,		 ///< = CS_GRP_JUMP
	MOS65XX_GRP_CALL,		 ///< = CS_GRP_RET
	MOS65XX_GRP_RET,		  ///< = CS_GRP_RET
	MOS65XX_GRP_INT,		  ///< = CS_GRP_INT
	MOS65XX_GRP_IRET = 5,	 ///< = CS_GRP_IRET
	MOS65XX_GRP_BRANCH_RELATIVE = 6, ///< = CS_GRP_BRANCH_RELATIVE
	MOS65XX_GRP_ENDING,// <-- mark the end of the list of groups
} mos65xx_group_type;

/// Operand type for instruction's operands
typedef enum mos65xx_op_type {
	MOS65XX_OP_INVALID = 0, ///< = CS_OP_INVALID (Uninitialized).
	MOS65XX_OP_REG, ///< = CS_OP_REG (Register operand).
	MOS65XX_OP_IMM, ///< = CS_OP_IMM (Immediate operand).
	MOS65XX_OP_MEM, ///< = CS_OP_MEM (Memory operand).
} mos65xx_op_type;

/// Instruction operand
typedef struct cs_mos65xx_op {
	mos65xx_op_type type;	///< operand type
	union {
		mos65xx_reg reg;	///< register value for REG operand
		uint16_t imm;		///< immediate value for IMM operand
		uint32_t mem;		///< address for MEM operand
	};
} cs_mos65xx_op;

/// The MOS65XX address mode and it's operands
typedef struct cs_mos65xx {
	mos65xx_address_mode am;
	bool modifies_flags;

	/// Number of operands of this instruction,
	/// or 0 when instruction has no operand.
	uint8_t op_count;
	cs_mos65xx_op operands[3]; ///< operands for this instruction.
} cs_mos65xx;

#ifdef __cplusplus
}
#endif

#endif //CAPSTONE_MOS65XX_H
