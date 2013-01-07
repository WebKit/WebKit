# Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

require "config"
require "ast"
require "opt"
require "risc"

class Node
    def armV7Single
        doubleOperand = armV7Operand
        raise "Bogus register name #{doubleOperand}" unless doubleOperand =~ /^d/
        "s" + ($~.post_match.to_i * 2).to_s
    end
end

class SpecialRegister
    def armV7Operand
        @name
    end
end

ARMv7_EXTRA_GPRS = [SpecialRegister.new("r9"), SpecialRegister.new("r8"), SpecialRegister.new("r3")]
ARMv7_EXTRA_FPRS = [SpecialRegister.new("d7")]
ARMv7_SCRATCH_FPR = SpecialRegister.new("d8")

def armV7MoveImmediate(value, register)
    # Currently we only handle the simple cases, and fall back to mov/movt for the complex ones.
    if value >= 0 && value < 256
        $asm.puts "movw #{register.armV7Operand}, \##{value}"
    elsif (~value) >= 0 && (~value) < 256
        $asm.puts "mvn #{register.armV7Operand}, \##{~value}"
    else
        $asm.puts "movw #{register.armV7Operand}, \##{value & 0xffff}"
        if (value & 0xffff0000) != 0
            $asm.puts "movt #{register.armV7Operand}, \##{(value >> 16) & 0xffff}"
        end
    end
end

class RegisterID
    def armV7Operand
        case name
        when "t0", "a0", "r0"
            "r0"
        when "t1", "a1", "r1"
            "r1"
        when "t2", "a2"
            "r2"
        when "a3"
            "r3"
        when "t3"
            "r4"
        when "t4"
            "r10"
        when "cfr"
            "r5"
        when "lr"
            "lr"
        when "sp"
            "sp"
        else
            raise "Bad register #{name} for ARMv7 at #{codeOriginString}"
        end
    end
end

class FPRegisterID
    def armV7Operand
        case name
        when "ft0", "fr"
            "d0"
        when "ft1"
            "d1"
        when "ft2"
            "d2"
        when "ft3"
            "d3"
        when "ft4"
            "d4"
        when "ft5"
            "d5"
        else
            raise "Bad register #{name} for ARMv7 at #{codeOriginString}"
        end
    end
end

class Immediate
    def armV7Operand
        raise "Invalid immediate #{value} at #{codeOriginString}" if value < 0 or value > 255
        "\##{value}"
    end
end

class Address
    def armV7Operand
        raise "Bad offset at #{codeOriginString}" if offset.value < -0xff or offset.value > 0xfff
        "[#{base.armV7Operand}, \##{offset.value}]"
    end
end

class BaseIndex
    def armV7Operand
        raise "Bad offset at #{codeOriginString}" if offset.value != 0
        "[#{base.armV7Operand}, #{index.armV7Operand}, lsl \##{scaleShift}]"
    end
end

class AbsoluteAddress
    def armV7Operand
        raise "Unconverted absolute address at #{codeOriginString}"
    end
end

#
# Lea support.
#

class Address
    def armV7EmitLea(destination)
        if destination == base
            $asm.puts "adds #{destination.armV7Operand}, \##{offset.value}"
        else
            $asm.puts "adds #{destination.armV7Operand}, #{base.armV7Operand}, \##{offset.value}"
        end
    end
end

class BaseIndex
    def armV7EmitLea(destination)
        raise "Malformed BaseIndex, offset should be zero at #{codeOriginString}" unless offset.value == 0
        $asm.puts "add.w #{destination.armV7Operand}, #{base.armV7Operand}, #{index.armV7Operand}, lsl \##{scaleShift}"
    end
end

# FIXME: we could support AbsoluteAddress for lea, but we don't.

#
# Actual lowering code follows.
#

