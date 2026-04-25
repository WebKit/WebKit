/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|*  * RISCV Disassembler                                                      *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2015 */

#include "../../MCInst.h"
#include "../../LEB128.h"
#include "../../cs_priv.h"

// Helper functions for extracting fields from encoded instructions.
// InsnType must either be integral or an APInt-like object that must:
// * Have a static const max_size_in_bits equal to the number of bits in the
//   encoding.
// * be default-constructible and copy-constructible
// * be constructible from a uint64_t
// * be constructible from an APInt (this can be private)
// * Support getBitsSet(loBit, hiBit)
// * be convertible to uint64_t
// * Support the ~, &, ==, !=, and |= operators with other objects of the same type
// * Support shift (<<, >>) with signed and unsigned integers on the RHS
// * Support put (<<) to raw_ostream&
#define FieldFromInstruction(fname, InsnType) \
static InsnType fname(InsnType insn, unsigned startBit, unsigned numBits) \
{ \
  InsnType fieldMask; \
  if (numBits == sizeof(InsnType)*8) \
    fieldMask = (InsnType)(-1LL); \
  else \
    fieldMask = (((InsnType)1 << numBits) - 1) << startBit; \
  return (insn & fieldMask) >> startBit; \
}

static const uint8_t DecoderTable16[] = {
/* 0 */       MCD_OPC_ExtractField, 13, 3,  // Inst{15-13} ...
/* 3 */       MCD_OPC_FilterValue, 0, 77, 0, 0, // Skip to: 85
/* 8 */       MCD_OPC_ExtractField, 0, 2,  // Inst{1-0} ...
/* 11 */      MCD_OPC_FilterValue, 0, 25, 0, 0, // Skip to: 41
/* 16 */      MCD_OPC_CheckPredicate, 0, 11, 0, 0, // Skip to: 32
/* 21 */      MCD_OPC_CheckField, 2, 11, 0, 4, 0, 0, // Skip to: 32
/* 28 */      MCD_OPC_Decode, 182, 2, 0, // Opcode: C_UNIMP
/* 32 */      MCD_OPC_CheckPredicate, 0, 116, 2, 0, // Skip to: 665
/* 37 */      MCD_OPC_Decode, 144, 2, 1, // Opcode: C_ADDI4SPN
/* 41 */      MCD_OPC_FilterValue, 1, 25, 0, 0, // Skip to: 71
/* 46 */      MCD_OPC_CheckPredicate, 0, 11, 0, 0, // Skip to: 62
/* 51 */      MCD_OPC_CheckField, 7, 6, 0, 4, 0, 0, // Skip to: 62
/* 58 */      MCD_OPC_Decode, 171, 2, 0, // Opcode: C_NOP
/* 62 */      MCD_OPC_CheckPredicate, 0, 86, 2, 0, // Skip to: 665
/* 67 */      MCD_OPC_Decode, 142, 2, 2, // Opcode: C_ADDI
/* 71 */      MCD_OPC_FilterValue, 2, 77, 2, 0, // Skip to: 665
/* 76 */      MCD_OPC_CheckPredicate, 0, 72, 2, 0, // Skip to: 665
/* 81 */      MCD_OPC_Decode, 175, 2, 3, // Opcode: C_SLLI
/* 85 */      MCD_OPC_FilterValue, 1, 45, 0, 0, // Skip to: 135
/* 90 */      MCD_OPC_ExtractField, 0, 2,  // Inst{1-0} ...
/* 93 */      MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 107
/* 98 */      MCD_OPC_CheckPredicate, 1, 50, 2, 0, // Skip to: 665
/* 103 */     MCD_OPC_Decode, 152, 2, 4, // Opcode: C_FLD
/* 107 */     MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 121
/* 112 */     MCD_OPC_CheckPredicate, 2, 36, 2, 0, // Skip to: 665
/* 117 */     MCD_OPC_Decode, 145, 2, 2, // Opcode: C_ADDIW
/* 121 */     MCD_OPC_FilterValue, 2, 27, 2, 0, // Skip to: 665
/* 126 */     MCD_OPC_CheckPredicate, 1, 22, 2, 0, // Skip to: 665
/* 131 */     MCD_OPC_Decode, 153, 2, 5, // Opcode: C_FLDSP
/* 135 */     MCD_OPC_FilterValue, 2, 45, 0, 0, // Skip to: 185
/* 140 */     MCD_OPC_ExtractField, 0, 2,  // Inst{1-0} ...
/* 143 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 157
/* 148 */     MCD_OPC_CheckPredicate, 0, 0, 2, 0, // Skip to: 665
/* 153 */     MCD_OPC_Decode, 168, 2, 6, // Opcode: C_LW
/* 157 */     MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 171
/* 162 */     MCD_OPC_CheckPredicate, 0, 242, 1, 0, // Skip to: 665
/* 167 */     MCD_OPC_Decode, 166, 2, 7, // Opcode: C_LI
/* 171 */     MCD_OPC_FilterValue, 2, 233, 1, 0, // Skip to: 665
/* 176 */     MCD_OPC_CheckPredicate, 0, 228, 1, 0, // Skip to: 665
/* 181 */     MCD_OPC_Decode, 169, 2, 8, // Opcode: C_LWSP
/* 185 */     MCD_OPC_FilterValue, 3, 61, 0, 0, // Skip to: 251
/* 190 */     MCD_OPC_ExtractField, 0, 2,  // Inst{1-0} ...
/* 193 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 207
/* 198 */     MCD_OPC_CheckPredicate, 2, 206, 1, 0, // Skip to: 665
/* 203 */     MCD_OPC_Decode, 164, 2, 9, // Opcode: C_LD
/* 207 */     MCD_OPC_FilterValue, 1, 25, 0, 0, // Skip to: 237
/* 212 */     MCD_OPC_CheckPredicate, 0, 11, 0, 0, // Skip to: 228
/* 217 */     MCD_OPC_CheckField, 7, 5, 2, 4, 0, 0, // Skip to: 228
/* 224 */     MCD_OPC_Decode, 143, 2, 10, // Opcode: C_ADDI16SP
/* 228 */     MCD_OPC_CheckPredicate, 0, 176, 1, 0, // Skip to: 665
/* 233 */     MCD_OPC_Decode, 167, 2, 11, // Opcode: C_LUI
/* 237 */     MCD_OPC_FilterValue, 2, 167, 1, 0, // Skip to: 665
/* 242 */     MCD_OPC_CheckPredicate, 2, 162, 1, 0, // Skip to: 665
/* 247 */     MCD_OPC_Decode, 165, 2, 12, // Opcode: C_LDSP
/* 251 */     MCD_OPC_FilterValue, 4, 3, 1, 0, // Skip to: 515
/* 256 */     MCD_OPC_ExtractField, 0, 2,  // Inst{1-0} ...
/* 259 */     MCD_OPC_FilterValue, 1, 167, 0, 0, // Skip to: 431
/* 264 */     MCD_OPC_ExtractField, 10, 2,  // Inst{11-10} ...
/* 267 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 281
/* 272 */     MCD_OPC_CheckPredicate, 0, 132, 1, 0, // Skip to: 665
/* 277 */     MCD_OPC_Decode, 177, 2, 13, // Opcode: C_SRLI
/* 281 */     MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 295
/* 286 */     MCD_OPC_CheckPredicate, 0, 118, 1, 0, // Skip to: 665
/* 291 */     MCD_OPC_Decode, 176, 2, 13, // Opcode: C_SRAI
/* 295 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 309
/* 300 */     MCD_OPC_CheckPredicate, 0, 104, 1, 0, // Skip to: 665
/* 305 */     MCD_OPC_Decode, 148, 2, 14, // Opcode: C_ANDI
/* 309 */     MCD_OPC_FilterValue, 3, 95, 1, 0, // Skip to: 665
/* 314 */     MCD_OPC_ExtractField, 5, 2,  // Inst{6-5} ...
/* 317 */     MCD_OPC_FilterValue, 0, 31, 0, 0, // Skip to: 353
/* 322 */     MCD_OPC_ExtractField, 12, 1,  // Inst{12} ...
/* 325 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 339
/* 330 */     MCD_OPC_CheckPredicate, 0, 74, 1, 0, // Skip to: 665
/* 335 */     MCD_OPC_Decode, 178, 2, 15, // Opcode: C_SUB
/* 339 */     MCD_OPC_FilterValue, 1, 65, 1, 0, // Skip to: 665
/* 344 */     MCD_OPC_CheckPredicate, 2, 60, 1, 0, // Skip to: 665
/* 349 */     MCD_OPC_Decode, 179, 2, 15, // Opcode: C_SUBW
/* 353 */     MCD_OPC_FilterValue, 1, 31, 0, 0, // Skip to: 389
/* 358 */     MCD_OPC_ExtractField, 12, 1,  // Inst{12} ...
/* 361 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 375
/* 366 */     MCD_OPC_CheckPredicate, 0, 38, 1, 0, // Skip to: 665
/* 371 */     MCD_OPC_Decode, 183, 2, 15, // Opcode: C_XOR
/* 375 */     MCD_OPC_FilterValue, 1, 29, 1, 0, // Skip to: 665
/* 380 */     MCD_OPC_CheckPredicate, 2, 24, 1, 0, // Skip to: 665
/* 385 */     MCD_OPC_Decode, 146, 2, 15, // Opcode: C_ADDW
/* 389 */     MCD_OPC_FilterValue, 2, 16, 0, 0, // Skip to: 410
/* 394 */     MCD_OPC_CheckPredicate, 0, 10, 1, 0, // Skip to: 665
/* 399 */     MCD_OPC_CheckField, 12, 1, 0, 3, 1, 0, // Skip to: 665
/* 406 */     MCD_OPC_Decode, 172, 2, 15, // Opcode: C_OR
/* 410 */     MCD_OPC_FilterValue, 3, 250, 0, 0, // Skip to: 665
/* 415 */     MCD_OPC_CheckPredicate, 0, 245, 0, 0, // Skip to: 665
/* 420 */     MCD_OPC_CheckField, 12, 1, 0, 238, 0, 0, // Skip to: 665
/* 427 */     MCD_OPC_Decode, 147, 2, 15, // Opcode: C_AND
/* 431 */     MCD_OPC_FilterValue, 2, 229, 0, 0, // Skip to: 665
/* 436 */     MCD_OPC_ExtractField, 12, 1,  // Inst{12} ...
/* 439 */     MCD_OPC_FilterValue, 0, 25, 0, 0, // Skip to: 469
/* 444 */     MCD_OPC_CheckPredicate, 0, 11, 0, 0, // Skip to: 460
/* 449 */     MCD_OPC_CheckField, 2, 5, 0, 4, 0, 0, // Skip to: 460
/* 456 */     MCD_OPC_Decode, 163, 2, 16, // Opcode: C_JR
/* 460 */     MCD_OPC_CheckPredicate, 0, 200, 0, 0, // Skip to: 665
/* 465 */     MCD_OPC_Decode, 170, 2, 17, // Opcode: C_MV
/* 469 */     MCD_OPC_FilterValue, 1, 191, 0, 0, // Skip to: 665
/* 474 */     MCD_OPC_CheckPredicate, 0, 11, 0, 0, // Skip to: 490
/* 479 */     MCD_OPC_CheckField, 2, 10, 0, 4, 0, 0, // Skip to: 490
/* 486 */     MCD_OPC_Decode, 151, 2, 0, // Opcode: C_EBREAK
/* 490 */     MCD_OPC_CheckPredicate, 0, 11, 0, 0, // Skip to: 506
/* 495 */     MCD_OPC_CheckField, 2, 5, 0, 4, 0, 0, // Skip to: 506
/* 502 */     MCD_OPC_Decode, 162, 2, 16, // Opcode: C_JALR
/* 506 */     MCD_OPC_CheckPredicate, 0, 154, 0, 0, // Skip to: 665
/* 511 */     MCD_OPC_Decode, 141, 2, 18, // Opcode: C_ADD
/* 515 */     MCD_OPC_FilterValue, 5, 45, 0, 0, // Skip to: 565
/* 520 */     MCD_OPC_ExtractField, 0, 2,  // Inst{1-0} ...
/* 523 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 537
/* 528 */     MCD_OPC_CheckPredicate, 1, 132, 0, 0, // Skip to: 665
/* 533 */     MCD_OPC_Decode, 156, 2, 4, // Opcode: C_FSD
/* 537 */     MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 551
/* 542 */     MCD_OPC_CheckPredicate, 0, 118, 0, 0, // Skip to: 665
/* 547 */     MCD_OPC_Decode, 160, 2, 19, // Opcode: C_J
/* 551 */     MCD_OPC_FilterValue, 2, 109, 0, 0, // Skip to: 665
/* 556 */     MCD_OPC_CheckPredicate, 1, 104, 0, 0, // Skip to: 665
/* 561 */     MCD_OPC_Decode, 157, 2, 20, // Opcode: C_FSDSP
/* 565 */     MCD_OPC_FilterValue, 6, 45, 0, 0, // Skip to: 615
/* 570 */     MCD_OPC_ExtractField, 0, 2,  // Inst{1-0} ...
/* 573 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 587
/* 578 */     MCD_OPC_CheckPredicate, 0, 82, 0, 0, // Skip to: 665
/* 583 */     MCD_OPC_Decode, 180, 2, 6, // Opcode: C_SW
/* 587 */     MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 601
/* 592 */     MCD_OPC_CheckPredicate, 0, 68, 0, 0, // Skip to: 665
/* 597 */     MCD_OPC_Decode, 149, 2, 21, // Opcode: C_BEQZ
/* 601 */     MCD_OPC_FilterValue, 2, 59, 0, 0, // Skip to: 665
/* 606 */     MCD_OPC_CheckPredicate, 0, 54, 0, 0, // Skip to: 665
/* 611 */     MCD_OPC_Decode, 181, 2, 22, // Opcode: C_SWSP
/* 615 */     MCD_OPC_FilterValue, 7, 45, 0, 0, // Skip to: 665
/* 620 */     MCD_OPC_ExtractField, 0, 2,  // Inst{1-0} ...
/* 623 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 637
/* 628 */     MCD_OPC_CheckPredicate, 2, 32, 0, 0, // Skip to: 665
/* 633 */     MCD_OPC_Decode, 173, 2, 9, // Opcode: C_SD
/* 637 */     MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 651
/* 642 */     MCD_OPC_CheckPredicate, 0, 18, 0, 0, // Skip to: 665
/* 647 */     MCD_OPC_Decode, 150, 2, 21, // Opcode: C_BNEZ
/* 651 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 665
/* 656 */     MCD_OPC_CheckPredicate, 2, 4, 0, 0, // Skip to: 665
/* 661 */     MCD_OPC_Decode, 174, 2, 23, // Opcode: C_SDSP
/* 665 */     MCD_OPC_Fail,
  0
};

