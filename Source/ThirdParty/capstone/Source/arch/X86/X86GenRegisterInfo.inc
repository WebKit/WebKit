
/* Capstone Disassembly Engine, http://www.capstone-engine.org */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

/*===- TableGen'erated file -------------------------------------*- C++ -*-===*|*                                                                            *|
|* Target Register Enum Values                                                 *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#ifdef GET_REGINFO_ENUM
#undef GET_REGINFO_ENUM

enum {
  X86_NoRegister,
  X86_AH = 1,
  X86_AL = 2,
  X86_AX = 3,
  X86_BH = 4,
  X86_BL = 5,
  X86_BP = 6,
  X86_BPH = 7,
  X86_BPL = 8,
  X86_BX = 9,
  X86_CH = 10,
  X86_CL = 11,
  X86_CS = 12,
  X86_CX = 13,
  X86_DF = 14,
  X86_DH = 15,
  X86_DI = 16,
  X86_DIH = 17,
  X86_DIL = 18,
  X86_DL = 19,
  X86_DS = 20,
  X86_DX = 21,
  X86_EAX = 22,
  X86_EBP = 23,
  X86_EBX = 24,
  X86_ECX = 25,
  X86_EDI = 26,
  X86_EDX = 27,
  X86_EFLAGS = 28,
  X86_EIP = 29,
  X86_EIZ = 30,
  X86_ES = 31,
  X86_ESI = 32,
  X86_ESP = 33,
  X86_FPSW = 34,
  X86_FS = 35,
  X86_GS = 36,
  X86_HAX = 37,
  X86_HBP = 38,
  X86_HBX = 39,
  X86_HCX = 40,
  X86_HDI = 41,
  X86_HDX = 42,
  X86_HIP = 43,
  X86_HSI = 44,
  X86_HSP = 45,
  X86_IP = 46,
  X86_RAX = 47,
  X86_RBP = 48,
  X86_RBX = 49,
  X86_RCX = 50,
  X86_RDI = 51,
  X86_RDX = 52,
  X86_RIP = 53,
  X86_RIZ = 54,
  X86_RSI = 55,
  X86_RSP = 56,
  X86_SI = 57,
  X86_SIH = 58,
  X86_SIL = 59,
  X86_SP = 60,
  X86_SPH = 61,
  X86_SPL = 62,
  X86_SS = 63,
  X86_SSP = 64,
  X86_BND0 = 65,
  X86_BND1 = 66,
  X86_BND2 = 67,
  X86_BND3 = 68,
  X86_CR0 = 69,
  X86_CR1 = 70,
  X86_CR2 = 71,
  X86_CR3 = 72,
  X86_CR4 = 73,
  X86_CR5 = 74,
  X86_CR6 = 75,
  X86_CR7 = 76,
  X86_CR8 = 77,
  X86_CR9 = 78,
  X86_CR10 = 79,
  X86_CR11 = 80,
  X86_CR12 = 81,
  X86_CR13 = 82,
  X86_CR14 = 83,
  X86_CR15 = 84,
  X86_DR0 = 85,
  X86_DR1 = 86,
  X86_DR2 = 87,
  X86_DR3 = 88,
  X86_DR4 = 89,
  X86_DR5 = 90,
  X86_DR6 = 91,
  X86_DR7 = 92,
  X86_DR8 = 93,
  X86_DR9 = 94,
  X86_DR10 = 95,
  X86_DR11 = 96,
  X86_DR12 = 97,
  X86_DR13 = 98,
  X86_DR14 = 99,
  X86_DR15 = 100,
  X86_FP0 = 101,
  X86_FP1 = 102,
  X86_FP2 = 103,
  X86_FP3 = 104,
  X86_FP4 = 105,
  X86_FP5 = 106,
  X86_FP6 = 107,
  X86_FP7 = 108,
  X86_K0 = 109,
  X86_K1 = 110,
  X86_K2 = 111,
  X86_K3 = 112,
  X86_K4 = 113,
  X86_K5 = 114,
  X86_K6 = 115,
  X86_K7 = 116,
  X86_MM0 = 117,
  X86_MM1 = 118,
  X86_MM2 = 119,
  X86_MM3 = 120,
  X86_MM4 = 121,
  X86_MM5 = 122,
  X86_MM6 = 123,
  X86_MM7 = 124,
  X86_R8 = 125,
  X86_R9 = 126,
  X86_R10 = 127,
  X86_R11 = 128,
  X86_R12 = 129,
  X86_R13 = 130,
  X86_R14 = 131,
  X86_R15 = 132,
  X86_ST0 = 133,
  X86_ST1 = 134,
  X86_ST2 = 135,
  X86_ST3 = 136,
  X86_ST4 = 137,
  X86_ST5 = 138,
  X86_ST6 = 139,
  X86_ST7 = 140,
  X86_XMM0 = 141,
  X86_XMM1 = 142,
  X86_XMM2 = 143,
  X86_XMM3 = 144,
  X86_XMM4 = 145,
  X86_XMM5 = 146,
  X86_XMM6 = 147,
  X86_XMM7 = 148,
  X86_XMM8 = 149,
  X86_XMM9 = 150,
  X86_XMM10 = 151,
  X86_XMM11 = 152,
  X86_XMM12 = 153,
  X86_XMM13 = 154,
  X86_XMM14 = 155,
  X86_XMM15 = 156,
  X86_XMM16 = 157,
  X86_XMM17 = 158,
  X86_XMM18 = 159,
  X86_XMM19 = 160,
  X86_XMM20 = 161,
  X86_XMM21 = 162,
  X86_XMM22 = 163,
  X86_XMM23 = 164,
  X86_XMM24 = 165,
  X86_XMM25 = 166,
  X86_XMM26 = 167,
  X86_XMM27 = 168,
  X86_XMM28 = 169,
  X86_XMM29 = 170,
  X86_XMM30 = 171,
  X86_XMM31 = 172,
  X86_YMM0 = 173,
  X86_YMM1 = 174,
  X86_YMM2 = 175,
  X86_YMM3 = 176,
  X86_YMM4 = 177,
  X86_YMM5 = 178,
  X86_YMM6 = 179,
  X86_YMM7 = 180,
  X86_YMM8 = 181,
  X86_YMM9 = 182,
  X86_YMM10 = 183,
  X86_YMM11 = 184,
  X86_YMM12 = 185,
  X86_YMM13 = 186,
  X86_YMM14 = 187,
  X86_YMM15 = 188,
  X86_YMM16 = 189,
  X86_YMM17 = 190,
  X86_YMM18 = 191,
  X86_YMM19 = 192,
  X86_YMM20 = 193,
  X86_YMM21 = 194,
  X86_YMM22 = 195,
  X86_YMM23 = 196,
  X86_YMM24 = 197,
  X86_YMM25 = 198,
  X86_YMM26 = 199,
  X86_YMM27 = 200,
  X86_YMM28 = 201,
  X86_YMM29 = 202,
  X86_YMM30 = 203,
  X86_YMM31 = 204,
  X86_ZMM0 = 205,
  X86_ZMM1 = 206,
  X86_ZMM2 = 207,
  X86_ZMM3 = 208,
  X86_ZMM4 = 209,
  X86_ZMM5 = 210,
  X86_ZMM6 = 211,
  X86_ZMM7 = 212,
  X86_ZMM8 = 213,
  X86_ZMM9 = 214,
  X86_ZMM10 = 215,
  X86_ZMM11 = 216,
  X86_ZMM12 = 217,
  X86_ZMM13 = 218,
  X86_ZMM14 = 219,
  X86_ZMM15 = 220,
  X86_ZMM16 = 221,
  X86_ZMM17 = 222,
  X86_ZMM18 = 223,
  X86_ZMM19 = 224,
  X86_ZMM20 = 225,
  X86_ZMM21 = 226,
  X86_ZMM22 = 227,
  X86_ZMM23 = 228,
  X86_ZMM24 = 229,
  X86_ZMM25 = 230,
  X86_ZMM26 = 231,
  X86_ZMM27 = 232,
  X86_ZMM28 = 233,
  X86_ZMM29 = 234,
  X86_ZMM30 = 235,
  X86_ZMM31 = 236,
  X86_R8B = 237,
  X86_R9B = 238,
  X86_R10B = 239,
  X86_R11B = 240,
  X86_R12B = 241,
  X86_R13B = 242,
  X86_R14B = 243,
  X86_R15B = 244,
  X86_R8BH = 245,
  X86_R9BH = 246,
  X86_R10BH = 247,
  X86_R11BH = 248,
  X86_R12BH = 249,
  X86_R13BH = 250,
  X86_R14BH = 251,
  X86_R15BH = 252,
  X86_R8D = 253,
  X86_R9D = 254,
  X86_R10D = 255,
  X86_R11D = 256,
  X86_R12D = 257,
  X86_R13D = 258,
  X86_R14D = 259,
  X86_R15D = 260,
  X86_R8W = 261,
  X86_R9W = 262,
  X86_R10W = 263,
  X86_R11W = 264,
  X86_R12W = 265,
  X86_R13W = 266,
  X86_R14W = 267,
  X86_R15W = 268,
  X86_R8WH = 269,
  X86_R9WH = 270,
  X86_R10WH = 271,
  X86_R11WH = 272,
  X86_R12WH = 273,
  X86_R13WH = 274,
  X86_R14WH = 275,
  X86_R15WH = 276,
  X86_NUM_TARGET_REGS 	// 277
};

// Register classes
enum {
  X86_GR8RegClassID = 0,
  X86_GRH8RegClassID = 1,
  X86_GR8_NOREXRegClassID = 2,
  X86_GR8_ABCD_HRegClassID = 3,
  X86_GR8_ABCD_LRegClassID = 4,
  X86_GRH16RegClassID = 5,
  X86_GR16RegClassID = 6,
  X86_GR16_NOREXRegClassID = 7,
  X86_VK1RegClassID = 8,
  X86_VK16RegClassID = 9,
  X86_VK2RegClassID = 10,
  X86_VK4RegClassID = 11,
  X86_VK8RegClassID = 12,
  X86_VK16WMRegClassID = 13,
  X86_VK1WMRegClassID = 14,
  X86_VK2WMRegClassID = 15,
  X86_VK4WMRegClassID = 16,
  X86_VK8WMRegClassID = 17,
  X86_SEGMENT_REGRegClassID = 18,
  X86_GR16_ABCDRegClassID = 19,
  X86_FPCCRRegClassID = 20,
  X86_FR32XRegClassID = 21,
  X86_LOW32_ADDR_ACCESS_RBPRegClassID = 22,
  X86_LOW32_ADDR_ACCESSRegClassID = 23,
  X86_LOW32_ADDR_ACCESS_RBP_with_sub_8bitRegClassID = 24,
  X86_DEBUG_REGRegClassID = 25,
  X86_FR32RegClassID = 26,
  X86_GR32RegClassID = 27,
  X86_GR32_NOSPRegClassID = 28,
  X86_LOW32_ADDR_ACCESS_RBP_with_sub_16bit_in_GR16_NOREXRegClassID = 29,
  X86_GR32_NOREXRegClassID = 30,
  X86_VK32RegClassID = 31,
  X86_GR32_NOREX_NOSPRegClassID = 32,
  X86_RFP32RegClassID = 33,
  X86_VK32WMRegClassID = 34,
  X86_GR32_ABCDRegClassID = 35,
  X86_GR32_TCRegClassID = 36,
  X86_GR32_ADRegClassID = 37,
  X86_LOW32_ADDR_ACCESS_RBP_with_sub_32bitRegClassID = 38,
  X86_CCRRegClassID = 39,
  X86_DFCCRRegClassID = 40,
  X86_LOW32_ADDR_ACCESS_RBP_with_sub_8bit_with_sub_32bitRegClassID = 41,
  X86_LOW32_ADDR_ACCESS_with_sub_32bitRegClassID = 42,
  X86_RFP64RegClassID = 43,
  X86_FR64XRegClassID = 44,
  X86_GR64RegClassID = 45,
  X86_CONTROL_REGRegClassID = 46,
  X86_FR64RegClassID = 47,
  X86_GR64_with_sub_8bitRegClassID = 48,
  X86_GR64_NOSPRegClassID = 49,
  X86_GR64_NOREXRegClassID = 50,
  X86_GR64_TCRegClassID = 51,
  X86_GR64_NOSP_and_GR64_TCRegClassID = 52,
  X86_GR64_TCW64RegClassID = 53,
  X86_GR64_with_sub_16bit_in_GR16_NOREXRegClassID = 54,
  X86_VK64RegClassID = 55,
  X86_VR64RegClassID = 56,
  X86_GR64_NOREX_NOSPRegClassID = 57,
  X86_GR64_NOSP_and_GR64_TCW64RegClassID = 58,
  X86_GR64_TC_and_GR64_TCW64RegClassID = 59,
  X86_VK64WMRegClassID = 60,
  X86_GR64_NOREX_and_GR64_TCRegClassID = 61,
  X86_GR64_TC_and_GR64_NOSP_and_GR64_TCW64RegClassID = 62,
  X86_GR64_NOREX_NOSP_and_GR64_TCRegClassID = 63,
  X86_GR64_ABCDRegClassID = 64,
  X86_GR64_NOREX_and_GR64_TCW64RegClassID = 65,
  X86_GR64_with_sub_32bit_in_GR32_TCRegClassID = 66,
  X86_GR64_ADRegClassID = 67,
  X86_GR64_and_LOW32_ADDR_ACCESS_RBPRegClassID = 68,
  X86_GR64_NOREX_NOSP_and_LOW32_ADDR_ACCESS_RBPRegClassID = 69,
  X86_GR64_and_LOW32_ADDR_ACCESSRegClassID = 70,
  X86_RSTRegClassID = 71,
  X86_RFP80RegClassID = 72,
  X86_VR128XRegClassID = 73,
  X86_VR128RegClassID = 74,
  X86_VR128HRegClassID = 75,
  X86_VR128LRegClassID = 76,
  X86_BNDRRegClassID = 77,
  X86_VR256XRegClassID = 78,
  X86_VR256RegClassID = 79,
  X86_VR256HRegClassID = 80,
  X86_VR256LRegClassID = 81,
  X86_VR512RegClassID = 82,
  X86_VR512_with_sub_xmm_in_FR32RegClassID = 83,
  X86_VR512_with_sub_xmm_in_VR128HRegClassID = 84,
  X86_VR512_with_sub_xmm_in_VR128LRegClassID = 85,
};
#endif // GET_REGINFO_ENUM

#ifdef GET_REGINFO_MC_DESC
#define GET_REGINFO_MC_DESC


static const MCPhysReg X86RegDiffLists[] = {
  /* 0 */ 0, 1, 0,
  /* 3 */ 64875, 1, 1, 0,
  /* 7 */ 65259, 1, 1, 0,
  /* 11 */ 65397, 1, 1, 0,
  /* 15 */ 65466, 1, 1, 0,
  /* 19 */ 2, 1, 0,
  /* 22 */ 4, 1, 0,
  /* 25 */ 6, 1, 0,
  /* 28 */ 11, 1, 0,
  /* 31 */ 22, 1, 0,
  /* 34 */ 26, 1, 0,
  /* 37 */ 29, 1, 0,
  /* 40 */ 64851, 1, 0,
  /* 43 */ 10, 3, 0,
  /* 46 */ 4, 0,
  /* 48 */ 5, 0,
  /* 50 */ 65292, 1, 7, 0,
  /* 54 */ 65417, 1, 7, 0,
  /* 58 */ 10, 3, 7, 0,
  /* 62 */ 65512, 8, 0,
  /* 65 */ 65342, 1, 11, 0,
  /* 69 */ 65348, 1, 11, 0,
  /* 73 */ 65442, 1, 11, 0,
  /* 77 */ 65448, 1, 11, 0,
  /* 81 */ 12, 0,
  /* 83 */ 65342, 1, 14, 0,
  /* 87 */ 65348, 1, 14, 0,
  /* 91 */ 65442, 1, 14, 0,
  /* 95 */ 65448, 1, 14, 0,
  /* 99 */ 21, 0,
  /* 101 */ 22, 0,
  /* 103 */ 65534, 65509, 23, 0,
  /* 107 */ 65535, 65509, 23, 0,
  /* 111 */ 65534, 65511, 23, 0,
  /* 115 */ 65535, 65511, 23, 0,
  /* 119 */ 65524, 23, 0,
  /* 122 */ 128, 8, 65512, 8, 24, 0,
  /* 128 */ 65519, 24, 0,
  /* 131 */ 65522, 24, 0,
  /* 134 */ 65511, 65526, 2, 65535, 24, 0,
  /* 140 */ 2, 6, 25, 0,
  /* 144 */ 6, 6, 25, 0,
  /* 148 */ 65534, 10, 25, 0,
  /* 152 */ 65535, 10, 25, 0,
  /* 156 */ 2, 12, 25, 0,
  /* 160 */ 3, 12, 25, 0,
  /* 164 */ 4, 15, 25, 0,
  /* 168 */ 5, 15, 25, 0,
  /* 172 */ 65534, 17, 25, 0,
  /* 176 */ 65535, 17, 25, 0,
  /* 180 */ 1, 19, 25, 0,
  /* 184 */ 2, 19, 25, 0,
  /* 188 */ 65521, 25, 0,
  /* 191 */ 26, 0,
  /* 193 */ 65511, 65530, 65534, 65532, 27, 0,
  /* 199 */ 65511, 65524, 65534, 65535, 30, 0,
  /* 205 */ 65511, 65519, 2, 65535, 31, 0,
  /* 211 */ 32, 32, 0,
  /* 214 */ 65511, 65521, 65532, 65535, 35, 0,
  /* 220 */ 65511, 65517, 65535, 65535, 36, 0,
  /* 226 */ 64829, 0,
  /* 228 */ 64900, 0,
  /* 230 */ 64923, 0,
  /* 232 */ 65131, 0,
  /* 234 */ 65520, 65408, 0,
  /* 237 */ 16, 65528, 65408, 0,
  /* 241 */ 24, 65528, 65408, 0,
  /* 245 */ 65430, 0,
  /* 247 */ 65432, 0,
  /* 249 */ 65461, 0,
  /* 251 */ 65493, 0,
  /* 253 */ 65504, 65504, 0,
  /* 256 */ 65509, 0,
  /* 258 */ 65511, 0,
  /* 260 */ 65514, 0,
  /* 262 */ 65513, 27, 2, 65535, 65520, 0,
  /* 268 */ 65513, 25, 2, 65535, 65522, 0,
  /* 274 */ 65525, 0,
  /* 276 */ 65530, 0,
  /* 278 */ 65531, 0,
  /* 280 */ 65534, 65532, 0,
  /* 283 */ 65512, 17, 65533, 0,
  /* 287 */ 65534, 0,
  /* 289 */ 2, 65535, 0,
  /* 292 */ 65532, 65535, 0,
  /* 295 */ 65534, 65535, 0,
  /* 298 */ 65535, 65535, 0,
};

