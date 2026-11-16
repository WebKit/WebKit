//==-- TriCoreInstrFormats.td - TriCore Instruction Formats -*- tablegen -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Describe TriCore instructions format
//
//  CPU INSTRUCTION FORMATS
//
//  op1     - primary operation code
//  op2     - secondary operation code
//  s1      - source register 1
//  s2      - source register 2
//  s3      - source register 3
//  d       - destination register
//  b       - bit value
//  n       -
//            - multiplication result shift value (0b00 or 0b01)
//            - address shift value in add scale
//            - default to zero in all other operations using the RR format
//            - coprocessor number for coprocessor instructions
//  const[b=(4|9|16)]  - b bits immediate value
//  disp[b=(4|8|15|24)]  - b bits displacement value
//  off[b=(4|10|16)] - b bits offset value
//
//===----------------------------------------------------------------------===//

class InstTriCore<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {

  let Namespace = "TriCore";
  /// outs and ins are inherited from the instruction class.
  dag OutOperandList = outs;
  dag InOperandList  = ins;
  let AsmString   = asmstr;
  let Pattern = pattern;
}

// TriCore pseudo instructions format
class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
   : InstTriCore<outs, ins, asmstr, pattern> {
  let isPseudo = 1;
}

//===----------------------------------------------------------------------===//
// Generic 16-bit Instruction Format
//===----------------------------------------------------------------------===//
class T16<dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstTriCore<outs, ins, asmstr, pattern> {
  field bits<16> Inst;
  let Size = 2;
  field bits<16> SoftFail = 0;
}

//===----------------------------------------------------------------------===//
// Generic 32-bit Instruction Format
//===----------------------------------------------------------------------===//
class T32<dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstTriCore<outs, ins, asmstr, pattern> {
  field bits<32> Inst;
  let Size = 4;
  field bits<32> SoftFail = 0;
}