class Sequence
    def getModifiedListARMv7
        result = @list
        result = riscLowerSimpleBranchOps(result)
        result = riscLowerHardBranchOps(result)
        result = riscLowerShiftOps(result)
        result = riscLowerMalformedAddresses(result) {
            | node, address |
            if address.is_a? BaseIndex
                address.offset.value == 0
            elsif address.is_a? Address
                (-0xff..0xfff).include? address.offset.value
            else
                false
            end
        }
        result = riscLowerMalformedAddressesDouble(result)
        result = riscLowerMisplacedImmediates(result)
        result = riscLowerMalformedImmediates(result, 0..0xff)
        result = riscLowerMisplacedAddresses(result)
        result = riscLowerRegisterReuse(result)
        result = assignRegistersToTemporaries(result, :gpr, ARMv7_EXTRA_GPRS)
        result = assignRegistersToTemporaries(result, :fpr, ARMv7_EXTRA_FPRS)
        return result
    end
end

def armV7Operands(operands)
    operands.map{|v| v.armV7Operand}.join(", ")
end

def armV7FlippedOperands(operands)
    armV7Operands([operands[-1]] + operands[0..-2])
end

def emitArmV7Compact(opcode2, opcode3, operands)
    if operands.size == 3
        $asm.puts "#{opcode3} #{armV7FlippedOperands(operands)}"
    else
        raise unless operands.size == 2
        raise unless operands[1].register?
        if operands[0].immediate?
            $asm.puts "#{opcode3} #{operands[1].armV7Operand}, #{operands[1].armV7Operand}, #{operands[0].armV7Operand}"
        else
            $asm.puts "#{opcode2} #{armV7FlippedOperands(operands)}"
        end
    end
end

def emitArmV7(opcode, operands)
    if operands.size == 3
        $asm.puts "#{opcode} #{armV7FlippedOperands(operands)}"
    else
        raise unless operands.size == 2
        $asm.puts "#{opcode} #{operands[1].armV7Operand}, #{operands[1].armV7Operand}, #{operands[0].armV7Operand}"
    end
end

def emitArmV7DoubleBranch(branchOpcode, operands)
    $asm.puts "vcmpe.f64 #{armV7Operands(operands[0..1])}"
    $asm.puts "vmrs apsr_nzcv, fpscr"
    $asm.puts "#{branchOpcode} #{operands[2].asmLabel}"
end

def emitArmV7Test(operands)
    value = operands[0]
    case operands.size
    when 2
        mask = Immediate.new(codeOrigin, -1)
    when 3
        mask = operands[1]
    else
        raise "Expected 2 or 3 operands but got #{operands.size} at #{codeOriginString}"
    end
    
    if mask.immediate? and mask.value == -1
        $asm.puts "tst #{value.armV7Operand}, #{value.armV7Operand}"
    elsif mask.immediate?
        $asm.puts "tst.w #{value.armV7Operand}, #{mask.armV7Operand}"
    else
        $asm.puts "tst #{value.armV7Operand}, #{mask.armV7Operand}"
    end
end

def emitArmV7Compare(operands, code)
    $asm.puts "movs #{operands[2].armV7Operand}, \#0"
    $asm.puts "cmp #{operands[0].armV7Operand}, #{operands[1].armV7Operand}"
    $asm.puts "it #{code}"
    $asm.puts "mov#{code} #{operands[2].armV7Operand}, \#1"
end

def emitArmV7TestSet(operands, code)
    $asm.puts "movs #{operands[-1].armV7Operand}, \#0"
    emitArmV7Test(operands)
    $asm.puts "it #{code}"
    $asm.puts "mov#{code} #{operands[-1].armV7Operand}, \#1"
end

