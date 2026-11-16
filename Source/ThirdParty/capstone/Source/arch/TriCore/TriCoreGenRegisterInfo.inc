/* Capstone Disassembly Engine, http://www.capstone-engine.org */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2022, */
/*    Rot127 <unisono@quyllur.org> 2022-2023 */
/* Automatically generated file by Capstone's LLVM TableGen Disassembler Backend. */

/* LLVM-commit: <commit> */
/* LLVM-tag: <tag> */

/* Do not edit. */

/* Capstone's LLVM TableGen Backends: */
/* https://github.com/capstone-engine/llvm-capstone */

#ifdef GET_REGINFO_ENUM
#undef GET_REGINFO_ENUM

enum {
  TRICORE_NoRegister,
  TRICORE_FCX = 1,
  TRICORE_PC = 2,
  TRICORE_PCXI = 3,
  TRICORE_PSW = 4,
  TRICORE_A0 = 5,
  TRICORE_A1 = 6,
  TRICORE_A2 = 7,
  TRICORE_A3 = 8,
  TRICORE_A4 = 9,
  TRICORE_A5 = 10,
  TRICORE_A6 = 11,
  TRICORE_A7 = 12,
  TRICORE_A8 = 13,
  TRICORE_A9 = 14,
  TRICORE_A10 = 15,
  TRICORE_A11 = 16,
  TRICORE_A12 = 17,
  TRICORE_A13 = 18,
  TRICORE_A14 = 19,
  TRICORE_A15 = 20,
  TRICORE_D0 = 21,
  TRICORE_D1 = 22,
  TRICORE_D2 = 23,
  TRICORE_D3 = 24,
  TRICORE_D4 = 25,
  TRICORE_D5 = 26,
  TRICORE_D6 = 27,
  TRICORE_D7 = 28,
  TRICORE_D8 = 29,
  TRICORE_D9 = 30,
  TRICORE_D10 = 31,
  TRICORE_D11 = 32,
  TRICORE_D12 = 33,
  TRICORE_D13 = 34,
  TRICORE_D14 = 35,
  TRICORE_D15 = 36,
  TRICORE_E0 = 37,
  TRICORE_E2 = 38,
  TRICORE_E4 = 39,
  TRICORE_E6 = 40,
  TRICORE_E8 = 41,
  TRICORE_E10 = 42,
  TRICORE_E12 = 43,
  TRICORE_E14 = 44,
  TRICORE_P0 = 45,
  TRICORE_P2 = 46,
  TRICORE_P4 = 47,
  TRICORE_P6 = 48,
  TRICORE_P8 = 49,
  TRICORE_P10 = 50,
  TRICORE_P12 = 51,
  TRICORE_P14 = 52,
  TRICORE_A0_A1 = 53,
  TRICORE_A2_A3 = 54,
  TRICORE_A4_A5 = 55,
  TRICORE_A6_A7 = 56,
  TRICORE_A8_A9 = 57,
  TRICORE_A10_A11 = 58,
  TRICORE_A12_A13 = 59,
  TRICORE_A14_A15 = 60,
  NUM_TARGET_REGS // 61
};

// Register classes

enum {
  TriCore_RARegClassID = 0,
  TriCore_RDRegClassID = 1,
  TriCore_PSRegsRegClassID = 2,
  TriCore_PairAddrRegsRegClassID = 3,
  TriCore_RERegClassID = 4,
  TriCore_RPRegClassID = 5,

};

// Subregister indices

enum {
  TriCore_NoSubRegister,
  TriCore_subreg_even,	// 1
  TriCore_subreg_odd,	// 2
  TriCore_NUM_TARGET_SUBREGS
};
#endif // GET_REGINFO_ENUM

#ifdef GET_REGINFO_MC_DESC
#undef GET_REGINFO_MC_DESC

