/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|* Target Register Enum Values                                                *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */


#ifdef GET_REGINFO_ENUM
#undef GET_REGINFO_ENUM

enum {
  SystemZ_NoRegister,
  SystemZ_CC = 1,
  SystemZ_A0 = 2,
  SystemZ_A1 = 3,
  SystemZ_A2 = 4,
  SystemZ_A3 = 5,
  SystemZ_A4 = 6,
  SystemZ_A5 = 7,
  SystemZ_A6 = 8,
  SystemZ_A7 = 9,
  SystemZ_A8 = 10,
  SystemZ_A9 = 11,
  SystemZ_A10 = 12,
  SystemZ_A11 = 13,
  SystemZ_A12 = 14,
  SystemZ_A13 = 15,
  SystemZ_A14 = 16,
  SystemZ_A15 = 17,
  SystemZ_C0 = 18,
  SystemZ_C1 = 19,
  SystemZ_C2 = 20,
  SystemZ_C3 = 21,
  SystemZ_C4 = 22,
  SystemZ_C5 = 23,
  SystemZ_C6 = 24,
  SystemZ_C7 = 25,
  SystemZ_C8 = 26,
  SystemZ_C9 = 27,
  SystemZ_C10 = 28,
  SystemZ_C11 = 29,
  SystemZ_C12 = 30,
  SystemZ_C13 = 31,
  SystemZ_C14 = 32,
  SystemZ_C15 = 33,
  SystemZ_V0 = 34,
  SystemZ_V1 = 35,
  SystemZ_V2 = 36,
  SystemZ_V3 = 37,
  SystemZ_V4 = 38,
  SystemZ_V5 = 39,
  SystemZ_V6 = 40,
  SystemZ_V7 = 41,
  SystemZ_V8 = 42,
  SystemZ_V9 = 43,
  SystemZ_V10 = 44,
  SystemZ_V11 = 45,
  SystemZ_V12 = 46,
  SystemZ_V13 = 47,
  SystemZ_V14 = 48,
  SystemZ_V15 = 49,
  SystemZ_V16 = 50,
  SystemZ_V17 = 51,
  SystemZ_V18 = 52,
  SystemZ_V19 = 53,
  SystemZ_V20 = 54,
  SystemZ_V21 = 55,
  SystemZ_V22 = 56,
  SystemZ_V23 = 57,
  SystemZ_V24 = 58,
  SystemZ_V25 = 59,
  SystemZ_V26 = 60,
  SystemZ_V27 = 61,
  SystemZ_V28 = 62,
  SystemZ_V29 = 63,
  SystemZ_V30 = 64,
  SystemZ_V31 = 65,
  SystemZ_F0D = 66,
  SystemZ_F1D = 67,
  SystemZ_F2D = 68,
  SystemZ_F3D = 69,
  SystemZ_F4D = 70,
  SystemZ_F5D = 71,
  SystemZ_F6D = 72,
  SystemZ_F7D = 73,
  SystemZ_F8D = 74,
  SystemZ_F9D = 75,
  SystemZ_F10D = 76,
  SystemZ_F11D = 77,
  SystemZ_F12D = 78,
  SystemZ_F13D = 79,
  SystemZ_F14D = 80,
  SystemZ_F15D = 81,
  SystemZ_F16D = 82,
  SystemZ_F17D = 83,
  SystemZ_F18D = 84,
  SystemZ_F19D = 85,
  SystemZ_F20D = 86,
  SystemZ_F21D = 87,
  SystemZ_F22D = 88,
  SystemZ_F23D = 89,
  SystemZ_F24D = 90,
  SystemZ_F25D = 91,
  SystemZ_F26D = 92,
  SystemZ_F27D = 93,
  SystemZ_F28D = 94,
  SystemZ_F29D = 95,
  SystemZ_F30D = 96,
  SystemZ_F31D = 97,
  SystemZ_F0Q = 98,
  SystemZ_F1Q = 99,
  SystemZ_F4Q = 100,
  SystemZ_F5Q = 101,
  SystemZ_F8Q = 102,
  SystemZ_F9Q = 103,
  SystemZ_F12Q = 104,
  SystemZ_F13Q = 105,
  SystemZ_F0S = 106,
  SystemZ_F1S = 107,
  SystemZ_F2S = 108,
  SystemZ_F3S = 109,
  SystemZ_F4S = 110,
  SystemZ_F5S = 111,
  SystemZ_F6S = 112,
  SystemZ_F7S = 113,
  SystemZ_F8S = 114,
  SystemZ_F9S = 115,
  SystemZ_F10S = 116,
  SystemZ_F11S = 117,
  SystemZ_F12S = 118,
  SystemZ_F13S = 119,
  SystemZ_F14S = 120,
  SystemZ_F15S = 121,
  SystemZ_F16S = 122,
  SystemZ_F17S = 123,
  SystemZ_F18S = 124,
  SystemZ_F19S = 125,
  SystemZ_F20S = 126,
  SystemZ_F21S = 127,
  SystemZ_F22S = 128,
  SystemZ_F23S = 129,
  SystemZ_F24S = 130,
  SystemZ_F25S = 131,
  SystemZ_F26S = 132,
  SystemZ_F27S = 133,
  SystemZ_F28S = 134,
  SystemZ_F29S = 135,
  SystemZ_F30S = 136,
  SystemZ_F31S = 137,
  SystemZ_R0D = 138,
  SystemZ_R1D = 139,
  SystemZ_R2D = 140,
  SystemZ_R3D = 141,
  SystemZ_R4D = 142,
  SystemZ_R5D = 143,
  SystemZ_R6D = 144,
  SystemZ_R7D = 145,
  SystemZ_R8D = 146,
  SystemZ_R9D = 147,
  SystemZ_R10D = 148,
  SystemZ_R11D = 149,
  SystemZ_R12D = 150,
  SystemZ_R13D = 151,
  SystemZ_R14D = 152,
  SystemZ_R15D = 153,
  SystemZ_R0H = 154,
  SystemZ_R1H = 155,
  SystemZ_R2H = 156,
  SystemZ_R3H = 157,
  SystemZ_R4H = 158,
  SystemZ_R5H = 159,
  SystemZ_R6H = 160,
  SystemZ_R7H = 161,
  SystemZ_R8H = 162,
  SystemZ_R9H = 163,
  SystemZ_R10H = 164,
  SystemZ_R11H = 165,
  SystemZ_R12H = 166,
  SystemZ_R13H = 167,
  SystemZ_R14H = 168,
  SystemZ_R15H = 169,
  SystemZ_R0L = 170,
  SystemZ_R1L = 171,
  SystemZ_R2L = 172,
  SystemZ_R3L = 173,
  SystemZ_R4L = 174,
  SystemZ_R5L = 175,
  SystemZ_R6L = 176,
  SystemZ_R7L = 177,
  SystemZ_R8L = 178,
  SystemZ_R9L = 179,
  SystemZ_R10L = 180,
  SystemZ_R11L = 181,
  SystemZ_R12L = 182,
  SystemZ_R13L = 183,
  SystemZ_R14L = 184,
  SystemZ_R15L = 185,
  SystemZ_R0Q = 186,
  SystemZ_R2Q = 187,
  SystemZ_R4Q = 188,
  SystemZ_R6Q = 189,
  SystemZ_R8Q = 190,
  SystemZ_R10Q = 191,
  SystemZ_R12Q = 192,
  SystemZ_R14Q = 193,
  SystemZ_NUM_TARGET_REGS 	// 194
};