static const uint8_t DecoderTable32[] = {
/* 0 */       MCD_OPC_ExtractField, 0, 7,  // Inst{6-0} ...
/* 3 */       MCD_OPC_FilterValue, 3, 76, 0, 0, // Skip to: 84
/* 8 */       MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 11 */      MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 20
/* 16 */      MCD_OPC_Decode, 129, 3, 24, // Opcode: LB
/* 20 */      MCD_OPC_FilterValue, 1, 4, 0, 0, // Skip to: 29
/* 25 */      MCD_OPC_Decode, 132, 3, 24, // Opcode: LH
/* 29 */      MCD_OPC_FilterValue, 2, 4, 0, 0, // Skip to: 38
/* 34 */      MCD_OPC_Decode, 143, 3, 24, // Opcode: LW
/* 38 */      MCD_OPC_FilterValue, 3, 9, 0, 0, // Skip to: 52
/* 43 */      MCD_OPC_CheckPredicate, 3, 55, 15, 0, // Skip to: 3943
/* 48 */      MCD_OPC_Decode, 131, 3, 24, // Opcode: LD
/* 52 */      MCD_OPC_FilterValue, 4, 4, 0, 0, // Skip to: 61
/* 57 */      MCD_OPC_Decode, 130, 3, 24, // Opcode: LBU
/* 61 */      MCD_OPC_FilterValue, 5, 4, 0, 0, // Skip to: 70
/* 66 */      MCD_OPC_Decode, 133, 3, 24, // Opcode: LHU
/* 70 */      MCD_OPC_FilterValue, 6, 28, 15, 0, // Skip to: 3943
/* 75 */      MCD_OPC_CheckPredicate, 3, 23, 15, 0, // Skip to: 3943
/* 80 */      MCD_OPC_Decode, 144, 3, 24, // Opcode: LWU
/* 84 */      MCD_OPC_FilterValue, 7, 31, 0, 0, // Skip to: 120
/* 89 */      MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 92 */      MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 106
/* 97 */      MCD_OPC_CheckPredicate, 4, 1, 15, 0, // Skip to: 3943
/* 102 */     MCD_OPC_Decode, 224, 2, 25, // Opcode: FLW
/* 106 */     MCD_OPC_FilterValue, 3, 248, 14, 0, // Skip to: 3943
/* 111 */     MCD_OPC_CheckPredicate, 5, 243, 14, 0, // Skip to: 3943
/* 116 */     MCD_OPC_Decode, 219, 2, 26, // Opcode: FLD
/* 120 */     MCD_OPC_FilterValue, 15, 52, 0, 0, // Skip to: 177
/* 125 */     MCD_OPC_ExtractField, 7, 13,  // Inst{19-7} ...
/* 128 */     MCD_OPC_FilterValue, 0, 28, 0, 0, // Skip to: 161
/* 133 */     MCD_OPC_ExtractField, 28, 4,  // Inst{31-28} ...
/* 136 */     MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 145
/* 141 */     MCD_OPC_Decode, 214, 2, 27, // Opcode: FENCE
/* 145 */     MCD_OPC_FilterValue, 8, 209, 14, 0, // Skip to: 3943
/* 150 */     MCD_OPC_CheckField, 20, 8, 51, 202, 14, 0, // Skip to: 3943
/* 157 */     MCD_OPC_Decode, 216, 2, 0, // Opcode: FENCE_TSO
/* 161 */     MCD_OPC_FilterValue, 32, 193, 14, 0, // Skip to: 3943
/* 166 */     MCD_OPC_CheckField, 20, 12, 0, 186, 14, 0, // Skip to: 3943
/* 173 */     MCD_OPC_Decode, 215, 2, 0, // Opcode: FENCE_I
/* 177 */     MCD_OPC_FilterValue, 19, 99, 0, 0, // Skip to: 281
/* 182 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 185 */     MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 194
/* 190 */     MCD_OPC_Decode, 179, 1, 24, // Opcode: ADDI
/* 194 */     MCD_OPC_FilterValue, 1, 11, 0, 0, // Skip to: 210
/* 199 */     MCD_OPC_CheckField, 26, 6, 0, 153, 14, 0, // Skip to: 3943
/* 206 */     MCD_OPC_Decode, 170, 3, 28, // Opcode: SLLI
/* 210 */     MCD_OPC_FilterValue, 2, 4, 0, 0, // Skip to: 219
/* 215 */     MCD_OPC_Decode, 174, 3, 24, // Opcode: SLTI
/* 219 */     MCD_OPC_FilterValue, 3, 4, 0, 0, // Skip to: 228
/* 224 */     MCD_OPC_Decode, 175, 3, 24, // Opcode: SLTIU
/* 228 */     MCD_OPC_FilterValue, 4, 4, 0, 0, // Skip to: 237
/* 233 */     MCD_OPC_Decode, 193, 3, 24, // Opcode: XORI
/* 237 */     MCD_OPC_FilterValue, 5, 21, 0, 0, // Skip to: 263
/* 242 */     MCD_OPC_ExtractField, 26, 6,  // Inst{31-26} ...
/* 245 */     MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 254
/* 250 */     MCD_OPC_Decode, 183, 3, 28, // Opcode: SRLI
/* 254 */     MCD_OPC_FilterValue, 16, 100, 14, 0, // Skip to: 3943
/* 259 */     MCD_OPC_Decode, 178, 3, 28, // Opcode: SRAI
/* 263 */     MCD_OPC_FilterValue, 6, 4, 0, 0, // Skip to: 272
/* 268 */     MCD_OPC_Decode, 152, 3, 24, // Opcode: ORI
/* 272 */     MCD_OPC_FilterValue, 7, 82, 14, 0, // Skip to: 3943
/* 277 */     MCD_OPC_Decode, 255, 1, 24, // Opcode: ANDI
/* 281 */     MCD_OPC_FilterValue, 23, 4, 0, 0, // Skip to: 290
/* 286 */     MCD_OPC_Decode, 128, 2, 29, // Opcode: AUIPC
/* 290 */     MCD_OPC_FilterValue, 27, 74, 0, 0, // Skip to: 369
/* 295 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 298 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 312
/* 303 */     MCD_OPC_CheckPredicate, 3, 51, 14, 0, // Skip to: 3943
/* 308 */     MCD_OPC_Decode, 180, 1, 24, // Opcode: ADDIW
/* 312 */     MCD_OPC_FilterValue, 1, 16, 0, 0, // Skip to: 333
/* 317 */     MCD_OPC_CheckPredicate, 3, 37, 14, 0, // Skip to: 3943
/* 322 */     MCD_OPC_CheckField, 25, 7, 0, 30, 14, 0, // Skip to: 3943
/* 329 */     MCD_OPC_Decode, 171, 3, 30, // Opcode: SLLIW
/* 333 */     MCD_OPC_FilterValue, 5, 21, 14, 0, // Skip to: 3943
/* 338 */     MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 341 */     MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 355
/* 346 */     MCD_OPC_CheckPredicate, 3, 8, 14, 0, // Skip to: 3943
/* 351 */     MCD_OPC_Decode, 184, 3, 30, // Opcode: SRLIW
/* 355 */     MCD_OPC_FilterValue, 32, 255, 13, 0, // Skip to: 3943
/* 360 */     MCD_OPC_CheckPredicate, 3, 250, 13, 0, // Skip to: 3943
/* 365 */     MCD_OPC_Decode, 179, 3, 30, // Opcode: SRAIW
/* 369 */     MCD_OPC_FilterValue, 35, 44, 0, 0, // Skip to: 418
/* 374 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 377 */     MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 386
/* 382 */     MCD_OPC_Decode, 157, 3, 31, // Opcode: SB
/* 386 */     MCD_OPC_FilterValue, 1, 4, 0, 0, // Skip to: 395
/* 391 */     MCD_OPC_Decode, 168, 3, 31, // Opcode: SH
/* 395 */     MCD_OPC_FilterValue, 2, 4, 0, 0, // Skip to: 404
/* 400 */     MCD_OPC_Decode, 188, 3, 31, // Opcode: SW
/* 404 */     MCD_OPC_FilterValue, 3, 206, 13, 0, // Skip to: 3943
/* 409 */     MCD_OPC_CheckPredicate, 3, 201, 13, 0, // Skip to: 3943
/* 414 */     MCD_OPC_Decode, 166, 3, 31, // Opcode: SD
/* 418 */     MCD_OPC_FilterValue, 39, 31, 0, 0, // Skip to: 454
/* 423 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 426 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 440
/* 431 */     MCD_OPC_CheckPredicate, 4, 179, 13, 0, // Skip to: 3943
/* 436 */     MCD_OPC_Decode, 254, 2, 32, // Opcode: FSW
/* 440 */     MCD_OPC_FilterValue, 3, 170, 13, 0, // Skip to: 3943
/* 445 */     MCD_OPC_CheckPredicate, 5, 165, 13, 0, // Skip to: 3943
/* 450 */     MCD_OPC_Decode, 243, 2, 33, // Opcode: FSD
/* 454 */     MCD_OPC_FilterValue, 47, 107, 6, 0, // Skip to: 2102
/* 459 */     MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 462 */     MCD_OPC_FilterValue, 0, 31, 0, 0, // Skip to: 498
/* 467 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 470 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 484
/* 475 */     MCD_OPC_CheckPredicate, 6, 135, 13, 0, // Skip to: 3943
/* 480 */     MCD_OPC_Decode, 186, 1, 34, // Opcode: AMOADD_W
/* 484 */     MCD_OPC_FilterValue, 3, 126, 13, 0, // Skip to: 3943
/* 489 */     MCD_OPC_CheckPredicate, 7, 121, 13, 0, // Skip to: 3943
/* 494 */     MCD_OPC_Decode, 182, 1, 34, // Opcode: AMOADD_D
/* 498 */     MCD_OPC_FilterValue, 1, 31, 0, 0, // Skip to: 534
/* 503 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 506 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 520
/* 511 */     MCD_OPC_CheckPredicate, 6, 99, 13, 0, // Skip to: 3943
/* 516 */     MCD_OPC_Decode, 189, 1, 34, // Opcode: AMOADD_W_RL
/* 520 */     MCD_OPC_FilterValue, 3, 90, 13, 0, // Skip to: 3943
/* 525 */     MCD_OPC_CheckPredicate, 7, 85, 13, 0, // Skip to: 3943
/* 530 */     MCD_OPC_Decode, 185, 1, 34, // Opcode: AMOADD_D_RL
/* 534 */     MCD_OPC_FilterValue, 2, 31, 0, 0, // Skip to: 570
/* 539 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 542 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 556
/* 547 */     MCD_OPC_CheckPredicate, 6, 63, 13, 0, // Skip to: 3943
/* 552 */     MCD_OPC_Decode, 187, 1, 34, // Opcode: AMOADD_W_AQ
/* 556 */     MCD_OPC_FilterValue, 3, 54, 13, 0, // Skip to: 3943
/* 561 */     MCD_OPC_CheckPredicate, 7, 49, 13, 0, // Skip to: 3943
/* 566 */     MCD_OPC_Decode, 183, 1, 34, // Opcode: AMOADD_D_AQ
/* 570 */     MCD_OPC_FilterValue, 3, 31, 0, 0, // Skip to: 606
/* 575 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 578 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 592
/* 583 */     MCD_OPC_CheckPredicate, 6, 27, 13, 0, // Skip to: 3943
/* 588 */     MCD_OPC_Decode, 188, 1, 34, // Opcode: AMOADD_W_AQ_RL
/* 592 */     MCD_OPC_FilterValue, 3, 18, 13, 0, // Skip to: 3943
/* 597 */     MCD_OPC_CheckPredicate, 7, 13, 13, 0, // Skip to: 3943
/* 602 */     MCD_OPC_Decode, 184, 1, 34, // Opcode: AMOADD_D_AQ_RL
/* 606 */     MCD_OPC_FilterValue, 4, 31, 0, 0, // Skip to: 642
/* 611 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 614 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 628
/* 619 */     MCD_OPC_CheckPredicate, 6, 247, 12, 0, // Skip to: 3943
/* 624 */     MCD_OPC_Decode, 242, 1, 34, // Opcode: AMOSWAP_W
/* 628 */     MCD_OPC_FilterValue, 3, 238, 12, 0, // Skip to: 3943
/* 633 */     MCD_OPC_CheckPredicate, 7, 233, 12, 0, // Skip to: 3943
/* 638 */     MCD_OPC_Decode, 238, 1, 34, // Opcode: AMOSWAP_D
/* 642 */     MCD_OPC_FilterValue, 5, 31, 0, 0, // Skip to: 678
/* 647 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 650 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 664
/* 655 */     MCD_OPC_CheckPredicate, 6, 211, 12, 0, // Skip to: 3943
/* 660 */     MCD_OPC_Decode, 245, 1, 34, // Opcode: AMOSWAP_W_RL
/* 664 */     MCD_OPC_FilterValue, 3, 202, 12, 0, // Skip to: 3943
/* 669 */     MCD_OPC_CheckPredicate, 7, 197, 12, 0, // Skip to: 3943
/* 674 */     MCD_OPC_Decode, 241, 1, 34, // Opcode: AMOSWAP_D_RL
/* 678 */     MCD_OPC_FilterValue, 6, 31, 0, 0, // Skip to: 714
/* 683 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 686 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 700
/* 691 */     MCD_OPC_CheckPredicate, 6, 175, 12, 0, // Skip to: 3943
/* 696 */     MCD_OPC_Decode, 243, 1, 34, // Opcode: AMOSWAP_W_AQ
/* 700 */     MCD_OPC_FilterValue, 3, 166, 12, 0, // Skip to: 3943
/* 705 */     MCD_OPC_CheckPredicate, 7, 161, 12, 0, // Skip to: 3943
/* 710 */     MCD_OPC_Decode, 239, 1, 34, // Opcode: AMOSWAP_D_AQ
/* 714 */     MCD_OPC_FilterValue, 7, 31, 0, 0, // Skip to: 750
/* 719 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 722 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 736
/* 727 */     MCD_OPC_CheckPredicate, 6, 139, 12, 0, // Skip to: 3943
/* 732 */     MCD_OPC_Decode, 244, 1, 34, // Opcode: AMOSWAP_W_AQ_RL
/* 736 */     MCD_OPC_FilterValue, 3, 130, 12, 0, // Skip to: 3943
/* 741 */     MCD_OPC_CheckPredicate, 7, 125, 12, 0, // Skip to: 3943
/* 746 */     MCD_OPC_Decode, 240, 1, 34, // Opcode: AMOSWAP_D_AQ_RL
/* 750 */     MCD_OPC_FilterValue, 8, 45, 0, 0, // Skip to: 800
/* 755 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 758 */     MCD_OPC_FilterValue, 2, 16, 0, 0, // Skip to: 779
/* 763 */     MCD_OPC_CheckPredicate, 6, 103, 12, 0, // Skip to: 3943
/* 768 */     MCD_OPC_CheckField, 20, 5, 0, 96, 12, 0, // Skip to: 3943
/* 775 */     MCD_OPC_Decode, 138, 3, 35, // Opcode: LR_W
/* 779 */     MCD_OPC_FilterValue, 3, 87, 12, 0, // Skip to: 3943
/* 784 */     MCD_OPC_CheckPredicate, 7, 82, 12, 0, // Skip to: 3943
/* 789 */     MCD_OPC_CheckField, 20, 5, 0, 75, 12, 0, // Skip to: 3943
/* 796 */     MCD_OPC_Decode, 134, 3, 35, // Opcode: LR_D
/* 800 */     MCD_OPC_FilterValue, 9, 45, 0, 0, // Skip to: 850
/* 805 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 808 */     MCD_OPC_FilterValue, 2, 16, 0, 0, // Skip to: 829
/* 813 */     MCD_OPC_CheckPredicate, 6, 53, 12, 0, // Skip to: 3943
/* 818 */     MCD_OPC_CheckField, 20, 5, 0, 46, 12, 0, // Skip to: 3943
/* 825 */     MCD_OPC_Decode, 141, 3, 35, // Opcode: LR_W_RL
/* 829 */     MCD_OPC_FilterValue, 3, 37, 12, 0, // Skip to: 3943
/* 834 */     MCD_OPC_CheckPredicate, 7, 32, 12, 0, // Skip to: 3943
/* 839 */     MCD_OPC_CheckField, 20, 5, 0, 25, 12, 0, // Skip to: 3943
/* 846 */     MCD_OPC_Decode, 137, 3, 35, // Opcode: LR_D_RL
/* 850 */     MCD_OPC_FilterValue, 10, 45, 0, 0, // Skip to: 900
/* 855 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 858 */     MCD_OPC_FilterValue, 2, 16, 0, 0, // Skip to: 879
/* 863 */     MCD_OPC_CheckPredicate, 6, 3, 12, 0, // Skip to: 3943
/* 868 */     MCD_OPC_CheckField, 20, 5, 0, 252, 11, 0, // Skip to: 3943
/* 875 */     MCD_OPC_Decode, 139, 3, 35, // Opcode: LR_W_AQ
/* 879 */     MCD_OPC_FilterValue, 3, 243, 11, 0, // Skip to: 3943
/* 884 */     MCD_OPC_CheckPredicate, 7, 238, 11, 0, // Skip to: 3943
/* 889 */     MCD_OPC_CheckField, 20, 5, 0, 231, 11, 0, // Skip to: 3943
/* 896 */     MCD_OPC_Decode, 135, 3, 35, // Opcode: LR_D_AQ
/* 900 */     MCD_OPC_FilterValue, 11, 45, 0, 0, // Skip to: 950
/* 905 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 908 */     MCD_OPC_FilterValue, 2, 16, 0, 0, // Skip to: 929
/* 913 */     MCD_OPC_CheckPredicate, 6, 209, 11, 0, // Skip to: 3943
/* 918 */     MCD_OPC_CheckField, 20, 5, 0, 202, 11, 0, // Skip to: 3943
/* 925 */     MCD_OPC_Decode, 140, 3, 35, // Opcode: LR_W_AQ_RL
/* 929 */     MCD_OPC_FilterValue, 3, 193, 11, 0, // Skip to: 3943
/* 934 */     MCD_OPC_CheckPredicate, 7, 188, 11, 0, // Skip to: 3943
/* 939 */     MCD_OPC_CheckField, 20, 5, 0, 181, 11, 0, // Skip to: 3943
/* 946 */     MCD_OPC_Decode, 136, 3, 35, // Opcode: LR_D_AQ_RL
/* 950 */     MCD_OPC_FilterValue, 12, 31, 0, 0, // Skip to: 986
/* 955 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 958 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 972
/* 963 */     MCD_OPC_CheckPredicate, 6, 159, 11, 0, // Skip to: 3943
/* 968 */     MCD_OPC_Decode, 162, 3, 34, // Opcode: SC_W
/* 972 */     MCD_OPC_FilterValue, 3, 150, 11, 0, // Skip to: 3943
/* 977 */     MCD_OPC_CheckPredicate, 7, 145, 11, 0, // Skip to: 3943
/* 982 */     MCD_OPC_Decode, 158, 3, 34, // Opcode: SC_D
/* 986 */     MCD_OPC_FilterValue, 13, 31, 0, 0, // Skip to: 1022
/* 991 */     MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 994 */     MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1008
/* 999 */     MCD_OPC_CheckPredicate, 6, 123, 11, 0, // Skip to: 3943
/* 1004 */    MCD_OPC_Decode, 165, 3, 34, // Opcode: SC_W_RL
/* 1008 */    MCD_OPC_FilterValue, 3, 114, 11, 0, // Skip to: 3943
/* 1013 */    MCD_OPC_CheckPredicate, 7, 109, 11, 0, // Skip to: 3943
/* 1018 */    MCD_OPC_Decode, 161, 3, 34, // Opcode: SC_D_RL
/* 1022 */    MCD_OPC_FilterValue, 14, 31, 0, 0, // Skip to: 1058
/* 1027 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1030 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1044
/* 1035 */    MCD_OPC_CheckPredicate, 6, 87, 11, 0, // Skip to: 3943
/* 1040 */    MCD_OPC_Decode, 163, 3, 34, // Opcode: SC_W_AQ
/* 1044 */    MCD_OPC_FilterValue, 3, 78, 11, 0, // Skip to: 3943
/* 1049 */    MCD_OPC_CheckPredicate, 7, 73, 11, 0, // Skip to: 3943
/* 1054 */    MCD_OPC_Decode, 159, 3, 34, // Opcode: SC_D_AQ
/* 1058 */    MCD_OPC_FilterValue, 15, 31, 0, 0, // Skip to: 1094
/* 1063 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1066 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1080
/* 1071 */    MCD_OPC_CheckPredicate, 6, 51, 11, 0, // Skip to: 3943
/* 1076 */    MCD_OPC_Decode, 164, 3, 34, // Opcode: SC_W_AQ_RL
/* 1080 */    MCD_OPC_FilterValue, 3, 42, 11, 0, // Skip to: 3943
/* 1085 */    MCD_OPC_CheckPredicate, 7, 37, 11, 0, // Skip to: 3943
/* 1090 */    MCD_OPC_Decode, 160, 3, 34, // Opcode: SC_D_AQ_RL
/* 1094 */    MCD_OPC_FilterValue, 16, 31, 0, 0, // Skip to: 1130
/* 1099 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1102 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1116
/* 1107 */    MCD_OPC_CheckPredicate, 6, 15, 11, 0, // Skip to: 3943
/* 1112 */    MCD_OPC_Decode, 250, 1, 34, // Opcode: AMOXOR_W
/* 1116 */    MCD_OPC_FilterValue, 3, 6, 11, 0, // Skip to: 3943
/* 1121 */    MCD_OPC_CheckPredicate, 7, 1, 11, 0, // Skip to: 3943
/* 1126 */    MCD_OPC_Decode, 246, 1, 34, // Opcode: AMOXOR_D
/* 1130 */    MCD_OPC_FilterValue, 17, 31, 0, 0, // Skip to: 1166
/* 1135 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1138 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1152
/* 1143 */    MCD_OPC_CheckPredicate, 6, 235, 10, 0, // Skip to: 3943
/* 1148 */    MCD_OPC_Decode, 253, 1, 34, // Opcode: AMOXOR_W_RL
/* 1152 */    MCD_OPC_FilterValue, 3, 226, 10, 0, // Skip to: 3943
/* 1157 */    MCD_OPC_CheckPredicate, 7, 221, 10, 0, // Skip to: 3943
/* 1162 */    MCD_OPC_Decode, 249, 1, 34, // Opcode: AMOXOR_D_RL
/* 1166 */    MCD_OPC_FilterValue, 18, 31, 0, 0, // Skip to: 1202
/* 1171 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1174 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1188
/* 1179 */    MCD_OPC_CheckPredicate, 6, 199, 10, 0, // Skip to: 3943
/* 1184 */    MCD_OPC_Decode, 251, 1, 34, // Opcode: AMOXOR_W_AQ
/* 1188 */    MCD_OPC_FilterValue, 3, 190, 10, 0, // Skip to: 3943
/* 1193 */    MCD_OPC_CheckPredicate, 7, 185, 10, 0, // Skip to: 3943
/* 1198 */    MCD_OPC_Decode, 247, 1, 34, // Opcode: AMOXOR_D_AQ
/* 1202 */    MCD_OPC_FilterValue, 19, 31, 0, 0, // Skip to: 1238
/* 1207 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1210 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1224
/* 1215 */    MCD_OPC_CheckPredicate, 6, 163, 10, 0, // Skip to: 3943
/* 1220 */    MCD_OPC_Decode, 252, 1, 34, // Opcode: AMOXOR_W_AQ_RL
/* 1224 */    MCD_OPC_FilterValue, 3, 154, 10, 0, // Skip to: 3943
/* 1229 */    MCD_OPC_CheckPredicate, 7, 149, 10, 0, // Skip to: 3943
/* 1234 */    MCD_OPC_Decode, 248, 1, 34, // Opcode: AMOXOR_D_AQ_RL
/* 1238 */    MCD_OPC_FilterValue, 32, 31, 0, 0, // Skip to: 1274
/* 1243 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1246 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1260
/* 1251 */    MCD_OPC_CheckPredicate, 6, 127, 10, 0, // Skip to: 3943
/* 1256 */    MCD_OPC_Decode, 234, 1, 34, // Opcode: AMOOR_W
/* 1260 */    MCD_OPC_FilterValue, 3, 118, 10, 0, // Skip to: 3943
/* 1265 */    MCD_OPC_CheckPredicate, 7, 113, 10, 0, // Skip to: 3943
/* 1270 */    MCD_OPC_Decode, 230, 1, 34, // Opcode: AMOOR_D
/* 1274 */    MCD_OPC_FilterValue, 33, 31, 0, 0, // Skip to: 1310
/* 1279 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1282 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1296
/* 1287 */    MCD_OPC_CheckPredicate, 6, 91, 10, 0, // Skip to: 3943
/* 1292 */    MCD_OPC_Decode, 237, 1, 34, // Opcode: AMOOR_W_RL
/* 1296 */    MCD_OPC_FilterValue, 3, 82, 10, 0, // Skip to: 3943
/* 1301 */    MCD_OPC_CheckPredicate, 7, 77, 10, 0, // Skip to: 3943
/* 1306 */    MCD_OPC_Decode, 233, 1, 34, // Opcode: AMOOR_D_RL
/* 1310 */    MCD_OPC_FilterValue, 34, 31, 0, 0, // Skip to: 1346
/* 1315 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1318 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1332
/* 1323 */    MCD_OPC_CheckPredicate, 6, 55, 10, 0, // Skip to: 3943
/* 1328 */    MCD_OPC_Decode, 235, 1, 34, // Opcode: AMOOR_W_AQ
/* 1332 */    MCD_OPC_FilterValue, 3, 46, 10, 0, // Skip to: 3943
/* 1337 */    MCD_OPC_CheckPredicate, 7, 41, 10, 0, // Skip to: 3943
/* 1342 */    MCD_OPC_Decode, 231, 1, 34, // Opcode: AMOOR_D_AQ
/* 1346 */    MCD_OPC_FilterValue, 35, 31, 0, 0, // Skip to: 1382
/* 1351 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1354 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1368
/* 1359 */    MCD_OPC_CheckPredicate, 6, 19, 10, 0, // Skip to: 3943
/* 1364 */    MCD_OPC_Decode, 236, 1, 34, // Opcode: AMOOR_W_AQ_RL
/* 1368 */    MCD_OPC_FilterValue, 3, 10, 10, 0, // Skip to: 3943
/* 1373 */    MCD_OPC_CheckPredicate, 7, 5, 10, 0, // Skip to: 3943
/* 1378 */    MCD_OPC_Decode, 232, 1, 34, // Opcode: AMOOR_D_AQ_RL
/* 1382 */    MCD_OPC_FilterValue, 48, 31, 0, 0, // Skip to: 1418
/* 1387 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1390 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1404
/* 1395 */    MCD_OPC_CheckPredicate, 6, 239, 9, 0, // Skip to: 3943
/* 1400 */    MCD_OPC_Decode, 194, 1, 34, // Opcode: AMOAND_W
/* 1404 */    MCD_OPC_FilterValue, 3, 230, 9, 0, // Skip to: 3943
/* 1409 */    MCD_OPC_CheckPredicate, 7, 225, 9, 0, // Skip to: 3943
/* 1414 */    MCD_OPC_Decode, 190, 1, 34, // Opcode: AMOAND_D
/* 1418 */    MCD_OPC_FilterValue, 49, 31, 0, 0, // Skip to: 1454
/* 1423 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1426 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1440
/* 1431 */    MCD_OPC_CheckPredicate, 6, 203, 9, 0, // Skip to: 3943
/* 1436 */    MCD_OPC_Decode, 197, 1, 34, // Opcode: AMOAND_W_RL
/* 1440 */    MCD_OPC_FilterValue, 3, 194, 9, 0, // Skip to: 3943
/* 1445 */    MCD_OPC_CheckPredicate, 7, 189, 9, 0, // Skip to: 3943
/* 1450 */    MCD_OPC_Decode, 193, 1, 34, // Opcode: AMOAND_D_RL
/* 1454 */    MCD_OPC_FilterValue, 50, 31, 0, 0, // Skip to: 1490
/* 1459 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1462 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1476
/* 1467 */    MCD_OPC_CheckPredicate, 6, 167, 9, 0, // Skip to: 3943
/* 1472 */    MCD_OPC_Decode, 195, 1, 34, // Opcode: AMOAND_W_AQ
/* 1476 */    MCD_OPC_FilterValue, 3, 158, 9, 0, // Skip to: 3943
/* 1481 */    MCD_OPC_CheckPredicate, 7, 153, 9, 0, // Skip to: 3943
/* 1486 */    MCD_OPC_Decode, 191, 1, 34, // Opcode: AMOAND_D_AQ
/* 1490 */    MCD_OPC_FilterValue, 51, 31, 0, 0, // Skip to: 1526
/* 1495 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1498 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1512
/* 1503 */    MCD_OPC_CheckPredicate, 6, 131, 9, 0, // Skip to: 3943
/* 1508 */    MCD_OPC_Decode, 196, 1, 34, // Opcode: AMOAND_W_AQ_RL
/* 1512 */    MCD_OPC_FilterValue, 3, 122, 9, 0, // Skip to: 3943
/* 1517 */    MCD_OPC_CheckPredicate, 7, 117, 9, 0, // Skip to: 3943
/* 1522 */    MCD_OPC_Decode, 192, 1, 34, // Opcode: AMOAND_D_AQ_RL
/* 1526 */    MCD_OPC_FilterValue, 64, 31, 0, 0, // Skip to: 1562
/* 1531 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1534 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1548
/* 1539 */    MCD_OPC_CheckPredicate, 6, 95, 9, 0, // Skip to: 3943
/* 1544 */    MCD_OPC_Decode, 226, 1, 34, // Opcode: AMOMIN_W
/* 1548 */    MCD_OPC_FilterValue, 3, 86, 9, 0, // Skip to: 3943
/* 1553 */    MCD_OPC_CheckPredicate, 7, 81, 9, 0, // Skip to: 3943
/* 1558 */    MCD_OPC_Decode, 222, 1, 34, // Opcode: AMOMIN_D
/* 1562 */    MCD_OPC_FilterValue, 65, 31, 0, 0, // Skip to: 1598
/* 1567 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1570 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1584
/* 1575 */    MCD_OPC_CheckPredicate, 6, 59, 9, 0, // Skip to: 3943
/* 1580 */    MCD_OPC_Decode, 229, 1, 34, // Opcode: AMOMIN_W_RL
/* 1584 */    MCD_OPC_FilterValue, 3, 50, 9, 0, // Skip to: 3943
/* 1589 */    MCD_OPC_CheckPredicate, 7, 45, 9, 0, // Skip to: 3943
/* 1594 */    MCD_OPC_Decode, 225, 1, 34, // Opcode: AMOMIN_D_RL
/* 1598 */    MCD_OPC_FilterValue, 66, 31, 0, 0, // Skip to: 1634
/* 1603 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1606 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1620
/* 1611 */    MCD_OPC_CheckPredicate, 6, 23, 9, 0, // Skip to: 3943
/* 1616 */    MCD_OPC_Decode, 227, 1, 34, // Opcode: AMOMIN_W_AQ
/* 1620 */    MCD_OPC_FilterValue, 3, 14, 9, 0, // Skip to: 3943
/* 1625 */    MCD_OPC_CheckPredicate, 7, 9, 9, 0, // Skip to: 3943
/* 1630 */    MCD_OPC_Decode, 223, 1, 34, // Opcode: AMOMIN_D_AQ
/* 1634 */    MCD_OPC_FilterValue, 67, 31, 0, 0, // Skip to: 1670
/* 1639 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1642 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1656
/* 1647 */    MCD_OPC_CheckPredicate, 6, 243, 8, 0, // Skip to: 3943
/* 1652 */    MCD_OPC_Decode, 228, 1, 34, // Opcode: AMOMIN_W_AQ_RL
/* 1656 */    MCD_OPC_FilterValue, 3, 234, 8, 0, // Skip to: 3943
/* 1661 */    MCD_OPC_CheckPredicate, 7, 229, 8, 0, // Skip to: 3943
/* 1666 */    MCD_OPC_Decode, 224, 1, 34, // Opcode: AMOMIN_D_AQ_RL
/* 1670 */    MCD_OPC_FilterValue, 80, 31, 0, 0, // Skip to: 1706
/* 1675 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1678 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1692
/* 1683 */    MCD_OPC_CheckPredicate, 6, 207, 8, 0, // Skip to: 3943
/* 1688 */    MCD_OPC_Decode, 210, 1, 34, // Opcode: AMOMAX_W
/* 1692 */    MCD_OPC_FilterValue, 3, 198, 8, 0, // Skip to: 3943
/* 1697 */    MCD_OPC_CheckPredicate, 7, 193, 8, 0, // Skip to: 3943
/* 1702 */    MCD_OPC_Decode, 206, 1, 34, // Opcode: AMOMAX_D
/* 1706 */    MCD_OPC_FilterValue, 81, 31, 0, 0, // Skip to: 1742
/* 1711 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1714 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1728
/* 1719 */    MCD_OPC_CheckPredicate, 6, 171, 8, 0, // Skip to: 3943
/* 1724 */    MCD_OPC_Decode, 213, 1, 34, // Opcode: AMOMAX_W_RL
/* 1728 */    MCD_OPC_FilterValue, 3, 162, 8, 0, // Skip to: 3943
/* 1733 */    MCD_OPC_CheckPredicate, 7, 157, 8, 0, // Skip to: 3943
/* 1738 */    MCD_OPC_Decode, 209, 1, 34, // Opcode: AMOMAX_D_RL
/* 1742 */    MCD_OPC_FilterValue, 82, 31, 0, 0, // Skip to: 1778
/* 1747 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1750 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1764
/* 1755 */    MCD_OPC_CheckPredicate, 6, 135, 8, 0, // Skip to: 3943
/* 1760 */    MCD_OPC_Decode, 211, 1, 34, // Opcode: AMOMAX_W_AQ
/* 1764 */    MCD_OPC_FilterValue, 3, 126, 8, 0, // Skip to: 3943
/* 1769 */    MCD_OPC_CheckPredicate, 7, 121, 8, 0, // Skip to: 3943
/* 1774 */    MCD_OPC_Decode, 207, 1, 34, // Opcode: AMOMAX_D_AQ
/* 1778 */    MCD_OPC_FilterValue, 83, 31, 0, 0, // Skip to: 1814
/* 1783 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1786 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1800
/* 1791 */    MCD_OPC_CheckPredicate, 6, 99, 8, 0, // Skip to: 3943
/* 1796 */    MCD_OPC_Decode, 212, 1, 34, // Opcode: AMOMAX_W_AQ_RL
/* 1800 */    MCD_OPC_FilterValue, 3, 90, 8, 0, // Skip to: 3943
/* 1805 */    MCD_OPC_CheckPredicate, 7, 85, 8, 0, // Skip to: 3943
/* 1810 */    MCD_OPC_Decode, 208, 1, 34, // Opcode: AMOMAX_D_AQ_RL
/* 1814 */    MCD_OPC_FilterValue, 96, 31, 0, 0, // Skip to: 1850
/* 1819 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1822 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1836
/* 1827 */    MCD_OPC_CheckPredicate, 6, 63, 8, 0, // Skip to: 3943
/* 1832 */    MCD_OPC_Decode, 218, 1, 34, // Opcode: AMOMINU_W
/* 1836 */    MCD_OPC_FilterValue, 3, 54, 8, 0, // Skip to: 3943
/* 1841 */    MCD_OPC_CheckPredicate, 7, 49, 8, 0, // Skip to: 3943
/* 1846 */    MCD_OPC_Decode, 214, 1, 34, // Opcode: AMOMINU_D
/* 1850 */    MCD_OPC_FilterValue, 97, 31, 0, 0, // Skip to: 1886
/* 1855 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1858 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1872
/* 1863 */    MCD_OPC_CheckPredicate, 6, 27, 8, 0, // Skip to: 3943
/* 1868 */    MCD_OPC_Decode, 221, 1, 34, // Opcode: AMOMINU_W_RL
/* 1872 */    MCD_OPC_FilterValue, 3, 18, 8, 0, // Skip to: 3943
/* 1877 */    MCD_OPC_CheckPredicate, 7, 13, 8, 0, // Skip to: 3943
/* 1882 */    MCD_OPC_Decode, 217, 1, 34, // Opcode: AMOMINU_D_RL
/* 1886 */    MCD_OPC_FilterValue, 98, 31, 0, 0, // Skip to: 1922
/* 1891 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1894 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1908
/* 1899 */    MCD_OPC_CheckPredicate, 6, 247, 7, 0, // Skip to: 3943
/* 1904 */    MCD_OPC_Decode, 219, 1, 34, // Opcode: AMOMINU_W_AQ
/* 1908 */    MCD_OPC_FilterValue, 3, 238, 7, 0, // Skip to: 3943
/* 1913 */    MCD_OPC_CheckPredicate, 7, 233, 7, 0, // Skip to: 3943
/* 1918 */    MCD_OPC_Decode, 215, 1, 34, // Opcode: AMOMINU_D_AQ
/* 1922 */    MCD_OPC_FilterValue, 99, 31, 0, 0, // Skip to: 1958
/* 1927 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1930 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1944
/* 1935 */    MCD_OPC_CheckPredicate, 6, 211, 7, 0, // Skip to: 3943
/* 1940 */    MCD_OPC_Decode, 220, 1, 34, // Opcode: AMOMINU_W_AQ_RL
/* 1944 */    MCD_OPC_FilterValue, 3, 202, 7, 0, // Skip to: 3943
/* 1949 */    MCD_OPC_CheckPredicate, 7, 197, 7, 0, // Skip to: 3943
/* 1954 */    MCD_OPC_Decode, 216, 1, 34, // Opcode: AMOMINU_D_AQ_RL
/* 1958 */    MCD_OPC_FilterValue, 112, 31, 0, 0, // Skip to: 1994
/* 1963 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 1966 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 1980
/* 1971 */    MCD_OPC_CheckPredicate, 6, 175, 7, 0, // Skip to: 3943
/* 1976 */    MCD_OPC_Decode, 202, 1, 34, // Opcode: AMOMAXU_W
/* 1980 */    MCD_OPC_FilterValue, 3, 166, 7, 0, // Skip to: 3943
/* 1985 */    MCD_OPC_CheckPredicate, 7, 161, 7, 0, // Skip to: 3943
/* 1990 */    MCD_OPC_Decode, 198, 1, 34, // Opcode: AMOMAXU_D
/* 1994 */    MCD_OPC_FilterValue, 113, 31, 0, 0, // Skip to: 2030
/* 1999 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 2002 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 2016
/* 2007 */    MCD_OPC_CheckPredicate, 6, 139, 7, 0, // Skip to: 3943
/* 2012 */    MCD_OPC_Decode, 205, 1, 34, // Opcode: AMOMAXU_W_RL
/* 2016 */    MCD_OPC_FilterValue, 3, 130, 7, 0, // Skip to: 3943
/* 2021 */    MCD_OPC_CheckPredicate, 7, 125, 7, 0, // Skip to: 3943
/* 2026 */    MCD_OPC_Decode, 201, 1, 34, // Opcode: AMOMAXU_D_RL
/* 2030 */    MCD_OPC_FilterValue, 114, 31, 0, 0, // Skip to: 2066
/* 2035 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 2038 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 2052
/* 2043 */    MCD_OPC_CheckPredicate, 6, 103, 7, 0, // Skip to: 3943
/* 2048 */    MCD_OPC_Decode, 203, 1, 34, // Opcode: AMOMAXU_W_AQ
/* 2052 */    MCD_OPC_FilterValue, 3, 94, 7, 0, // Skip to: 3943
/* 2057 */    MCD_OPC_CheckPredicate, 7, 89, 7, 0, // Skip to: 3943
/* 2062 */    MCD_OPC_Decode, 199, 1, 34, // Opcode: AMOMAXU_D_AQ
/* 2066 */    MCD_OPC_FilterValue, 115, 80, 7, 0, // Skip to: 3943
/* 2071 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 2074 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 2088
/* 2079 */    MCD_OPC_CheckPredicate, 6, 67, 7, 0, // Skip to: 3943
/* 2084 */    MCD_OPC_Decode, 204, 1, 34, // Opcode: AMOMAXU_W_AQ_RL
/* 2088 */    MCD_OPC_FilterValue, 3, 58, 7, 0, // Skip to: 3943
/* 2093 */    MCD_OPC_CheckPredicate, 7, 53, 7, 0, // Skip to: 3943
/* 2098 */    MCD_OPC_Decode, 200, 1, 34, // Opcode: AMOMAXU_D_AQ_RL
/* 2102 */    MCD_OPC_FilterValue, 51, 13, 1, 0, // Skip to: 2376
/* 2107 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 2110 */    MCD_OPC_FilterValue, 0, 35, 0, 0, // Skip to: 2150
/* 2115 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2118 */    MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 2127
/* 2123 */    MCD_OPC_Decode, 178, 1, 34, // Opcode: ADD
/* 2127 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 2141
/* 2132 */    MCD_OPC_CheckPredicate, 8, 14, 7, 0, // Skip to: 3943
/* 2137 */    MCD_OPC_Decode, 146, 3, 34, // Opcode: MUL
/* 2141 */    MCD_OPC_FilterValue, 32, 5, 7, 0, // Skip to: 3943
/* 2146 */    MCD_OPC_Decode, 186, 3, 34, // Opcode: SUB
/* 2150 */    MCD_OPC_FilterValue, 1, 26, 0, 0, // Skip to: 2181
/* 2155 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2158 */    MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 2167
/* 2163 */    MCD_OPC_Decode, 169, 3, 34, // Opcode: SLL
/* 2167 */    MCD_OPC_FilterValue, 1, 235, 6, 0, // Skip to: 3943
/* 2172 */    MCD_OPC_CheckPredicate, 8, 230, 6, 0, // Skip to: 3943
/* 2177 */    MCD_OPC_Decode, 147, 3, 34, // Opcode: MULH
/* 2181 */    MCD_OPC_FilterValue, 2, 26, 0, 0, // Skip to: 2212
/* 2186 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2189 */    MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 2198
/* 2194 */    MCD_OPC_Decode, 173, 3, 34, // Opcode: SLT
/* 2198 */    MCD_OPC_FilterValue, 1, 204, 6, 0, // Skip to: 3943
/* 2203 */    MCD_OPC_CheckPredicate, 8, 199, 6, 0, // Skip to: 3943
/* 2208 */    MCD_OPC_Decode, 148, 3, 34, // Opcode: MULHSU
/* 2212 */    MCD_OPC_FilterValue, 3, 26, 0, 0, // Skip to: 2243
/* 2217 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2220 */    MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 2229
/* 2225 */    MCD_OPC_Decode, 176, 3, 34, // Opcode: SLTU
/* 2229 */    MCD_OPC_FilterValue, 1, 173, 6, 0, // Skip to: 3943
/* 2234 */    MCD_OPC_CheckPredicate, 8, 168, 6, 0, // Skip to: 3943
/* 2239 */    MCD_OPC_Decode, 149, 3, 34, // Opcode: MULHU
/* 2243 */    MCD_OPC_FilterValue, 4, 26, 0, 0, // Skip to: 2274
/* 2248 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2251 */    MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 2260
/* 2256 */    MCD_OPC_Decode, 192, 3, 34, // Opcode: XOR
/* 2260 */    MCD_OPC_FilterValue, 1, 142, 6, 0, // Skip to: 3943
/* 2265 */    MCD_OPC_CheckPredicate, 8, 137, 6, 0, // Skip to: 3943
/* 2270 */    MCD_OPC_Decode, 184, 2, 34, // Opcode: DIV
/* 2274 */    MCD_OPC_FilterValue, 5, 35, 0, 0, // Skip to: 2314
/* 2279 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2282 */    MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 2291
/* 2287 */    MCD_OPC_Decode, 182, 3, 34, // Opcode: SRL
/* 2291 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 2305
/* 2296 */    MCD_OPC_CheckPredicate, 8, 106, 6, 0, // Skip to: 3943
/* 2301 */    MCD_OPC_Decode, 185, 2, 34, // Opcode: DIVU
/* 2305 */    MCD_OPC_FilterValue, 32, 97, 6, 0, // Skip to: 3943
/* 2310 */    MCD_OPC_Decode, 177, 3, 34, // Opcode: SRA
/* 2314 */    MCD_OPC_FilterValue, 6, 26, 0, 0, // Skip to: 2345
/* 2319 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2322 */    MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 2331
/* 2327 */    MCD_OPC_Decode, 151, 3, 34, // Opcode: OR
/* 2331 */    MCD_OPC_FilterValue, 1, 71, 6, 0, // Skip to: 3943
/* 2336 */    MCD_OPC_CheckPredicate, 8, 66, 6, 0, // Skip to: 3943
/* 2341 */    MCD_OPC_Decode, 153, 3, 34, // Opcode: REM
/* 2345 */    MCD_OPC_FilterValue, 7, 57, 6, 0, // Skip to: 3943
/* 2350 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2353 */    MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 2362
/* 2358 */    MCD_OPC_Decode, 254, 1, 34, // Opcode: AND
/* 2362 */    MCD_OPC_FilterValue, 1, 40, 6, 0, // Skip to: 3943
/* 2367 */    MCD_OPC_CheckPredicate, 8, 35, 6, 0, // Skip to: 3943
/* 2372 */    MCD_OPC_Decode, 154, 3, 34, // Opcode: REMU
/* 2376 */    MCD_OPC_FilterValue, 55, 4, 0, 0, // Skip to: 2385
/* 2381 */    MCD_OPC_Decode, 142, 3, 29, // Opcode: LUI
/* 2385 */    MCD_OPC_FilterValue, 59, 187, 0, 0, // Skip to: 2577
/* 2390 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 2393 */    MCD_OPC_FilterValue, 0, 45, 0, 0, // Skip to: 2443
/* 2398 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2401 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2415
/* 2406 */    MCD_OPC_CheckPredicate, 3, 252, 5, 0, // Skip to: 3943
/* 2411 */    MCD_OPC_Decode, 181, 1, 34, // Opcode: ADDW
/* 2415 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 2429
/* 2420 */    MCD_OPC_CheckPredicate, 9, 238, 5, 0, // Skip to: 3943
/* 2425 */    MCD_OPC_Decode, 150, 3, 34, // Opcode: MULW
/* 2429 */    MCD_OPC_FilterValue, 32, 229, 5, 0, // Skip to: 3943
/* 2434 */    MCD_OPC_CheckPredicate, 3, 224, 5, 0, // Skip to: 3943
/* 2439 */    MCD_OPC_Decode, 187, 3, 34, // Opcode: SUBW
/* 2443 */    MCD_OPC_FilterValue, 1, 16, 0, 0, // Skip to: 2464
/* 2448 */    MCD_OPC_CheckPredicate, 3, 210, 5, 0, // Skip to: 3943
/* 2453 */    MCD_OPC_CheckField, 25, 7, 0, 203, 5, 0, // Skip to: 3943
/* 2460 */    MCD_OPC_Decode, 172, 3, 34, // Opcode: SLLW
/* 2464 */    MCD_OPC_FilterValue, 4, 16, 0, 0, // Skip to: 2485
/* 2469 */    MCD_OPC_CheckPredicate, 9, 189, 5, 0, // Skip to: 3943
/* 2474 */    MCD_OPC_CheckField, 25, 7, 1, 182, 5, 0, // Skip to: 3943
/* 2481 */    MCD_OPC_Decode, 187, 2, 34, // Opcode: DIVW
/* 2485 */    MCD_OPC_FilterValue, 5, 45, 0, 0, // Skip to: 2535
/* 2490 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2493 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2507
/* 2498 */    MCD_OPC_CheckPredicate, 3, 160, 5, 0, // Skip to: 3943
/* 2503 */    MCD_OPC_Decode, 185, 3, 34, // Opcode: SRLW
/* 2507 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 2521
/* 2512 */    MCD_OPC_CheckPredicate, 9, 146, 5, 0, // Skip to: 3943
/* 2517 */    MCD_OPC_Decode, 186, 2, 34, // Opcode: DIVUW
/* 2521 */    MCD_OPC_FilterValue, 32, 137, 5, 0, // Skip to: 3943
/* 2526 */    MCD_OPC_CheckPredicate, 3, 132, 5, 0, // Skip to: 3943
/* 2531 */    MCD_OPC_Decode, 180, 3, 34, // Opcode: SRAW
/* 2535 */    MCD_OPC_FilterValue, 6, 16, 0, 0, // Skip to: 2556
/* 2540 */    MCD_OPC_CheckPredicate, 9, 118, 5, 0, // Skip to: 3943
/* 2545 */    MCD_OPC_CheckField, 25, 7, 1, 111, 5, 0, // Skip to: 3943
/* 2552 */    MCD_OPC_Decode, 156, 3, 34, // Opcode: REMW
/* 2556 */    MCD_OPC_FilterValue, 7, 102, 5, 0, // Skip to: 3943
/* 2561 */    MCD_OPC_CheckPredicate, 9, 97, 5, 0, // Skip to: 3943
/* 2566 */    MCD_OPC_CheckField, 25, 7, 1, 90, 5, 0, // Skip to: 3943
/* 2573 */    MCD_OPC_Decode, 155, 3, 34, // Opcode: REMUW
/* 2577 */    MCD_OPC_FilterValue, 67, 31, 0, 0, // Skip to: 2613
/* 2582 */    MCD_OPC_ExtractField, 25, 2,  // Inst{26-25} ...
/* 2585 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2599
/* 2590 */    MCD_OPC_CheckPredicate, 4, 68, 5, 0, // Skip to: 3943
/* 2595 */    MCD_OPC_Decode, 226, 2, 36, // Opcode: FMADD_S
/* 2599 */    MCD_OPC_FilterValue, 1, 59, 5, 0, // Skip to: 3943
/* 2604 */    MCD_OPC_CheckPredicate, 5, 54, 5, 0, // Skip to: 3943
/* 2609 */    MCD_OPC_Decode, 225, 2, 37, // Opcode: FMADD_D
/* 2613 */    MCD_OPC_FilterValue, 71, 31, 0, 0, // Skip to: 2649
/* 2618 */    MCD_OPC_ExtractField, 25, 2,  // Inst{26-25} ...
/* 2621 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2635
/* 2626 */    MCD_OPC_CheckPredicate, 4, 32, 5, 0, // Skip to: 3943
/* 2631 */    MCD_OPC_Decode, 232, 2, 36, // Opcode: FMSUB_S
/* 2635 */    MCD_OPC_FilterValue, 1, 23, 5, 0, // Skip to: 3943
/* 2640 */    MCD_OPC_CheckPredicate, 5, 18, 5, 0, // Skip to: 3943
/* 2645 */    MCD_OPC_Decode, 231, 2, 37, // Opcode: FMSUB_D
/* 2649 */    MCD_OPC_FilterValue, 75, 31, 0, 0, // Skip to: 2685
/* 2654 */    MCD_OPC_ExtractField, 25, 2,  // Inst{26-25} ...
/* 2657 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2671
/* 2662 */    MCD_OPC_CheckPredicate, 4, 252, 4, 0, // Skip to: 3943
/* 2667 */    MCD_OPC_Decode, 242, 2, 36, // Opcode: FNMSUB_S
/* 2671 */    MCD_OPC_FilterValue, 1, 243, 4, 0, // Skip to: 3943
/* 2676 */    MCD_OPC_CheckPredicate, 5, 238, 4, 0, // Skip to: 3943
/* 2681 */    MCD_OPC_Decode, 241, 2, 37, // Opcode: FNMSUB_D
/* 2685 */    MCD_OPC_FilterValue, 79, 31, 0, 0, // Skip to: 2721
/* 2690 */    MCD_OPC_ExtractField, 25, 2,  // Inst{26-25} ...
/* 2693 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2707
/* 2698 */    MCD_OPC_CheckPredicate, 4, 216, 4, 0, // Skip to: 3943
/* 2703 */    MCD_OPC_Decode, 240, 2, 36, // Opcode: FNMADD_S
/* 2707 */    MCD_OPC_FilterValue, 1, 207, 4, 0, // Skip to: 3943
/* 2712 */    MCD_OPC_CheckPredicate, 5, 202, 4, 0, // Skip to: 3943
/* 2717 */    MCD_OPC_Decode, 239, 2, 37, // Opcode: FNMADD_D
/* 2721 */    MCD_OPC_FilterValue, 83, 136, 3, 0, // Skip to: 3630
/* 2726 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 2729 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2743
/* 2734 */    MCD_OPC_CheckPredicate, 4, 180, 4, 0, // Skip to: 3943
/* 2739 */    MCD_OPC_Decode, 191, 2, 38, // Opcode: FADD_S
/* 2743 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 2757
/* 2748 */    MCD_OPC_CheckPredicate, 5, 166, 4, 0, // Skip to: 3943
/* 2753 */    MCD_OPC_Decode, 190, 2, 39, // Opcode: FADD_D
/* 2757 */    MCD_OPC_FilterValue, 4, 9, 0, 0, // Skip to: 2771
/* 2762 */    MCD_OPC_CheckPredicate, 4, 152, 4, 0, // Skip to: 3943
/* 2767 */    MCD_OPC_Decode, 253, 2, 38, // Opcode: FSUB_S
/* 2771 */    MCD_OPC_FilterValue, 5, 9, 0, 0, // Skip to: 2785
/* 2776 */    MCD_OPC_CheckPredicate, 5, 138, 4, 0, // Skip to: 3943
/* 2781 */    MCD_OPC_Decode, 252, 2, 39, // Opcode: FSUB_D
/* 2785 */    MCD_OPC_FilterValue, 8, 9, 0, 0, // Skip to: 2799
/* 2790 */    MCD_OPC_CheckPredicate, 4, 124, 4, 0, // Skip to: 3943
/* 2795 */    MCD_OPC_Decode, 234, 2, 38, // Opcode: FMUL_S
/* 2799 */    MCD_OPC_FilterValue, 9, 9, 0, 0, // Skip to: 2813
/* 2804 */    MCD_OPC_CheckPredicate, 5, 110, 4, 0, // Skip to: 3943
/* 2809 */    MCD_OPC_Decode, 233, 2, 39, // Opcode: FMUL_D
/* 2813 */    MCD_OPC_FilterValue, 12, 9, 0, 0, // Skip to: 2827
/* 2818 */    MCD_OPC_CheckPredicate, 4, 96, 4, 0, // Skip to: 3943
/* 2823 */    MCD_OPC_Decode, 213, 2, 38, // Opcode: FDIV_S
/* 2827 */    MCD_OPC_FilterValue, 13, 9, 0, 0, // Skip to: 2841
/* 2832 */    MCD_OPC_CheckPredicate, 5, 82, 4, 0, // Skip to: 3943
/* 2837 */    MCD_OPC_Decode, 212, 2, 39, // Opcode: FDIV_D
/* 2841 */    MCD_OPC_FilterValue, 16, 45, 0, 0, // Skip to: 2891
/* 2846 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 2849 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2863
/* 2854 */    MCD_OPC_CheckPredicate, 4, 60, 4, 0, // Skip to: 3943
/* 2859 */    MCD_OPC_Decode, 249, 2, 40, // Opcode: FSGNJ_S
/* 2863 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 2877
/* 2868 */    MCD_OPC_CheckPredicate, 4, 46, 4, 0, // Skip to: 3943
/* 2873 */    MCD_OPC_Decode, 245, 2, 40, // Opcode: FSGNJN_S
/* 2877 */    MCD_OPC_FilterValue, 2, 37, 4, 0, // Skip to: 3943
/* 2882 */    MCD_OPC_CheckPredicate, 4, 32, 4, 0, // Skip to: 3943
/* 2887 */    MCD_OPC_Decode, 247, 2, 40, // Opcode: FSGNJX_S
/* 2891 */    MCD_OPC_FilterValue, 17, 45, 0, 0, // Skip to: 2941
/* 2896 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 2899 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2913
/* 2904 */    MCD_OPC_CheckPredicate, 5, 10, 4, 0, // Skip to: 3943
/* 2909 */    MCD_OPC_Decode, 248, 2, 41, // Opcode: FSGNJ_D
/* 2913 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 2927
/* 2918 */    MCD_OPC_CheckPredicate, 5, 252, 3, 0, // Skip to: 3943
/* 2923 */    MCD_OPC_Decode, 244, 2, 41, // Opcode: FSGNJN_D
/* 2927 */    MCD_OPC_FilterValue, 2, 243, 3, 0, // Skip to: 3943
/* 2932 */    MCD_OPC_CheckPredicate, 5, 238, 3, 0, // Skip to: 3943
/* 2937 */    MCD_OPC_Decode, 246, 2, 41, // Opcode: FSGNJX_D
/* 2941 */    MCD_OPC_FilterValue, 20, 31, 0, 0, // Skip to: 2977
/* 2946 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 2949 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2963
/* 2954 */    MCD_OPC_CheckPredicate, 4, 216, 3, 0, // Skip to: 3943
/* 2959 */    MCD_OPC_Decode, 230, 2, 40, // Opcode: FMIN_S
/* 2963 */    MCD_OPC_FilterValue, 1, 207, 3, 0, // Skip to: 3943
/* 2968 */    MCD_OPC_CheckPredicate, 4, 202, 3, 0, // Skip to: 3943
/* 2973 */    MCD_OPC_Decode, 228, 2, 40, // Opcode: FMAX_S
/* 2977 */    MCD_OPC_FilterValue, 21, 31, 0, 0, // Skip to: 3013
/* 2982 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 2985 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 2999
/* 2990 */    MCD_OPC_CheckPredicate, 5, 180, 3, 0, // Skip to: 3943
/* 2995 */    MCD_OPC_Decode, 229, 2, 41, // Opcode: FMIN_D
/* 2999 */    MCD_OPC_FilterValue, 1, 171, 3, 0, // Skip to: 3943
/* 3004 */    MCD_OPC_CheckPredicate, 5, 166, 3, 0, // Skip to: 3943
/* 3009 */    MCD_OPC_Decode, 227, 2, 41, // Opcode: FMAX_D
/* 3013 */    MCD_OPC_FilterValue, 32, 16, 0, 0, // Skip to: 3034
/* 3018 */    MCD_OPC_CheckPredicate, 5, 152, 3, 0, // Skip to: 3943
/* 3023 */    MCD_OPC_CheckField, 20, 5, 1, 145, 3, 0, // Skip to: 3943
/* 3030 */    MCD_OPC_Decode, 203, 2, 42, // Opcode: FCVT_S_D
/* 3034 */    MCD_OPC_FilterValue, 33, 23, 0, 0, // Skip to: 3062
/* 3039 */    MCD_OPC_CheckPredicate, 5, 131, 3, 0, // Skip to: 3943
/* 3044 */    MCD_OPC_CheckField, 20, 5, 0, 124, 3, 0, // Skip to: 3943
/* 3051 */    MCD_OPC_CheckField, 12, 3, 0, 117, 3, 0, // Skip to: 3943
/* 3058 */    MCD_OPC_Decode, 196, 2, 43, // Opcode: FCVT_D_S
/* 3062 */    MCD_OPC_FilterValue, 44, 16, 0, 0, // Skip to: 3083
/* 3067 */    MCD_OPC_CheckPredicate, 4, 103, 3, 0, // Skip to: 3943
/* 3072 */    MCD_OPC_CheckField, 20, 5, 0, 96, 3, 0, // Skip to: 3943
/* 3079 */    MCD_OPC_Decode, 251, 2, 44, // Opcode: FSQRT_S
/* 3083 */    MCD_OPC_FilterValue, 45, 16, 0, 0, // Skip to: 3104
/* 3088 */    MCD_OPC_CheckPredicate, 5, 82, 3, 0, // Skip to: 3943
/* 3093 */    MCD_OPC_CheckField, 20, 5, 0, 75, 3, 0, // Skip to: 3943
/* 3100 */    MCD_OPC_Decode, 250, 2, 45, // Opcode: FSQRT_D
/* 3104 */    MCD_OPC_FilterValue, 80, 45, 0, 0, // Skip to: 3154
/* 3109 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 3112 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 3126
/* 3117 */    MCD_OPC_CheckPredicate, 4, 53, 3, 0, // Skip to: 3943
/* 3122 */    MCD_OPC_Decode, 221, 2, 46, // Opcode: FLE_S
/* 3126 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 3140
/* 3131 */    MCD_OPC_CheckPredicate, 4, 39, 3, 0, // Skip to: 3943
/* 3136 */    MCD_OPC_Decode, 223, 2, 46, // Opcode: FLT_S
/* 3140 */    MCD_OPC_FilterValue, 2, 30, 3, 0, // Skip to: 3943
/* 3145 */    MCD_OPC_CheckPredicate, 4, 25, 3, 0, // Skip to: 3943
/* 3150 */    MCD_OPC_Decode, 218, 2, 46, // Opcode: FEQ_S
/* 3154 */    MCD_OPC_FilterValue, 81, 45, 0, 0, // Skip to: 3204
/* 3159 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 3162 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 3176
/* 3167 */    MCD_OPC_CheckPredicate, 5, 3, 3, 0, // Skip to: 3943
/* 3172 */    MCD_OPC_Decode, 220, 2, 47, // Opcode: FLE_D
/* 3176 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 3190
/* 3181 */    MCD_OPC_CheckPredicate, 5, 245, 2, 0, // Skip to: 3943
/* 3186 */    MCD_OPC_Decode, 222, 2, 47, // Opcode: FLT_D
/* 3190 */    MCD_OPC_FilterValue, 2, 236, 2, 0, // Skip to: 3943
/* 3195 */    MCD_OPC_CheckPredicate, 5, 231, 2, 0, // Skip to: 3943
/* 3200 */    MCD_OPC_Decode, 217, 2, 47, // Opcode: FEQ_D
/* 3204 */    MCD_OPC_FilterValue, 96, 59, 0, 0, // Skip to: 3268
/* 3209 */    MCD_OPC_ExtractField, 20, 5,  // Inst{24-20} ...
/* 3212 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 3226
/* 3217 */    MCD_OPC_CheckPredicate, 4, 209, 2, 0, // Skip to: 3943
/* 3222 */    MCD_OPC_Decode, 211, 2, 48, // Opcode: FCVT_W_S
/* 3226 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 3240
/* 3231 */    MCD_OPC_CheckPredicate, 4, 195, 2, 0, // Skip to: 3943
/* 3236 */    MCD_OPC_Decode, 209, 2, 48, // Opcode: FCVT_WU_S
/* 3240 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 3254
/* 3245 */    MCD_OPC_CheckPredicate, 10, 181, 2, 0, // Skip to: 3943
/* 3250 */    MCD_OPC_Decode, 202, 2, 48, // Opcode: FCVT_L_S
/* 3254 */    MCD_OPC_FilterValue, 3, 172, 2, 0, // Skip to: 3943
/* 3259 */    MCD_OPC_CheckPredicate, 10, 167, 2, 0, // Skip to: 3943
/* 3264 */    MCD_OPC_Decode, 200, 2, 48, // Opcode: FCVT_LU_S
/* 3268 */    MCD_OPC_FilterValue, 97, 59, 0, 0, // Skip to: 3332
/* 3273 */    MCD_OPC_ExtractField, 20, 5,  // Inst{24-20} ...
/* 3276 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 3290
/* 3281 */    MCD_OPC_CheckPredicate, 5, 145, 2, 0, // Skip to: 3943
/* 3286 */    MCD_OPC_Decode, 210, 2, 49, // Opcode: FCVT_W_D
/* 3290 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 3304
/* 3295 */    MCD_OPC_CheckPredicate, 5, 131, 2, 0, // Skip to: 3943
/* 3300 */    MCD_OPC_Decode, 208, 2, 49, // Opcode: FCVT_WU_D
/* 3304 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 3318
/* 3309 */    MCD_OPC_CheckPredicate, 11, 117, 2, 0, // Skip to: 3943
/* 3314 */    MCD_OPC_Decode, 201, 2, 49, // Opcode: FCVT_L_D
/* 3318 */    MCD_OPC_FilterValue, 3, 108, 2, 0, // Skip to: 3943
/* 3323 */    MCD_OPC_CheckPredicate, 11, 103, 2, 0, // Skip to: 3943
/* 3328 */    MCD_OPC_Decode, 199, 2, 49, // Opcode: FCVT_LU_D
/* 3332 */    MCD_OPC_FilterValue, 104, 59, 0, 0, // Skip to: 3396
/* 3337 */    MCD_OPC_ExtractField, 20, 5,  // Inst{24-20} ...
/* 3340 */    MCD_OPC_FilterValue, 0, 9, 0, 0, // Skip to: 3354
/* 3345 */    MCD_OPC_CheckPredicate, 4, 81, 2, 0, // Skip to: 3943
/* 3350 */    MCD_OPC_Decode, 206, 2, 50, // Opcode: FCVT_S_W
/* 3354 */    MCD_OPC_FilterValue, 1, 9, 0, 0, // Skip to: 3368
/* 3359 */    MCD_OPC_CheckPredicate, 4, 67, 2, 0, // Skip to: 3943
/* 3364 */    MCD_OPC_Decode, 207, 2, 50, // Opcode: FCVT_S_WU
/* 3368 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 3382
/* 3373 */    MCD_OPC_CheckPredicate, 10, 53, 2, 0, // Skip to: 3943
/* 3378 */    MCD_OPC_Decode, 204, 2, 50, // Opcode: FCVT_S_L
/* 3382 */    MCD_OPC_FilterValue, 3, 44, 2, 0, // Skip to: 3943
/* 3387 */    MCD_OPC_CheckPredicate, 10, 39, 2, 0, // Skip to: 3943
/* 3392 */    MCD_OPC_Decode, 205, 2, 50, // Opcode: FCVT_S_LU
/* 3396 */    MCD_OPC_FilterValue, 105, 73, 0, 0, // Skip to: 3474
/* 3401 */    MCD_OPC_ExtractField, 20, 5,  // Inst{24-20} ...
/* 3404 */    MCD_OPC_FilterValue, 0, 16, 0, 0, // Skip to: 3425
/* 3409 */    MCD_OPC_CheckPredicate, 5, 17, 2, 0, // Skip to: 3943
/* 3414 */    MCD_OPC_CheckField, 12, 3, 0, 10, 2, 0, // Skip to: 3943
/* 3421 */    MCD_OPC_Decode, 197, 2, 51, // Opcode: FCVT_D_W
/* 3425 */    MCD_OPC_FilterValue, 1, 16, 0, 0, // Skip to: 3446
/* 3430 */    MCD_OPC_CheckPredicate, 5, 252, 1, 0, // Skip to: 3943
/* 3435 */    MCD_OPC_CheckField, 12, 3, 0, 245, 1, 0, // Skip to: 3943
/* 3442 */    MCD_OPC_Decode, 198, 2, 51, // Opcode: FCVT_D_WU
/* 3446 */    MCD_OPC_FilterValue, 2, 9, 0, 0, // Skip to: 3460
/* 3451 */    MCD_OPC_CheckPredicate, 11, 231, 1, 0, // Skip to: 3943
/* 3456 */    MCD_OPC_Decode, 194, 2, 52, // Opcode: FCVT_D_L
/* 3460 */    MCD_OPC_FilterValue, 3, 222, 1, 0, // Skip to: 3943
/* 3465 */    MCD_OPC_CheckPredicate, 11, 217, 1, 0, // Skip to: 3943
/* 3470 */    MCD_OPC_Decode, 195, 2, 52, // Opcode: FCVT_D_LU
/* 3474 */    MCD_OPC_FilterValue, 112, 45, 0, 0, // Skip to: 3524
/* 3479 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 3482 */    MCD_OPC_FilterValue, 0, 16, 0, 0, // Skip to: 3503
/* 3487 */    MCD_OPC_CheckPredicate, 4, 195, 1, 0, // Skip to: 3943
/* 3492 */    MCD_OPC_CheckField, 20, 5, 0, 188, 1, 0, // Skip to: 3943
/* 3499 */    MCD_OPC_Decode, 238, 2, 53, // Opcode: FMV_X_W
/* 3503 */    MCD_OPC_FilterValue, 1, 179, 1, 0, // Skip to: 3943
/* 3508 */    MCD_OPC_CheckPredicate, 4, 174, 1, 0, // Skip to: 3943
/* 3513 */    MCD_OPC_CheckField, 20, 5, 0, 167, 1, 0, // Skip to: 3943
/* 3520 */    MCD_OPC_Decode, 193, 2, 53, // Opcode: FCLASS_S
/* 3524 */    MCD_OPC_FilterValue, 113, 45, 0, 0, // Skip to: 3574
/* 3529 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 3532 */    MCD_OPC_FilterValue, 0, 16, 0, 0, // Skip to: 3553
/* 3537 */    MCD_OPC_CheckPredicate, 11, 145, 1, 0, // Skip to: 3943
/* 3542 */    MCD_OPC_CheckField, 20, 5, 0, 138, 1, 0, // Skip to: 3943
/* 3549 */    MCD_OPC_Decode, 237, 2, 54, // Opcode: FMV_X_D
/* 3553 */    MCD_OPC_FilterValue, 1, 129, 1, 0, // Skip to: 3943
/* 3558 */    MCD_OPC_CheckPredicate, 5, 124, 1, 0, // Skip to: 3943
/* 3563 */    MCD_OPC_CheckField, 20, 5, 0, 117, 1, 0, // Skip to: 3943
/* 3570 */    MCD_OPC_Decode, 192, 2, 54, // Opcode: FCLASS_D
/* 3574 */    MCD_OPC_FilterValue, 120, 23, 0, 0, // Skip to: 3602
/* 3579 */    MCD_OPC_CheckPredicate, 4, 103, 1, 0, // Skip to: 3943
/* 3584 */    MCD_OPC_CheckField, 20, 5, 0, 96, 1, 0, // Skip to: 3943
/* 3591 */    MCD_OPC_CheckField, 12, 3, 0, 89, 1, 0, // Skip to: 3943
/* 3598 */    MCD_OPC_Decode, 236, 2, 55, // Opcode: FMV_W_X
/* 3602 */    MCD_OPC_FilterValue, 121, 80, 1, 0, // Skip to: 3943
/* 3607 */    MCD_OPC_CheckPredicate, 11, 75, 1, 0, // Skip to: 3943
/* 3612 */    MCD_OPC_CheckField, 20, 5, 0, 68, 1, 0, // Skip to: 3943
/* 3619 */    MCD_OPC_CheckField, 12, 3, 0, 61, 1, 0, // Skip to: 3943
/* 3626 */    MCD_OPC_Decode, 235, 2, 51, // Opcode: FMV_D_X
/* 3630 */    MCD_OPC_FilterValue, 99, 57, 0, 0, // Skip to: 3692
/* 3635 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 3638 */    MCD_OPC_FilterValue, 0, 4, 0, 0, // Skip to: 3647
/* 3643 */    MCD_OPC_Decode, 129, 2, 56, // Opcode: BEQ
/* 3647 */    MCD_OPC_FilterValue, 1, 4, 0, 0, // Skip to: 3656
/* 3652 */    MCD_OPC_Decode, 134, 2, 56, // Opcode: BNE
/* 3656 */    MCD_OPC_FilterValue, 4, 4, 0, 0, // Skip to: 3665
/* 3661 */    MCD_OPC_Decode, 132, 2, 56, // Opcode: BLT
/* 3665 */    MCD_OPC_FilterValue, 5, 4, 0, 0, // Skip to: 3674
/* 3670 */    MCD_OPC_Decode, 130, 2, 56, // Opcode: BGE
/* 3674 */    MCD_OPC_FilterValue, 6, 4, 0, 0, // Skip to: 3683
/* 3679 */    MCD_OPC_Decode, 133, 2, 56, // Opcode: BLTU
/* 3683 */    MCD_OPC_FilterValue, 7, 255, 0, 0, // Skip to: 3943
/* 3688 */    MCD_OPC_Decode, 131, 2, 56, // Opcode: BGEU
/* 3692 */    MCD_OPC_FilterValue, 103, 11, 0, 0, // Skip to: 3708
/* 3697 */    MCD_OPC_CheckField, 12, 3, 0, 239, 0, 0, // Skip to: 3943
/* 3704 */    MCD_OPC_Decode, 128, 3, 24, // Opcode: JALR
/* 3708 */    MCD_OPC_FilterValue, 111, 4, 0, 0, // Skip to: 3717
/* 3713 */    MCD_OPC_Decode, 255, 2, 57, // Opcode: JAL
/* 3717 */    MCD_OPC_FilterValue, 115, 221, 0, 0, // Skip to: 3943
/* 3722 */    MCD_OPC_ExtractField, 12, 3,  // Inst{14-12} ...
/* 3725 */    MCD_OPC_FilterValue, 0, 139, 0, 0, // Skip to: 3869
/* 3730 */    MCD_OPC_ExtractField, 25, 7,  // Inst{31-25} ...
/* 3733 */    MCD_OPC_FilterValue, 0, 51, 0, 0, // Skip to: 3789
/* 3738 */    MCD_OPC_ExtractField, 15, 10,  // Inst{24-15} ...
/* 3741 */    MCD_OPC_FilterValue, 0, 11, 0, 0, // Skip to: 3757
/* 3746 */    MCD_OPC_CheckField, 7, 5, 0, 190, 0, 0, // Skip to: 3943
/* 3753 */    MCD_OPC_Decode, 189, 2, 0, // Opcode: ECALL
/* 3757 */    MCD_OPC_FilterValue, 32, 11, 0, 0, // Skip to: 3773
/* 3762 */    MCD_OPC_CheckField, 7, 5, 0, 174, 0, 0, // Skip to: 3943
/* 3769 */    MCD_OPC_Decode, 188, 2, 0, // Opcode: EBREAK
/* 3773 */    MCD_OPC_FilterValue, 64, 165, 0, 0, // Skip to: 3943
/* 3778 */    MCD_OPC_CheckField, 7, 5, 0, 158, 0, 0, // Skip to: 3943
/* 3785 */    MCD_OPC_Decode, 190, 3, 0, // Opcode: URET
/* 3789 */    MCD_OPC_FilterValue, 8, 36, 0, 0, // Skip to: 3830
/* 3794 */    MCD_OPC_ExtractField, 15, 10,  // Inst{24-15} ...
/* 3797 */    MCD_OPC_FilterValue, 64, 11, 0, 0, // Skip to: 3813
/* 3802 */    MCD_OPC_CheckField, 7, 5, 0, 134, 0, 0, // Skip to: 3943
/* 3809 */    MCD_OPC_Decode, 181, 3, 0, // Opcode: SRET
/* 3813 */    MCD_OPC_FilterValue, 160, 1, 124, 0, 0, // Skip to: 3943
/* 3819 */    MCD_OPC_CheckField, 7, 5, 0, 117, 0, 0, // Skip to: 3943
/* 3826 */    MCD_OPC_Decode, 191, 3, 0, // Opcode: WFI
/* 3830 */    MCD_OPC_FilterValue, 9, 11, 0, 0, // Skip to: 3846
/* 3835 */    MCD_OPC_CheckField, 7, 5, 0, 101, 0, 0, // Skip to: 3943
/* 3842 */    MCD_OPC_Decode, 167, 3, 58, // Opcode: SFENCE_VMA
/* 3846 */    MCD_OPC_FilterValue, 24, 92, 0, 0, // Skip to: 3943
/* 3851 */    MCD_OPC_CheckField, 15, 10, 64, 85, 0, 0, // Skip to: 3943
/* 3858 */    MCD_OPC_CheckField, 7, 5, 0, 78, 0, 0, // Skip to: 3943
/* 3865 */    MCD_OPC_Decode, 145, 3, 0, // Opcode: MRET
/* 3869 */    MCD_OPC_FilterValue, 1, 24, 0, 0, // Skip to: 3898
/* 3874 */    MCD_OPC_CheckField, 15, 17, 128, 128, 6, 11, 0, 0, // Skip to: 3894
/* 3883 */    MCD_OPC_CheckField, 7, 5, 0, 4, 0, 0, // Skip to: 3894
/* 3890 */    MCD_OPC_Decode, 189, 3, 0, // Opcode: UNIMP
/* 3894 */    MCD_OPC_Decode, 139, 2, 59, // Opcode: CSRRW
/* 3898 */    MCD_OPC_FilterValue, 2, 4, 0, 0, // Skip to: 3907
/* 3903 */    MCD_OPC_Decode, 137, 2, 59, // Opcode: CSRRS
/* 3907 */    MCD_OPC_FilterValue, 3, 4, 0, 0, // Skip to: 3916
/* 3912 */    MCD_OPC_Decode, 135, 2, 59, // Opcode: CSRRC
/* 3916 */    MCD_OPC_FilterValue, 5, 4, 0, 0, // Skip to: 3925
/* 3921 */    MCD_OPC_Decode, 140, 2, 60, // Opcode: CSRRWI
/* 3925 */    MCD_OPC_FilterValue, 6, 4, 0, 0, // Skip to: 3934
/* 3930 */    MCD_OPC_Decode, 138, 2, 60, // Opcode: CSRRSI
/* 3934 */    MCD_OPC_FilterValue, 7, 4, 0, 0, // Skip to: 3943
/* 3939 */    MCD_OPC_Decode, 136, 2, 60, // Opcode: CSRRCI
/* 3943 */    MCD_OPC_Fail,
  0
};

