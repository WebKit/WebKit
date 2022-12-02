# Copyright (C) 2011-2022 Apple Inc. All rights reserved.
# Copyright (C) 2014 University of Szeged. All rights reserved.
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

require "ast"
require "opt"
require "risc"

# Naming conventions:
#
# x<number>  => GPR. This is both the generic name of the register, and the name used
#               to indicate that the register is used in 64-bit mode.
# w<number>  => GPR in 32-bit mode. This is the low 32-bits of the GPR. If it is
#               mutated then the high 32-bit part of the register is zero filled.
# q<number>  => FPR. This is the generic name of the register.
# d<number>  => FPR used as an IEEE 64-bit binary floating point number (i.e. double).
#
# GPR conventions, to match the baseline JIT:
#
#  x0  => t0, a0, wa0, r0
#  x1  => t1, a1, wa1, r1
#  x2  => t2, a2, wa2
#  x3  => t3, a3, wa3
#  x4  => t4, a4, wa4
#  x5  => t5, a5, wa5
#  x6  => t6, a6, wa6
#  x7  => t7, a7, wa7
#  x9  => ws0
# x10  => ws1
# x13  =>                  (scratch)
# x16  =>                  (scratch)
# x17  =>                  (scratch)
# x25  =>             csr6 (metadataTable)
# x26  =>             csr7 (PB)
# x27  =>             csr8 (numberTag)
# x28  =>             csr9 (notCellMask)
# x29  => cfr
#  sp  => sp
#  lr  => lr
#
# FPR conventions, to match the baseline JIT:
#
#  q0  => ft0, fa0, wfa0, fr
#  q1  => ft1, fa1, wfa1
#  q2  => ft2, fa2, wfa2
#  q3  => ft3, fa3, wfa3
#  q4  => ft4,      wfa4          (unused in baseline)
#  q5  => ft5,      wfa5          (unused in baseline)
#  q6  =>           wfa6          (unused in baseline)
#  q7  =>           wfa7          (unused in baseline)
#  q8  => csfr0                   (Only the lower 64 bits)
#  q9  => csfr1                   (Only the lower 64 bits)
# q10  => csfr2                   (Only the lower 64 bits)
# q11  => csfr3                   (Only the lower 64 bits)
# q12  => csfr4                   (Only the lower 64 bits)
# q13  => csfr5                   (Only the lower 64 bits)
# q14  => csfr6                   (Only the lower 64 bits)
# q15  => csfr7                   (Only the lower 64 bits)
# q31  => scratch

def arm64GPRName(name, kind)
    raise "bad GPR name #{name}" unless name =~ /^x/
    number = name[1..-1]
    case kind
    when :word
        "w" + number
    when :ptr
        prefix = $currentSettings["ADDRESS64"] ? "x" : "w"
        prefix + number
    when :quad
        "x" + number
    else
        raise "Wrong kind: #{kind}"
    end
end

def arm64FPRName(name, kind)
    raise "bad FPR name #{name}" unless name =~ /^q/
    case kind
    when :double
        "d" + name[1..-1]
    when :float
        "s" + name[1..-1]
    when :vector
        "v" + name[1..-1]
    when :vector_with_interpretation
        "q" + name[1..-1]
    else
        raise "bad FPR kind #{kind}"
    end
end

class SpecialRegister
    def arm64Operand(kind)
        case @name
        when /^x/
            arm64GPRName(@name, kind)
        when /^q/
            arm64FPRName(@name, kind)
        else
            raise "Bad name: #{@name}"
        end
    end
end

ARM64_EXTRA_GPRS = [SpecialRegister.new("x16"), SpecialRegister.new("x17"), SpecialRegister.new("x13")]
ARM64_EXTRA_FPRS = [SpecialRegister.new("q31")]

class RegisterID
    def arm64Operand(kind)
        case @name
        when 't0', 'a0', 'r0', 'wa0'
            arm64GPRName('x0', kind)
        when 't1', 'a1', 'r1', 'wa1'
            arm64GPRName('x1', kind)
        when 't2', 'a2', 'wa2'
            arm64GPRName('x2', kind)
        when 't3', 'a3', 'wa3'
            arm64GPRName('x3', kind)
        when 't4', 'a4', 'wa4'
            arm64GPRName('x4', kind)
        when 't5', 'a5', 'wa5'
          arm64GPRName('x5', kind)
        when 't6', 'a6', 'wa6'
          arm64GPRName('x6', kind)
        when 't7', 'a7', 'wa7'
          arm64GPRName('x7', kind)
        when 'ws0'
          arm64GPRName('x9', kind)
        when 'ws1'
          arm64GPRName('x10', kind)
        when 'cfr'
            arm64GPRName('x29', kind)
        when 'csr0'
            arm64GPRName('x19', kind)
        when 'csr1'
            arm64GPRName('x20', kind)
        when 'csr2'
            arm64GPRName('x21', kind)
        when 'csr3'
            arm64GPRName('x22', kind)
        when 'csr4'
            arm64GPRName('x23', kind)
        when 'csr5'
            arm64GPRName('x24', kind)
        when 'csr6'
            arm64GPRName('x25', kind)
        when 'csr7'
            arm64GPRName('x26', kind)
        when 'csr8'
            arm64GPRName('x27', kind)
        when 'csr9'
            arm64GPRName('x28', kind)
        when 'sp'
            'sp'
        when 'lr'
            'x30'
        else
            raise "Bad register name #{@name} at #{codeOriginString}"
        end
    end
end

class FPRegisterID
    def arm64Operand(kind)
        case @name
        when 'ft0', 'fr', 'fa0', 'wfa0'
            arm64FPRName('q0', kind)
        when 'ft1', 'fa1', 'wfa1'
            arm64FPRName('q1', kind)
        when 'ft2', 'fa2', 'wfa2'
            arm64FPRName('q2', kind)
        when 'ft3', 'fa3', 'wfa3'
            arm64FPRName('q3', kind)
        when 'ft4', 'wfa4'
            arm64FPRName('q4', kind)
        when 'ft5', 'wfa5'
            arm64FPRName('q5', kind)
        when 'wfa6'
            arm64FPRName('q6', kind)
        when 'wfa7'
            arm64FPRName('q7', kind)
        when 'csfr0'
            arm64FPRName('q8', kind)
        when 'csfr1'
            arm64FPRName('q9', kind)
        when 'csfr2'
            arm64FPRName('q10', kind)
        when 'csfr3'
            arm64FPRName('q11', kind)
        when 'csfr4'
            arm64FPRName('q12', kind)
        when 'csfr5'
            arm64FPRName('q13', kind)
        when 'csfr6'
            arm64FPRName('q14', kind)
        when 'csfr7'
            arm64FPRName('q15', kind)
        else "Bad register name #{@name} at #{codeOriginString}"
        end
    end
end

class Immediate
    def arm64Operand(kind)
        "\##{value}"
    end
end

class Address
    def arm64Operand(kind)
        case kind
        when :quad, :ptr, :double
            if $currentSettings["ADDRESS64"]
                raise "Invalid offset #{offset.value} at #{codeOriginString}" if offset.value < -255 or offset.value > 32760
            else
                raise "Invalid offset #{offset.value} at #{codeOriginString}" if offset.value < -255 or offset.value > 16380
            end
        when :word, :int, :float
            raise "Invalid offset #{offset.value} at #{codeOriginString}" if offset.value < -255 or offset.value > 16380
        else
            raise "Invalid offset #{offset.value} at #{codeOriginString}" if offset.value < -255 or offset.value > 4095
        end
        offset.value.zero? \
            ? "[#{base.arm64Operand(:quad)}]"
            : "[#{base.arm64Operand(:quad)}, \##{offset.value}]"
    end

    def arm64SimpleAddressOperand(kind)
        raise "Invalid offset #{offset.value} at #{codeOriginString}" if offset.value != 0
        "[#{base.arm64Operand(:quad)}]"
    end
    
    def arm64PairAddressOperand(kind)
        raise "Invalid offset #{offset.value} at #{codeOriginString}" if offset.value < -512 or offset.value > 504
        "[#{base.arm64Operand(:quad)}, \##{offset.value}]"
    end

    def arm64EmitLea(destination, kind)
        offset.value.zero? \
            ? ($asm.puts "mov #{destination.arm64Operand(kind)}, #{base.arm64Operand(kind)}")
            : ($asm.puts "add #{destination.arm64Operand(kind)}, #{base.arm64Operand(kind)}, \##{offset.value}")
    end