// Register classes
enum {
  SystemZ_GRX32BitRegClassID = 0,
  SystemZ_VR32BitRegClassID = 1,
  SystemZ_AR32BitRegClassID = 2,
  SystemZ_FP32BitRegClassID = 3,
  SystemZ_GR32BitRegClassID = 4,
  SystemZ_GRH32BitRegClassID = 5,
  SystemZ_ADDR32BitRegClassID = 6,
  SystemZ_CCRRegClassID = 7,
  SystemZ_AnyRegBitRegClassID = 8,
  SystemZ_AnyRegBit_with_subreg_r32RegClassID = 9,
  SystemZ_VR64BitRegClassID = 10,
  SystemZ_AnyRegBit_with_subreg_r64RegClassID = 11,
  SystemZ_CR64BitRegClassID = 12,
  SystemZ_FP64BitRegClassID = 13,
  SystemZ_GR64BitRegClassID = 14,
  SystemZ_ADDR64BitRegClassID = 15,
  SystemZ_VR128BitRegClassID = 16,
  SystemZ_VF128BitRegClassID = 17,
  SystemZ_FP128BitRegClassID = 18,
  SystemZ_GR128BitRegClassID = 19,
  SystemZ_ADDR128BitRegClassID = 20,
};
#endif // GET_REGINFO_ENUM

/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|* MC Register Information                                                    *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/


#ifdef GET_REGINFO_MC_DESC
#undef GET_REGINFO_MC_DESC