static const uint16_t X86SubRegIdxLists[] = {
  /* 0 */ 1, 2, 0,
  /* 3 */ 1, 3, 0,
  /* 6 */ 6, 4, 1, 2, 5, 0,
  /* 12 */ 6, 4, 1, 3, 5, 0,
  /* 18 */ 6, 4, 5, 0,
  /* 22 */ 8, 7, 0,
};

static const MCRegisterDesc X86RegDesc[] = {
  { 5, 0, 0, 0, 0, 0 },
  { 873, 2, 184, 2, 4641, 0 },
  { 1014, 2, 180, 2, 4641, 0 },
  { 1148, 298, 181, 0, 0, 2 },
  { 879, 2, 168, 2, 4593, 0 },
  { 1017, 2, 164, 2, 4593, 0 },
  { 1043, 289, 173, 3, 352, 5 },
  { 936, 2, 176, 2, 768, 0 },
  { 1034, 2, 172, 2, 736, 0 },
  { 1160, 292, 165, 0, 304, 2 },
  { 922, 2, 160, 2, 4497, 0 },
  { 1020, 2, 156, 2, 4497, 0 },
  { 1082, 2, 2, 2, 4497, 0 },
  { 1172, 295, 157, 0, 400, 2 },
  { 870, 2, 2, 2, 4449, 0 },
  { 925, 2, 144, 2, 4449, 0 },
  { 991, 289, 149, 3, 448, 5 },
  { 928, 2, 152, 2, 1296, 0 },
  { 1026, 2, 148, 2, 4130, 0 },
  { 1023, 2, 140, 2, 4417, 0 },
  { 1085, 2, 2, 2, 4417, 0 },
  { 1184, 280, 141, 0, 688, 2 },
  { 1147, 221, 142, 7, 1524, 8 },
  { 1042, 206, 142, 13, 1236, 12 },
  { 1159, 215, 142, 7, 1460, 8 },
  { 1171, 200, 142, 7, 1172, 8 },
  { 990, 135, 142, 13, 869, 12 },
  { 1183, 194, 142, 7, 928, 8 },
  { 1094, 2, 2, 2, 1584, 0 },
  { 1054, 284, 126, 19, 496, 16 },
  { 1195, 2, 2, 2, 4417, 0 },
  { 1088, 2, 2, 2, 4417, 0 },
  { 1002, 269, 105, 13, 243, 12 },
  { 1066, 263, 105, 13, 243, 12 },
  { 1142, 2, 2, 2, 4593, 0 },
  { 1091, 2, 2, 2, 4593, 0 },
  { 1098, 2, 2, 2, 4593, 0 },
  { 1151, 2, 188, 2, 4161, 0 },
  { 1046, 2, 188, 2, 4161, 0 },
  { 1163, 2, 188, 2, 4161, 0 },
  { 1175, 2, 188, 2, 4161, 0 },
  { 994, 2, 188, 2, 4161, 0 },
  { 1187, 2, 188, 2, 4161, 0 },
  { 1058, 2, 131, 2, 3923, 0 },
  { 1006, 2, 119, 2, 3955, 0 },
  { 1070, 2, 119, 2, 3955, 0 },
  { 1055, 2, 128, 2, 1616, 0 },
  { 1155, 220, 2, 6, 1396, 8 },
  { 1050, 205, 2, 12, 1108, 12 },
  { 1167, 214, 2, 6, 1332, 8 },
  { 1179, 199, 2, 6, 1044, 8 },
  { 998, 134, 2, 12, 805, 12 },
  { 1191, 193, 2, 6, 928, 8 },
  { 1062, 283, 2, 18, 496, 16 },
  { 1199, 2, 2, 2, 3488, 0 },
  { 1010, 268, 2, 12, 179, 12 },
  { 1074, 262, 2, 12, 179, 12 },
  { 1003, 289, 112, 3, 544, 5 },
  { 932, 2, 115, 2, 3152, 0 },
  { 1030, 2, 111, 2, 3056, 0 },
  { 1067, 289, 104, 3, 592, 5 },
  { 940, 2, 107, 2, 3248, 0 },
  { 1038, 2, 103, 2, 3719, 0 },
  { 1101, 2, 2, 2, 4097, 0 },
  { 1078, 2, 2, 2, 4097, 0 },
  { 64, 2, 2, 2, 4097, 0 },
  { 167, 2, 2, 2, 4097, 0 },
  { 252, 2, 2, 2, 4097, 0 },
  { 337, 2, 2, 2, 4097, 0 },
  { 91, 2, 2, 2, 4097, 0 },
  { 194, 2, 2, 2, 4097, 0 },
  { 279, 2, 2, 2, 4097, 0 },
  { 364, 2, 2, 2, 4097, 0 },
  { 444, 2, 2, 2, 4097, 0 },
  { 524, 2, 2, 2, 4097, 0 },
  { 594, 2, 2, 2, 4097, 0 },
  { 664, 2, 2, 2, 4097, 0 },
  { 727, 2, 2, 2, 4097, 0 },
  { 786, 2, 2, 2, 4097, 0 },
  { 18, 2, 2, 2, 4097, 0 },
  { 121, 2, 2, 2, 4097, 0 },
  { 224, 2, 2, 2, 4097, 0 },
  { 309, 2, 2, 2, 4097, 0 },
  { 394, 2, 2, 2, 4097, 0 },
  { 474, 2, 2, 2, 4097, 0 },
  { 95, 2, 2, 2, 4097, 0 },
  { 198, 2, 2, 2, 4097, 0 },
  { 283, 2, 2, 2, 4097, 0 },
  { 368, 2, 2, 2, 4097, 0 },
  { 448, 2, 2, 2, 4097, 0 },
  { 528, 2, 2, 2, 4097, 0 },
  { 598, 2, 2, 2, 4097, 0 },
  { 668, 2, 2, 2, 4097, 0 },
  { 731, 2, 2, 2, 4097, 0 },
  { 790, 2, 2, 2, 4097, 0 },
  { 23, 2, 2, 2, 4097, 0 },
  { 126, 2, 2, 2, 4097, 0 },
  { 229, 2, 2, 2, 4097, 0 },
  { 314, 2, 2, 2, 4097, 0 },
  { 399, 2, 2, 2, 4097, 0 },
  { 479, 2, 2, 2, 4097, 0 },
  { 87, 2, 2, 2, 4097, 0 },
  { 190, 2, 2, 2, 4097, 0 },
  { 275, 2, 2, 2, 4097, 0 },
  { 360, 2, 2, 2, 4097, 0 },
  { 440, 2, 2, 2, 4097, 0 },
  { 520, 2, 2, 2, 4097, 0 },
  { 590, 2, 2, 2, 4097, 0 },
  { 660, 2, 2, 2, 4097, 0 },
  { 69, 2, 2, 2, 4097, 0 },
  { 172, 2, 2, 2, 4097, 0 },
  { 257, 2, 2, 2, 4097, 0 },
  { 342, 2, 2, 2, 4097, 0 },
  { 422, 2, 2, 2, 4097, 0 },
  { 502, 2, 2, 2, 4097, 0 },
  { 572, 2, 2, 2, 4097, 0 },
  { 642, 2, 2, 2, 4097, 0 },
  { 73, 2, 2, 2, 4097, 0 },
  { 176, 2, 2, 2, 4097, 0 },
  { 261, 2, 2, 2, 4097, 0 },
  { 346, 2, 2, 2, 4097, 0 },
  { 426, 2, 2, 2, 4097, 0 },
  { 506, 2, 2, 2, 4097, 0 },
  { 576, 2, 2, 2, 4097, 0 },
  { 646, 2, 2, 2, 4097, 0 },
  { 728, 122, 2, 12, 115, 12 },
  { 787, 122, 2, 12, 115, 12 },
  { 19, 122, 2, 12, 115, 12 },
  { 122, 122, 2, 12, 115, 12 },
  { 225, 122, 2, 12, 115, 12 },
  { 310, 122, 2, 12, 115, 12 },
  { 395, 122, 2, 12, 115, 12 },
  { 475, 122, 2, 12, 115, 12 },
  { 99, 2, 2, 2, 4385, 0 },
  { 202, 2, 2, 2, 4385, 0 },
  { 287, 2, 2, 2, 4385, 0 },
  { 372, 2, 2, 2, 4385, 0 },
  { 452, 2, 2, 2, 4385, 0 },
  { 532, 2, 2, 2, 4385, 0 },
  { 602, 2, 2, 2, 4385, 0 },
  { 672, 2, 2, 2, 4385, 0 },
  { 72, 2, 211, 2, 4385, 0 },
  { 175, 2, 211, 2, 4385, 0 },
  { 260, 2, 211, 2, 4385, 0 },
  { 345, 2, 211, 2, 4385, 0 },
  { 425, 2, 211, 2, 4385, 0 },
  { 505, 2, 211, 2, 4385, 0 },
  { 575, 2, 211, 2, 4385, 0 },
  { 645, 2, 211, 2, 4385, 0 },
  { 712, 2, 211, 2, 4385, 0 },
  { 771, 2, 211, 2, 4385, 0 },
  { 0, 2, 211, 2, 4385, 0 },
  { 103, 2, 211, 2, 4385, 0 },
  { 206, 2, 211, 2, 4385, 0 },
  { 291, 2, 211, 2, 4385, 0 },
  { 376, 2, 211, 2, 4385, 0 },
  { 456, 2, 211, 2, 4385, 0 },
  { 536, 2, 211, 2, 4385, 0 },
  { 606, 2, 211, 2, 4385, 0 },
  { 676, 2, 211, 2, 4385, 0 },
  { 735, 2, 211, 2, 4385, 0 },
  { 28, 2, 211, 2, 4385, 0 },
  { 131, 2, 211, 2, 4385, 0 },
  { 234, 2, 211, 2, 4385, 0 },
  { 319, 2, 211, 2, 4385, 0 },
  { 404, 2, 211, 2, 4385, 0 },
  { 484, 2, 211, 2, 4385, 0 },
  { 554, 2, 211, 2, 4385, 0 },
  { 624, 2, 211, 2, 4385, 0 },
  { 694, 2, 211, 2, 4385, 0 },
  { 753, 2, 211, 2, 4385, 0 },
  { 46, 2, 211, 2, 4385, 0 },
  { 149, 2, 211, 2, 4385, 0 },
  { 77, 254, 212, 23, 4017, 19 },
  { 180, 254, 212, 23, 4017, 19 },
  { 265, 254, 212, 23, 4017, 19 },
  { 350, 254, 212, 23, 4017, 19 },
  { 430, 254, 212, 23, 4017, 19 },
  { 510, 254, 212, 23, 4017, 19 },
  { 580, 254, 212, 23, 4017, 19 },
  { 650, 254, 212, 23, 4017, 19 },
  { 717, 254, 212, 23, 4017, 19 },
  { 776, 254, 212, 23, 4017, 19 },
  { 6, 254, 212, 23, 4017, 19 },
  { 109, 254, 212, 23, 4017, 19 },
  { 212, 254, 212, 23, 4017, 19 },
  { 297, 254, 212, 23, 4017, 19 },
  { 382, 254, 212, 23, 4017, 19 },
  { 462, 254, 212, 23, 4017, 19 },
  { 542, 254, 212, 23, 4017, 19 },
  { 612, 254, 212, 23, 4017, 19 },
  { 682, 254, 212, 23, 4017, 19 },
  { 741, 254, 212, 23, 4017, 19 },
  { 34, 254, 212, 23, 4017, 19 },
  { 137, 254, 212, 23, 4017, 19 },
  { 240, 254, 212, 23, 4017, 19 },
  { 325, 254, 212, 23, 4017, 19 },
  { 410, 254, 212, 23, 4017, 19 },
  { 490, 254, 212, 23, 4017, 19 },
  { 560, 254, 212, 23, 4017, 19 },
  { 630, 254, 212, 23, 4017, 19 },
  { 700, 254, 212, 23, 4017, 19 },
  { 759, 254, 212, 23, 4017, 19 },
  { 52, 254, 212, 23, 4017, 19 },
  { 155, 254, 212, 23, 4017, 19 },
  { 82, 253, 2, 22, 3985, 19 },
  { 185, 253, 2, 22, 3985, 19 },
  { 270, 253, 2, 22, 3985, 19 },
  { 355, 253, 2, 22, 3985, 19 },
  { 435, 253, 2, 22, 3985, 19 },
  { 515, 253, 2, 22, 3985, 19 },
  { 585, 253, 2, 22, 3985, 19 },
  { 655, 253, 2, 22, 3985, 19 },
  { 722, 253, 2, 22, 3985, 19 },
  { 781, 253, 2, 22, 3985, 19 },
  { 12, 253, 2, 22, 3985, 19 },
  { 115, 253, 2, 22, 3985, 19 },
  { 218, 253, 2, 22, 3985, 19 },
  { 303, 253, 2, 22, 3985, 19 },
  { 388, 253, 2, 22, 3985, 19 },
  { 468, 253, 2, 22, 3985, 19 },
  { 548, 253, 2, 22, 3985, 19 },
  { 618, 253, 2, 22, 3985, 19 },
  { 688, 253, 2, 22, 3985, 19 },
  { 747, 253, 2, 22, 3985, 19 },
  { 40, 253, 2, 22, 3985, 19 },
  { 143, 253, 2, 22, 3985, 19 },
  { 246, 253, 2, 22, 3985, 19 },
  { 331, 253, 2, 22, 3985, 19 },
  { 416, 253, 2, 22, 3985, 19 },
  { 496, 253, 2, 22, 3985, 19 },
  { 566, 253, 2, 22, 3985, 19 },
  { 636, 253, 2, 22, 3985, 19 },
  { 706, 253, 2, 22, 3985, 19 },
  { 765, 253, 2, 22, 3985, 19 },
  { 58, 253, 2, 22, 3985, 19 },
  { 161, 253, 2, 22, 3985, 19 },
  { 824, 2, 241, 2, 3683, 0 },
  { 828, 2, 241, 2, 3683, 0 },
  { 794, 2, 241, 2, 3683, 0 },
  { 799, 2, 241, 2, 3683, 0 },
  { 804, 2, 241, 2, 3683, 0 },
  { 809, 2, 241, 2, 3683, 0 },
  { 814, 2, 241, 2, 3683, 0 },
  { 819, 2, 241, 2, 3683, 0 },
  { 912, 2, 237, 2, 3651, 0 },
  { 917, 2, 237, 2, 3651, 0 },
  { 876, 2, 237, 2, 3651, 0 },
  { 882, 2, 237, 2, 3651, 0 },
  { 888, 2, 237, 2, 3651, 0 },
  { 894, 2, 237, 2, 3651, 0 },
  { 900, 2, 237, 2, 3651, 0 },
  { 906, 2, 237, 2, 3651, 0 },
  { 862, 123, 235, 13, 51, 12 },
  { 866, 123, 235, 13, 51, 12 },
  { 832, 123, 235, 13, 51, 12 },
  { 837, 123, 235, 13, 51, 12 },
  { 842, 123, 235, 13, 51, 12 },
  { 847, 123, 235, 13, 51, 12 },
  { 852, 123, 235, 13, 51, 12 },
  { 857, 123, 235, 13, 51, 12 },
  { 1134, 62, 238, 3, 643, 5 },
  { 1138, 62, 238, 3, 643, 5 },
  { 1104, 62, 238, 3, 643, 5 },
  { 1109, 62, 238, 3, 643, 5 },
  { 1114, 62, 238, 3, 643, 5 },
  { 1119, 62, 238, 3, 643, 5 },
  { 1124, 62, 238, 3, 643, 5 },
  { 1129, 62, 238, 3, 643, 5 },
  { 980, 2, 234, 2, 3619, 0 },
  { 985, 2, 234, 2, 3619, 0 },
  { 944, 2, 234, 2, 3619, 0 },
  { 950, 2, 234, 2, 3619, 0 },
  { 956, 2, 234, 2, 3619, 0 },
  { 962, 2, 234, 2, 3619, 0 },
  { 968, 2, 234, 2, 3619, 0 },
  { 974, 2, 234, 2, 3619, 0 },
};

  // GR8 Register Class...
  static const MCPhysReg GR8[] = {
    X86_AL, X86_CL, X86_DL, X86_AH, X86_CH, X86_DH, X86_BL, X86_BH, X86_SIL, X86_DIL, X86_BPL, X86_SPL, X86_R8B, X86_R9B, X86_R10B, X86_R11B, X86_R14B, X86_R15B, X86_R12B, X86_R13B,
  };
  // GR8 Bit set.
  static const uint8_t GR8Bits[] = {
    0x36, 0x8d, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // GRH8 Register Class...
  static const MCPhysReg GRH8[] = {
    X86_SIH, X86_DIH, X86_BPH, X86_SPH, X86_R8BH, X86_R9BH, X86_R10BH, X86_R11BH, X86_R12BH, X86_R13BH, X86_R14BH, X86_R15BH,
  };
  // GRH8 Bit set.
  static const uint8_t GRH8Bits[] = {
    0x80, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // GR8_NOREX Register Class...
  static const MCPhysReg GR8_NOREX[] = {
    X86_AL, X86_CL, X86_DL, X86_AH, X86_CH, X86_DH, X86_BL, X86_BH,
  };
  // GR8_NOREX Bit set.
  static const uint8_t GR8_NOREXBits[] = {
    0x36, 0x8c, 0x08,
  };
  // GR8_ABCD_H Register Class...
  static const MCPhysReg GR8_ABCD_H[] = {
    X86_AH, X86_CH, X86_DH, X86_BH,
  };
  // GR8_ABCD_H Bit set.
  static const uint8_t GR8_ABCD_HBits[] = {
    0x12, 0x84,
  };
  // GR8_ABCD_L Register Class...
  static const MCPhysReg GR8_ABCD_L[] = {
    X86_AL, X86_CL, X86_DL, X86_BL,
  };
  // GR8_ABCD_L Bit set.
  static const uint8_t GR8_ABCD_LBits[] = {
    0x24, 0x08, 0x08,
  };
  // GRH16 Register Class...
  static const MCPhysReg GRH16[] = {
    X86_HAX, X86_HCX, X86_HDX, X86_HSI, X86_HDI, X86_HBX, X86_HBP, X86_HSP, X86_HIP, X86_R8WH, X86_R9WH, X86_R10WH, X86_R11WH, X86_R12WH, X86_R13WH, X86_R14WH, X86_R15WH,
  };
  // GRH16 Bit set.
  static const uint8_t GRH16Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // GR16 Register Class...
  static const MCPhysReg GR16[] = {
    X86_AX, X86_CX, X86_DX, X86_SI, X86_DI, X86_BX, X86_BP, X86_SP, X86_R8W, X86_R9W, X86_R10W, X86_R11W, X86_R14W, X86_R15W, X86_R12W, X86_R13W,
  };
  // GR16 Bit set.
  static const uint8_t GR16Bits[] = {
    0x48, 0x22, 0x21, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // GR16_NOREX Register Class...
  static const MCPhysReg GR16_NOREX[] = {
    X86_AX, X86_CX, X86_DX, X86_SI, X86_DI, X86_BX, X86_BP, X86_SP,
  };
  // GR16_NOREX Bit set.
  static const uint8_t GR16_NOREXBits[] = {
    0x48, 0x22, 0x21, 0x00, 0x00, 0x00, 0x00, 0x12,
  };
  // VK1 Register Class...
  static const MCPhysReg VK1[] = {
    X86_K0, X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK1 Bit set.
  static const uint8_t VK1Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VK16 Register Class...
  static const MCPhysReg VK16[] = {
    X86_K0, X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK16 Bit set.
  static const uint8_t VK16Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VK2 Register Class...
  static const MCPhysReg VK2[] = {
    X86_K0, X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK2 Bit set.
  static const uint8_t VK2Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VK4 Register Class...
  static const MCPhysReg VK4[] = {
    X86_K0, X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK4 Bit set.
  static const uint8_t VK4Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VK8 Register Class...
  static const MCPhysReg VK8[] = {
    X86_K0, X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK8 Bit set.
  static const uint8_t VK8Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VK16WM Register Class...
  static const MCPhysReg VK16WM[] = {
    X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK16WM Bit set.
  static const uint8_t VK16WMBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f,
  };
  // VK1WM Register Class...
  static const MCPhysReg VK1WM[] = {
    X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK1WM Bit set.
  static const uint8_t VK1WMBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f,
  };
  // VK2WM Register Class...
  static const MCPhysReg VK2WM[] = {
    X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK2WM Bit set.
  static const uint8_t VK2WMBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f,
  };
  // VK4WM Register Class...
  static const MCPhysReg VK4WM[] = {
    X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK4WM Bit set.
  static const uint8_t VK4WMBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f,
  };
  // VK8WM Register Class...
  static const MCPhysReg VK8WM[] = {
    X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK8WM Bit set.
  static const uint8_t VK8WMBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f,
  };
  // SEGMENT_REG Register Class...
  static const MCPhysReg SEGMENT_REG[] = {
    X86_CS, X86_DS, X86_SS, X86_ES, X86_FS, X86_GS,
  };
  // SEGMENT_REG Bit set.
  static const uint8_t SEGMENT_REGBits[] = {
    0x00, 0x10, 0x10, 0x80, 0x18, 0x00, 0x00, 0x80,
  };
  // GR16_ABCD Register Class...
  static const MCPhysReg GR16_ABCD[] = {
    X86_AX, X86_CX, X86_DX, X86_BX,
  };
  // GR16_ABCD Bit set.
  static const uint8_t GR16_ABCDBits[] = {
    0x08, 0x22, 0x20,
  };
  // FPCCR Register Class...
  static const MCPhysReg FPCCR[] = {
    X86_FPSW,
  };
  // FPCCR Bit set.
  static const uint8_t FPCCRBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x04,
  };
  // FR32X Register Class...
  static const MCPhysReg FR32X[] = {
    X86_XMM0, X86_XMM1, X86_XMM2, X86_XMM3, X86_XMM4, X86_XMM5, X86_XMM6, X86_XMM7, X86_XMM8, X86_XMM9, X86_XMM10, X86_XMM11, X86_XMM12, X86_XMM13, X86_XMM14, X86_XMM15, X86_XMM16, X86_XMM17, X86_XMM18, X86_XMM19, X86_XMM20, X86_XMM21, X86_XMM22, X86_XMM23, X86_XMM24, X86_XMM25, X86_XMM26, X86_XMM27, X86_XMM28, X86_XMM29, X86_XMM30, X86_XMM31,
  };
  // FR32X Bit set.
  static const uint8_t FR32XBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x1f,
  };
  // LOW32_ADDR_ACCESS_RBP Register Class...
  static const MCPhysReg LOW32_ADDR_ACCESS_RBP[] = {
    X86_EAX, X86_ECX, X86_EDX, X86_ESI, X86_EDI, X86_EBX, X86_EBP, X86_ESP, X86_R8D, X86_R9D, X86_R10D, X86_R11D, X86_R14D, X86_R15D, X86_R12D, X86_R13D, X86_RIP, X86_RBP,
  };
  // LOW32_ADDR_ACCESS_RBP Bit set.
  static const uint8_t LOW32_ADDR_ACCESS_RBPBits[] = {
    0x00, 0x00, 0xc0, 0x0f, 0x03, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // LOW32_ADDR_ACCESS Register Class...
  static const MCPhysReg LOW32_ADDR_ACCESS[] = {
    X86_EAX, X86_ECX, X86_EDX, X86_ESI, X86_EDI, X86_EBX, X86_EBP, X86_ESP, X86_R8D, X86_R9D, X86_R10D, X86_R11D, X86_R14D, X86_R15D, X86_R12D, X86_R13D, X86_RIP,
  };
  // LOW32_ADDR_ACCESS Bit set.
  static const uint8_t LOW32_ADDR_ACCESSBits[] = {
    0x00, 0x00, 0xc0, 0x0f, 0x03, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // LOW32_ADDR_ACCESS_RBP_with_sub_8bit Register Class...
  static const MCPhysReg LOW32_ADDR_ACCESS_RBP_with_sub_8bit[] = {
    X86_EAX, X86_ECX, X86_EDX, X86_ESI, X86_EDI, X86_EBX, X86_EBP, X86_ESP, X86_R8D, X86_R9D, X86_R10D, X86_R11D, X86_R14D, X86_R15D, X86_R12D, X86_R13D, X86_RBP,
  };
  // LOW32_ADDR_ACCESS_RBP_with_sub_8bit Bit set.
  static const uint8_t LOW32_ADDR_ACCESS_RBP_with_sub_8bitBits[] = {
    0x00, 0x00, 0xc0, 0x0f, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // DEBUG_REG Register Class...
  static const MCPhysReg DEBUG_REG[] = {
    X86_DR0, X86_DR1, X86_DR2, X86_DR3, X86_DR4, X86_DR5, X86_DR6, X86_DR7, X86_DR8, X86_DR9, X86_DR10, X86_DR11, X86_DR12, X86_DR13, X86_DR14, X86_DR15,
  };
  // DEBUG_REG Bit set.
  static const uint8_t DEBUG_REGBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x1f,
  };
  // FR32 Register Class...
  static const MCPhysReg FR32[] = {
    X86_XMM0, X86_XMM1, X86_XMM2, X86_XMM3, X86_XMM4, X86_XMM5, X86_XMM6, X86_XMM7, X86_XMM8, X86_XMM9, X86_XMM10, X86_XMM11, X86_XMM12, X86_XMM13, X86_XMM14, X86_XMM15,
  };
  // FR32 Bit set.
  static const uint8_t FR32Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x1f,
  };
  // GR32 Register Class...
  static const MCPhysReg GR32[] = {
    X86_EAX, X86_ECX, X86_EDX, X86_ESI, X86_EDI, X86_EBX, X86_EBP, X86_ESP, X86_R8D, X86_R9D, X86_R10D, X86_R11D, X86_R14D, X86_R15D, X86_R12D, X86_R13D,
  };
  // GR32 Bit set.
  static const uint8_t GR32Bits[] = {
    0x00, 0x00, 0xc0, 0x0f, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // GR32_NOSP Register Class...
  static const MCPhysReg GR32_NOSP[] = {
    X86_EAX, X86_ECX, X86_EDX, X86_ESI, X86_EDI, X86_EBX, X86_EBP, X86_R8D, X86_R9D, X86_R10D, X86_R11D, X86_R14D, X86_R15D, X86_R12D, X86_R13D,
  };
  // GR32_NOSP Bit set.
  static const uint8_t GR32_NOSPBits[] = {
    0x00, 0x00, 0xc0, 0x0f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // LOW32_ADDR_ACCESS_RBP_with_sub_16bit_in_GR16_NOREX Register Class...
  static const MCPhysReg LOW32_ADDR_ACCESS_RBP_with_sub_16bit_in_GR16_NOREX[] = {
    X86_EAX, X86_ECX, X86_EDX, X86_ESI, X86_EDI, X86_EBX, X86_EBP, X86_ESP, X86_RBP,
  };
  // LOW32_ADDR_ACCESS_RBP_with_sub_16bit_in_GR16_NOREX Bit set.
  static const uint8_t LOW32_ADDR_ACCESS_RBP_with_sub_16bit_in_GR16_NOREXBits[] = {
    0x00, 0x00, 0xc0, 0x0f, 0x03, 0x00, 0x01,
  };
  // GR32_NOREX Register Class...
  static const MCPhysReg GR32_NOREX[] = {
    X86_EAX, X86_ECX, X86_EDX, X86_ESI, X86_EDI, X86_EBX, X86_EBP, X86_ESP,
  };
  // GR32_NOREX Bit set.
  static const uint8_t GR32_NOREXBits[] = {
    0x00, 0x00, 0xc0, 0x0f, 0x03,
  };
  // VK32 Register Class...
  static const MCPhysReg VK32[] = {
    X86_K0, X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK32 Bit set.
  static const uint8_t VK32Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // GR32_NOREX_NOSP Register Class...
  static const MCPhysReg GR32_NOREX_NOSP[] = {
    X86_EAX, X86_ECX, X86_EDX, X86_ESI, X86_EDI, X86_EBX, X86_EBP,
  };
  // GR32_NOREX_NOSP Bit set.
  static const uint8_t GR32_NOREX_NOSPBits[] = {
    0x00, 0x00, 0xc0, 0x0f, 0x01,
  };
  // RFP32 Register Class...
  static const MCPhysReg RFP32[] = {
    X86_FP0, X86_FP1, X86_FP2, X86_FP3, X86_FP4, X86_FP5, X86_FP6,
  };
  // RFP32 Bit set.
  static const uint8_t RFP32Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x0f,
  };
  // VK32WM Register Class...
  static const MCPhysReg VK32WM[] = {
    X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK32WM Bit set.
  static const uint8_t VK32WMBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f,
  };
  // GR32_ABCD Register Class...
  static const MCPhysReg GR32_ABCD[] = {
    X86_EAX, X86_ECX, X86_EDX, X86_EBX,
  };
  // GR32_ABCD Bit set.
  static const uint8_t GR32_ABCDBits[] = {
    0x00, 0x00, 0x40, 0x0b,
  };
  // GR32_TC Register Class...
  static const MCPhysReg GR32_TC[] = {
    X86_EAX, X86_ECX, X86_EDX,
  };
  // GR32_TC Bit set.
  static const uint8_t GR32_TCBits[] = {
    0x00, 0x00, 0x40, 0x0a,
  };
  // GR32_AD Register Class...
  static const MCPhysReg GR32_AD[] = {
    X86_EAX, X86_EDX,
  };
  // GR32_AD Bit set.
  static const uint8_t GR32_ADBits[] = {
    0x00, 0x00, 0x40, 0x08,
  };
  // LOW32_ADDR_ACCESS_RBP_with_sub_32bit Register Class...
  static const MCPhysReg LOW32_ADDR_ACCESS_RBP_with_sub_32bit[] = {
    X86_RIP, X86_RBP,
  };
  // LOW32_ADDR_ACCESS_RBP_with_sub_32bit Bit set.
  static const uint8_t LOW32_ADDR_ACCESS_RBP_with_sub_32bitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21,
  };
  // CCR Register Class...
  static const MCPhysReg CCR[] = {
    X86_EFLAGS,
  };
  // CCR Bit set.
  static const uint8_t CCRBits[] = {
    0x00, 0x00, 0x00, 0x10,
  };
  // DFCCR Register Class...
  static const MCPhysReg DFCCR[] = {
    X86_DF,
  };
  // DFCCR Bit set.
  static const uint8_t DFCCRBits[] = {
    0x00, 0x40,
  };
  // LOW32_ADDR_ACCESS_RBP_with_sub_8bit_with_sub_32bit Register Class...
  static const MCPhysReg LOW32_ADDR_ACCESS_RBP_with_sub_8bit_with_sub_32bit[] = {
    X86_RBP,
  };
  // LOW32_ADDR_ACCESS_RBP_with_sub_8bit_with_sub_32bit Bit set.
  static const uint8_t LOW32_ADDR_ACCESS_RBP_with_sub_8bit_with_sub_32bitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  };
  // LOW32_ADDR_ACCESS_with_sub_32bit Register Class...
  static const MCPhysReg LOW32_ADDR_ACCESS_with_sub_32bit[] = {
    X86_RIP,
  };
  // LOW32_ADDR_ACCESS_with_sub_32bit Bit set.
  static const uint8_t LOW32_ADDR_ACCESS_with_sub_32bitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
  };
  // RFP64 Register Class...
  static const MCPhysReg RFP64[] = {
    X86_FP0, X86_FP1, X86_FP2, X86_FP3, X86_FP4, X86_FP5, X86_FP6,
  };
  // RFP64 Bit set.
  static const uint8_t RFP64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x0f,
  };
  // FR64X Register Class...
  static const MCPhysReg FR64X[] = {
    X86_XMM0, X86_XMM1, X86_XMM2, X86_XMM3, X86_XMM4, X86_XMM5, X86_XMM6, X86_XMM7, X86_XMM8, X86_XMM9, X86_XMM10, X86_XMM11, X86_XMM12, X86_XMM13, X86_XMM14, X86_XMM15, X86_XMM16, X86_XMM17, X86_XMM18, X86_XMM19, X86_XMM20, X86_XMM21, X86_XMM22, X86_XMM23, X86_XMM24, X86_XMM25, X86_XMM26, X86_XMM27, X86_XMM28, X86_XMM29, X86_XMM30, X86_XMM31,
  };
  // FR64X Bit set.
  static const uint8_t FR64XBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x1f,
  };
  // GR64 Register Class...
  static const MCPhysReg GR64[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI, X86_R8, X86_R9, X86_R10, X86_R11, X86_RBX, X86_R14, X86_R15, X86_R12, X86_R13, X86_RBP, X86_RSP, X86_RIP,
  };
  // GR64 Bit set.
  static const uint8_t GR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // CONTROL_REG Register Class...
  static const MCPhysReg CONTROL_REG[] = {
    X86_CR0, X86_CR1, X86_CR2, X86_CR3, X86_CR4, X86_CR5, X86_CR6, X86_CR7, X86_CR8, X86_CR9, X86_CR10, X86_CR11, X86_CR12, X86_CR13, X86_CR14, X86_CR15,
  };
  // CONTROL_REG Bit set.
  static const uint8_t CONTROL_REGBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x1f,
  };
  // FR64 Register Class...
  static const MCPhysReg FR64[] = {
    X86_XMM0, X86_XMM1, X86_XMM2, X86_XMM3, X86_XMM4, X86_XMM5, X86_XMM6, X86_XMM7, X86_XMM8, X86_XMM9, X86_XMM10, X86_XMM11, X86_XMM12, X86_XMM13, X86_XMM14, X86_XMM15,
  };
  // FR64 Bit set.
  static const uint8_t FR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x1f,
  };
  // GR64_with_sub_8bit Register Class...
  static const MCPhysReg GR64_with_sub_8bit[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI, X86_R8, X86_R9, X86_R10, X86_R11, X86_RBX, X86_R14, X86_R15, X86_R12, X86_R13, X86_RBP, X86_RSP,
  };
  // GR64_with_sub_8bit Bit set.
  static const uint8_t GR64_with_sub_8bitBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // GR64_NOSP Register Class...
  static const MCPhysReg GR64_NOSP[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI, X86_R8, X86_R9, X86_R10, X86_R11, X86_RBX, X86_R14, X86_R15, X86_R12, X86_R13, X86_RBP,
  };
  // GR64_NOSP Bit set.
  static const uint8_t GR64_NOSPBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // GR64_NOREX Register Class...
  static const MCPhysReg GR64_NOREX[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI, X86_RBX, X86_RBP, X86_RSP, X86_RIP,
  };
  // GR64_NOREX Bit set.
  static const uint8_t GR64_NOREXBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x01,
  };
  // GR64_TC Register Class...
  static const MCPhysReg GR64_TC[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI, X86_R8, X86_R9, X86_R11, X86_RIP,
  };
  // GR64_TC Bit set.
  static const uint8_t GR64_TCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x01,
  };
  // GR64_NOSP_and_GR64_TC Register Class...
  static const MCPhysReg GR64_NOSP_and_GR64_TC[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI, X86_R8, X86_R9, X86_R11,
  };
  // GR64_NOSP_and_GR64_TC Bit set.
  static const uint8_t GR64_NOSP_and_GR64_TCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x01,
  };
  // GR64_TCW64 Register Class...
  static const MCPhysReg GR64_TCW64[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_R8, X86_R9, X86_R10, X86_R11, X86_RIP,
  };
  // GR64_TCW64 Bit set.
  static const uint8_t GR64_TCW64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x01,
  };
  // GR64_with_sub_16bit_in_GR16_NOREX Register Class...
  static const MCPhysReg GR64_with_sub_16bit_in_GR16_NOREX[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI, X86_RBX, X86_RBP, X86_RSP,
  };
  // GR64_with_sub_16bit_in_GR16_NOREX Bit set.
  static const uint8_t GR64_with_sub_16bit_in_GR16_NOREXBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9f, 0x01,
  };
  // VK64 Register Class...
  static const MCPhysReg VK64[] = {
    X86_K0, X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK64 Bit set.
  static const uint8_t VK64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VR64 Register Class...
  static const MCPhysReg VR64[] = {
    X86_MM0, X86_MM1, X86_MM2, X86_MM3, X86_MM4, X86_MM5, X86_MM6, X86_MM7,
  };
  // VR64 Bit set.
  static const uint8_t VR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // GR64_NOREX_NOSP Register Class...
  static const MCPhysReg GR64_NOREX_NOSP[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI, X86_RBX, X86_RBP,
  };
  // GR64_NOREX_NOSP Bit set.
  static const uint8_t GR64_NOREX_NOSPBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9f,
  };
  // GR64_NOSP_and_GR64_TCW64 Register Class...
  static const MCPhysReg GR64_NOSP_and_GR64_TCW64[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_R8, X86_R9, X86_R10, X86_R11,
  };
  // GR64_NOSP_and_GR64_TCW64 Bit set.
  static const uint8_t GR64_NOSP_and_GR64_TCW64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x01,
  };
  // GR64_TC_and_GR64_TCW64 Register Class...
  static const MCPhysReg GR64_TC_and_GR64_TCW64[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_R8, X86_R9, X86_R11, X86_RIP,
  };
  // GR64_TC_and_GR64_TCW64 Bit set.
  static const uint8_t GR64_TC_and_GR64_TCW64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x01,
  };
  // VK64WM Register Class...
  static const MCPhysReg VK64WM[] = {
    X86_K1, X86_K2, X86_K3, X86_K4, X86_K5, X86_K6, X86_K7,
  };
  // VK64WM Bit set.
  static const uint8_t VK64WMBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f,
  };
  // GR64_NOREX_and_GR64_TC Register Class...
  static const MCPhysReg GR64_NOREX_and_GR64_TC[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI, X86_RIP,
  };
  // GR64_NOREX_and_GR64_TC Bit set.
  static const uint8_t GR64_NOREX_and_GR64_TCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbc,
  };
  // GR64_TC_and_GR64_NOSP_and_GR64_TCW64 Register Class...
  static const MCPhysReg GR64_TC_and_GR64_NOSP_and_GR64_TCW64[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_R8, X86_R9, X86_R11,
  };
  // GR64_TC_and_GR64_NOSP_and_GR64_TCW64 Bit set.
  static const uint8_t GR64_TC_and_GR64_NOSP_and_GR64_TCW64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x01,
  };
  // GR64_NOREX_NOSP_and_GR64_TC Register Class...
  static const MCPhysReg GR64_NOREX_NOSP_and_GR64_TC[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_RDI,
  };
  // GR64_NOREX_NOSP_and_GR64_TC Bit set.
  static const uint8_t GR64_NOREX_NOSP_and_GR64_TCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9c,
  };
  // GR64_ABCD Register Class...
  static const MCPhysReg GR64_ABCD[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RBX,
  };
  // GR64_ABCD Bit set.
  static const uint8_t GR64_ABCDBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x16,
  };
  // GR64_NOREX_and_GR64_TCW64 Register Class...
  static const MCPhysReg GR64_NOREX_and_GR64_TCW64[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RIP,
  };
  // GR64_NOREX_and_GR64_TCW64 Bit set.
  static const uint8_t GR64_NOREX_and_GR64_TCW64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x34,
  };
  // GR64_with_sub_32bit_in_GR32_TC Register Class...
  static const MCPhysReg GR64_with_sub_32bit_in_GR32_TC[] = {
    X86_RAX, X86_RCX, X86_RDX,
  };
  // GR64_with_sub_32bit_in_GR32_TC Bit set.
  static const uint8_t GR64_with_sub_32bit_in_GR32_TCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x14,
  };
  // GR64_AD Register Class...
  static const MCPhysReg GR64_AD[] = {
    X86_RAX, X86_RDX,
  };
  // GR64_AD Bit set.
  static const uint8_t GR64_ADBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x10,
  };
  // GR64_and_LOW32_ADDR_ACCESS_RBP Register Class...
  static const MCPhysReg GR64_and_LOW32_ADDR_ACCESS_RBP[] = {
    X86_RBP, X86_RIP,
  };
  // GR64_and_LOW32_ADDR_ACCESS_RBP Bit set.
  static const uint8_t GR64_and_LOW32_ADDR_ACCESS_RBPBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21,
  };
  // GR64_NOREX_NOSP_and_LOW32_ADDR_ACCESS_RBP Register Class...
  static const MCPhysReg GR64_NOREX_NOSP_and_LOW32_ADDR_ACCESS_RBP[] = {
    X86_RBP,
  };
  // GR64_NOREX_NOSP_and_LOW32_ADDR_ACCESS_RBP Bit set.
  static const uint8_t GR64_NOREX_NOSP_and_LOW32_ADDR_ACCESS_RBPBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  };
  // GR64_and_LOW32_ADDR_ACCESS Register Class...
  static const MCPhysReg GR64_and_LOW32_ADDR_ACCESS[] = {
    X86_RIP,
  };
  // GR64_and_LOW32_ADDR_ACCESS Bit set.
  static const uint8_t GR64_and_LOW32_ADDR_ACCESSBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
  };
  // RST Register Class...
  static const MCPhysReg RST[] = {
    X86_ST0, X86_ST1, X86_ST2, X86_ST3, X86_ST4, X86_ST5, X86_ST6, X86_ST7,
  };
  // RST Bit set.
  static const uint8_t RSTBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // RFP80 Register Class...
  static const MCPhysReg RFP80[] = {
    X86_FP0, X86_FP1, X86_FP2, X86_FP3, X86_FP4, X86_FP5, X86_FP6,
  };
  // RFP80 Bit set.
  static const uint8_t RFP80Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x0f,
  };
  // VR128X Register Class...
  static const MCPhysReg VR128X[] = {
    X86_XMM0, X86_XMM1, X86_XMM2, X86_XMM3, X86_XMM4, X86_XMM5, X86_XMM6, X86_XMM7, X86_XMM8, X86_XMM9, X86_XMM10, X86_XMM11, X86_XMM12, X86_XMM13, X86_XMM14, X86_XMM15, X86_XMM16, X86_XMM17, X86_XMM18, X86_XMM19, X86_XMM20, X86_XMM21, X86_XMM22, X86_XMM23, X86_XMM24, X86_XMM25, X86_XMM26, X86_XMM27, X86_XMM28, X86_XMM29, X86_XMM30, X86_XMM31,
  };
  // VR128X Bit set.
  static const uint8_t VR128XBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x1f,
  };
  // VR128 Register Class...
  static const MCPhysReg VR128[] = {
    X86_XMM0, X86_XMM1, X86_XMM2, X86_XMM3, X86_XMM4, X86_XMM5, X86_XMM6, X86_XMM7, X86_XMM8, X86_XMM9, X86_XMM10, X86_XMM11, X86_XMM12, X86_XMM13, X86_XMM14, X86_XMM15,
  };
  // VR128 Bit set.
  static const uint8_t VR128Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x1f,
  };
  // VR128H Register Class...
  static const MCPhysReg VR128H[] = {
    X86_XMM8, X86_XMM9, X86_XMM10, X86_XMM11, X86_XMM12, X86_XMM13, X86_XMM14, X86_XMM15,
  };
  // VR128H Bit set.
  static const uint8_t VR128HBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VR128L Register Class...
  static const MCPhysReg VR128L[] = {
    X86_XMM0, X86_XMM1, X86_XMM2, X86_XMM3, X86_XMM4, X86_XMM5, X86_XMM6, X86_XMM7,
  };
  // VR128L Bit set.
  static const uint8_t VR128LBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // BNDR Register Class...
  static const MCPhysReg BNDR[] = {
    X86_BND0, X86_BND1, X86_BND2, X86_BND3,
  };
  // BNDR Bit set.
  static const uint8_t BNDRBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e,
  };
  // VR256X Register Class...
  static const MCPhysReg VR256X[] = {
    X86_YMM0, X86_YMM1, X86_YMM2, X86_YMM3, X86_YMM4, X86_YMM5, X86_YMM6, X86_YMM7, X86_YMM8, X86_YMM9, X86_YMM10, X86_YMM11, X86_YMM12, X86_YMM13, X86_YMM14, X86_YMM15, X86_YMM16, X86_YMM17, X86_YMM18, X86_YMM19, X86_YMM20, X86_YMM21, X86_YMM22, X86_YMM23, X86_YMM24, X86_YMM25, X86_YMM26, X86_YMM27, X86_YMM28, X86_YMM29, X86_YMM30, X86_YMM31,
  };
  // VR256X Bit set.
  static const uint8_t VR256XBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x1f,
  };
  // VR256 Register Class...
  static const MCPhysReg VR256[] = {
    X86_YMM0, X86_YMM1, X86_YMM2, X86_YMM3, X86_YMM4, X86_YMM5, X86_YMM6, X86_YMM7, X86_YMM8, X86_YMM9, X86_YMM10, X86_YMM11, X86_YMM12, X86_YMM13, X86_YMM14, X86_YMM15,
  };
  // VR256 Bit set.
  static const uint8_t VR256Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x1f,
  };
  // VR256H Register Class...
  static const MCPhysReg VR256H[] = {
    X86_YMM8, X86_YMM9, X86_YMM10, X86_YMM11, X86_YMM12, X86_YMM13, X86_YMM14, X86_YMM15,
  };
  // VR256H Bit set.
  static const uint8_t VR256HBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VR256L Register Class...
  static const MCPhysReg VR256L[] = {
    X86_YMM0, X86_YMM1, X86_YMM2, X86_YMM3, X86_YMM4, X86_YMM5, X86_YMM6, X86_YMM7,
  };
  // VR256L Bit set.
  static const uint8_t VR256LBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VR512 Register Class...
  static const MCPhysReg VR512[] = {
    X86_ZMM0, X86_ZMM1, X86_ZMM2, X86_ZMM3, X86_ZMM4, X86_ZMM5, X86_ZMM6, X86_ZMM7, X86_ZMM8, X86_ZMM9, X86_ZMM10, X86_ZMM11, X86_ZMM12, X86_ZMM13, X86_ZMM14, X86_ZMM15, X86_ZMM16, X86_ZMM17, X86_ZMM18, X86_ZMM19, X86_ZMM20, X86_ZMM21, X86_ZMM22, X86_ZMM23, X86_ZMM24, X86_ZMM25, X86_ZMM26, X86_ZMM27, X86_ZMM28, X86_ZMM29, X86_ZMM30, X86_ZMM31,
  };
  // VR512 Bit set.
  static const uint8_t VR512Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x1f,
  };
  // VR512_with_sub_xmm_in_FR32 Register Class...
  static const MCPhysReg VR512_with_sub_xmm_in_FR32[] = {
    X86_ZMM0, X86_ZMM1, X86_ZMM2, X86_ZMM3, X86_ZMM4, X86_ZMM5, X86_ZMM6, X86_ZMM7, X86_ZMM8, X86_ZMM9, X86_ZMM10, X86_ZMM11, X86_ZMM12, X86_ZMM13, X86_ZMM14, X86_ZMM15,
  };
  // VR512_with_sub_xmm_in_FR32 Bit set.
  static const uint8_t VR512_with_sub_xmm_in_FR32Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x1f,
  };
  // VR512_with_sub_xmm_in_VR128H Register Class...
  static const MCPhysReg VR512_with_sub_xmm_in_VR128H[] = {
    X86_ZMM8, X86_ZMM9, X86_ZMM10, X86_ZMM11, X86_ZMM12, X86_ZMM13, X86_ZMM14, X86_ZMM15,
  };
  // VR512_with_sub_xmm_in_VR128H Bit set.
  static const uint8_t VR512_with_sub_xmm_in_VR128HBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };
  // VR512_with_sub_xmm_in_VR128L Register Class...
  static const MCPhysReg VR512_with_sub_xmm_in_VR128L[] = {
    X86_ZMM0, X86_ZMM1, X86_ZMM2, X86_ZMM3, X86_ZMM4, X86_ZMM5, X86_ZMM6, X86_ZMM7,
  };
  // VR512_with_sub_xmm_in_VR128L Bit set.
  static const uint8_t VR512_with_sub_xmm_in_VR128LBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x1f,
  };


