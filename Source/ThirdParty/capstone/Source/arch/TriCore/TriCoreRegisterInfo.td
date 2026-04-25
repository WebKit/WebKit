//==-- TriCoreRegisterInfo.td - TriCore Register defs ------*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the TriCore register file
//===----------------------------------------------------------------------===//

class TriCoreReg<string n, list<string> altNames = []> : Register<n, altNames> {
  field bits<16> Num;
  let Namespace = "TriCore";
  let HWEncoding = Num;
}

// General Purpose Data Registers
class TriCoreDataReg<bits<16> num, string n, list<string> altNames = []> : TriCoreReg<n, altNames> {
  let Num = num;
}

// General Purpose Address Registers
class TriCoreAddrReg<bits<16> num, string n, list<string> altNames = []> : TriCoreReg<n, altNames> {
  let Num = num;
}

// Program Status Register
class TriCorePSReg<bits<16> num, string n, list<string> altNames = []> : TriCoreReg<n, altNames> {
  let Num = num;
}

class TriCoreRegWithSubregs<bits<16> num, string n, list<Register> subregs>
: RegisterWithSubRegs<n, subregs> {
  field bits<16> Num;

  let Num = num;
  let Namespace = "TriCore";
}

//===----------------------------------------------------------------------===//
//@Registers
//===----------------------------------------------------------------------===//
// The register string, such as "d0" or "d13" will show on "llvm-objdump -d"

def D0 : TriCoreDataReg<0, "d0">, DwarfRegNum<[0]>;
def D1 : TriCoreDataReg<1, "d1">, DwarfRegNum<[1]>;
def D2 : TriCoreDataReg<2, "d2">, DwarfRegNum<[2]>;
def D3 : TriCoreDataReg<3, "d3">, DwarfRegNum<[3]>;
def D4 : TriCoreDataReg<4, "d4">, DwarfRegNum<[4]>;
def D5 : TriCoreDataReg<5, "d5">, DwarfRegNum<[5]>;
def D6 : TriCoreDataReg<6, "d6">, DwarfRegNum<[6]>;
def D7 : TriCoreDataReg<7, "d7">, DwarfRegNum<[7]>;
def D8 : TriCoreDataReg<8, "d8">, DwarfRegNum<[8]>;
def D9 : TriCoreDataReg<9, "d9">, DwarfRegNum<[9]>;
def D10 : TriCoreDataReg<10, "d10">, DwarfRegNum<[10]>;
def D11 : TriCoreDataReg<11, "d11">, DwarfRegNum<[11]>;
def D12 : TriCoreDataReg<12, "d12">, DwarfRegNum<[12]>;
def D13 : TriCoreDataReg<13, "d13">, DwarfRegNum<[13]>;
def D14 : TriCoreDataReg<14, "d14">, DwarfRegNum<[14]>;
def D15 : TriCoreDataReg<15, "d15">, DwarfRegNum<[15]>;


def A0 : TriCoreAddrReg<0, "a0">, DwarfRegNum<[16]>;
def A1 : TriCoreAddrReg<1, "a1">, DwarfRegNum<[17]>;
def A2 : TriCoreAddrReg<2, "a2">, DwarfRegNum<[18]>;
def A3 : TriCoreAddrReg<3, "a3">, DwarfRegNum<[19]>;
def A4 : TriCoreAddrReg<4, "a4">, DwarfRegNum<[20]>;
def A5 : TriCoreAddrReg<5, "a5">, DwarfRegNum<[21]>;
def A6 : TriCoreAddrReg<6, "a6">, DwarfRegNum<[22]>;
def A7 : TriCoreAddrReg<7, "a7">, DwarfRegNum<[23]>;
def A8 : TriCoreAddrReg<8, "a8">, DwarfRegNum<[24]>;
def A9 : TriCoreAddrReg<9, "a9">, DwarfRegNum<[25]>;
def A10 : TriCoreAddrReg<10, "sp", ["a10"]>, DwarfRegNum<[26]>;
def A11 : TriCoreAddrReg<11, "a11">, DwarfRegNum<[27]>;
def A12 : TriCoreAddrReg<12, "a12">, DwarfRegNum<[28]>;
def A13 : TriCoreAddrReg<13, "a13">, DwarfRegNum<[29]>;
def A14 : TriCoreAddrReg<14, "a14">, DwarfRegNum<[30]>;
def A15 : TriCoreAddrReg<15, "a15">, DwarfRegNum<[31]>;