static const MCPhysReg SystemZRegDiffLists[] = {
  /* 0 */ 64857, 1, 1, 1, 0,
  /* 5 */ 65325, 1, 0,
  /* 8 */ 65471, 2, 0,
  /* 11 */ 65473, 2, 0,
  /* 14 */ 65475, 2, 0,
  /* 17 */ 65477, 2, 0,
  /* 20 */ 32, 40, 0,
  /* 23 */ 65506, 40, 65494, 40, 0,
  /* 28 */ 65508, 40, 65494, 40, 0,
  /* 33 */ 65510, 40, 65494, 40, 0,
  /* 38 */ 65512, 40, 65494, 40, 0,
  /* 43 */ 65504, 40, 0,
  /* 46 */ 65520, 40, 0,
  /* 49 */ 65504, 41, 0,
  /* 52 */ 65520, 41, 0,
  /* 55 */ 65504, 42, 0,
  /* 58 */ 65520, 42, 0,
  /* 61 */ 65504, 43, 0,
  /* 64 */ 65520, 43, 0,
  /* 67 */ 65504, 44, 0,
  /* 70 */ 65520, 44, 0,
  /* 73 */ 65504, 45, 0,
  /* 76 */ 65520, 45, 0,
  /* 79 */ 65504, 46, 0,
  /* 82 */ 65520, 46, 0,
  /* 85 */ 65504, 47, 0,
  /* 88 */ 65520, 47, 0,
  /* 91 */ 65504, 48, 0,
  /* 94 */ 65520, 48, 0,
  /* 97 */ 65496, 65504, 56, 0,
  /* 101 */ 65496, 65504, 58, 0,
  /* 105 */ 65496, 65504, 60, 0,
  /* 109 */ 65496, 65504, 62, 0,
  /* 113 */ 65496, 65504, 64, 0,
  /* 117 */ 65261, 0,
  /* 119 */ 65294, 0,
  /* 121 */ 65463, 0,
  /* 123 */ 65503, 0,
  /* 125 */ 65496, 65504, 0,
  /* 128 */ 65489, 32, 65520, 65519, 32, 65520, 0,
  /* 135 */ 65490, 32, 65520, 65519, 32, 65520, 0,
  /* 142 */ 65491, 32, 65520, 65519, 32, 65520, 0,
  /* 149 */ 65492, 32, 65520, 65519, 32, 65520, 0,
  /* 156 */ 65493, 32, 65520, 65519, 32, 65520, 0,
  /* 163 */ 65494, 32, 65520, 65519, 32, 65520, 0,
  /* 170 */ 65495, 32, 65520, 65519, 32, 65520, 0,
  /* 177 */ 65496, 32, 65520, 65519, 32, 65520, 0,
  /* 184 */ 65535, 0,
};

static const uint16_t SystemZSubRegIdxLists[] = {
  /* 0 */ 6, 1, 0,
  /* 3 */ 7, 6, 1, 2, 4, 3, 0,
  /* 10 */ 7, 8, 2, 5, 0,
  /* 15 */ 9, 8, 0,
};