static const MCPhysReg TriCoreRegDiffLists[] = {
  /* 0 */ 65434, 1, 0,
  /* 3 */ 65450, 1, 0,
  /* 6 */ 65482, 1, 0,
  /* 9 */ 65488, 1, 0,
  /* 12 */ 65489, 1, 0,
  /* 15 */ 65490, 1, 0,
  /* 18 */ 65491, 1, 0,
  /* 21 */ 65492, 1, 0,
  /* 24 */ 65493, 1, 0,
  /* 27 */ 65494, 1, 0,
  /* 30 */ 65495, 1, 0,
  /* 33 */ 65496, 1, 0,
  /* 36 */ 65497, 1, 0,
  /* 39 */ 65498, 1, 0,
  /* 42 */ 65499, 1, 0,
  /* 45 */ 65500, 1, 0,
  /* 48 */ 65501, 1, 0,
  /* 51 */ 65502, 1, 0,
  /* 54 */ 65503, 1, 0,
  /* 57 */ 65520, 1, 0,
  /* 60 */ 65521, 1, 0,
  /* 63 */ 65522, 1, 0,
  /* 66 */ 65523, 1, 0,
  /* 69 */ 65524, 1, 0,
  /* 72 */ 65525, 1, 0,
  /* 75 */ 65526, 1, 0,
  /* 78 */ 65527, 1, 0,
  /* 81 */ 32, 8, 0,
  /* 84 */ 33, 8, 0,
  /* 87 */ 34, 8, 0,
  /* 90 */ 35, 8, 0,
  /* 93 */ 36, 8, 0,
  /* 96 */ 37, 8, 0,
  /* 99 */ 38, 8, 0,
  /* 102 */ 39, 8, 0,
  /* 105 */ 40, 8, 0,
  /* 108 */ 9, 0,
  /* 110 */ 10, 0,
  /* 112 */ 11, 0,
  /* 114 */ 12, 0,
  /* 116 */ 13, 0,
  /* 118 */ 14, 0,
  /* 120 */ 15, 0,
  /* 122 */ 16, 0,
  /* 124 */ 65535, 0,
};

static const uint16_t TriCoreSubRegIdxLists[] = {
  /* 0 */ 1, 2, 0,
};