static const uint8_t DecoderTableRISCV32Only_16[] = {
/* 0 */       MCD_OPC_ExtractField, 0, 2,  // Inst{1-0} ...
/* 3 */       MCD_OPC_FilterValue, 0, 31, 0, 0, // Skip to: 39
/* 8 */       MCD_OPC_ExtractField, 13, 3,  // Inst{15-13} ...
/* 11 */      MCD_OPC_FilterValue, 3, 9, 0, 0, // Skip to: 25
/* 16 */      MCD_OPC_CheckPredicate, 12, 75, 0, 0, // Skip to: 96
/* 21 */      MCD_OPC_Decode, 154, 2, 61, // Opcode: C_FLW
/* 25 */      MCD_OPC_FilterValue, 7, 66, 0, 0, // Skip to: 96
/* 30 */      MCD_OPC_CheckPredicate, 12, 61, 0, 0, // Skip to: 96
/* 35 */      MCD_OPC_Decode, 158, 2, 61, // Opcode: C_FSW
/* 39 */      MCD_OPC_FilterValue, 1, 16, 0, 0, // Skip to: 60
/* 44 */      MCD_OPC_CheckPredicate, 13, 47, 0, 0, // Skip to: 96
/* 49 */      MCD_OPC_CheckField, 13, 3, 1, 40, 0, 0, // Skip to: 96
/* 56 */      MCD_OPC_Decode, 161, 2, 19, // Opcode: C_JAL
/* 60 */      MCD_OPC_FilterValue, 2, 31, 0, 0, // Skip to: 96
/* 65 */      MCD_OPC_ExtractField, 13, 3,  // Inst{15-13} ...
/* 68 */      MCD_OPC_FilterValue, 3, 9, 0, 0, // Skip to: 82
/* 73 */      MCD_OPC_CheckPredicate, 12, 18, 0, 0, // Skip to: 96
/* 78 */      MCD_OPC_Decode, 155, 2, 62, // Opcode: C_FLWSP
/* 82 */      MCD_OPC_FilterValue, 7, 9, 0, 0, // Skip to: 96
/* 87 */      MCD_OPC_CheckPredicate, 12, 4, 0, 0, // Skip to: 96
/* 92 */      MCD_OPC_Decode, 159, 2, 63, // Opcode: C_FSWSP
/* 96 */      MCD_OPC_Fail,
  0
};