static const MCRegisterDesc SystemZRegDesc[] = { // Descriptors
  { 3, 0, 0, 0, 0, 0 },
  { 226, 4, 4, 2, 2945, 0 },
  { 20, 4, 4, 2, 2945, 0 },
  { 49, 4, 4, 2, 2945, 0 },
  { 74, 4, 4, 2, 2945, 0 },
  { 99, 4, 4, 2, 2945, 0 },
  { 124, 4, 4, 2, 2945, 0 },
  { 149, 4, 4, 2, 2945, 0 },
  { 166, 4, 4, 2, 2945, 0 },
  { 183, 4, 4, 2, 2945, 0 },
  { 200, 4, 4, 2, 2945, 0 },
  { 217, 4, 4, 2, 2945, 0 },
  { 0, 4, 4, 2, 2945, 0 },
  { 29, 4, 4, 2, 2945, 0 },
  { 58, 4, 4, 2, 2945, 0 },
  { 83, 4, 4, 2, 2945, 0 },
  { 108, 4, 4, 2, 2945, 0 },
  { 133, 4, 4, 2, 2945, 0 },
  { 23, 4, 4, 2, 2945, 0 },
  { 52, 4, 4, 2, 2945, 0 },
  { 77, 4, 4, 2, 2945, 0 },
  { 102, 4, 4, 2, 2945, 0 },
  { 127, 4, 4, 2, 2945, 0 },
  { 152, 4, 4, 2, 2945, 0 },
  { 169, 4, 4, 2, 2945, 0 },
  { 186, 4, 4, 2, 2945, 0 },
  { 203, 4, 4, 2, 2945, 0 },
  { 220, 4, 4, 2, 2945, 0 },
  { 4, 4, 4, 2, 2945, 0 },
  { 33, 4, 4, 2, 2945, 0 },
  { 62, 4, 4, 2, 2945, 0 },
  { 87, 4, 4, 2, 2945, 0 },
  { 112, 4, 4, 2, 2945, 0 },
  { 137, 4, 4, 2, 2945, 0 },
  { 26, 20, 4, 15, 2945, 8 },
  { 55, 20, 4, 15, 2945, 8 },
  { 80, 20, 4, 15, 2945, 8 },
  { 105, 20, 4, 15, 2945, 8 },
  { 130, 20, 4, 15, 2945, 8 },
  { 155, 20, 4, 15, 2945, 8 },
  { 172, 20, 4, 15, 2945, 8 },
  { 189, 20, 4, 15, 2945, 8 },
  { 206, 20, 4, 15, 2945, 8 },
  { 223, 20, 4, 15, 2945, 8 },
  { 8, 20, 4, 15, 2945, 8 },
  { 37, 20, 4, 15, 2945, 8 },
  { 66, 20, 4, 15, 2945, 8 },
  { 91, 20, 4, 15, 2945, 8 },
  { 116, 20, 4, 15, 2945, 8 },
  { 141, 20, 4, 15, 2945, 8 },
  { 158, 20, 4, 15, 2945, 8 },
  { 175, 20, 4, 15, 2945, 8 },
  { 192, 20, 4, 15, 2945, 8 },
  { 209, 20, 4, 15, 2945, 8 },
  { 12, 20, 4, 15, 2945, 8 },
  { 41, 20, 4, 15, 2945, 8 },
  { 70, 20, 4, 15, 2945, 8 },
  { 95, 20, 4, 15, 2945, 8 },
  { 120, 20, 4, 15, 2945, 8 },
  { 145, 20, 4, 15, 2945, 8 },
  { 162, 20, 4, 15, 2945, 8 },
  { 179, 20, 4, 15, 2945, 8 },
  { 196, 20, 4, 15, 2945, 8 },
  { 213, 20, 4, 15, 2945, 8 },
  { 16, 20, 4, 15, 2945, 8 },
  { 45, 20, 4, 15, 2945, 8 },
  { 249, 21, 114, 16, 1969, 8 },
  { 277, 21, 114, 16, 1969, 8 },
  { 300, 21, 110, 16, 1969, 8 },
  { 323, 21, 110, 16, 1969, 8 },
  { 346, 21, 110, 16, 1969, 8 },
  { 369, 21, 110, 16, 1969, 8 },
  { 387, 21, 106, 16, 1969, 8 },
  { 405, 21, 106, 16, 1969, 8 },
  { 423, 21, 106, 16, 1969, 8 },
  { 441, 21, 106, 16, 1969, 8 },
  { 229, 21, 102, 16, 1969, 8 },
  { 257, 21, 102, 16, 1969, 8 },
  { 285, 21, 102, 16, 1969, 8 },
  { 308, 21, 102, 16, 1969, 8 },
  { 331, 21, 98, 16, 1969, 8 },
  { 354, 21, 98, 16, 1969, 8 },
  { 377, 21, 126, 16, 1969, 8 },
  { 395, 21, 126, 16, 1969, 8 },
  { 413, 21, 126, 16, 1969, 8 },
  { 431, 21, 126, 16, 1969, 8 },
  { 239, 21, 126, 16, 1969, 8 },
  { 267, 21, 126, 16, 1969, 8 },
  { 295, 21, 126, 16, 1969, 8 },
  { 318, 21, 126, 16, 1969, 8 },
  { 341, 21, 126, 16, 1969, 8 },
  { 364, 21, 126, 16, 1969, 8 },
  { 382, 21, 126, 16, 1969, 8 },
  { 400, 21, 126, 16, 1969, 8 },
  { 418, 21, 126, 16, 1969, 8 },
  { 436, 21, 126, 16, 1969, 8 },
  { 244, 21, 126, 16, 1969, 8 },
  { 272, 21, 126, 16, 1969, 8 },
  { 594, 23, 4, 10, 129, 7 },
  { 602, 23, 4, 10, 129, 7 },
  { 630, 28, 4, 10, 177, 7 },
  { 638, 28, 4, 10, 177, 7 },
  { 646, 33, 4, 10, 225, 7 },
  { 654, 33, 4, 10, 225, 7 },
  { 606, 38, 4, 10, 273, 7 },
  { 620, 38, 4, 10, 273, 7 },
  { 673, 4, 113, 2, 1937, 0 },
  { 692, 4, 113, 2, 1937, 0 },
  { 706, 4, 109, 2, 1937, 0 },
  { 720, 4, 109, 2, 1937, 0 },
  { 734, 4, 109, 2, 1937, 0 },
  { 748, 4, 109, 2, 1937, 0 },
  { 762, 4, 105, 2, 1937, 0 },
  { 776, 4, 105, 2, 1937, 0 },
  { 790, 4, 105, 2, 1937, 0 },
  { 804, 4, 105, 2, 1937, 0 },
  { 658, 4, 101, 2, 1937, 0 },
  { 677, 4, 101, 2, 1937, 0 },
  { 696, 4, 101, 2, 1937, 0 },
  { 710, 4, 101, 2, 1937, 0 },
  { 724, 4, 97, 2, 1937, 0 },
  { 738, 4, 97, 2, 1937, 0 },
  { 752, 4, 125, 2, 1937, 0 },
  { 766, 4, 125, 2, 1937, 0 },
  { 780, 4, 125, 2, 1937, 0 },
  { 794, 4, 125, 2, 1937, 0 },
  { 663, 4, 125, 2, 1937, 0 },
  { 682, 4, 125, 2, 1937, 0 },
  { 701, 4, 125, 2, 1937, 0 },
  { 715, 4, 125, 2, 1937, 0 },
  { 729, 4, 125, 2, 1937, 0 },
  { 743, 4, 125, 2, 1937, 0 },
  { 757, 4, 125, 2, 1937, 0 },
  { 771, 4, 125, 2, 1937, 0 },
  { 785, 4, 125, 2, 1937, 0 },
  { 799, 4, 125, 2, 1937, 0 },
  { 668, 4, 125, 2, 1937, 0 },
  { 687, 4, 125, 2, 1937, 0 },
  { 253, 132, 92, 0, 82, 4 },
  { 281, 132, 86, 0, 82, 4 },
  { 304, 132, 86, 0, 82, 4 },
  { 327, 132, 80, 0, 82, 4 },
  { 350, 132, 80, 0, 82, 4 },
  { 373, 132, 74, 0, 82, 4 },
  { 391, 132, 74, 0, 82, 4 },
  { 409, 132, 68, 0, 82, 4 },
  { 427, 132, 68, 0, 82, 4 },
  { 445, 132, 62, 0, 82, 4 },
  { 234, 132, 62, 0, 82, 4 },
  { 262, 132, 56, 0, 82, 4 },
  { 290, 132, 56, 0, 82, 4 },
  { 313, 132, 50, 0, 82, 4 },
  { 336, 132, 50, 0, 82, 4 },
  { 359, 132, 21, 0, 82, 4 },
  { 454, 4, 94, 2, 1906, 0 },
  { 463, 4, 88, 2, 1906, 0 },
  { 472, 4, 88, 2, 1906, 0 },
  { 481, 4, 82, 2, 1906, 0 },
  { 490, 4, 82, 2, 1906, 0 },
  { 499, 4, 76, 2, 1906, 0 },
  { 503, 4, 76, 2, 1906, 0 },
  { 507, 4, 70, 2, 1906, 0 },
  { 511, 4, 70, 2, 1906, 0 },
  { 515, 4, 64, 2, 1906, 0 },
  { 449, 4, 64, 2, 1906, 0 },
  { 458, 4, 58, 2, 1906, 0 },
  { 467, 4, 58, 2, 1906, 0 },
  { 476, 4, 52, 2, 1906, 0 },
  { 485, 4, 52, 2, 1906, 0 },
  { 494, 4, 46, 2, 1906, 0 },
  { 524, 4, 91, 2, 1874, 0 },
  { 533, 4, 85, 2, 1874, 0 },
  { 542, 4, 85, 2, 1874, 0 },
  { 551, 4, 79, 2, 1874, 0 },
  { 560, 4, 79, 2, 1874, 0 },
  { 569, 4, 73, 2, 1874, 0 },
  { 573, 4, 73, 2, 1874, 0 },
  { 577, 4, 67, 2, 1874, 0 },
  { 581, 4, 67, 2, 1874, 0 },
  { 585, 4, 61, 2, 1874, 0 },
  { 519, 4, 61, 2, 1874, 0 },
  { 528, 4, 55, 2, 1874, 0 },
  { 537, 4, 55, 2, 1874, 0 },
  { 546, 4, 49, 2, 1874, 0 },
  { 555, 4, 49, 2, 1874, 0 },
  { 564, 4, 43, 2, 1874, 0 },
  { 598, 128, 4, 3, 4, 2 },
  { 616, 135, 4, 3, 4, 2 },
  { 634, 142, 4, 3, 4, 2 },
  { 642, 149, 4, 3, 4, 2 },
  { 650, 156, 4, 3, 4, 2 },
  { 589, 163, 4, 3, 4, 2 },
  { 611, 170, 4, 3, 4, 2 },
  { 625, 177, 4, 3, 4, 2 },
};

  // GRX32Bit Register Class...
  static const MCPhysReg GRX32Bit[] = {
    SystemZ_R0L, SystemZ_R1L, SystemZ_R2L, SystemZ_R3L, SystemZ_R4L, SystemZ_R5L, SystemZ_R0H, SystemZ_R1H, SystemZ_R2H, SystemZ_R3H, SystemZ_R4H, SystemZ_R5H, SystemZ_R15L, SystemZ_R15H, SystemZ_R14L, SystemZ_R14H, SystemZ_R13L, SystemZ_R13H, SystemZ_R12L, SystemZ_R12H, SystemZ_R11L, SystemZ_R11H, SystemZ_R10L, SystemZ_R10H, SystemZ_R9L, SystemZ_R9H, SystemZ_R8L, SystemZ_R8H, SystemZ_R7L, SystemZ_R7H, SystemZ_R6L, SystemZ_R6H,
  };

  // GRX32Bit Bit set.
  static const uint8_t GRX32BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0x03,
  };

  // VR32Bit Register Class...
  static const MCPhysReg VR32Bit[] = {
    SystemZ_F0S, SystemZ_F1S, SystemZ_F2S, SystemZ_F3S, SystemZ_F4S, SystemZ_F5S, SystemZ_F6S, SystemZ_F7S, SystemZ_F16S, SystemZ_F17S, SystemZ_F18S, SystemZ_F19S, SystemZ_F20S, SystemZ_F21S, SystemZ_F22S, SystemZ_F23S, SystemZ_F24S, SystemZ_F25S, SystemZ_F26S, SystemZ_F27S, SystemZ_F28S, SystemZ_F29S, SystemZ_F30S, SystemZ_F31S, SystemZ_F8S, SystemZ_F9S, SystemZ_F10S, SystemZ_F11S, SystemZ_F12S, SystemZ_F13S, SystemZ_F14S, SystemZ_F15S,
  };

  // VR32Bit Bit set.
  static const uint8_t VR32BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0x03,
  };

  // AR32Bit Register Class...
  static const MCPhysReg AR32Bit[] = {
    SystemZ_A0, SystemZ_A1, SystemZ_A2, SystemZ_A3, SystemZ_A4, SystemZ_A5, SystemZ_A6, SystemZ_A7, SystemZ_A8, SystemZ_A9, SystemZ_A10, SystemZ_A11, SystemZ_A12, SystemZ_A13, SystemZ_A14, SystemZ_A15,
  };

  // AR32Bit Bit set.
  static const uint8_t AR32BitBits[] = {
    0xfc, 0xff, 0x03,
  };

  // FP32Bit Register Class...
  static const MCPhysReg FP32Bit[] = {
    SystemZ_F0S, SystemZ_F1S, SystemZ_F2S, SystemZ_F3S, SystemZ_F4S, SystemZ_F5S, SystemZ_F6S, SystemZ_F7S, SystemZ_F8S, SystemZ_F9S, SystemZ_F10S, SystemZ_F11S, SystemZ_F12S, SystemZ_F13S, SystemZ_F14S, SystemZ_F15S,
  };

  // FP32Bit Bit set.
  static const uint8_t FP32BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };

  // GR32Bit Register Class...
  static const MCPhysReg GR32Bit[] = {
    SystemZ_R0L, SystemZ_R1L, SystemZ_R2L, SystemZ_R3L, SystemZ_R4L, SystemZ_R5L, SystemZ_R15L, SystemZ_R14L, SystemZ_R13L, SystemZ_R12L, SystemZ_R11L, SystemZ_R10L, SystemZ_R9L, SystemZ_R8L, SystemZ_R7L, SystemZ_R6L,
  };

  // GR32Bit Bit set.
  static const uint8_t GR32BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };

  // GRH32Bit Register Class...
  static const MCPhysReg GRH32Bit[] = {
    SystemZ_R0H, SystemZ_R1H, SystemZ_R2H, SystemZ_R3H, SystemZ_R4H, SystemZ_R5H, SystemZ_R15H, SystemZ_R14H, SystemZ_R13H, SystemZ_R12H, SystemZ_R11H, SystemZ_R10H, SystemZ_R9H, SystemZ_R8H, SystemZ_R7H, SystemZ_R6H,
  };

  // GRH32Bit Bit set.
  static const uint8_t GRH32BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };

  // ADDR32Bit Register Class...
  static const MCPhysReg ADDR32Bit[] = {
    SystemZ_R1L, SystemZ_R2L, SystemZ_R3L, SystemZ_R4L, SystemZ_R5L, SystemZ_R15L, SystemZ_R14L, SystemZ_R13L, SystemZ_R12L, SystemZ_R11L, SystemZ_R10L, SystemZ_R9L, SystemZ_R8L, SystemZ_R7L, SystemZ_R6L,
  };

  // ADDR32Bit Bit set.
  static const uint8_t ADDR32BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x03,
  };

  // CCR Register Class...
  static const MCPhysReg CCR[] = {
    SystemZ_CC,
  };

  // CCR Bit set.
  static const uint8_t CCRBits[] = {
    0x02,
  };

  // AnyRegBit Register Class...
  static const MCPhysReg AnyRegBit[] = {
    SystemZ_R0D, SystemZ_R1D, SystemZ_R2D, SystemZ_R3D, SystemZ_R4D, SystemZ_R5D, SystemZ_R6D, SystemZ_R7D, SystemZ_R8D, SystemZ_R9D, SystemZ_R10D, SystemZ_R11D, SystemZ_R12D, SystemZ_R13D, SystemZ_R14D, SystemZ_R15D, SystemZ_F0D, SystemZ_F1D, SystemZ_F2D, SystemZ_F3D, SystemZ_F4D, SystemZ_F5D, SystemZ_F6D, SystemZ_F7D, SystemZ_F8D, SystemZ_F9D, SystemZ_F10D, SystemZ_F11D, SystemZ_F12D, SystemZ_F13D, SystemZ_F14D, SystemZ_F15D, SystemZ_V0, SystemZ_V1, SystemZ_V2, SystemZ_V3, SystemZ_V4, SystemZ_V5, SystemZ_V6, SystemZ_V7, SystemZ_V8, SystemZ_V9, SystemZ_V10, SystemZ_V11, SystemZ_V12, SystemZ_V13, SystemZ_V14, SystemZ_V15,
  };

  // AnyRegBit Bit set.
  static const uint8_t AnyRegBitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03, 0x00, 0xfc, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };

  // AnyRegBit_with_subreg_r32 Register Class...
  static const MCPhysReg AnyRegBit_with_subreg_r32[] = {
    SystemZ_F0D, SystemZ_F1D, SystemZ_F2D, SystemZ_F3D, SystemZ_F4D, SystemZ_F5D, SystemZ_F6D, SystemZ_F7D, SystemZ_F8D, SystemZ_F9D, SystemZ_F10D, SystemZ_F11D, SystemZ_F12D, SystemZ_F13D, SystemZ_F14D, SystemZ_F15D, SystemZ_V0, SystemZ_V1, SystemZ_V2, SystemZ_V3, SystemZ_V4, SystemZ_V5, SystemZ_V6, SystemZ_V7, SystemZ_V8, SystemZ_V9, SystemZ_V10, SystemZ_V11, SystemZ_V12, SystemZ_V13, SystemZ_V14, SystemZ_V15,
  };

  // AnyRegBit_with_subreg_r32 Bit set.
  static const uint8_t AnyRegBit_with_subreg_r32Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03, 0x00, 0xfc, 0xff, 0x03,
  };

  // VR64Bit Register Class...
  static const MCPhysReg VR64Bit[] = {
    SystemZ_F0D, SystemZ_F1D, SystemZ_F2D, SystemZ_F3D, SystemZ_F4D, SystemZ_F5D, SystemZ_F6D, SystemZ_F7D, SystemZ_F16D, SystemZ_F17D, SystemZ_F18D, SystemZ_F19D, SystemZ_F20D, SystemZ_F21D, SystemZ_F22D, SystemZ_F23D, SystemZ_F24D, SystemZ_F25D, SystemZ_F26D, SystemZ_F27D, SystemZ_F28D, SystemZ_F29D, SystemZ_F30D, SystemZ_F31D, SystemZ_F8D, SystemZ_F9D, SystemZ_F10D, SystemZ_F11D, SystemZ_F12D, SystemZ_F13D, SystemZ_F14D, SystemZ_F15D,
  };

  // VR64Bit Bit set.
  static const uint8_t VR64BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0x03,
  };

  // AnyRegBit_with_subreg_r64 Register Class...
  static const MCPhysReg AnyRegBit_with_subreg_r64[] = {
    SystemZ_V0, SystemZ_V1, SystemZ_V2, SystemZ_V3, SystemZ_V4, SystemZ_V5, SystemZ_V6, SystemZ_V7, SystemZ_V8, SystemZ_V9, SystemZ_V10, SystemZ_V11, SystemZ_V12, SystemZ_V13, SystemZ_V14, SystemZ_V15,
  };

  // AnyRegBit_with_subreg_r64 Bit set.
  static const uint8_t AnyRegBit_with_subreg_r64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };

  // CR64Bit Register Class...
  static const MCPhysReg CR64Bit[] = {
    SystemZ_C0, SystemZ_C1, SystemZ_C2, SystemZ_C3, SystemZ_C4, SystemZ_C5, SystemZ_C6, SystemZ_C7, SystemZ_C8, SystemZ_C9, SystemZ_C10, SystemZ_C11, SystemZ_C12, SystemZ_C13, SystemZ_C14, SystemZ_C15,
  };

  // CR64Bit Bit set.
  static const uint8_t CR64BitBits[] = {
    0x00, 0x00, 0xfc, 0xff, 0x03,
  };

  // FP64Bit Register Class...
  static const MCPhysReg FP64Bit[] = {
    SystemZ_F0D, SystemZ_F1D, SystemZ_F2D, SystemZ_F3D, SystemZ_F4D, SystemZ_F5D, SystemZ_F6D, SystemZ_F7D, SystemZ_F8D, SystemZ_F9D, SystemZ_F10D, SystemZ_F11D, SystemZ_F12D, SystemZ_F13D, SystemZ_F14D, SystemZ_F15D,
  };

  // FP64Bit Bit set.
  static const uint8_t FP64BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };

  // GR64Bit Register Class...
  static const MCPhysReg GR64Bit[] = {
    SystemZ_R0D, SystemZ_R1D, SystemZ_R2D, SystemZ_R3D, SystemZ_R4D, SystemZ_R5D, SystemZ_R15D, SystemZ_R14D, SystemZ_R13D, SystemZ_R12D, SystemZ_R11D, SystemZ_R10D, SystemZ_R9D, SystemZ_R8D, SystemZ_R7D, SystemZ_R6D,
  };

  // GR64Bit Bit set.
  static const uint8_t GR64BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };

  // ADDR64Bit Register Class...
  static const MCPhysReg ADDR64Bit[] = {
    SystemZ_R1D, SystemZ_R2D, SystemZ_R3D, SystemZ_R4D, SystemZ_R5D, SystemZ_R15D, SystemZ_R14D, SystemZ_R13D, SystemZ_R12D, SystemZ_R11D, SystemZ_R10D, SystemZ_R9D, SystemZ_R8D, SystemZ_R7D, SystemZ_R6D,
  };

  // ADDR64Bit Bit set.
  static const uint8_t ADDR64BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x03,
  };

  // VR128Bit Register Class...
  static const MCPhysReg VR128Bit[] = {
    SystemZ_V0, SystemZ_V1, SystemZ_V2, SystemZ_V3, SystemZ_V4, SystemZ_V5, SystemZ_V6, SystemZ_V7, SystemZ_V16, SystemZ_V17, SystemZ_V18, SystemZ_V19, SystemZ_V20, SystemZ_V21, SystemZ_V22, SystemZ_V23, SystemZ_V24, SystemZ_V25, SystemZ_V26, SystemZ_V27, SystemZ_V28, SystemZ_V29, SystemZ_V30, SystemZ_V31, SystemZ_V8, SystemZ_V9, SystemZ_V10, SystemZ_V11, SystemZ_V12, SystemZ_V13, SystemZ_V14, SystemZ_V15,
  };

  // VR128Bit Bit set.
  static const uint8_t VR128BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0x03,
  };

  // VF128Bit Register Class...
  static const MCPhysReg VF128Bit[] = {
    SystemZ_V0, SystemZ_V1, SystemZ_V2, SystemZ_V3, SystemZ_V4, SystemZ_V5, SystemZ_V6, SystemZ_V7, SystemZ_V8, SystemZ_V9, SystemZ_V10, SystemZ_V11, SystemZ_V12, SystemZ_V13, SystemZ_V14, SystemZ_V15,
  };

  // VF128Bit Bit set.
  static const uint8_t VF128BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };

  // FP128Bit Register Class...
  static const MCPhysReg FP128Bit[] = {
    SystemZ_F0Q, SystemZ_F1Q, SystemZ_F4Q, SystemZ_F5Q, SystemZ_F8Q, SystemZ_F9Q, SystemZ_F12Q, SystemZ_F13Q,
  };

  // FP128Bit Bit set.
  static const uint8_t FP128BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x03,
  };

  // GR128Bit Register Class...
  static const MCPhysReg GR128Bit[] = {
    SystemZ_R0Q, SystemZ_R2Q, SystemZ_R4Q, SystemZ_R12Q, SystemZ_R10Q, SystemZ_R8Q, SystemZ_R6Q, SystemZ_R14Q,
  };

  // GR128Bit Bit set.
  static const uint8_t GR128BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x03,
  };

  // ADDR128Bit Register Class...
  static const MCPhysReg ADDR128Bit[] = {
    SystemZ_R2Q, SystemZ_R4Q, SystemZ_R12Q, SystemZ_R10Q, SystemZ_R8Q, SystemZ_R6Q, SystemZ_R14Q,
  };

  // ADDR128Bit Bit set.
  static const uint8_t ADDR128BitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x03,
  };