end

class BaseIndex
    def arm64Operand(kind)
        raise "Invalid offset #{offset.value} at #{codeOriginString}" if offset.value != 0
        scaleShift.zero? \
            ? "[#{base.arm64Operand(:quad)}, #{index.arm64Operand(:quad)}]"
            : "[#{base.arm64Operand(:quad)}, #{index.arm64Operand(:quad)}, lsl \##{scaleShift}]"
    end

    def arm64EmitLea(destination, kind)
        scaleShift.zero? \
            ? ($asm.puts "add #{destination.arm64Operand(kind)}, #{base.arm64Operand(kind)}, #{index.arm64Operand(kind)}")
            : ($asm.puts "add #{destination.arm64Operand(kind)}, #{base.arm64Operand(kind)}, #{index.arm64Operand(kind)}, lsl \##{scaleShift}")
    end

    def arm64PairAddressOperand(kind)
        raise "Unconverted base index address #{address.value} at #{codeOriginString}"
    end
end

class AbsoluteAddress
    def arm64Operand(kind)
        raise "Unconverted absolute address #{address.value} at #{codeOriginString}"
    end
    def arm64PairAddressOperand(kind)
        raise "Unconverted absolute address #{address.value} at #{codeOriginString}"
    end
end

# FIXME: We could support AbsoluteAddress for lea, but we don't.

#
# Actual lowering code follows.
#

def isMalformedArm64LoadStoreAddress(opcode, operand)
    malformed = false
    if operand.is_a? Address
        case opcode
        when "loadp", "storep", "loadq", "storeq"
            if $currentSettings["ADDRESS64"]
                malformed ||= (not (-255..32760).include? operand.offset.value)
                malformed ||= (not (operand.offset.value % 8).zero?)
            else
                malformed ||= (not (-255..16380).include? operand.offset.value)
            end
        when "loadd", "stored"
            malformed ||= (not (-255..32760).include? operand.offset.value)
            malformed ||= (not (operand.offset.value % 8).zero?)
        when "loadi", "loadis", "storei"
            malformed ||= (not (-255..16380).include? operand.offset.value)
        else
            # This is just a conservative estimate of the max offset.
            malformed ||= (not (-255..4095).include? operand.offset.value)
        end
    end
    malformed
end

def isMalformedArm64LoadStorePairAddress(opcode, operand)
    malformed = false
    if operand.is_a? Address
        malformed ||= (not (-512..504).include? operand.offset.value)
        malformed ||= (not (operand.offset.value % 8).zero?)
    end
    malformed
end

