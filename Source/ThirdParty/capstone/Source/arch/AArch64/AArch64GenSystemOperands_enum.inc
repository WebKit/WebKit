/* Capstone Disassembly Engine, http://www.capstone-engine.org */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

enum PStateValues {
  AArch64PState_SPSel = 5,
  AArch64PState_DAIFSet = 30,
  AArch64PState_DAIFClr = 31,
  AArch64PState_PAN = 4,
  AArch64PState_UAO = 3,
  AArch64PState_DIT = 26,
  AArch64PState_SSBS = 25,
  AArch64PState_TCO = 28,
};

enum ExactFPImmValues {
  AArch64ExactFPImm_zero = 0,
  AArch64ExactFPImm_half = 1,
  AArch64ExactFPImm_one = 2,
  AArch64ExactFPImm_two = 3,
};