static const MCRegisterClass SystemZMCRegisterClasses[] = {
  { GRX32Bit, GRX32BitBits, sizeof(GRX32BitBits) },
  { VR32Bit, VR32BitBits, sizeof(VR32BitBits) },
  { AR32Bit, AR32BitBits, sizeof(AR32BitBits) },
  { FP32Bit, FP32BitBits, sizeof(FP32BitBits) },
  { GR32Bit, GR32BitBits, sizeof(GR32BitBits) },
  { GRH32Bit, GRH32BitBits, sizeof(GRH32BitBits) },
  { ADDR32Bit, ADDR32BitBits, sizeof(ADDR32BitBits) },
  { CCR, CCRBits, sizeof(CCRBits) },
  { AnyRegBit, AnyRegBitBits, sizeof(AnyRegBitBits) },
  { AnyRegBit_with_subreg_r32, AnyRegBit_with_subreg_r32Bits, sizeof(AnyRegBit_with_subreg_r32Bits) },
  { VR64Bit, VR64BitBits, sizeof(VR64BitBits) },
  { AnyRegBit_with_subreg_r64, AnyRegBit_with_subreg_r64Bits, sizeof(AnyRegBit_with_subreg_r64Bits) },
  { CR64Bit, CR64BitBits, sizeof(CR64BitBits) },
  { FP64Bit, FP64BitBits, sizeof(FP64BitBits) },
  { GR64Bit, GR64BitBits, sizeof(GR64BitBits) },
  { ADDR64Bit, ADDR64BitBits, sizeof(ADDR64BitBits) },
  { VR128Bit, VR128BitBits, sizeof(VR128BitBits) },
  { VF128Bit, VF128BitBits, sizeof(VF128BitBits) },
  { FP128Bit, FP128BitBits, sizeof(FP128BitBits) },
  { GR128Bit, GR128BitBits, sizeof(GR128BitBits) },
  { ADDR128Bit, ADDR128BitBits, sizeof(ADDR128BitBits) },
};

#endif // GET_REGINFO_MC_DESC