static const MCRegisterClass X86MCRegisterClasses[] = {
  { GR8, GR8Bits, sizeof(GR8Bits) },
  { GRH8, GRH8Bits, sizeof(GRH8Bits) },
  { GR8_NOREX, GR8_NOREXBits, sizeof(GR8_NOREXBits) },
  { GR8_ABCD_H, GR8_ABCD_HBits, sizeof(GR8_ABCD_HBits) },
  { GR8_ABCD_L, GR8_ABCD_LBits, sizeof(GR8_ABCD_LBits) },
  { GRH16, GRH16Bits, sizeof(GRH16Bits) },
  { GR16, GR16Bits, sizeof(GR16Bits) },
  { GR16_NOREX, GR16_NOREXBits, sizeof(GR16_NOREXBits) },
  { VK1, VK1Bits, sizeof(VK1Bits) },
  { VK16, VK16Bits, sizeof(VK16Bits) },
  { VK2, VK2Bits, sizeof(VK2Bits) },
  { VK4, VK4Bits, sizeof(VK4Bits) },
  { VK8, VK8Bits, sizeof(VK8Bits) },
  { VK16WM, VK16WMBits, sizeof(VK16WMBits) },
  { VK1WM, VK1WMBits, sizeof(VK1WMBits) },
  { VK2WM, VK2WMBits, sizeof(VK2WMBits) },
  { VK4WM, VK4WMBits, sizeof(VK4WMBits) },
  { VK8WM, VK8WMBits, sizeof(VK8WMBits) },
  { SEGMENT_REG, SEGMENT_REGBits, sizeof(SEGMENT_REGBits) },
  { GR16_ABCD, GR16_ABCDBits, sizeof(GR16_ABCDBits) },
  { FPCCR, FPCCRBits, sizeof(FPCCRBits) },
  { FR32X, FR32XBits, sizeof(FR32XBits) },
  { LOW32_ADDR_ACCESS_RBP, LOW32_ADDR_ACCESS_RBPBits, sizeof(LOW32_ADDR_ACCESS_RBPBits) },
  { LOW32_ADDR_ACCESS, LOW32_ADDR_ACCESSBits, sizeof(LOW32_ADDR_ACCESSBits) },
  { LOW32_ADDR_ACCESS_RBP_with_sub_8bit, LOW32_ADDR_ACCESS_RBP_with_sub_8bitBits, sizeof(LOW32_ADDR_ACCESS_RBP_with_sub_8bitBits) },
  { DEBUG_REG, DEBUG_REGBits, sizeof(DEBUG_REGBits) },
  { FR32, FR32Bits, sizeof(FR32Bits) },
  { GR32, GR32Bits, sizeof(GR32Bits) },
  { GR32_NOSP, GR32_NOSPBits, sizeof(GR32_NOSPBits) },
  { LOW32_ADDR_ACCESS_RBP_with_sub_16bit_in_GR16_NOREX, LOW32_ADDR_ACCESS_RBP_with_sub_16bit_in_GR16_NOREXBits, sizeof(LOW32_ADDR_ACCESS_RBP_with_sub_16bit_in_GR16_NOREXBits) },
  { GR32_NOREX, GR32_NOREXBits, sizeof(GR32_NOREXBits) },
  { VK32, VK32Bits, sizeof(VK32Bits) },
  { GR32_NOREX_NOSP, GR32_NOREX_NOSPBits, sizeof(GR32_NOREX_NOSPBits) },
  { RFP32, RFP32Bits, sizeof(RFP32Bits) },
  { VK32WM, VK32WMBits, sizeof(VK32WMBits) },
  { GR32_ABCD, GR32_ABCDBits, sizeof(GR32_ABCDBits) },
  { GR32_TC, GR32_TCBits, sizeof(GR32_TCBits) },
  { GR32_AD, GR32_ADBits, sizeof(GR32_ADBits) },
  { LOW32_ADDR_ACCESS_RBP_with_sub_32bit, LOW32_ADDR_ACCESS_RBP_with_sub_32bitBits, sizeof(LOW32_ADDR_ACCESS_RBP_with_sub_32bitBits) },
  { CCR, CCRBits, sizeof(CCRBits) },
  { DFCCR, DFCCRBits, sizeof(DFCCRBits) },
  { LOW32_ADDR_ACCESS_RBP_with_sub_8bit_with_sub_32bit, LOW32_ADDR_ACCESS_RBP_with_sub_8bit_with_sub_32bitBits, sizeof(LOW32_ADDR_ACCESS_RBP_with_sub_8bit_with_sub_32bitBits) },
  { LOW32_ADDR_ACCESS_with_sub_32bit, LOW32_ADDR_ACCESS_with_sub_32bitBits, sizeof(LOW32_ADDR_ACCESS_with_sub_32bitBits) },
  { RFP64, RFP64Bits, sizeof(RFP64Bits) },
  { FR64X, FR64XBits, sizeof(FR64XBits) },
  { GR64, GR64Bits, sizeof(GR64Bits) },
  { CONTROL_REG, CONTROL_REGBits, sizeof(CONTROL_REGBits) },
  { FR64, FR64Bits, sizeof(FR64Bits) },
  { GR64_with_sub_8bit, GR64_with_sub_8bitBits, sizeof(GR64_with_sub_8bitBits) },
  { GR64_NOSP, GR64_NOSPBits, sizeof(GR64_NOSPBits) },
  { GR64_NOREX, GR64_NOREXBits, sizeof(GR64_NOREXBits) },
  { GR64_TC, GR64_TCBits, sizeof(GR64_TCBits) },
  { GR64_NOSP_and_GR64_TC, GR64_NOSP_and_GR64_TCBits, sizeof(GR64_NOSP_and_GR64_TCBits) },
  { GR64_TCW64, GR64_TCW64Bits, sizeof(GR64_TCW64Bits) },
  { GR64_with_sub_16bit_in_GR16_NOREX, GR64_with_sub_16bit_in_GR16_NOREXBits, sizeof(GR64_with_sub_16bit_in_GR16_NOREXBits) },
  { VK64, VK64Bits, sizeof(VK64Bits) },
  { VR64, VR64Bits, sizeof(VR64Bits) },
  { GR64_NOREX_NOSP, GR64_NOREX_NOSPBits, sizeof(GR64_NOREX_NOSPBits) },
  { GR64_NOSP_and_GR64_TCW64, GR64_NOSP_and_GR64_TCW64Bits, sizeof(GR64_NOSP_and_GR64_TCW64Bits) },
  { GR64_TC_and_GR64_TCW64, GR64_TC_and_GR64_TCW64Bits, sizeof(GR64_TC_and_GR64_TCW64Bits) },
  { VK64WM, VK64WMBits, sizeof(VK64WMBits) },
  { GR64_NOREX_and_GR64_TC, GR64_NOREX_and_GR64_TCBits, sizeof(GR64_NOREX_and_GR64_TCBits) },
  { GR64_TC_and_GR64_NOSP_and_GR64_TCW64, GR64_TC_and_GR64_NOSP_and_GR64_TCW64Bits, sizeof(GR64_TC_and_GR64_NOSP_and_GR64_TCW64Bits) },
  { GR64_NOREX_NOSP_and_GR64_TC, GR64_NOREX_NOSP_and_GR64_TCBits, sizeof(GR64_NOREX_NOSP_and_GR64_TCBits) },
  { GR64_ABCD, GR64_ABCDBits, sizeof(GR64_ABCDBits) },
  { GR64_NOREX_and_GR64_TCW64, GR64_NOREX_and_GR64_TCW64Bits, sizeof(GR64_NOREX_and_GR64_TCW64Bits) },
  { GR64_with_sub_32bit_in_GR32_TC, GR64_with_sub_32bit_in_GR32_TCBits, sizeof(GR64_with_sub_32bit_in_GR32_TCBits) },
  { GR64_AD, GR64_ADBits, sizeof(GR64_ADBits) },
  { GR64_and_LOW32_ADDR_ACCESS_RBP, GR64_and_LOW32_ADDR_ACCESS_RBPBits, sizeof(GR64_and_LOW32_ADDR_ACCESS_RBPBits) },
  { GR64_NOREX_NOSP_and_LOW32_ADDR_ACCESS_RBP, GR64_NOREX_NOSP_and_LOW32_ADDR_ACCESS_RBPBits, sizeof(GR64_NOREX_NOSP_and_LOW32_ADDR_ACCESS_RBPBits) },
  { GR64_and_LOW32_ADDR_ACCESS, GR64_and_LOW32_ADDR_ACCESSBits, sizeof(GR64_and_LOW32_ADDR_ACCESSBits) },
  { RST, RSTBits, sizeof(RSTBits) },
  { RFP80, RFP80Bits, sizeof(RFP80Bits) },
  { VR128X, VR128XBits, sizeof(VR128XBits) },
  { VR128, VR128Bits, sizeof(VR128Bits) },
  { VR128H, VR128HBits, sizeof(VR128HBits) },
  { VR128L, VR128LBits, sizeof(VR128LBits) },
  { BNDR, BNDRBits, sizeof(BNDRBits) },
  { VR256X, VR256XBits, sizeof(VR256XBits) },
  { VR256, VR256Bits, sizeof(VR256Bits) },
  { VR256H, VR256HBits, sizeof(VR256HBits) },
  { VR256L, VR256LBits, sizeof(VR256LBits) },
  { VR512, VR512Bits, sizeof(VR512Bits) },
  { VR512_with_sub_xmm_in_FR32, VR512_with_sub_xmm_in_FR32Bits, sizeof(VR512_with_sub_xmm_in_FR32Bits) },
  { VR512_with_sub_xmm_in_VR128H, VR512_with_sub_xmm_in_VR128HBits, sizeof(VR512_with_sub_xmm_in_VR128HBits) },
  { VR512_with_sub_xmm_in_VR128L, VR512_with_sub_xmm_in_VR128LBits, sizeof(VR512_with_sub_xmm_in_VR128LBits) },
};

#endif // GET_REGINFO_MC_DESC