static bool checkDecoderPredicate(unsigned Idx, uint64_t Bits)
{
  switch (Idx) {
  default: CS_ASSERT(0 && "Invalid index!");
  case 0:
    return (Bits & RISCV_FeatureStdExtC);
  case 1:
    return (Bits & RISCV_FeatureStdExtC) && (Bits & RISCV_FeatureStdExtD);
  case 2:
    return (Bits & RISCV_FeatureStdExtC) && (Bits & RISCV_Feature64Bit);
  case 3:
    return (Bits & RISCV_Feature64Bit);
  case 4:
    return (Bits & RISCV_FeatureStdExtF);
  case 5:
    return (Bits & RISCV_FeatureStdExtD);
  case 6:
    return (Bits & RISCV_FeatureStdExtA);
  case 7:
    return (Bits & RISCV_FeatureStdExtA) && (Bits & RISCV_Feature64Bit);
  case 8:
    return (Bits & RISCV_FeatureStdExtM);
  case 9:
    return (Bits & RISCV_FeatureStdExtM) && (Bits & RISCV_Feature64Bit);
  case 10:
    return (Bits & RISCV_FeatureStdExtF) && (Bits & RISCV_Feature64Bit);
  case 11:
    return (Bits & RISCV_FeatureStdExtD) && (Bits & RISCV_Feature64Bit);
  case 12:
    return (Bits & RISCV_FeatureStdExtC) && (Bits & RISCV_FeatureStdExtF) && !(Bits & RISCV_Feature64Bit);
  case 13:
    return (Bits & RISCV_FeatureStdExtC) && !(Bits & RISCV_Feature64Bit);
  }
}