def arm64LowerMalformedLoadStoreAddresses(list)
    newList = []

    list.each {
        | node |
        if node.is_a? Instruction
            if node.opcode =~ /^storepair/
                if isMalformedArm64LoadStorePairAddress(node.opcode, node.operands[2])
                    address = node.operands[2]
                    tmp = Tmp.new(codeOrigin, :gpr)
                    newList << Instruction.new(node.codeOrigin, "move", [address.offset, tmp])
                    newList << Instruction.new(node.codeOrigin, node.opcode, [node.operands[0], node.operands[1], BaseIndex.new(node.codeOrigin, address.base, tmp, Immediate.new(codeOrigin, 1), Immediate.new(codeOrigin, 0))], node.annotation)
                else
                    newList << node
                end
            elsif node.opcode =~ /^loadpair/
                if isMalformedArm64LoadStorePairAddress(node.opcode, node.operands[0])
                    address = node.operands[0]
                    tmp = Tmp.new(codeOrigin, :gpr)
                    newList << Instruction.new(node.codeOrigin, "move", [address.offset, tmp])
                    newList << Instruction.new(node.codeOrigin, node.opcode, [BaseIndex.new(node.codeOrigin, address.base, tmp, Immediate.new(codeOrigin, 1), Immediate.new(codeOrigin, 0)), node.operands[1], node.operands[2]], node.annotation)
                else
                    newList << node
                end
            elsif node.opcode =~ /^store/ and isMalformedArm64LoadStoreAddress(node.opcode, node.operands[1])
                address = node.operands[1]
                tmp = Tmp.new(codeOrigin, :gpr)
                newList << Instruction.new(node.codeOrigin, "move", [address.offset, tmp])
                newList << Instruction.new(node.codeOrigin, node.opcode, [node.operands[0], BaseIndex.new(node.codeOrigin, address.base, tmp, Immediate.new(codeOrigin, 1), Immediate.new(codeOrigin, 0))], node.annotation)
            elsif node.opcode =~ /^load/ and isMalformedArm64LoadStoreAddress(node.opcode, node.operands[0])
                address = node.operands[0]
                tmp = Tmp.new(codeOrigin, :gpr)
                newList << Instruction.new(node.codeOrigin, "move", [address.offset, tmp])
                newList << Instruction.new(node.codeOrigin, node.opcode, [BaseIndex.new(node.codeOrigin, address.base, tmp, Immediate.new(codeOrigin, 1), Immediate.new(codeOrigin, 0)), node.operands[1]], node.annotation)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def arm64LowerLabelReferences(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "loadi", "loadis", "loadp", "loadq", "loadb", "loadbsi", "loadbsq", "loadh", "loadhsi", "loadhsq", "leap", "loadv"
                labelRef = node.operands[0]
                if labelRef.is_a? LabelReference
                    dest = node.operands[1]
                    newList << Instruction.new(codeOrigin, "globaladdr", [LabelReference.new(node.codeOrigin, labelRef.label), dest])
                    if node.opcode != "leap" or labelRef.offset != 0
                        newList << Instruction.new(codeOrigin, node.opcode, [Address.new(node.codeOrigin, dest, Immediate.new(node.codeOrigin, labelRef.offset)), dest])
                    end
                else
                    newList << node
                end
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def arm64FixSpecialRegisterArithmeticMode(list)
    newList = []
    def usesSpecialRegister(node)
        node.children.any? {
            |operand|
            if operand.is_a? RegisterID and operand.name =~ /sp/
                true
            elsif operand.is_a? Address or operand.is_a? BaseIndex
                usesSpecialRegister(operand)
            else
                false
            end
        }
    end


    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "addp", "subp", "mulp", "divp", "leap"
                if not $currentSettings["ADDRESS64"] and usesSpecialRegister(node)
                    newOpcode = node.opcode.sub(/(.*)p/, '\1q')
                    node = Instruction.new(node.codeOrigin, newOpcode, node.operands, node.annotation)
                end
            when /^bp/
                if not $currentSettings["ADDRESS64"] and usesSpecialRegister(node)
                    newOpcode = node.opcode.sub(/^bp(.*)/, 'bq\1')
                    node = Instruction.new(node.codeOrigin, newOpcode, node.operands, node.annotation)
                end
            end
        end
        newList << node
    }
    newList
end

class Sequence
    def getModifiedListARM64(result = @list)
        result = riscLowerNot(result)
        result = riscLowerSimpleBranchOps(result)

        result = $currentSettings["ADDRESS64"] ? riscLowerHardBranchOps64(result) : riscLowerHardBranchOps(result)
        result = riscLowerShiftOps(result)
        result = arm64LowerMalformedLoadStoreAddresses(result)
        result = arm64LowerLabelReferences(result)
        result = riscLowerMalformedAddresses(result) {
            | node, address |
            isLoadStorePairOp = false
            case node.opcode
            when "loadb", "loadbsi", "loadbsq", "storeb", /^bb/, /^btb/, /^cb/, /^tb/, "loadlinkacqb", "storecondrelb", /^atomic[a-z]+b$/
                size = 1
            when "loadh", "loadhsi", "loadhsq", "orh", "storeh", "loadlinkacqh", "storecondrelh", /^atomic[a-z]+h$/
                size = 2
            when "loadi", "loadis", "storei", "addi", "andi", "lshifti", "muli", "negi",
                "noti", "ori", "rshifti", "urshifti", "subi", "xori", /^bi/, /^bti/,
                /^ci/, /^ti/, "addis", "subis", "mulis", "smulli", "leai", "loadf", "storef", "loadlinkacqi", "storecondreli",
                /^atomic[a-z]+i$/
                size = 4
            when "loadp", "storep", "loadq", "storeq", "loadd", "stored", "lshiftp", "lshiftq", "negp", "negq", "rshiftp", "rshiftq",
                "urshiftp", "urshiftq", "addp", "addq", "mulp", "mulq", "andp", "andq", "orp", "orq", "subp", "subq", "xorp", "xorq", "addd",
                "divd", "subd", "muld", "sqrtd", /^bp/, /^bq/, /^btp/, /^btq/, /^cp/, /^cq/, /^tp/, /^tq/, /^bd/,
                "jmp", "call", "leap", "leaq", "loadlinkacqq", "storecondrelq", /^atomic[a-z]+q$/, "loadv", "storev"
                size = $currentSettings["ADDRESS64"] ? 8 : 4
            when "loadpairq", "storepairq", "loadpaird", "storepaird"
                size = 16
                isLoadStorePairOp = true
            else
                raise "Bad instruction #{node.opcode} for heap access at #{node.codeOriginString}: #{node.dump}"
            end
            
            if address.is_a? BaseIndex
                address.offset.value == 0 and
                    (node.opcode =~ /^lea/ or address.scale == 1 or address.scale == size)
            elsif address.is_a? Address
                if isLoadStorePairOp
                    not isMalformedArm64LoadStorePairAddress(node.opcode, address)
                else
                    not isMalformedArm64LoadStoreAddress(node.opcode, address)
                end
            else
                false
            end
        }

        result = riscLowerMisplacedImmediates(result, ["storeb", "storeh", "storei", "storep", "storeq"])

        # The rules for which immediates are valid for and/or/xor instructions are fairly involved, see https://dinfuehr.github.io/blog/encoding-of-immediate-values-on-aarch64/
        validLogicalImmediates = []
        def rotate(value, n, size)
            mask = (1 << size) - 1
            shiftedValue = value << n
            result = (shiftedValue & mask) | ((shiftedValue & ~mask) >> size)
            return result
        end
        def replicate(value, size)
            until size == 64 do
                value = value | (value << size)
                size *= 2
            end
            return value
        end
        size = 2
        until size > 64 do
            for numberOfOnes in 1..(size-1) do
                for rotation in 0..(size-1) do
                    immediate = 0;
                    for i in 0..(numberOfOnes-1) do
                        immediate = immediate*2 + 1
                    end
                    immediate = rotate(immediate, rotation, size)
                    immediate = replicate(immediate, size)
                    validLogicalImmediates << immediate
                end
            end
            size *= 2
        end
        result = riscLowerMalformedImmediates(result, 0..4095, validLogicalImmediates)

        result = riscLowerMisplacedAddresses(result)
        result = riscLowerMalformedAddresses(result) {
            | node, address |
            case node.opcode
            when /^loadpair/, /^storepair/
#                not (address.is_a? Address and not (-512..504).include? address.offset.value)
                not address.is_a? Address or not isMalformedArm64LoadStorePairAddress(node.opcode, address)
            when /^load/, /^store/
                not address.is_a? Address or not isMalformedArm64LoadStoreAddress(node.opcode, address)
            when /^lea/
                true
            when /^atomic/
                true
            else
                raise "Bad instruction #{node.opcode} for heap access at #{node.codeOriginString}"
            end
        }
        result = riscLowerTest(result)
        result = arm64FixSpecialRegisterArithmeticMode(result)
        result = assignRegistersToTemporaries(result, :gpr, ARM64_EXTRA_GPRS)
        result = assignRegistersToTemporaries(result, :fpr, ARM64_EXTRA_FPRS)
        return result
    end
end

def arm64Operands(operands, kinds)
    if kinds.is_a? Array
        raise "Mismatched operand lists: #{operands.inspect} and #{kinds.inspect}" if operands.size != kinds.size
    else
        kinds = operands.map{ kinds }
    end
    (0...operands.size).map {
        | index |
        operands[index].arm64Operand(kinds[index])
    }.join(', ')
end

def arm64FlippedOperands(operands, kinds)
    if kinds.is_a? Array
        kinds = [kinds[-1]] + kinds[0..-2]
    end
    arm64Operands([operands[-1]] + operands[0..-2], kinds)
end

# TAC = three address code.
def arm64TACOperands(operands, kind)
    if operands.size == 3
        return arm64FlippedOperands(operands, kind)
    end
    
    raise unless operands.size == 2
    
    return operands[1].arm64Operand(kind) + ", " + arm64FlippedOperands(operands, kind)
end

def emitARM64Add(opcode, operands, kind)
    if operands.size == 3
        raise unless operands[1].register?
        raise unless operands[2].register?
        
        if operands[0].immediate?
            if operands[0].value == 0 and opcode !~ /s$/
                if operands[1] != operands[2]
                    $asm.puts "mov #{arm64FlippedOperands(operands[1..2], kind)}"
                end
            else
                $asm.puts "#{opcode} #{arm64Operands(operands.reverse, kind)}"
            end
            return
        end
        
        raise unless operands[0].register?
        $asm.puts "#{opcode} #{arm64FlippedOperands(operands, kind)}"
        return
    end
    
    raise unless operands.size == 2
    
    if operands[0].immediate? and operands[0].value == 0 and opcode !~ /s$/
        return
    end
    
    $asm.puts "#{opcode} #{arm64TACOperands(operands, kind)}"
end

def emitARM64Mul(opcode, operands, kind)
    if operands.size == 2 and operands[0].is_a? Immediate
        imm = operands[0].value
        if imm > 0 and isPowerOfTwo(imm)
            emitARM64LShift([Immediate.new(nil, Math.log2(imm).to_i), operands[1]], kind)
            return
        end
    end

    $asm.puts "madd #{arm64TACOperands(operands, kind)}, #{arm64GPRName('xzr', kind)}"
end

def emitARM64Sub(opcode, operands, kind)
    if operands.size == 3
        raise unless operands[0].register?
        raise unless operands[2].register?

        if operands[1].immediate?
            if operands[1].value == 0 and opcode !~ /s$/
                if operands[0] != operands[2]
                    $asm.puts "mov #{arm64FlippedOperands([operands[0], operands[2]], kind)}"
                end
                return
            end
        end
    end

    if operands.size == 2
        if operands[0].immediate? and operands[0].value == 0 and opcode !~ /s$/
            return
        end
    end

    emitARM64TAC(opcode, operands, kind)
end

def emitARM64Unflipped(opcode, operands, kind)
    $asm.puts "#{opcode} #{arm64Operands(operands, kind)}"
end

def emitARM64TAC(opcode, operands, kind)
    $asm.puts "#{opcode} #{arm64TACOperands(operands, kind)}"
end

def emitARM64Div(opcode, operands, kind)
    if operands.size == 2
        operands = [operands[1], operands[1], operands[0]]
    elsif operands.size == 3
        operands = [operands[2], operands[1], operands[0]]
    else
        raise
    end
    $asm.puts "#{opcode} #{arm64Operands(operands, kind)}"
end

def emitARM64TACWithOperandSuffix(opcode, operands, kind)
    raise unless [:float, :double].include? kind
    size = kind == :float ? 8 : 16
    operands = operands.map { |operand|
        raise unless operand.is_a? FPRegisterID
        "#{operand.arm64Operand(:vector)}.#{size}b"
    }
    if operands.length == 2
      operands = [operands[1], operands[1], operands[0]]
    else
      raise unless operands.length == 3
      operands = [operands[2], operands[0], operands[1]]
    end
    $asm.puts "#{opcode} #{operands.join(", ")}"
end

def emitARM64(opcode, operands, kind)
    $asm.puts "#{opcode} #{arm64FlippedOperands(operands, kind)}"
end

def emitARM64Access(opcode, opcodeNegativeOffset, register, memory, kind)
    if memory.is_a? Address and memory.offset.value < 0
        raise unless -256 <= memory.offset.value
        $asm.puts "#{opcodeNegativeOffset} #{register.arm64Operand(kind)}, #{memory.arm64Operand(kind)}"
        return
    end

    $asm.puts "#{opcode} #{register.arm64Operand(kind)}, #{memory.arm64Operand(kind)}"
end

def emitARM64Shift(opcodeRegs, opcodeImmediate, operands, kind)
    if operands.size == 3 and operands[1].immediate?
        magicNumbers = yield operands[1].value
        $asm.puts "#{opcodeImmediate} #{operands[2].arm64Operand(kind)}, #{operands[0].arm64Operand(kind)}, \##{magicNumbers[0]}, \##{magicNumbers[1]}"
        return
    end
    
    if operands.size == 2 and operands[0].immediate?
        magicNumbers = yield operands[0].value
        $asm.puts "#{opcodeImmediate} #{operands[1].arm64Operand(kind)}, #{operands[1].arm64Operand(kind)}, \##{magicNumbers[0]}, \##{magicNumbers[1]}"
        return
    end
    
    emitARM64TAC(opcodeRegs, operands, kind)
end

def emitARM64LShift(operands, kind)
    emitARM64Shift("lslv", "ubfm", operands, kind) {
        | value |
        case kind
        when :word
            [32 - value, 31 - value]
        when :ptr
            bitSize = $currentSettings["ADDRESS64"] ? 64 : 32
            [bitSize - value, bitSize - 1 - value]
        when :quad
            [64 - value, 63 - value]
        end
    }
end

def emitARM64Branch(opcode, operands, kind, branchOpcode)
    emitARM64Unflipped(opcode, operands[0..-2], kind)
    $asm.puts "#{branchOpcode} #{operands[-1].asmLabel}"
end

def emitARM64CompareFP(operands, kind, compareCode)
    emitARM64Unflipped("fcmp", operands[0..-2], kind)
    $asm.puts "cset #{operands[-1].arm64Operand(:word)}, #{compareCode}"
end

def emitARM64Compare(operands, kind, compareCode)
    emitARM64Unflipped("subs #{arm64GPRName('xzr', kind)}, ", operands[0..-2], kind)
    $asm.puts "csinc #{operands[-1].arm64Operand(:word)}, wzr, wzr, #{compareCode}"
end

def emitARM64MoveImmediate(value, target)
    numberOfFilledHalfWords = 0
    numberOfZeroHalfWords = 0
    [48, 32, 16, 0].each {
        | shift |
        currentValue = (value >> shift) & 0xffff
        if currentValue == 0xffff
            numberOfFilledHalfWords += 1
        end
        if currentValue == 0
            numberOfZeroHalfWords += 1
        end
    }
    fillOtherHalfWordsWithOnes = (numberOfFilledHalfWords > numberOfZeroHalfWords)

    first = true
    [48, 32, 16, 0].each {
        | shift |
        currentValue = (value >> shift) & 0xffff
        next if currentValue == (fillOtherHalfWordsWithOnes ? 0xffff : 0) and (shift != 0 or !first)
        if first
            if fillOtherHalfWordsWithOnes
                shift.zero? \
                    ? ($asm.puts "movn #{target.arm64Operand(:quad)}, \##{(~currentValue) & 0xffff}")
                    : ($asm.puts "movn #{target.arm64Operand(:quad)}, \##{(~currentValue) & 0xffff}, lsl \##{shift}")
            else
                shift.zero? \
                    ? ($asm.puts "movz #{target.arm64Operand(:quad)}, \##{currentValue}")
                    : ($asm.puts "movz #{target.arm64Operand(:quad)}, \##{currentValue}, lsl \##{shift}")
            end
            first = false
        else
            shift.zero? \
                ? ($asm.puts "movk #{target.arm64Operand(:quad)}, \##{currentValue}")
                : ($asm.puts "movk #{target.arm64Operand(:quad)}, \##{currentValue}, lsl \##{shift}")
        end
    }
end

class Instruction
    def lowerARM64
        case opcode
        when 'addi'
            emitARM64Add("add", operands, :word)
        when 'addis'
            emitARM64Add("adds", operands, :word)
        when 'addp'
            emitARM64Add("add", operands, :ptr)
        when 'addps'
            emitARM64Add("adds", operands, :ptr)
        when 'addq'
            emitARM64Add("add", operands, :quad)
        when "andi"
            emitARM64TAC("and", operands, :word)
        when "andp"
            emitARM64TAC("and", operands, :ptr)
        when "andq"
            emitARM64TAC("and", operands, :quad)
        when "ori"
            emitARM64TAC("orr", operands, :word)
        when "orp"
            emitARM64TAC("orr", operands, :ptr)
        when "orq"
            emitARM64TAC("orr", operands, :quad)
        when "orh"
            emitARM64TAC("orr", operands, :word) # not :half because 16-bit registers don't exist on ARM.
        when "xori"
            emitARM64TAC("eor", operands, :word)
        when "xorp"
            emitARM64TAC("eor", operands, :ptr)
        when "xorq"
            emitARM64TAC("eor", operands, :quad)
        when 'divi'
            emitARM64Div("udiv", operands, :word)
        when 'divis'
            emitARM64Div("sdiv", operands, :word)
        when 'divq'
            emitARM64Div("udiv", operands, :quad)
        when 'divqs'
            emitARM64Div("sdiv", operands, :quad)
        when "lshifti"
            emitARM64LShift(operands, :word)
        when "lshiftp"
            emitARM64LShift(operands, :ptr)
        when "lshiftq"
            emitARM64LShift(operands, :quad)
        when "rshifti"
            emitARM64Shift("asrv", "sbfm", operands, :word) {
                | value |
                [value, 31]
            }
        when "rshiftp"
            emitARM64Shift("asrv", "sbfm", operands, :ptr) {
                | value |
                bitSize = $currentSettings["ADDRESS64"] ? 64 : 32
                [value, bitSize - 1]
            }
        when "rshiftq"
            emitARM64Shift("asrv", "sbfm", operands, :quad) {
                | value |
                [value, 63]
            }
        when "urshifti"
            emitARM64Shift("lsrv", "ubfm", operands, :word) {
                | value |
                [value, 31]
            }
        when "urshiftp"
            emitARM64Shift("lsrv", "ubfm", operands, :ptr) {
                | value |
                bitSize = $currentSettings["ADDRESS64"] ? 64 : 32
                [value, bitSize - 1]
            }
        when "urshiftq"
            emitARM64Shift("lsrv", "ubfm", operands, :quad) {
                | value |
                [value, 63]
            }
        when "muli"
            emitARM64Mul('mul', operands, :word)
        when "mulp"
            emitARM64Mul('mul', operands, :ptr)
        when "mulq"
            emitARM64Mul('mul', operands, :quad)
        when "subi"
            emitARM64Sub("sub", operands, :word)
        when "subp"
            emitARM64Sub("sub", operands, :ptr)
        when "subq"
            emitARM64Sub("sub", operands, :quad)
        when "subis"
            emitARM64Sub("subs", operands, :word)
        when "negi"
            $asm.puts "sub #{operands[0].arm64Operand(:word)}, wzr, #{operands[0].arm64Operand(:word)}"
        when "negp"
            $asm.puts "sub #{operands[0].arm64Operand(:ptr)}, #{arm64GPRName('xzr', :ptr)}, #{operands[0].arm64Operand(:ptr)}"
        when "negq"
            $asm.puts "sub #{operands[0].arm64Operand(:quad)}, xzr, #{operands[0].arm64Operand(:quad)}"
        when "notq"
            $asm.puts "mvn #{operands[0].arm64Operand(:quad)}, #{operands[0].arm64Operand(:quad)}"
        when "loadi"
            emitARM64Access("ldr", "ldur", operands[1], operands[0], :word)
        when "loadis"
            emitARM64Access("ldrsw", "ldursw", operands[1], operands[0], :quad)
        when "loadp"
            emitARM64Access("ldr", "ldur", operands[1], operands[0], :ptr)
        when "loadq"
            emitARM64Access("ldr", "ldur", operands[1], operands[0], :quad)
        when "storei"
            emitARM64Unflipped("str", operands, :word)
        when "storep"
            emitARM64Unflipped("str", operands, :ptr)
        when "storeq"
            emitARM64Unflipped("str", operands, :quad)
        when "loadb"
            emitARM64Access("ldrb", "ldurb", operands[1], operands[0], :word)
        when "loadbsi"
            emitARM64Access("ldrsb", "ldursb", operands[1], operands[0], :word)
        when "loadbsq"
            emitARM64Access("ldrsb", "ldursb", operands[1], operands[0], :quad)
        when "storeb"
            emitARM64Unflipped("strb", operands, :word)
        when "loadh"
            emitARM64Access("ldrh", "ldurh", operands[1], operands[0], :word)
        when "loadhsi"
            emitARM64Access("ldrsh", "ldursh", operands[1], operands[0], :word)
        when "loadhsq"
            emitARM64Access("ldrsh", "ldursh", operands[1], operands[0], :quad)
        when "storeh"
            emitARM64Unflipped("strh", operands, :word)
        when "loadd"
            emitARM64Access("ldr", "ldur", operands[1], operands[0], :double)
        when "stored"
            emitARM64Unflipped("str", operands, :double)
        when "loadv"
            emitARM64Access("ldr", "ldur", operands[1], operands[0], :vector_with_interpretation)
        when "storev"
            emitARM64Unflipped("str", operands, :vector_with_interpretation)
        when "addd"
            emitARM64TAC("fadd", operands, :double)
        when "divd"
            emitARM64TAC("fdiv", operands, :double)
        when "subd"
            emitARM64TAC("fsub", operands, :double)
        when "muld"
            emitARM64TAC("fmul", operands, :double)
        when "sqrtd"
            emitARM64("fsqrt", operands, :double)
        when "bdeq"
            emitARM64Branch("fcmp", operands, :double, "b.eq")
        when "bdneq"
            emitARM64Unflipped("fcmp", operands[0..1], :double)
            isUnordered = LocalLabel.unique("bdneq")
            $asm.puts "b.vs #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
            $asm.puts "b.ne #{operands[2].asmLabel}"
            isUnordered.lower($activeBackend)
        when "bdgt"
            emitARM64Branch("fcmp", operands, :double, "b.gt")
        when "bdgteq"
            emitARM64Branch("fcmp", operands, :double, "b.ge")
        when "bdlt"
            emitARM64Branch("fcmp", operands, :double, "b.mi")
        when "bdlteq"
            emitARM64Branch("fcmp", operands, :double, "b.ls")
        when "bdequn"
            emitARM64Unflipped("fcmp", operands[0..1], :double)
            $asm.puts "b.vs #{operands[2].asmLabel}"
            $asm.puts "b.eq #{operands[2].asmLabel}"
        when "bdnequn"
            emitARM64Branch("fcmp", operands, :double, "b.ne")
        when "bdgtun"
            emitARM64Branch("fcmp", operands, :double, "b.hi")
        when "bdgtequn"
            emitARM64Branch("fcmp", operands, :double, "b.pl")
        when "bdltun"
            emitARM64Branch("fcmp", operands, :double, "b.lt")
        when "bdltequn"
            emitARM64Branch("fcmp", operands, :double, "b.le")
        when "btd2i"
            # FIXME: May be a good idea to just get rid of this instruction, since the interpreter
            # currently does not use it.
            raise "ARM64 does not support this opcode yet, #{codeOriginString}"
        when "td2i"
            emitARM64("fcvtzs", operands, [:double, :word])
        when "bcd2i"
            # FIXME: Remove this instruction, or use it and implement it. Currently it's not
            # used.
            raise "ARM64 does not support this opcode yet, #{codeOriginString}"
        when "movdz"
            # FIXME: Remove it or support it.
            raise "ARM64 does not support this opcode yet, #{codeOriginString}"
        when "pop"
            operands.each_slice(2) {
                | ops |
                # Note that the operands are in the reverse order of the case for push.
                # This is due to the fact that order matters for pushing and popping, and 
                # on platforms that only push/pop one slot at a time they pop their 
                # arguments in the reverse order that they were pushed. In order to remain 
                # compatible with those platforms we assume here that that's what has been done.

                # So for example, if we did push(A, B, C, D), we would then pop(D, C, B, A).
                # But since the ordering of arguments doesn't change on arm64 between the stp and ldp 
                # instructions we need to flip flop the argument positions that were passed to us.
                $asm.puts "ldp #{ops[1].arm64Operand(:quad)}, #{ops[0].arm64Operand(:quad)}, [sp], #16"
            }
        when "push"
            operands.each_slice(2) {
                | ops |
                $asm.puts "stp #{ops[0].arm64Operand(:quad)}, #{ops[1].arm64Operand(:quad)}, [sp, #-16]!"
            }
        when "move"
            if operands[0].immediate?
                emitARM64MoveImmediate(operands[0].value, operands[1])
            elsif operands[0] != operands[1]
                emitARM64("mov", operands, :quad)
            end
        when "moved"
            emitARM64("fmov", operands, :double)
        when "sxi2p"
            emitARM64("sxtw", operands, [:word, :ptr])
        when "sxi2q"
            emitARM64("sxtw", operands, [:word, :quad])
        when "zxi2p"
            emitARM64("uxtw", operands, [:word, :ptr])
        when "zxi2q"
            emitARM64("uxtw", operands, [:word, :quad])
        when "sxb2i"
            emitARM64("sxtb", operands, [:word, :word])
        when "sxh2i"
            emitARM64("sxth", operands, [:word, :word])
        when "sxb2q"
            emitARM64("sxtb", operands, [:word, :quad])
        when "sxh2q"
            emitARM64("sxth", operands, [:word, :quad])
        when "nop"
            $asm.puts "nop"
        when "bieq", "bbeq"
            if operands[0].immediate? and operands[0].value == 0
                $asm.puts "cbz #{operands[1].arm64Operand(:word)}, #{operands[2].asmLabel}"
            elsif operands[1].immediate? and operands[1].value == 0
                $asm.puts "cbz #{operands[0].arm64Operand(:word)}, #{operands[2].asmLabel}"
            else
                emitARM64Branch("subs wzr, ", operands, :word, "b.eq")
            end
        when "bpeq"
            if operands[0].immediate? and operands[0].value == 0
                $asm.puts "cbz #{operands[1].arm64Operand(:ptr)}, #{operands[2].asmLabel}"
            elsif operands[1].immediate? and operands[1].value == 0
                $asm.puts "cbz #{operands[0].arm64Operand(:ptr)}, #{operands[2].asmLabel}"
            else
                emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.eq")
            end
        when "bqeq"
            if operands[0].immediate? and operands[0].value == 0
                $asm.puts "cbz #{operands[1].arm64Operand(:quad)}, #{operands[2].asmLabel}"
            elsif operands[1].immediate? and operands[1].value == 0
                $asm.puts "cbz #{operands[0].arm64Operand(:quad)}, #{operands[2].asmLabel}"
            else
                emitARM64Branch("subs xzr, ", operands, :quad, "b.eq")
            end
        when "bineq", "bbneq"
            if operands[0].immediate? and operands[0].value == 0
                $asm.puts "cbnz #{operands[1].arm64Operand(:word)}, #{operands[2].asmLabel}"
            elsif operands[1].immediate? and operands[1].value == 0
                $asm.puts "cbnz #{operands[0].arm64Operand(:word)}, #{operands[2].asmLabel}"
            else
                emitARM64Branch("subs wzr, ", operands, :word, "b.ne")
            end
        when "bpneq"
            if operands[0].immediate? and operands[0].value == 0
                $asm.puts "cbnz #{operands[1].arm64Operand(:ptr)}, #{operands[2].asmLabel}"
            elsif operands[1].immediate? and operands[1].value == 0
                $asm.puts "cbnz #{operands[0].arm64Operand(:ptr)}, #{operands[2].asmLabel}"
            else
                emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.ne")
            end
        when "bqneq"
            if operands[0].immediate? and operands[0].value == 0
                $asm.puts "cbnz #{operands[1].arm64Operand(:quad)}, #{operands[2].asmLabel}"
            elsif operands[1].immediate? and operands[1].value == 0
                $asm.puts "cbnz #{operands[0].arm64Operand(:quad)}, #{operands[2].asmLabel}"
            else
                emitARM64Branch("subs xzr, ", operands, :quad, "b.ne")
            end
        when "bia", "bba"
            emitARM64Branch("subs wzr, ", operands, :word, "b.hi")
        when "bpa"
            emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.hi")
        when "bqa"
            emitARM64Branch("subs xzr, ", operands, :quad, "b.hi")
        when "biaeq", "bbaeq"
            emitARM64Branch("subs wzr, ", operands, :word, "b.hs")
        when "bpaeq"
            emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.hs")
        when "bqaeq"
            emitARM64Branch("subs xzr, ", operands, :quad, "b.hs")
        when "bib", "bbb"
            emitARM64Branch("subs wzr, ", operands, :word, "b.lo")
        when "bpb"
            emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.lo")
        when "bqb"
            emitARM64Branch("subs xzr, ", operands, :quad, "b.lo")
        when "bibeq", "bbbeq"
            emitARM64Branch("subs wzr, ", operands, :word, "b.ls")
        when "bpbeq"
            emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.ls")
        when "bqbeq"
            emitARM64Branch("subs xzr, ", operands, :quad, "b.ls")
        when "bigt", "bbgt"
            emitARM64Branch("subs wzr, ", operands, :word, "b.gt")
        when "bpgt"
            emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.gt")
        when "bqgt"
            emitARM64Branch("subs xzr, ", operands, :quad, "b.gt")
        when "bigteq", "bbgteq"
            emitARM64Branch("subs wzr, ", operands, :word, "b.ge")
        when "bpgteq"
            emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.ge")
        when "bqgteq"
            emitARM64Branch("subs xzr, ", operands, :quad, "b.ge")
        when "bilt", "bblt"
            emitARM64Branch("subs wzr, ", operands, :word, "b.lt")
        when "bplt"
            emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.lt")
        when "bqlt"
            emitARM64Branch("subs xzr, ", operands, :quad, "b.lt")
        when "bilteq", "bblteq"
            emitARM64Branch("subs wzr, ", operands, :word, "b.le")
        when "bplteq"
            emitARM64Branch("subs #{arm64GPRName('xzr', :ptr)}, ", operands, :ptr, "b.le")
        when "bqlteq"
            emitARM64Branch("subs xzr, ", operands, :quad, "b.le")
        when "jmp"
            if operands[0].label?
                $asm.puts "b #{operands[0].asmLabel}"
            else
                emitARM64Unflipped("br", operands, :quad)
            end
        when "call"
            if operands[0].label?
                $asm.puts "bl #{operands[0].asmLabel}"
            else
                emitARM64Unflipped("blr", operands, :quad)
            end
        when "break"
            $asm.puts "brk \#0xc471"
        when "ret"
            $asm.puts "ret"
        when "cieq", "cbeq"
            emitARM64Compare(operands, :word, "ne")
        when "cpeq"
            emitARM64Compare(operands, :ptr, "ne")
        when "cqeq"
            emitARM64Compare(operands, :quad, "ne")
        when "cineq", "cbneq"
            emitARM64Compare(operands, :word, "eq")
        when "cpneq"
            emitARM64Compare(operands, :ptr, "eq")
        when "cqneq"
            emitARM64Compare(operands, :quad, "eq")
        when "cia", "cba"
            emitARM64Compare(operands, :word, "ls")
        when "cpa"
            emitARM64Compare(operands, :ptr, "ls")
        when "cqa"
            emitARM64Compare(operands, :quad, "ls")
        when "ciaeq", "cbaeq"
            emitARM64Compare(operands, :word, "lo")
        when "cpaeq"
            emitARM64Compare(operands, :ptr, "lo")
        when "cqaeq"
            emitARM64Compare(operands, :quad, "lo")
        when "cib", "cbb"
            emitARM64Compare(operands, :word, "hs")
        when "cpb"
            emitARM64Compare(operands, :ptr, "hs")
        when "cqb"
            emitARM64Compare(operands, :quad, "hs")
        when "cibeq", "cbbeq"
            emitARM64Compare(operands, :word, "hi")
        when "cpbeq"
            emitARM64Compare(operands, :ptr, "hi")
        when "cqbeq"
            emitARM64Compare(operands, :quad, "hi")
        when "cilt", "cblt"
            emitARM64Compare(operands, :word, "ge")
        when "cplt"
            emitARM64Compare(operands, :ptr, "ge")
        when "cqlt"
            emitARM64Compare(operands, :quad, "ge")
        when "cilteq", "cblteq"
            emitARM64Compare(operands, :word, "gt")
        when "cplteq"
            emitARM64Compare(operands, :ptr, "gt")
        when "cqlteq"
            emitARM64Compare(operands, :quad, "gt")
        when "cigt", "cbgt"
            emitARM64Compare(operands, :word, "le")
        when "cpgt"
            emitARM64Compare(operands, :ptr, "le")
        when "cqgt"
            emitARM64Compare(operands, :quad, "le")
        when "cigteq", "cbgteq"
            emitARM64Compare(operands, :word, "lt")
        when "cpgteq"
            emitARM64Compare(operands, :ptr, "lt")
        when "cqgteq"
            emitARM64Compare(operands, :quad, "lt")
        when "peek"
            $asm.puts "ldr #{operands[1].arm64Operand(:quad)}, [sp, \##{operands[0].value * 8}]"
        when "poke"
            $asm.puts "str #{operands[1].arm64Operand(:quad)}, [sp, \##{operands[0].value * 8}]"
        when "fp2d"
            emitARM64("fmov", operands, [:ptr, :double])
        when "fq2d"
            emitARM64("fmov", operands, [:quad, :double])
        when "fd2p"
            emitARM64("fmov", operands, [:double, :ptr])
        when "fd2q"
            emitARM64("fmov", operands, [:double, :quad])
        when "bo"
            $asm.puts "b.vs #{operands[0].asmLabel}"
        when "bs"
            $asm.puts "b.mi #{operands[0].asmLabel}"
        when "bz"
            $asm.puts "b.eq #{operands[0].asmLabel}"
        when "bnz"
            $asm.puts "b.ne #{operands[0].asmLabel}"
        when "leai"
            operands[0].arm64EmitLea(operands[1], :word)
        when "leap"
            operands[0].arm64EmitLea(operands[1], :ptr)
        when "leaq"
            operands[0].arm64EmitLea(operands[1], :quad)
        when "smulli"
            $asm.puts "smaddl #{operands[2].arm64Operand(:quad)}, #{operands[0].arm64Operand(:word)}, #{operands[1].arm64Operand(:word)}, xzr"
        when "memfence"
            $asm.puts "dmb sy"
        when "fence"
            $asm.puts "dmb ish"
        when "bfiq"
            $asm.puts "bfi #{operands[3].arm64Operand(:quad)}, #{operands[0].arm64Operand(:quad)}, #{operands[1].value}, #{operands[2].value}"
        when "pcrtoaddr"
            $asm.puts "adr #{operands[1].arm64Operand(:quad)}, #{operands[0].value}"
        when "globaladdr"
            uid = $asm.newUID

            # On Darwin, use Macho-O GOT relocation specifiers, along with
            # the labels required for the .loh directive.
            $asm.putStr("#if OS(DARWIN)")
            $asm.puts "Ljsc_llint_loh_adrp_#{uid}:"
            $asm.puts "adrp #{operands[1].arm64Operand(:quad)}, #{operands[0].asmLabel}@GOTPAGE"
            $asm.puts "Ljsc_llint_loh_ldr_#{uid}:"
            $asm.puts "ldr #{operands[1].arm64Operand(:quad)}, [#{operands[1].arm64Operand(:quad)}, #{operands[0].asmLabel}@GOTPAGEOFF]"

            # On Linux, use ELF GOT relocation specifiers.
            $asm.putStr("#elif OS(LINUX)")
            $asm.puts "adrp #{operands[1].arm64Operand(:quad)}, :got:#{operands[0].asmLabel}"
            $asm.puts "ldr #{operands[1].arm64Operand(:quad)}, [#{operands[1].arm64Operand(:quad)}, :got_lo12:#{operands[0].asmLabel}]"

            # Throw a compiler error everywhere else.
            $asm.putStr("#else")
            $asm.putStr("#error Missing globaladdr implementation")
            $asm.putStr("#endif")

            $asm.deferOSDarwinAction {
                # On Darwin, also include the .loh directive using the generated labels.
                $asm.puts ".loh AdrpLdrGot Ljsc_llint_loh_adrp_#{uid}, Ljsc_llint_loh_ldr_#{uid}"
            }

        when "andf", "andd"
            emitARM64TACWithOperandSuffix("and", operands, :double)
        when "orf", "ord"
            emitARM64TACWithOperandSuffix("orr", operands, :double)
        when "lrotatei"
            tmp = Tmp.new(codeOrigin, :gpr)
            Sequence.new(codeOrigin, [
                Instruction.new(codeOrigin, "move", [operands[0], tmp]),
                Instruction.new(codeOrigin, "negq", [tmp]),
                Instruction.new(codeOrigin, "rrotatei", [tmp, operands[1]]),
            ]).lower($activeBackend)
        when "lrotateq"
            tmp = Tmp.new(codeOrigin, :gpr)
            Sequence.new(codeOrigin, [
                Instruction.new(codeOrigin, "move", [operands[0], tmp]),
                Instruction.new(codeOrigin, "negq", [tmp]),
                Instruction.new(codeOrigin, "rrotateq", [tmp, operands[1]]),
            ]).lower($activeBackend)
        when "rrotatei"
            emitARM64TAC("ror", operands, :word)
        when "rrotateq"
            emitARM64TAC("ror", operands, :quad)
        when "loadf"
            emitARM64Access("ldr", "ldur", operands[1], operands[0], :float)
        when "storef"
            emitARM64Unflipped("str", operands, :float)
        when "addf"
            emitARM64TAC("fadd", operands, :float)
        when "divf"
            emitARM64TAC("fdiv", operands, :float)
        when "subf"
            emitARM64TAC("fsub", operands, :float)
        when "mulf"
            emitARM64TAC("fmul", operands, :float)
        when "sqrtf"
            emitARM64("fsqrt", operands, :float)
        when "floorf"
            emitARM64("frintm", operands, :float)
        when "floord"
            emitARM64("frintm", operands, :double)
        when "roundf"
            emitARM64("frintn", operands, :float)
        when "roundd"
            emitARM64("frintn", operands, :double)
        when "truncatef"
            emitARM64("frintz", operands, :float)
        when "truncated"
            emitARM64("frintz", operands, :double)
        when "truncatef2i"
            emitARM64("fcvtzu", operands, [:float, :word])
        when "truncatef2q"
            emitARM64("fcvtzu", operands, [:float, :quad])
        when "truncated2q"
            emitARM64("fcvtzu", operands, [:double, :quad])
        when "truncated2i"
            emitARM64("fcvtzu", operands, [:double, :word])
        when "truncatef2is"
            emitARM64("fcvtzs", operands, [:float, :word])
        when "truncated2is"
            emitARM64("fcvtzs", operands, [:double, :word])
        when "truncatef2qs"
            emitARM64("fcvtzs", operands, [:float, :quad])
        when "truncated2qs"
            emitARM64("fcvtzs", operands, [:double, :quad])
        when "ci2d"
            emitARM64("ucvtf", operands, [:word, :double])
        when "ci2ds"
            emitARM64("scvtf", operands, [:word, :double])
        when "ci2f"
            emitARM64("ucvtf", operands, [:word, :float])
        when "ci2fs"
            emitARM64("scvtf", operands, [:word, :float])
        when "cq2f"
            emitARM64("ucvtf", operands, [:quad, :float])
        when "cq2fs"
            emitARM64("scvtf", operands, [:quad, :float])
        when "cq2d"
            emitARM64("ucvtf", operands, [:quad, :double])
        when "cq2ds"
            emitARM64("scvtf", operands, [:quad, :double])
        when "cd2f"
            emitARM64("fcvt", operands, [:double, :float])
        when "cf2d"
            emitARM64("fcvt", operands, [:float, :double])
        when "bfeq"
            emitARM64Branch("fcmp", operands, :float, "b.eq")
        when "bfgt"
            emitARM64Branch("fcmp", operands, :float, "b.gt")
        when "bflt"
            emitARM64Branch("fcmp", operands, :float, "b.mi")
        when "bfgtun"
            emitARM64Branch("fcmp", operands, :float, "b.hi")
        when "bfgtequn"
            emitARM64Branch("fcmp", operands, :float, "b.pl")
        when "bfltun"
            emitARM64Branch("fcmp", operands, :float, "b.lt")
        when "bfltequn"
            emitARM64Branch("fcmp", operands, :float, "b.le")
        when "tzcnti"
            emitARM64("rbit", operands, :word)
            emitARM64("clz", [operands[1], operands[1]], :word)
        when "tzcntq"
            emitARM64("rbit", operands, :quad)
            emitARM64("clz", [operands[1], operands[1]], :quad)
        when "lzcnti"
            emitARM64("clz", operands, :word)
        when "lzcntq"
            emitARM64("clz", operands, :quad)
        when "absf"
            emitARM64("fabs", operands, :float)
        when "absd"
            emitARM64("fabs", operands, :double)
        when "negf"
            emitARM64("fneg", operands, :float)
        when "negd"
            emitARM64("fneg", operands, :double)
        when "ceilf"
            emitARM64("frintp", operands, :float)
        when "ceild"
            emitARM64("frintp", operands, :double)
        when "cfeq"
            emitARM64CompareFP(operands, :float, "eq")
        when "cdeq"
            emitARM64CompareFP(operands, :double, "eq")
        when "cfneq"
            $asm.puts "move $0, #{operands[2].arm64Operand(:word)}"
            emitARM64Unflipped("fcmp", operands[0..1], :float)
            isUnordered = LocalLabel.unique("cdneq")
            $asm.puts "b.vs #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
            $asm.puts "cset #{operands[2].arm64Operand(:word)}, ne"
            isUnordered.lower($activeBackend)
        when "cdneq"
            $asm.puts "move $0, #{operands[2].arm64Operand(:word)}"
            emitARM64Unflipped("fcmp", operands[0..1], :double)
            isUnordered = LocalLabel.unique("cdneq")
            $asm.puts "b.vs #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
            $asm.puts "cset #{operands[2].arm64Operand(:word)}, ne"
            isUnordered.lower($activeBackend)
        when "cfnequn"
            emitARM64CompareFP(operands, :float, "ne")
        when "cdnequn"
            emitARM64CompareFP(operands, :double, "ne")
        when "cflt"
            emitARM64CompareFP(operands, :float, "mi")
        when "cdlt"
            emitARM64CompareFP(operands, :double, "mi")
        when "cflteq"
            emitARM64CompareFP(operands, :float, "ls")
        when "cdlteq"
            emitARM64CompareFP(operands, :double, "ls")
        when "cfgt"
            emitARM64CompareFP(operands, :float, "gt")
        when "cdgt"
            emitARM64CompareFP(operands, :double, "gt")
        when "cfgteq"
            emitARM64CompareFP(operands, :float, "ge")
        when "cdgteq"
            emitARM64CompareFP(operands, :double, "ge")
        when "fi2f"
            emitARM64("fmov", operands, [:word, :float])
        when "ff2i"
            emitARM64("fmov", operands, [:float, :word])
        when "tls_loadp"
            tmp = ARM64_EXTRA_GPRS[0].arm64Operand(:ptr)
            if operands[0].immediate?
              offset = "\##{operands[0].value * 8}"
            else
              offset = operands[0].arm64Operand(:word)
            end
            $asm.puts "mrs #{tmp}, tpidrro_el0"
            $asm.puts "#if !HAVE(SIMPLIFIED_FAST_TLS_BASE)"
            $asm.puts "bic #{tmp}, #{tmp}, #7"
            $asm.puts "#endif"

            $asm.puts "ldr #{operands[1].arm64Operand(:ptr)}, [#{tmp}, #{offset}]"
        when "tls_storep"
            tmp = ARM64_EXTRA_GPRS[0].arm64Operand(:ptr)
            if operands[1].immediate?
              offset = "\##{operands[1].value * 8}"
            else
              offset = operands[1].arm64Operand(:word)
            end
            $asm.puts "mrs #{tmp}, tpidrro_el0"
            $asm.puts "#if !HAVE(SIMPLIFIED_FAST_TLS_BASE)"
            $asm.puts "bic #{tmp}, #{tmp}, #7"
            $asm.puts "#endif"
            $asm.puts "str #{operands[0].arm64Operand(:ptr)}, [#{tmp}, #{offset}]"
        when "loadlinkacqb"
            $asm.puts "ldaxrb #{operands[1].arm64Operand(:word)}, #{operands[0].arm64Operand(:word)}"
        when "loadlinkacqh"
            $asm.puts "ldaxrh #{operands[1].arm64Operand(:word)}, #{operands[0].arm64Operand(:word)}"
        when "loadlinkacqi"
            $asm.puts "ldaxr #{operands[1].arm64Operand(:word)}, #{operands[0].arm64Operand(:word)}"
        when "loadlinkacqq"
            $asm.puts "ldaxr #{operands[1].arm64Operand(:quad)}, #{operands[0].arm64Operand(:quad)}"
        when "storecondrelb"
            $asm.puts "stlxrb #{operands[0].arm64Operand(:word)}, #{operands[1].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}"
        when "storecondrelh"
            $asm.puts "stlxrh #{operands[0].arm64Operand(:word)}, #{operands[1].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}"
        when "storecondreli"
            $asm.puts "stlxr #{operands[0].arm64Operand(:word)}, #{operands[1].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}"
        when "storecondrelq"
            $asm.puts "stlxr #{operands[0].arm64Operand(:word)}, #{operands[1].arm64Operand(:quad)}, #{operands[2].arm64Operand(:quad)}"
        when "atomicxchgaddb"
            $asm.puts "ldaddalb #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgaddh"
            $asm.puts "ldaddalh #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgaddi"
            $asm.puts "ldaddal #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgaddq"
            $asm.puts "ldaddal #{operands[0].arm64Operand(:quad)}, #{operands[2].arm64Operand(:quad)}, #{operands[1].arm64SimpleAddressOperand(:quad)}"
        when "atomicxchgclearb"
            $asm.puts "ldclralb #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgclearh"
            $asm.puts "ldclralh #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgcleari"
            $asm.puts "ldclral #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgclearq"
            $asm.puts "ldclral #{operands[0].arm64Operand(:quad)}, #{operands[2].arm64Operand(:quad)}, #{operands[1].arm64SimpleAddressOperand(:quad)}"
        when "atomicxchgorb"
            $asm.puts "ldsetalb #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgorh"
            $asm.puts "ldsetalh #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgori"
            $asm.puts "ldsetal #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgorq"
            $asm.puts "ldsetal #{operands[0].arm64Operand(:quad)}, #{operands[2].arm64Operand(:quad)}, #{operands[1].arm64SimpleAddressOperand(:quad)}"
        when "atomicxchgxorb"
            $asm.puts "ldeoralb #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgxorh"
            $asm.puts "ldeoralh #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgxori"
            $asm.puts "ldeoral #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgxorq"
            $asm.puts "ldeoral #{operands[0].arm64Operand(:quad)}, #{operands[2].arm64Operand(:quad)}, #{operands[1].arm64SimpleAddressOperand(:quad)}"
        when "atomicxchgb"
            $asm.puts "swpalb #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgh"
            $asm.puts "swpalh #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgi"
            $asm.puts "swpal #{operands[0].arm64Operand(:word)}, #{operands[2].arm64Operand(:word)}, #{operands[1].arm64SimpleAddressOperand(:word)}"
        when "atomicxchgq"
            $asm.puts "swpal #{operands[0].arm64Operand(:quad)}, #{operands[2].arm64Operand(:quad)}, #{operands[1].arm64SimpleAddressOperand(:quad)}"
        when "atomicweakcasb"
            $asm.puts "casalb #{operands[0].arm64Operand(:word)}, #{operands[1].arm64Operand(:word)}, #{operands[2].arm64SimpleAddressOperand(:word)}"
        when "atomicweakcash"
            $asm.puts "casalh #{operands[0].arm64Operand(:word)}, #{operands[1].arm64Operand(:word)}, #{operands[2].arm64SimpleAddressOperand(:word)}"
        when "atomicweakcasi"
            $asm.puts "casal #{operands[0].arm64Operand(:word)}, #{operands[1].arm64Operand(:word)}, #{operands[2].arm64SimpleAddressOperand(:word)}"
        when "atomicweakcasq"
            $asm.puts "casal #{operands[0].arm64Operand(:quad)}, #{operands[1].arm64Operand(:quad)}, #{operands[2].arm64SimpleAddressOperand(:quad)}"
        when "atomicloadb"
            $asm.puts "ldarb #{operands[1].arm64Operand(:word)}, #{operands[0].arm64SimpleAddressOperand(:word)}"
        when "atomicloadh"
            $asm.puts "ldarh #{operands[1].arm64Operand(:word)}, #{operands[0].arm64SimpleAddressOperand(:word)}"
        when "atomicloadi"
            $asm.puts "ldar #{operands[1].arm64Operand(:word)}, #{operands[0].arm64SimpleAddressOperand(:word)}"
        when "atomicloadq"
            $asm.puts "ldar #{operands[1].arm64Operand(:quad)}, #{operands[0].arm64SimpleAddressOperand(:quad)}"
        when "loadpairq"
            $asm.puts "ldp #{operands[1].arm64Operand(:quad)}, #{operands[2].arm64Operand(:quad)}, #{operands[0].arm64PairAddressOperand(:quad)}"
        when "storepairq"
            $asm.puts "stp #{operands[0].arm64Operand(:quad)}, #{operands[1].arm64Operand(:quad)}, #{operands[2].arm64PairAddressOperand(:quad)}"
        when "loadpaird"
            $asm.puts "ldp #{operands[1].arm64Operand(:double)}, #{operands[2].arm64Operand(:double)}, #{operands[0].arm64PairAddressOperand(:double)}"
        when "storepaird"
            $asm.puts "stp #{operands[0].arm64Operand(:double)}, #{operands[1].arm64Operand(:double)}, #{operands[2].arm64PairAddressOperand(:double)}"
        else
            lowerDefault
        end
    end
end