static const MCRegisterDesc TriCoreRegDesc[] = { // Descriptors
  { 3, 0, 0, 0, 0, 0 },
  { 201, 2, 2, 2, 1985, 0 },
  { 189, 2, 2, 2, 1985, 0 },
  { 192, 2, 2, 2, 1985, 0 },
  { 197, 2, 2, 2, 1985, 0 },
  { 16, 2, 105, 2, 1985, 0 },
  { 43, 2, 102, 2, 1985, 0 },
  { 65, 2, 102, 2, 1985, 0 },
  { 92, 2, 99, 2, 1985, 0 },
  { 114, 2, 99, 2, 1985, 0 },
  { 141, 2, 96, 2, 1985, 0 },
  { 147, 2, 96, 2, 1985, 0 },
  { 162, 2, 93, 2, 1985, 0 },
  { 168, 2, 93, 2, 1985, 0 },
  { 183, 2, 90, 2, 1985, 0 },
  { 0, 2, 90, 2, 1985, 0 },
  { 32, 2, 87, 2, 1985, 0 },
  { 49, 2, 87, 2, 1985, 0 },
  { 81, 2, 84, 2, 1985, 0 },
  { 98, 2, 84, 2, 1985, 0 },
  { 130, 2, 81, 2, 1985, 0 },
  { 19, 2, 122, 2, 1985, 0 },
  { 46, 2, 120, 2, 1985, 0 },
  { 68, 2, 120, 2, 1985, 0 },
  { 95, 2, 118, 2, 1985, 0 },
  { 117, 2, 118, 2, 1985, 0 },
  { 144, 2, 116, 2, 1985, 0 },
  { 150, 2, 116, 2, 1985, 0 },
  { 165, 2, 114, 2, 1985, 0 },
  { 171, 2, 114, 2, 1985, 0 },
  { 186, 2, 112, 2, 1985, 0 },
  { 4, 2, 112, 2, 1985, 0 },
  { 36, 2, 110, 2, 1985, 0 },
  { 53, 2, 110, 2, 1985, 0 },
  { 85, 2, 108, 2, 1985, 0 },
  { 102, 2, 108, 2, 1985, 0 },
  { 134, 2, 82, 2, 1985, 0 },
  { 22, 57, 2, 0, 98, 2 },
  { 71, 60, 2, 0, 98, 2 },
  { 120, 63, 2, 0, 98, 2 },
  { 153, 66, 2, 0, 98, 2 },
  { 174, 69, 2, 0, 98, 2 },
  { 8, 72, 2, 0, 98, 2 },
  { 57, 75, 2, 0, 98, 2 },
  { 106, 78, 2, 0, 98, 2 },
  { 25, 33, 2, 0, 50, 2 },
  { 74, 36, 2, 0, 50, 2 },
  { 123, 39, 2, 0, 50, 2 },
  { 156, 42, 2, 0, 50, 2 },
  { 177, 45, 2, 0, 50, 2 },
  { 12, 48, 2, 0, 50, 2 },
  { 61, 51, 2, 0, 50, 2 },
  { 110, 54, 2, 0, 50, 2 },
  { 40, 9, 2, 0, 2, 2 },
  { 89, 12, 2, 0, 2, 2 },
  { 138, 15, 2, 0, 2, 2 },
  { 159, 18, 2, 0, 2, 2 },
  { 180, 21, 2, 0, 2, 2 },
  { 28, 24, 2, 0, 2, 2 },
  { 77, 27, 2, 0, 2, 2 },
  { 126, 30, 2, 0, 2, 2 },
};

  // RA Register Class...
  static const MCPhysReg RA[] = {
    TRICORE_A0, TRICORE_A1, TRICORE_A2, TRICORE_A3, TRICORE_A4, TRICORE_A5, TRICORE_A6, TRICORE_A7, TRICORE_A8, TRICORE_A9, TRICORE_A10, TRICORE_A11, TRICORE_A12, TRICORE_A13, TRICORE_A14, TRICORE_A15, 
  };

  // RA Bit set.
  static const uint8_t RABits[] = {
    0xe0, 0xff, 0x1f, 
  };

  // RD Register Class...
  static const MCPhysReg RD[] = {
    TRICORE_D0, TRICORE_D1, TRICORE_D2, TRICORE_D3, TRICORE_D4, TRICORE_D5, TRICORE_D6, TRICORE_D7, TRICORE_D8, TRICORE_D9, TRICORE_D10, TRICORE_D11, TRICORE_D12, TRICORE_D13, TRICORE_D14, TRICORE_D15, 
  };

  // RD Bit set.
  static const uint8_t RDBits[] = {
    0x00, 0x00, 0xe0, 0xff, 0x1f, 
  };

  // PSRegs Register Class...
  static const MCPhysReg PSRegs[] = {
    TRICORE_PSW, TRICORE_PCXI, TRICORE_PC, TRICORE_FCX, 
  };

  // PSRegs Bit set.
  static const uint8_t PSRegsBits[] = {
    0x1e, 
  };

  // PairAddrRegs Register Class...
  static const MCPhysReg PairAddrRegs[] = {
    TRICORE_A0_A1, TRICORE_A2_A3, TRICORE_A4_A5, TRICORE_A6_A7, TRICORE_A8_A9, TRICORE_A10_A11, TRICORE_A12_A13, TRICORE_A14_A15, 
  };

  // PairAddrRegs Bit set.
  static const uint8_t PairAddrRegsBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f, 
  };

  // RE Register Class...
  static const MCPhysReg RE[] = {
    TRICORE_E0, TRICORE_E2, TRICORE_E4, TRICORE_E6, TRICORE_E8, TRICORE_E10, TRICORE_E12, TRICORE_E14, 
  };

  // RE Bit set.
  static const uint8_t REBits[] = {
    0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f, 
  };

  // RP Register Class...
  static const MCPhysReg RP[] = {
    TRICORE_P0, TRICORE_P2, TRICORE_P4, TRICORE_P6, TRICORE_P8, TRICORE_P10, TRICORE_P12, TRICORE_P14, 
  };

  // RP Bit set.
  static const uint8_t RPBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f, 
  };

static const MCRegisterClass TriCoreMCRegisterClasses[] = {
  { RA, RABits, sizeof(RABits) },
  { RD, RDBits, sizeof(RDBits) },
  { PSRegs, PSRegsBits, sizeof(PSRegsBits) },
  { PairAddrRegs, PairAddrRegsBits, sizeof(PairAddrRegsBits) },
  { RE, REBits, sizeof(REBits) },
  { RP, RPBits, sizeof(RPBits) },
};

#endif // GET_REGINFO_MC_DESC