#define DecodeToMCInst(fname, fieldname, InsnType) \
static DecodeStatus fname(DecodeStatus S, unsigned Idx, InsnType insn, MCInst *MI, \
                          uint64_t Address, const void *Decoder,\
                          bool *DecodeComplete) {\
  *DecodeComplete = true;\
  InsnType tmp; \
  switch (Idx) { \
  default: CS_ASSERT(0 && "Invalid index!");\
  case 0: \
    return S; \
  case 1: \
    tmp = fieldname(insn, 2, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 5, 1) << 3; \
    tmp |= fieldname(insn, 6, 1) << 2; \
    tmp |= fieldname(insn, 7, 4) << 6; \
    tmp |= fieldname(insn, 11, 2) << 4; \
    if (decodeUImmNonZeroOperand(MI, tmp, Address, Decoder, 10) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 2: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 5) << 0; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeSImmOperand(MI, tmp, Address, Decoder, 6) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 3: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 5) << 0; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 6) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 4: \
    tmp = fieldname(insn, 2, 3); \
    if (DecodeFPR64CRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 5, 2) << 6; \
    tmp |= fieldname(insn, 10, 3) << 3; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 8) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 5: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 3) << 6; \
    tmp |= fieldname(insn, 5, 2) << 3; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 9) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 6: \
    tmp = fieldname(insn, 2, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 5, 1) << 6; \
    tmp |= fieldname(insn, 6, 1) << 2; \
    tmp |= fieldname(insn, 10, 3) << 3; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 7) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 7: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 5) << 0; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeSImmOperand(MI, tmp, Address, Decoder, 6) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 8: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 2) << 6; \
    tmp |= fieldname(insn, 4, 3) << 2; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 8) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 9: \
    tmp = fieldname(insn, 2, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 5, 2) << 6; \
    tmp |= fieldname(insn, 10, 3) << 3; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 8) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 10: \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 1) << 5; \
    tmp |= fieldname(insn, 3, 2) << 7; \
    tmp |= fieldname(insn, 5, 1) << 6; \
    tmp |= fieldname(insn, 6, 1) << 4; \
    tmp |= fieldname(insn, 12, 1) << 9; \
    if (decodeSImmNonZeroOperand(MI, tmp, Address, Decoder, 10) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 11: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0X2RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 5) << 0; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeCLUIImmOperand(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 12: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 3) << 6; \
    tmp |= fieldname(insn, 5, 2) << 3; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 9) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 13: \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 5) << 0; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 6) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 14: \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 5) << 0; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeSImmOperand(MI, tmp, Address, Decoder, 6) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 15: \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 2, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 16: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 17: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 2, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 18: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 2, 5); \
    if (DecodeGPRNoX0RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 19: \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 1) << 4; \
    tmp |= fieldname(insn, 3, 3) << 0; \
    tmp |= fieldname(insn, 6, 1) << 6; \
    tmp |= fieldname(insn, 7, 1) << 5; \
    tmp |= fieldname(insn, 8, 1) << 9; \
    tmp |= fieldname(insn, 9, 2) << 7; \
    tmp |= fieldname(insn, 11, 1) << 3; \
    tmp |= fieldname(insn, 12, 1) << 10; \
    if (decodeSImmOperandAndLsl1(MI, tmp, Address, Decoder, 12) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 20: \
    tmp = fieldname(insn, 2, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 7, 3) << 6; \
    tmp |= fieldname(insn, 10, 3) << 3; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 9) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 21: \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 1) << 4; \
    tmp |= fieldname(insn, 3, 2) << 0; \
    tmp |= fieldname(insn, 5, 2) << 5; \
    tmp |= fieldname(insn, 10, 2) << 2; \
    tmp |= fieldname(insn, 12, 1) << 7; \
    if (decodeSImmOperandAndLsl1(MI, tmp, Address, Decoder, 9) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 22: \
    tmp = fieldname(insn, 2, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 7, 2) << 6; \
    tmp |= fieldname(insn, 9, 4) << 2; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 8) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 23: \
    tmp = fieldname(insn, 2, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 7, 3) << 6; \
    tmp |= fieldname(insn, 10, 3) << 3; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 9) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 24: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 12); \
    if (decodeSImmOperand(MI, tmp, Address, Decoder, 12) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 25: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 12); \
    if (decodeSImmOperand(MI, tmp, Address, Decoder, 12) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 26: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 12); \
    if (decodeSImmOperand(MI, tmp, Address, Decoder, 12) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 27: \
    tmp = fieldname(insn, 24, 4); \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 4) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 4); \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 4) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 28: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 6); \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 6) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 29: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 20); \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 20) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 30: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 5) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 31: \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 7, 5) << 0; \
    tmp |= fieldname(insn, 25, 7) << 5; \
    if (decodeSImmOperand(MI, tmp, Address, Decoder, 12) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 32: \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 7, 5) << 0; \
    tmp |= fieldname(insn, 25, 7) << 5; \
    if (decodeSImmOperand(MI, tmp, Address, Decoder, 12) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 33: \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 7, 5) << 0; \
    tmp |= fieldname(insn, 25, 7) << 5; \
    if (decodeSImmOperand(MI, tmp, Address, Decoder, 12) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 34: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 35: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 36: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 27, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 37: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 27, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 38: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 39: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 40: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 41: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 42: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 43: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 44: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 45: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 46: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 47: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 48: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 49: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 50: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 51: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 52: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 12, 3); \
    if (decodeFRMArg(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 53: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 54: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeFPR64RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 55: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 56: \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 7, 1) << 10; \
    tmp |= fieldname(insn, 8, 4) << 0; \
    tmp |= fieldname(insn, 25, 6) << 4; \
    tmp |= fieldname(insn, 31, 1) << 11; \
    if (decodeSImmOperandAndLsl1(MI, tmp, Address, Decoder, 13) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 57: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 12, 8) << 11; \
    tmp |= fieldname(insn, 20, 1) << 10; \
    tmp |= fieldname(insn, 21, 10) << 0; \
    tmp |= fieldname(insn, 31, 1) << 19; \
    if (decodeSImmOperandAndLsl1(MI, tmp, Address, Decoder, 21) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 58: \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 59: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 12); \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 12) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 60: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeGPRRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 20, 12); \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 12) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 15, 5); \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 5) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 61: \
    tmp = fieldname(insn, 2, 3); \
    if (DecodeFPR32CRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = fieldname(insn, 7, 3); \
    if (DecodeGPRCRegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 5, 1) << 6; \
    tmp |= fieldname(insn, 6, 1) << 2; \
    tmp |= fieldname(insn, 10, 3) << 3; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 7) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 62: \
    tmp = fieldname(insn, 7, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 2, 2) << 6; \
    tmp |= fieldname(insn, 4, 3) << 2; \
    tmp |= fieldname(insn, 12, 1) << 5; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 8) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  case 63: \
    tmp = fieldname(insn, 2, 5); \
    if (DecodeFPR32RegisterClass(MI, tmp, Address, Decoder) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    tmp = 0; \
    tmp |= fieldname(insn, 7, 2) << 6; \
    tmp |= fieldname(insn, 9, 4) << 2; \
    if (decodeUImmOperand(MI, tmp, Address, Decoder, 8) == MCDisassembler_Fail) return MCDisassembler_Fail; \
    return S; \
  } \
}