//===----------------------------------------------------------------------===//
// 16-bit SB Instruction Format: <disp8|op1>
//===----------------------------------------------------------------------===//
class SB<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
        : T16<outs, ins, asmstr, pattern> {

  bits<8> disp8;
  let Inst{15-8} = disp8;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSBInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SBC Instruction Format: <const4|disp4|op1>
//===----------------------------------------------------------------------===//
class SBC<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
        : T16<outs, ins, asmstr, pattern> {

  bits<4> const4;
  bits<4> disp4;

  let Inst{15-12} = const4;
  let Inst{11-8} = disp4;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSBCInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SBR Instruction Format: <s2|disp4|op1>
//===----------------------------------------------------------------------===//
class SBR<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
        : T16<outs, ins, asmstr, pattern> {

  bits<4> s2;
  bits<4> disp4;

  let Inst{15-12} = s2;
  let Inst{11-8} = disp4;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSBRInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SBRN Instruction Format: <n|disp4|op1>
//===----------------------------------------------------------------------===//
class SBRN<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
        : T16<outs, ins, asmstr, pattern> {

  bits<4> n;
  bits<4> disp4;

  let Inst{15-12} = n;
  let Inst{11-8} = disp4;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSBRNInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SC Instruction Format: <const8|op1>
//===----------------------------------------------------------------------===//
class SC<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
    : T16<outs, ins, asmstr, pattern> {

  bits<8> const8;

  let Inst{15-8} = const8;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSCInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SLR Instruction Format: <s2|d|op1>
//===----------------------------------------------------------------------===//
class SLR<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
    : T16<outs, ins, asmstr, pattern> {

  bits<4> s2;
  bits<4> d;

  let Inst{15-12} = s2;
  let Inst{11-8} = d;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSLRInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SLRO Instruction Format: <off4|d|op1>
//===----------------------------------------------------------------------===//
class SLRO<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
    : T16<outs, ins, asmstr, pattern> {

  bits<4> off4;
  bits<4> d;

  let Inst{15-12} = off4;
  let Inst{11-8} = d;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSLROInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SR Instruction Format: <op2|s1/d|op1>
//===----------------------------------------------------------------------===//
class SR<bits<8> op1, bits<4> op2, dag outs, dag ins, string asmstr,
      list<dag> pattern> : T16<outs, ins, asmstr, pattern> {

  bits<4> s1_d;

  let Inst{15-12} = op2;
  let Inst{11-8} = s1_d;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSRInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SRC Instruction Format: <const4|s1/d|op1>
//===----------------------------------------------------------------------===//
class SRC<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
    : T16<outs, ins, asmstr, pattern> {

  bits<4> const4;
  bits<4> s1_d;

  let Inst{15-12} = const4;
  let Inst{11-8} = s1_d;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSRCInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SRO Instruction Format: <s2|off4|op1>
//===----------------------------------------------------------------------===//
class SRO<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
    : T16<outs, ins, asmstr, pattern> {

  bits<4> s2;
  bits<4> off4;

  let Inst{15-12} = s2;
  let Inst{11-8} = off4;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSROInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SRR Instruction Format: <s2|s1/d|op1>
//===----------------------------------------------------------------------===//
class SRR<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
    : T16<outs, ins, asmstr, pattern> {

  bits<4> s2;
  bits<4> s1_d;

  let Inst{15-12} = s2;
  let Inst{11-8} = s1_d;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSRRInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SRRS Instruction Format: <s2|s1/d|n|op1>
//===----------------------------------------------------------------------===//
class SRRS<bits<6> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
    : T16<outs, ins, asmstr, pattern> {

  bits<4> s2;
  bits<4> s1_d;
  bits<2> n;

  let Inst{15-12} = s2;
  let Inst{11-8} = s1_d;
  let Inst{7-6} = n;
  let Inst{5-0} = op1;
  let DecoderMethod = "DecodeSRRSInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SSR Instruction Format: <s2|s1/d|op1>
//===----------------------------------------------------------------------===//
class SSR<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
    : T16<outs, ins, asmstr, pattern> {

  bits<4> s2;
  bits<4> s1;

  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSSRInstruction";
}

//===----------------------------------------------------------------------===//
// 16-bit SSRO Instruction Format: <s2|s1/d|op1>
//===----------------------------------------------------------------------===//
class SSRO<bits<8> op1, dag outs, dag ins, string asmstr, list<dag> pattern>
    : T16<outs, ins, asmstr, pattern> {

  bits<4> off4;
  bits<4> s1;

  let Inst{15-12} = off4;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSSROInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit ABS Instruction Format:
// <off18[9:6]|op2|off18[13:10]|off18[5:0]|off18[17:14]|s1/d|op1>
//===----------------------------------------------------------------------===//
class ABS<bits<8> op1, bits<2> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<18> off18;
  bits<4> s1_d;

  let Inst{31-28} = off18{9-6};
  let Inst{27-26} = op2;
  let Inst{25-22} = off18{13-10};
  let Inst{21-16} = off18{5-0};
  let Inst{15-12} = off18{17-14};
  let Inst{11-8} = s1_d;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeABSInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit ABSB Instruction Format:
// <off18[9:6]|op2|off18[13:10]|off18[5:0]|off18[17:14]|b|bpos3|op1>
//===----------------------------------------------------------------------===//
class ABSB<bits<8> op1, bits<2> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<18> off18;
  bits<1> b;
  bits<3> bpos3;

  let Inst{31-28} = off18{9-6};
  let Inst{27-26} = op2;
  let Inst{25-22} = off18{13-10};
  let Inst{21-16} = off18{5-0};
  let Inst{15-12} = off18{17-14};
  let Inst{11} = b;
  let Inst{10-8} = bpos3;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeABSBInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit B Instruction Format:
// <disp24[15:0]|disp24[23:16]|op1>
//===----------------------------------------------------------------------===//
class B<bits<8> op1, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<24> disp24;

  let Inst{31-16} = disp24{15-0};
  let Inst{15-8} = disp24{23-16};
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeBInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit BIT Instruction Format: <d|pos2|op2|pos1|s2|s1|op1>
//===----------------------------------------------------------------------===//
class BIT<bits<8> op1, bits<2> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<5> pos2;
  bits<5> pos1;
  bits<4> s2;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-23} = pos2;
  let Inst{22-21} = op2;
  let Inst{20-16} = pos1;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeBITInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit BO Instruction Format: <off10[9:6]|op2|off10[5:0]|s2|s1/d|op1>
//===----------------------------------------------------------------------===//
class BO<bits<8> op1, bits<6> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<10> off10;
  bits<4> s2;
  bits<4> s1_d;

  let Inst{31-28} = off10{9-6};
  let Inst{27-22} = op2;
  let Inst{21-16} = off10{5-0};
  let Inst{15-12} = s2;
  let Inst{11-8} = s1_d;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeBOInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit BOL Instruction Format:
// <off16[9:6]|off16[15:10]|off16[5:0]|s2|s1/d|op1>
//===----------------------------------------------------------------------===//
class BOL<bits<8> op1, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<16> off16;
  bits<4> s2;
  bits<4> s1_d;

  let Inst{31-28} = off16{9-6};
  let Inst{27-22} = off16{15-10};
  let Inst{21-16} = off16{5-0};
  let Inst{15-12} = s2;
  let Inst{11-8} = s1_d;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeBOLInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit BRC Instruction Format: <op2|disp15|const4|s1|op1>
//===----------------------------------------------------------------------===//
class BRC<bits<8> op1, bits<1> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<15> disp15;
  bits<4> const4;
  bits<4> s1;

  let Inst{31} = op2;
  let Inst{30-16} = disp15;
  let Inst{15-12} = const4;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeBRCInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit BRN Instruction Format: <op2|disp15|n[3:0]|s1|n[4]|op1>
//===----------------------------------------------------------------------===//
class BRN<bits<7> op1, bits<1> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<15> disp15;
  bits<5> n;
  bits<4> s1;

  let Inst{31} = op2;
  let Inst{30-16} = disp15;
  let Inst{15-12} = n{3-0};
  let Inst{11-8} = s1;
  let Inst{7} = n{4};
  let Inst{6-0} = op1;
  let DecoderMethod = "DecodeBRNInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit BRR Instruction Format: <op2|disp15|const4|s1|op1>
//===----------------------------------------------------------------------===//
class BRR<bits<8> op1, bits<1> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<15> disp15;
  bits<4> s2;
  bits<4> s1;

  let Inst{31} = op2;
  let Inst{30-16} = disp15;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeBRRInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RC Instruction Format: <d|op2|const9|s1|op1>
//===----------------------------------------------------------------------===//
class RC<bits<8> op1, bits<7> op2, dag outs, dag ins, string asmstr,
                  list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<9> const9;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-21} = op2;
  let Inst{20-12} = const9;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRCInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RCPW Instruction Format: <d|pos|op2|width|const4|s1|op1>
//===----------------------------------------------------------------------===//
class RCPW<bits<8> op1, bits<2> op2 , dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<5> pos;
  bits<5> width;
  bits<4> const4;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-23} = pos;
  let Inst{22-21} = op2;
  let Inst{20-16} = width;
  let Inst{15-12} = const4;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRCPWInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RCR Instruction Format: <d|s3|op2|const9|s1|op1>
//===----------------------------------------------------------------------===//
class RCR<bits<8> op1, bits<3> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<4> s3;
  bits<9> const9;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-24} = s3;
  let Inst{23-21} = op2;
  let Inst{20-12} = const9;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRCRInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RCRR Instruction Format: <d|s3|op2|-|const4|s1|op1>
//===----------------------------------------------------------------------===//
class RCRR<bits<8> op1, bits<3> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<4> s3;
  bits<4> const4;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-24} = s3;
  let Inst{23-21} = op2;
  let Inst{20-16} = 0;
  let Inst{15-12} = const4;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRCRRInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RCRW Instruction Format: <d|s3|op2|width|const4|s1|op1>
//===----------------------------------------------------------------------===//
class RCRW<bits<8> op1, bits<3> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<4> s3;
  bits<5> width;
  bits<4> const4;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-24} = s3;
  let Inst{23-21} = op2;
  let Inst{20-16} = width;
  let Inst{15-12} = const4;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRCRWInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RLC Instruction Format: <d|const16|s1|op1>
//===----------------------------------------------------------------------===//
class RLC<bits<8> op1, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<16> const16;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-12} = const16;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRLCInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RR Instruction Format: <d|op2|-|n|s2|s1|op1>
//===----------------------------------------------------------------------===//
class RR<bits<8> op1, bits<8> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<2> n;
  bits<4> s2;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-20} = op2;
  let Inst{19-18} = 0;
  let Inst{17-16} = n;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRRInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RR1 Instruction Format: <d|op2|n|s2|s1|op1>
//===----------------------------------------------------------------------===//
class RR1<bits<8> op1, bits<10> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<2> n;
  bits<4> s2;
  bits<4> s1;


  let Inst{31-28} = d;
  let Inst{27-18} = op2;
  let Inst{17-16} = n;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRR1Instruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RR2 Instruction Format: <d|op2|s2|s1|op1>
//===----------------------------------------------------------------------===//
class RR2<bits<8> op1, bits<12> op2, dag outs, dag ins, string asmstr,
                 list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<4> s2;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-16} = op2;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRR2Instruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RRPW Instruction Format: <d|pos|op2|width|s2|s1|op1>
//===----------------------------------------------------------------------===//
class RRPW<bits<8> op1, bits<2> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<5> pos;
  bits<5> width;
  bits<4> s2;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-23} = pos;
  let Inst{22-21} = op2;
  let Inst{20-16} = width;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRRPWInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RRR Instruction Format: <d|s3|op2|-|n|s2|s1|op1>
//===----------------------------------------------------------------------===//
class RRR<bits<8> op1, bits<4> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<4> s3;
  bits<2> n;
  bits<4> s2;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-24} = s3;
  let Inst{23-20} = op2;
  let Inst{19-18} = 0;
  let Inst{17-16} = n;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRRRInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RRR1 Instruction Format: <d|s3|op2|n|s2|s1|op1>
//===----------------------------------------------------------------------===//
class RRR1<bits<8> op1, bits<6> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<4> s3;
  bits<2> n;
  bits<4> s2;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-24} = s3;
  let Inst{23-18} = op2;
  let Inst{17-16} = n;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRRR1Instruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RRR2 Instruction Format: <d|s3|op2|s2|s1|op1>
//===----------------------------------------------------------------------===//
class RRR2<bits<8> op1, bits<8> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> s1;
  bits<4> s2;
  bits<4> s3;
  bits<4> d;

  let Inst{31-28} = d;
  let Inst{27-24} = s3;
  let Inst{23-16} = op2;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRRR2Instruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RRRR Instruction Format: <d|s3|op2|-|s2|s1|op1>
//===----------------------------------------------------------------------===//
class RRRR<bits<8> op1, bits<3> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<4> s3;
  bits<4> s2;
  bits<4> s1;

  let Inst{31-28} = d;
  let Inst{27-24} = s3;
  let Inst{23-21} = op2;
  let Inst{20-16} = 0;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRRRRInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit RRRW Instruction Format: <d|s3|op2|width|s2|s1|op1>
//===----------------------------------------------------------------------===//
class RRRW<bits<8> op1, bits<3> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> d;
  bits<4> s3;
  bits<5> width;
  bits<4> s2;
  bits<4> s1;  

  let Inst{31-28} = d;
  let Inst{27-24} = s3;
  let Inst{23-21} = op2;
  let Inst{20-16} = width;
  let Inst{15-12} = s2;
  let Inst{11-8} = s1;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeRRRWInstruction";
}

//===----------------------------------------------------------------------===//
// 32-bit SYS Instruction Format: <-|op2|-|s1/d|op1>
//===----------------------------------------------------------------------===//
class SYS<bits<8> op1, bits<6> op2, dag outs, dag ins, string asmstr,
                list<dag> pattern> : T32<outs, ins, asmstr, pattern> {

  bits<4> s1_d;

  let Inst{31-28} = 0;
  let Inst{27-22} = op2;
  let Inst{21-12} = 0;
  let Inst{11-8} = s1_d;
  let Inst{7-0} = op1;
  let DecoderMethod = "DecodeSYSInstruction";
}