class Instruction
    def lowerARMv7
        $asm.codeOrigin codeOriginString if $enableCodeOriginComments
        $asm.annotation annotation if $enableInstrAnnotations

        case opcode
        when "addi", "addp", "addis", "addps"
            if opcode == "addis" or opcode == "addps"
                suffix = "s"
            else
                suffix = ""
            end
            if operands.size == 3 and operands[0].immediate?
                raise unless operands[1].register?
                raise unless operands[2].register?
                if operands[0].value == 0 and suffix.empty?
                    unless operands[1] == operands[2]
                        $asm.puts "mov #{operands[2].armV7Operand}, #{operands[1].armV7Operand}"
                    end
                else
                    $asm.puts "adds #{operands[2].armV7Operand}, #{operands[1].armV7Operand}, #{operands[0].armV7Operand}"
                end
            elsif operands.size == 3 and operands[0].immediate?
                raise unless operands[1].register?
                raise unless operands[2].register?
                $asm.puts "adds #{armV7FlippedOperands(operands)}"
            else
                if operands[0].immediate?
                    unless Immediate.new(nil, 0) == operands[0]
                        $asm.puts "adds #{armV7FlippedOperands(operands)}"
                    end
                else
                    $asm.puts "add#{suffix} #{armV7FlippedOperands(operands)}"
                end
            end
        when "andi", "andp"
            emitArmV7Compact("ands", "and", operands)
        when "ori", "orp"
            emitArmV7Compact("orrs", "orr", operands)
        when "oris"
            emitArmV7Compact("orrs", "orrs", operands)
        when "xori", "xorp"
            emitArmV7Compact("eors", "eor", operands)
        when "lshifti", "lshiftp"
            emitArmV7Compact("lsls", "lsls", operands)
        when "rshifti", "rshiftp"
            emitArmV7Compact("asrs", "asrs", operands)
        when "urshifti", "urshiftp"
            emitArmV7Compact("lsrs", "lsrs", operands)
        when "muli", "mulp"
            emitArmV7("mul", operands)
        when "subi", "subp", "subis"
            emitArmV7Compact("subs", "subs", operands)
        when "negi", "negp"
            $asm.puts "rsbs #{operands[0].armV7Operand}, #{operands[0].armV7Operand}, \#0"
        when "noti"
            $asm.puts "mvns #{operands[0].armV7Operand}, #{operands[0].armV7Operand}"
        when "loadi", "loadis", "loadp"
            $asm.puts "ldr #{armV7FlippedOperands(operands)}"
        when "storei", "storep"
            $asm.puts "str #{armV7Operands(operands)}"
        when "loadb"
            $asm.puts "ldrb #{armV7FlippedOperands(operands)}"
        when "loadbs"
            $asm.puts "ldrsb.w #{armV7FlippedOperands(operands)}"
        when "storeb"
            $asm.puts "strb #{armV7Operands(operands)}"
        when "loadh"
            $asm.puts "ldrh #{armV7FlippedOperands(operands)}"
        when "loadhs"
            $asm.puts "ldrsh.w #{armV7FlippedOperands(operands)}"
        when "storeh"
            $asm.puts "strh #{armV7Operands(operands)}"
        when "loadd"
            $asm.puts "vldr.64 #{armV7FlippedOperands(operands)}"
        when "stored"
            $asm.puts "vstr.64 #{armV7Operands(operands)}"
        when "addd"
            emitArmV7("vadd.f64", operands)
        when "divd"
            emitArmV7("vdiv.f64", operands)
        when "subd"
            emitArmV7("vsub.f64", operands)
        when "muld"
            emitArmV7("vmul.f64", operands)
        when "sqrtd"
            $asm.puts "vsqrt.f64 #{armV7FlippedOperands(operands)}"
        when "ci2d"
            $asm.puts "vmov #{operands[1].armV7Single}, #{operands[0].armV7Operand}"
            $asm.puts "vcvt.f64.s32 #{operands[1].armV7Operand}, #{operands[1].armV7Single}"
        when "bdeq"
            emitArmV7DoubleBranch("beq", operands)
        when "bdneq"
            $asm.puts "vcmpe.f64 #{armV7Operands(operands[0..1])}"
            $asm.puts "vmrs apsr_nzcv, fpscr"
            isUnordered = LocalLabel.unique("bdneq")
            $asm.puts "bvs #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
            $asm.puts "bne #{operands[2].asmLabel}"
            isUnordered.lower("ARMv7")
        when "bdgt"
            emitArmV7DoubleBranch("bgt", operands)
        when "bdgteq"
            emitArmV7DoubleBranch("bge", operands)
        when "bdlt"
            emitArmV7DoubleBranch("bmi", operands)
        when "bdlteq"
            emitArmV7DoubleBranch("bls", operands)
        when "bdequn"
            $asm.puts "vcmpe.f64 #{armV7Operands(operands[0..1])}"
            $asm.puts "vmrs apsr_nzcv, fpscr"
            $asm.puts "bvs #{operands[2].asmLabel}"
            $asm.puts "beq #{operands[2].asmLabel}"
        when "bdnequn"
            emitArmV7DoubleBranch("bne", operands)
        when "bdgtun"
            emitArmV7DoubleBranch("bhi", operands)
        when "bdgtequn"
            emitArmV7DoubleBranch("bpl", operands)
        when "bdltun"
            emitArmV7DoubleBranch("blt", operands)
        when "bdltequn"
            emitArmV7DoubleBranch("ble", operands)
        when "btd2i"
            # FIXME: may be a good idea to just get rid of this instruction, since the interpreter
            # currently does not use it.
            raise "ARMv7 does not support this opcode yet, #{codeOrigin}"
        when "td2i"
            $asm.puts "vcvt.s32.f64 #{ARMv7_SCRATCH_FPR.armV7Single}, #{operands[0].armV7Operand}"
            $asm.puts "vmov #{operands[1].armV7Operand}, #{ARMv7_SCRATCH_FPR.armV7Single}"
        when "bcd2i"
            $asm.puts "vcvt.s32.f64 #{ARMv7_SCRATCH_FPR.armV7Single}, #{operands[0].armV7Operand}"
            $asm.puts "vmov #{operands[1].armV7Operand}, #{ARMv7_SCRATCH_FPR.armV7Single}"
            $asm.puts "vcvt.f64.s32 #{ARMv7_SCRATCH_FPR.armV7Operand}, #{ARMv7_SCRATCH_FPR.armV7Single}"
            emitArmV7DoubleBranch("bne", [ARMv7_SCRATCH_FPR, operands[0], operands[2]])
            $asm.puts "tst #{operands[1].armV7Operand}, #{operands[1].armV7Operand}"
            $asm.puts "beq #{operands[2].asmLabel}"
        when "movdz"
            # FIXME: either support this or remove it.
            raise "ARMv7 does not support this opcode yet, #{codeOrigin}"
        when "pop"
            $asm.puts "pop #{operands[0].armV7Operand}"
        when "push"
            $asm.puts "push #{operands[0].armV7Operand}"
        when "move"
            if operands[0].immediate?
                armV7MoveImmediate(operands[0].value, operands[1])
            else
                $asm.puts "mov #{armV7FlippedOperands(operands)}"
            end
        when "nop"
            $asm.puts "nop"
        when "bieq", "bpeq", "bbeq"
            if Immediate.new(nil, 0) == operands[0]
                $asm.puts "tst #{operands[1].armV7Operand}, #{operands[1].armV7Operand}"
            elsif Immediate.new(nil, 0) == operands[1]
                $asm.puts "tst #{operands[0].armV7Operand}, #{operands[0].armV7Operand}"
            else
                $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            end
            $asm.puts "beq #{operands[2].asmLabel}"
        when "bineq", "bpneq", "bbneq"
            if Immediate.new(nil, 0) == operands[0]
                $asm.puts "tst #{operands[1].armV7Operand}, #{operands[1].armV7Operand}"
            elsif Immediate.new(nil, 0) == operands[1]
                $asm.puts "tst #{operands[0].armV7Operand}, #{operands[0].armV7Operand}"
            else
                $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            end
            $asm.puts "bne #{operands[2].asmLabel}"
        when "bia", "bpa", "bba"
            $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            $asm.puts "bhi #{operands[2].asmLabel}"
        when "biaeq", "bpaeq", "bbaeq"
            $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            $asm.puts "bhs #{operands[2].asmLabel}"
        when "bib", "bpb", "bbb"
            $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            $asm.puts "blo #{operands[2].asmLabel}"
        when "bibeq", "bpbeq", "bbbeq"
            $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            $asm.puts "bls #{operands[2].asmLabel}"
        when "bigt", "bpgt", "bbgt"
            $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            $asm.puts "bgt #{operands[2].asmLabel}"
        when "bigteq", "bpgteq", "bbgteq"
            $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            $asm.puts "bge #{operands[2].asmLabel}"
        when "bilt", "bplt", "bblt"
            $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            $asm.puts "blt #{operands[2].asmLabel}"
        when "bilteq", "bplteq", "bblteq"
            $asm.puts "cmp #{armV7Operands(operands[0..1])}"
            $asm.puts "ble #{operands[2].asmLabel}"
        when "btiz", "btpz", "btbz"
            emitArmV7Test(operands)
            $asm.puts "beq #{operands[-1].asmLabel}"
        when "btinz", "btpnz", "btbnz"
            emitArmV7Test(operands)
            $asm.puts "bne #{operands[-1].asmLabel}"
        when "btis", "btps", "btbs"
            emitArmV7Test(operands)
            $asm.puts "bmi #{operands[-1].asmLabel}"
        when "jmp"
            if operands[0].label?
                $asm.puts "b #{operands[0].asmLabel}"
            else
                $asm.puts "mov pc, #{operands[0].armV7Operand}"
            end
        when "call"
            if operands[0].label?
                $asm.puts "blx #{operands[0].asmLabel}"
            else
                $asm.puts "blx #{operands[0].armV7Operand}"
            end
        when "break"
            $asm.puts "bkpt #0"
        when "ret"
            $asm.puts "bx lr"
        when "cieq", "cpeq", "cbeq"
            emitArmV7Compare(operands, "eq")
        when "cineq", "cpneq", "cbneq"
            emitArmV7Compare(operands, "ne")
        when "cia", "cpa", "cba"
            emitArmV7Compare(operands, "hi")
        when "ciaeq", "cpaeq", "cbaeq"
            emitArmV7Compare(operands, "hs")
        when "cib", "cpb", "cbb"
            emitArmV7Compare(operands, "lo")
        when "cibeq", "cpbeq", "cbbeq"
            emitArmV7Compare(operands, "ls")
        when "cigt", "cpgt", "cbgt"
            emitArmV7Compare(operands, "gt")
        when "cigteq", "cpgteq", "cbgteq"
            emitArmV7Compare(operands, "ge")
        when "cilt", "cplt", "cblt"
            emitArmV7Compare(operands, "lt")
        when "cilteq", "cplteq", "cblteq"
            emitArmV7Compare(operands, "le")
        when "tis", "tbs", "tps"
            emitArmV7TestSet(operands, "mi")
        when "tiz", "tbz", "tpz"
            emitArmV7TestSet(operands, "eq")
        when "tinz", "tbnz", "tpnz"
            emitArmV7TestSet(operands, "ne")
        when "peek"
            $asm.puts "ldr #{operands[1].armV7Operand}, [sp, \##{operands[0].value * 4}]"
        when "poke"
            $asm.puts "str #{operands[1].armV7Operand}, [sp, \##{operands[0].value * 4}]"
        when "fii2d"
            $asm.puts "vmov #{operands[2].armV7Operand}, #{operands[0].armV7Operand}, #{operands[1].armV7Operand}"
        when "fd2ii"
            $asm.puts "vmov #{operands[1].armV7Operand}, #{operands[2].armV7Operand}, #{operands[0].armV7Operand}"
        when "bo"
            $asm.puts "bvs #{operands[0].asmLabel}"
        when "bs"
            $asm.puts "bmi #{operands[0].asmLabel}"
        when "bz"
            $asm.puts "beq #{operands[0].asmLabel}"
        when "bnz"
            $asm.puts "bne #{operands[0].asmLabel}"
        when "leai", "leap"
            operands[0].armV7EmitLea(operands[1])
        when "smulli"
            raise "Wrong number of arguments to smull in #{self.inspect} at #{codeOriginString}" unless operands.length == 4
            $asm.puts "smull #{operands[2].armV7Operand}, #{operands[3].armV7Operand}, #{operands[0].armV7Operand}, #{operands[1].armV7Operand}"
        else
            lowerDefault
        end
    end
end