let Namespace = "TriCore" in {
def subreg_even : SubRegIndex<32>;
def subreg_odd  : SubRegIndex<32, 32>;
}

//Extended 64-bit registers
let SubRegIndices = [subreg_even, subreg_odd] in {
def E0 : TriCoreRegWithSubregs<0,   "e0",  [D0,D1]   >, DwarfRegNum<[32]>;
def E2 : TriCoreRegWithSubregs<2,   "e2",  [D2,D3]   >, DwarfRegNum<[33]>;
def E4 : TriCoreRegWithSubregs<4,   "e4",  [D4,D5]   >, DwarfRegNum<[34]>;
def E6 : TriCoreRegWithSubregs<6,   "e6",  [D6,D7]   >, DwarfRegNum<[35]>;
def E8 : TriCoreRegWithSubregs<8,   "e8",  [D8,D9]   >, DwarfRegNum<[36]>;
def E10 : TriCoreRegWithSubregs<10, "e10", [D10,D11] >, DwarfRegNum<[37]>;
def E12 : TriCoreRegWithSubregs<12, "e12", [D12,D13] >, DwarfRegNum<[38]>;
def E14 : TriCoreRegWithSubregs<14, "e14", [D14,D15] >, DwarfRegNum<[39]>;
}

let SubRegIndices = [subreg_even, subreg_odd] in {
def P0 : TriCoreRegWithSubregs<0,   "p0",  [A0,A1]   >, DwarfRegNum<[40]>;
def P2 : TriCoreRegWithSubregs<2,   "p2",  [A2,A3]   >, DwarfRegNum<[41]>;
def P4 : TriCoreRegWithSubregs<4,   "p4",  [A4,A5]   >, DwarfRegNum<[42]>;
def P6 : TriCoreRegWithSubregs<6,   "p6",  [A6,A7]   >, DwarfRegNum<[43]>;
def P8 : TriCoreRegWithSubregs<8,   "p8",  [A8,A9]   >, DwarfRegNum<[44]>;
def P10 : TriCoreRegWithSubregs<10, "p10", [A10,A11] >, DwarfRegNum<[45]>;
def P12 : TriCoreRegWithSubregs<12, "p12", [A12,A13] >, DwarfRegNum<[46]>;
def P14 : TriCoreRegWithSubregs<14, "p14", [A14,A15] >, DwarfRegNum<[47]>;
}

//Program Status Information Registers
def PSW  : TriCorePSReg<0, "psw">, DwarfRegNum<[40]>;
def PCXI : TriCorePSReg<1, "pcxi">, DwarfRegNum<[41]>;
def PC   : TriCorePSReg<2, "pc">, DwarfRegNum<[42]>;
def FCX  : TriCorePSReg<3, "fcx">, DwarfRegNum<[43]>;

//===----------------------------------------------------------------------===//
//@Register Classes
//===----------------------------------------------------------------------===//

def RD : RegisterClass<"TriCore", [i32], 32, (add
    D0,  D1,  D2,  D3,  D4,
    D5,  D6,  D7,  D8,  D9,
    D10, D11, D12, D13, D14,
    D15)>;

def RA : RegisterClass<"TriCore", [i32], 32, (add
    A0,  A1,  A2,  A3,  A4,
    A5,  A6,  A7,  A8,  A9,
    A10, A11, A12, A13, A14,
    A15)>;

def RE : RegisterClass<"TriCore", [i64], 64, (add
    E0, E2, E4,
    E6, E8, E10,
    E12, E14)>;

def RP : RegisterClass<"TriCore", [i64], 64, (add
    P0, P2, P4,
    P6, P8, P10,
    P12, P14)>;

def PSRegs : RegisterClass<"TriCore", [i32], 32, (add
    PSW, PCXI, PC, FCX)>;

def TuplesPairAddrRegs : RegisterTuples<[subreg_even, subreg_odd],
                                        [(add A0,A2,A4,A6,A8,A10,A12,A14),
                                        (add A1,A3,A5,A7,A9,A11,A13,A15)]>;

def PairAddrRegs : RegisterClass<"TriCore", [i64], 64,
                                (add TuplesPairAddrRegs)> {
    let Size = 64;
}
