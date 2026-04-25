/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|* Subtarget Enumeration Source Fragment                                      *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

/* Capstone Disassembly Engine, http://www.capstone-engine.org */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2015 */


#ifdef GET_SUBTARGETINFO_ENUM
#undef GET_SUBTARGETINFO_ENUM

/*
  Make sure: 
  CS_MODE_RISCV64 = 0b11111
  CS_MODE_RISCV32 = 0b11110
*/

enum {
  RISCV_Feature64Bit   = 1ULL << 0,
  RISCV_FeatureStdExtA = 1ULL << 1,
  RISCV_FeatureStdExtC = 1ULL << 2,
  RISCV_FeatureStdExtD = 1ULL << 3,
  RISCV_FeatureStdExtF = 1ULL << 4,
  RISCV_FeatureStdExtM = 1ULL << 5,
  RISCV_FeatureRelax   = 1ULL << 6,
};

#endif // GET_SUBTARGETINFO_ENUM