#define DecodeInstruction(fname, fieldname, decoder, InsnType) \
static DecodeStatus fname(const uint8_t DecodeTable[], MCInst *MI,\
                          InsnType insn, uint64_t Address,\
                          const void *DisAsm, int feature) {\
  uint64_t Bits = getFeatureBits(feature);\
\
  const uint8_t *Ptr = DecodeTable;\
  uint32_t CurFieldValue = 0;\
  DecodeStatus S = MCDisassembler_Success;\
  while (true) {\
    switch (*Ptr) {\
    default:\
      return MCDisassembler_Fail;\
    case MCD_OPC_ExtractField: {\
      unsigned Start = *++Ptr;\
      unsigned Len = *++Ptr;\
      ++Ptr;\
      CurFieldValue = fieldname(insn, Start, Len);\
      break;\
    }\
    case MCD_OPC_FilterValue: {\
      unsigned Len;\
      InsnType Val = decodeULEB128(++Ptr, &Len);\
      Ptr += Len;\
      unsigned NumToSkip = *Ptr++;\
      NumToSkip |= (*Ptr++) << 8;\
      NumToSkip |= (*Ptr++) << 16;\
\
      if (Val != CurFieldValue)\
        Ptr += NumToSkip;\
      break;\
    }\
    case MCD_OPC_CheckField: {\
      unsigned Start = *++Ptr;\
      unsigned Len = *++Ptr;\
      InsnType FieldValue = fieldname(insn, Start, Len);\
      uint32_t ExpectedValue = decodeULEB128(++Ptr, &Len);\
      Ptr += Len;\
      unsigned NumToSkip = *Ptr++;\
      NumToSkip |= (*Ptr++) << 8;\
      NumToSkip |= (*Ptr++) << 16;\
\
      if (ExpectedValue != FieldValue)\
        Ptr += NumToSkip;\
      break;\
    }\
    case MCD_OPC_CheckPredicate: {\
      unsigned Len;\
      unsigned PIdx = decodeULEB128(++Ptr, &Len);\
      Ptr += Len;\
      unsigned NumToSkip = *Ptr++;\
      NumToSkip |= (*Ptr++) << 8;\
      NumToSkip |= (*Ptr++) << 16;\
      bool Pred;\
      if (!(Pred = checkDecoderPredicate(PIdx, Bits)))\
        Ptr += NumToSkip;\
      (void)Pred;\
      break;\
    }\
    case MCD_OPC_Decode: {\
      unsigned Len;\
      unsigned Opc = decodeULEB128(++Ptr, &Len);\
      Ptr += Len;\
      unsigned DecodeIdx = decodeULEB128(Ptr, &Len);\
      Ptr += Len;\
\
      MCInst_clear(MI);\
      MCInst_setOpcode(MI, Opc);\
      bool DecodeComplete;\
      S = decoder(S, DecodeIdx, insn, MI, Address, DisAsm, &DecodeComplete);\
      CS_ASSERT(DecodeComplete);\
\
      return S;\
    }\
    case MCD_OPC_TryDecode: {\
      unsigned Len;\
      unsigned Opc = decodeULEB128(++Ptr, &Len);\
      Ptr += Len;\
      unsigned DecodeIdx = decodeULEB128(Ptr, &Len);\
      Ptr += Len;\
      unsigned NumToSkip = *Ptr++;\
      NumToSkip |= (*Ptr++) << 8;\
      NumToSkip |= (*Ptr++) << 16;\
\
      MCInst TmpMI;\
      MCInst_setOpcode(&TmpMI, Opc);\
      bool DecodeComplete;\
      S = decoder(S, DecodeIdx, insn, &TmpMI, Address, DisAsm, &DecodeComplete);\
\
      if (DecodeComplete) {\
        *MI = TmpMI;\
        return S;\
      } else {\
        CS_ASSERT(S == MCDisassembler_Fail);\
        Ptr += NumToSkip;\
        S = MCDisassembler_Success;\
      }\
      break;\
    }\
    case MCD_OPC_SoftFail: {\
      unsigned Len;\
      InsnType PositiveMask = decodeULEB128(++Ptr, &Len);\
      Ptr += Len;\
      InsnType NegativeMask = decodeULEB128(Ptr, &Len);\
      Ptr += Len;\
      bool Fail = (insn & PositiveMask) || (~insn & NegativeMask);\
      if (Fail)\
        S = MCDisassembler_SoftFail;\
      break;\
    }\
    case MCD_OPC_Fail: {\
      return MCDisassembler_Fail;\
    }\
    }\
  }\
  CS_ASSERT(0 && "bogosity detected in disassembler state machine!");\
}

// For RISCV instruction is 32 bits.
FieldFromInstruction(fieldFromInstruction, uint32_t)
DecodeToMCInst(decodeToMCInst, fieldFromInstruction, uint32_t)
DecodeInstruction(decodeInstruction, fieldFromInstruction, decodeToMCInst, uint32_t)
