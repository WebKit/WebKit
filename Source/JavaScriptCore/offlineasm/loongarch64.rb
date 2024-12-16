# Copyright (C) 2011-2023 Apple Inc. All rights reserved.
# Copyright (C) 2023-2024 Loongson Technology. All rights reserved.
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


# Naming conventions
#
# r<number> => GPR, used to operate with 32-bit and 64-bit integer values
# f<number> => FPR, used to operate with 32-bit and 64-bit floating-point values
#
# GPR conventions, to match the baseline JIT:
#
# r0  => zero   (zero register)
# r1  => la     (return address register)
# r2  => tp
# r3  => sp     (stack pointer register)
# r4  => t0, a0, wa0, r0
# r5  => t1, a1, wa1, r1
# r6  => t2, a2, wa2
# r7  => t3, a3, wa3
# r8  => t4, a4, wa4
# r9  => t5, a5, wa5
# r10 => t6, a6, wa6
# r11 => t7, a7, wa7
# r12 => ws0
# r13 => ws1
# r14 => ws2
# r15 => ws3
# r16 =>        (scratch)
# r17 =>        (scratch)
# r18 =>        (scratch)
# r19 =>        (scratch)
# r20 =>        (scratch)
# r21 => rx
# r22 => cfr    (frame pointer register)
# r23 => csr0
# r24 => csr1
# r25 => csr2
# r26 => csr3
# r27 => csr4
# r28 => csr5   (metadataTable)
# r29 => csr6   (PB)
# r30 => csr7   (numberTag)
# r31 => csr8   (notCellMask)
#
# FPR conventions, to match the baseline JIT:
#
# f0  => fa0, wfa0
# f1  => fa1, wfa1
# f2  => fa2, wfa2
# f3  => fa3, wfa3
# f4  => fa4, wfa4
# f5  => fa5, wfa5
# f6  => fa6, wfa6
# f7  => fa7, wfa7
# f8  => ft0
# f9  => ft1
# f10 => ft2
# f11 => ft3
# f12 => ft4
# f13 => ft5
# f14 => ft6
# f15 => ft7
# f16 => ft8
# f17 => ft9
# f18 => ft10
# f19 => ft11
# f20 => ft12
# f21 => ft13
# f22 => ft14
# f23 => ft15
# f24 => csfr0
# f25 => csfr1
# f26 => csfr2
# f27 => csfr3
# f28 => csfr4
# f29 => csfr5
# f30 => csfr6
# f31 => csfr7

LOONGARCH64_EXTRA_GPRS = [SpecialRegister.new("$r18"), SpecialRegister.new("$r19"), SpecialRegister.new("$r20")]
LOONGARCH64_EXTRA_FPRS = [SpecialRegister.new("$f19"), SpecialRegister.new("$f20"), SpecialRegister.new("$f21")]


def loongarch64OperandTypes(operands)
    return operands.map {
        |op|
        if op.is_a? SpecialRegister
            case op.name
            when /^\$r/
                RegisterID
            when /^\$f/
                FPRegisterID
            else
                raise "Invalid SpecialRegister operand #{op.name}"
            end
        elsif op.is_a? Tmp
            case op.kind
            when :gpr
                RegisterID
            when :fpr
                FPRegisterID
            else
                raise "Invalid Tmp operand #{op.kind}"
            end
        else
            op.class
        end
    }
end

def loongarch64RaiseMismatchedOperands(operands)
    raise "Unable to match operands #{loongarch64OperandTypes(operands)}"
end

def loongarch64ValidateOperands(operands, *expected)
    loongarch64RaiseMismatchedOperands(operands) unless expected.include? loongarch64OperandTypes(operands)
end

def loongarch64ValidateImmediate(validation, value)
    case validation
    when :i_immediate
        (-0x800..0x7ff).include? value
    when :any_immediate
        true
    when :la32_shift_immediate
        (0..31).include? value
    when :la64_shift_immediate
        (0..63).include? value
    else
        raise "Invalid immediate validation #{validation}"
    end
end

class RegisterID
    def loongarch64Operand
        case @name
        when 'lr'
            '$r1'
        when 'sp'
            '$r3'
        when 't0', 'a0', 'wa0', 'r0'
            '$r4'
        when 't1', 'a1', 'wa1', 'r1'
            '$r5'
        when 't2', 'a2', 'wa2'
            '$r6'
        when 't3', 'a3', 'wa3'
            '$r7'
        when 't4', 'a4', 'wa4'
            '$r8'
        when 't5', 'a5', 'wa5'
            '$r9'
        when 't6', 'a6', 'wa6'
            '$r10'
        when 't7', 'a7', 'wa7'
            '$r11'
        when 'ws0'
            '$r12'
        when 'ws1'
            '$r13'
        when 'cfr'
            '$r22'
        when 'csr0'
            '$r23'
        when 'csr1'
            '$r24'
        when 'csr2'
            '$r25'
        when 'csr3'
            '$r26'
        when 'csr4'
            '$r27'
        when 'csr5'
            '$r28'
        when 'csr6'
            '$r29'
        when 'csr7'
            '$r30'
        when 'csr8'
            '$r31'
        else
            raise "Bad register name #{@name} at #{codeOriginString}"
        end
    end
end

class FPRegisterID
    def loongarch64Operand
        case @name
        when 'fa0', 'wfa0'
            '$f0'
        when 'fa1', 'wfa1'
            '$f1'
        when 'fa2', 'wfa2'
            '$f2'
        when 'fa3', 'wfa3'
            '$f3'
        when 'fa4', 'wfa4'
            '$f4'
        when 'fa5', 'wfa5'
            '$f5'
        when 'fa6', 'wfa6'
            '$f6'
        when 'fa7', 'wfa7'
            '$f7'
        when 'ft0'
            '$f8'
        when 'ft1'
            '$f9'
        when 'ft2'
            '$f10'
        when 'ft3'
            '$f11'
        when 'ft4'
            '$f12'
        when 'ft5'
            '$f13'
        when 'csfr0'
            '$f24'
        when 'csfr1'
            '$f25'
        when 'csfr2'
            '$f26'
        when 'csfr3'
            '$f27'
        when 'csfr4'
            '$f28'
        when 'csfr5'
            '$f29'
        when 'csfr6'
            '$f30'
        when 'csfr7'
            '$f31'
        else
            raise "Bad register name #{@name} at #{codeOriginString}"
        end
    end
end

class SpecialRegister
    def loongarch64Operand
        @name
    end
end

class Immediate
    def loongarch64Operand(validation = :i_immediate)
        raise "Invalid immediate value #{value} at #{codeOriginString}" if loongarch64RequiresLoad(validation)
        "#{value}"
    end

    def loongarch64RequiresLoad(validation = :i_immediate)
        not loongarch64ValidateImmediate(validation, value)
    end
end

class Address
    def loongarch64Operand
        raise "Invalid offset #{offset.value} at #{codeOriginString}" if loongarch64RequiresLoad
        "#{base.loongarch64Operand}, #{offset.value}"
    end

    def loongarch64RequiresLoad
        not loongarch64ValidateImmediate(:i_immediate, offset.value)
    end
end

def loongarch64LowerEmitMask(newList, node, size, source, destination)
    case size
    when :b, :h, :i
        case size
        when :b
            shiftSize = 56
        when :h
            shiftSize = 48
        when :i
            shiftSize = 32
        end
        newList << Instruction.new(node.codeOrigin, "slli.d", [source, Immediate.new(node.codeOrigin, shiftSize), destination])
        newList << Instruction.new(node.codeOrigin, "srli.d", [destination, Immediate.new(node.codeOrigin, shiftSize), destination])
    when :p, :q
    else
        raise "Invalid masking size"
    end
end

def loongarch64LowerEmitSignExtension(newList, node, size, source, destination)
    case size
    when :b, :h
        case size
        when :b
            shiftSize = 56
        when :h
            shiftSize = 32
        end
        newList << Instruction.new(node.codeOrigin, "slli.d", [source, Immediate.new(node.codeOrigin, shiftSize), destination])
        newList << Instruction.new(node.codeOrigin, "srai.d", [destination, Immediate.new(node.codeOrigin, shiftSize), destination])
    when :i
        newList << Instruction.new(node.codeOrigin, "addi.w", [source, Immediate.new(node.codeOrigin, 0), destination])
    when :p, :q
    else
        raise "Invalid extension size"
    end
end

def loongarch64LowerOperandIntoRegister(newList, node, operand)
    register = operand
    if operand.immediate?
        register = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "li.d", [operand, register])
    end

    raise "Invalid register type" unless loongarch64OperandTypes([register]) == [RegisterID]
    register
end

def loongarch64LowerOperandIntoRegisterAndSignExtend(newList, node, operand, size, forcedTmp = :none)
    source = loongarch64LowerOperandIntoRegister(newList, node, operand)
    destination = source

    if ([:b, :h, :i].include? size or forcedTmp == :forced_tmp) and not destination.is_a? Tmp
        destination = Tmp.new(node.codeOrigin, :gpr)
    end

    loongarch64LowerEmitSignExtension(newList, node, size, source, destination)
    destination
end

def loongarch64LowerMisplacedAddresses(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^b(add|sub)i(z|nz|s)$/
                case loongarch64OperandTypes(node.operands)
                when [Immediate, Address, LocalLabelReference]
                    tmp = Tmp.new(node.codeOrigin, :gpr)
                    newList << Instruction.new(node.codeOrigin, "loadi", [node.operands[1], tmp])
                    newList << Instruction.new(node.codeOrigin, "#{$1}i", [tmp, node.operands[0], tmp])
                    newList << Instruction.new(node.codeOrigin, "storei", [tmp, node.operands[1]])
                    newList << Instruction.new(node.codeOrigin, "bti#{$2}", [tmp, node.operands[2]])
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

def loongarch64LowerAddressLoads(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "leap", "leaq"
                case loongarch64OperandTypes(node.operands)
                when [Address, RegisterID]
                    address, dest = node.operands[0], node.operands[1]
                    raise "Invalid address" if address.loongarch64RequiresLoad
                    newList << Instruction.new(node.codeOrigin, "addi.d", [address.base, address.offset, dest])
                when [BaseIndex, RegisterID]
                    bi, dest = node.operands[0], node.operands[1]
                    newList << Instruction.new(node.codeOrigin, "slli.d", [bi.index, Immediate.new(node.codeOrigin, bi.scaleShift), dest])
                    newList << Instruction.new(node.codeOrigin, "add.d", [dest, bi.base, dest])
                    if bi.offset.value != 0
                        offset = Immediate.new(node.codeOrigin, bi.offset.value)
                        if offset.loongarch64RequiresLoad
                            tmp = Tmp.new(node.codeOrigin, :gpr)
                            newList << Instruction.new(node.codeOrigin, "li.d", [offset, tmp])
                            newList << Instruction.new(node.codeOrigin, "add.d", [dest, tmp, dest])
                        else
                            newList << Instruction.new(node.codeOrigin, "addi.d", [dest, offset, dest])
                        end
                    end
                when [LabelReference, RegisterID]
                    label, dest = node.operands[0], node.operands[1]
                    newList << Instruction.new(node.codeOrigin, "la", [label, dest])
                    if label.offset != 0
                        offset = Immediate.new(node.codeOrigin, label.offset)
                        if offset.loongarch64RequiresLoad
                            tmp = Tmp.new(node.codeOrigin, :gpr)
                            newList << Instruction.new(node.codeOrigin, "li.d", [offset, tmp])
                            newList << Instruction.new(node.codeOrigin, "add.d", [dest, tmp, dest])
                        else
                            newList << Instruction.new(node.codeOrigin, "addi.d", [dest, offset, dest])
                        end
                    end
                else
                    loongarch64RaiseMismatchedOperands(node.operands)
                end
            when "globaladdr"
                loongarch64ValidateOperands(node.operands, [LabelReference, RegisterID])
                newList << Instruction.new(node.codeOrigin, "la", node.operands)
            when "pcrtoaddr"
                loongarch64ValidateOperands(node.operands, [LabelReference, RegisterID])
                newList << Instruction.new(node.codeOrigin, "la.local", node.operands)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def loongarch64LowerImmediateSubtraction(list)
    def emit(newList, node, size, operands)
        loongarch64ValidateOperands(operands, [RegisterID, Immediate, RegisterID])
        nimmediate = Immediate.new(node.codeOrigin, -operands[1].value)
        if nimmediate.loongarch64RequiresLoad
            tmp = Tmp.new(node.codeOrigin, :gpr)
            newList << Instruction.new(node.codeOrigin, "li.d", [operands[1], tmp])
            newList << Instruction.new(node.codeOrigin, "sub.d", [operands[0], tmp, operands[2]])
        else
            newList << Instruction.new(node.codeOrigin, "addi.d", [operands[0], nimmediate, operands[2]])
        end
        loongarch64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^sub(i|p|q)$/
                case loongarch64OperandTypes(node.operands)
                when [RegisterID, Immediate, RegisterID]
                    emit(newList, node, $1.to_sym, node.operands)
                when [Immediate, RegisterID]
                    emit(newList, node, $1.to_sym, [node.operands[1], node.operands[0], node.operands[1]])
                else
                    raise "Invalid immediate subtraction pattern" if loongarch64OperandTypes(node.operands).include? Immediate
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

def loongarch64LowerOperation(list)
    def emitLoadOperation(newList, node, size)
        loongarch64ValidateOperands(node.operands, [Address, RegisterID])

        case size
        when :b
            suffix = "bu"
        when :bsi, :bsq
            suffix = "b"
        when :h
            suffix = "hu"
        when :hsi, :hsq
            suffix = "h"
        when :i
            suffix = "wu"
        when :is
            suffix = "w"
        when :p, :q
            suffix = "d"
        else
            raise "Invalid size #{size}"
        end

        newList << Instruction.new(node.codeOrigin, "ld.#{suffix}", node.operands)

        case size
        when :bsi, :hsi
            loongarch64LowerEmitMask(newList, node, :i, node.operands[1], node.operands[1])
        when :bsq, :hsq
            # Nothing to do
        end
    end

    def emitStoreOperation(newList, node, size)
        loongarch64ValidateOperands(node.operands, [RegisterID, Address])

        case size
        when :b
            suffix = "b"
        when :h
            suffix = "h"
        when :i
            suffix = "w"
        when :p, :q
            suffix = "d"
        else
            raise "Invalid size #{size}"
        end

        newList << Instruction.new(node.codeOrigin, "st.#{suffix}", node.operands)
    end

    def emitMove(newList, node)
        case loongarch64OperandTypes(node.operands)
        when [RegisterID, RegisterID]
            moveOpcode = "move"
        when [Immediate, RegisterID]
            moveOpcode = "li.d"
        else
            loongarch64RaiseMismatchedOperands(node.operands)
        end

        newList << Instruction.new(node.codeOrigin, "#{moveOpcode}", node.operands)
    end

    def emitJump(newList, node)
        case loongarch64OperandTypes(node.operands)
        when [RegisterID]
            jumpOpcode = "jr"
        when [LabelReference], [LocalLabelReference]
            jumpOpcode = "b"
        else
            loongarch64RaiseMismatchedOperands(node.operands)
        end

        newList << Instruction.new(node.codeOrigin, "#{jumpOpcode}", node.operands)
    end

    def emitCall(newList, node)
        case loongarch64OperandTypes(node.operands)
        when [RegisterID]
            callOpcode = "jalr"
        when [LabelReference]
            callOpcode = "bl"
        else
            loongarch64RaiseMismatchedOperands(node.operands)
        end

        newList << Instruction.new(node.codeOrigin, "#{callOpcode}", node.operands)
    end

    def emitPush(newList, node)
        sp = RegisterID.forName(node.codeOrigin, 'sp')
        size = 8 * node.operands.size
        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, -size), sp])
        node.operands.reverse.each_with_index {
            | op, index |
            offset = size - 8 * (index + 1)
            newList << Instruction.new(node.codeOrigin, "st.d", [op, Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, offset))])
        }
    end

    def emitPop(newList, node)
        sp = RegisterID.forName(node.codeOrigin, 'sp')
        size = 8 * node.operands.size
        node.operands.each_with_index {
            | op, index |
            offset = size - 8 * (index + 1)
            newList << Instruction.new(node.codeOrigin, "ld.d", [Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, offset)), op])
        }
        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, size), sp])
    end

    def emitAdditionOperation(newList, node, operation, size)
        operands = node.operands
        if operands.size == 2
            operands = [operands[1], operands[0], operands[1]]
        end
        if loongarch64OperandTypes(operands) == [Immediate, RegisterID, RegisterID]
            raise "Invalid subtraction pattern" if operation == :sub
            operands = [operands[1], operands[0], operands[2]]
        end
        loongarch64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID], [RegisterID, Immediate, RegisterID])

        case operation
        when :add, :sub
            additionOpcode = operation.to_s
        else
            raise "Invalid operation #{operation}"
        end

        raise "Invalid subtraction of immediate" if operands[1].is_a? Immediate and operation == :sub
        additionOpcode += ((operands[1].is_a? Immediate) ? "i" : "") + (size == :i ? ".w" : ".d")
        newList << Instruction.new(node.codeOrigin, "#{additionOpcode}", operands)
        loongarch64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    def emitMultiplicationOperation(newList, node, operation, size, signedness)
        operands = node.operands
        if operands.size == 2
            operands = [operands[1], operands[0], operands[1]]
        end
        if loongarch64OperandTypes(operands) == [Immediate, RegisterID, RegisterID]
            raise "Invalid division/remainder pattern" if [:div, :rem].include? operation
            operands = [operands[1], operands[0], operands[2]]
        end
        loongarch64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID], [RegisterID, Immediate, RegisterID])

        case operation
        when :mul
            multiplicationOpcode = "mul"
        when :div
            multiplicationOpcode = operation.to_s
        when :rem
            multiplicationOpcode = "mod"
        else
            raise "Invalid operation #{operation}"
        end

        multiplicationOpcode += (size == :i ? ".w" : ".d")

        case operation
        when :mul
        when :div, :rem
            multiplicationOpcode += (signedness != :s ? "u" : "")
        else
            raise "Invalid operation #{operation}"
        end

        newList << Instruction.new(node.codeOrigin, "#{multiplicationOpcode}", operands)
        loongarch64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    def emitShiftOperation(newList, node, operation, size)
        operands = node.operands
        if operands.size == 2
            operands = [operands[1], operands[0], operands[1]]
        end
        loongarch64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID], [RegisterID, Immediate, RegisterID])

        case operation
        when :l
            shiftOpcode = "sll"
        when :r
            shiftOpcode = "sra"
        when :ur
            shiftOpcode = "srl"
        else
            raise "Invalid operation #{operation}"
        end

        shiftOpcode += ((operands[1].is_a? Immediate) ? "i" : "") + (size == :i ? ".w" : ".d")
        newList << Instruction.new(node.codeOrigin, "#{shiftOpcode}", operands)
        loongarch64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    def emitRotateOperation(newList, node, direction, size)
        loongarch64ValidateOperands(node.operands, [RegisterID, RegisterID])

        lhs = node.operands[1]
        rhs = node.operands[0]

        case size
        when :i
            bits = 32
            suffix = "w"
        when :q
            bits = 64
            suffix = "d"
        end

        inverseAmount = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, bits), inverseAmount])
        realAmount = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "mod.#{suffix}", [rhs, inverseAmount, realAmount])
        newList << Instruction.new(node.codeOrigin, "sub.#{suffix}", [inverseAmount, realAmount, inverseAmount])
        leftRegister = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "sll.#{suffix}", [lhs, direction == :l ? realAmount : inverseAmount, leftRegister])
        rightRegister = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "srl.#{suffix}", [lhs, direction == :l ? inverseAmount : realAmount, rightRegister])
        newList << Instruction.new(node.codeOrigin, "or", [leftRegister, rightRegister, lhs])
    end

    def emitLogicalOperation(newList, node, operation, size)
        operands = node.operands
        if operands.size == 2
            operands = [operands[1], operands[0], operands[1]]
        end
        loongarch64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID], [RegisterID, Immediate, RegisterID])

        case operation
        when :and, :or, :xor
            logicalOpcode = operation.to_s
        else
            raise "Invalid operation #{operation}"
        end

        if operands[1].is_a? Immediate
            if operands[1].value < 0
                rscratch = SpecialRegister.new("$r19")
                newList << Instruction.new(node.codeOrigin, "li.d", [operands[1], rscratch])
                operands = [operands[0], rscratch, operands[2]]
            else
                logicalOpcode += "i"
            end
        end
        newList << Instruction.new(node.codeOrigin, "#{logicalOpcode}", operands)
        loongarch64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    def emitComplementOperation(newList, node, operation, size)
        loongarch64ValidateOperands(node.operands, [RegisterID])

        zero = SpecialRegister.new("$r0")
        rscratch  = SpecialRegister.new("$r20")
        case operation
        when :neg
            complementOpcode = size == :i ? "sub.w" : "sub.d"
            newList << Instruction.new(node.codeOrigin, "#{complementOpcode}", [zero, node.operands[0], node.operands[0]])
        when :not
            complementOpcode = "xor"
            newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, -1), rscratch])
            newList << Instruction.new(node.codeOrigin, "#{complementOpcode}", [rscratch, node.operands[0], node.operands[0]])
        else
            raise "Invalid operation #{operation}"
        end

        loongarch64LowerEmitMask(newList, node, size, node.operands[0], node.operands[0])
    end

    def emitBitExtensionOperation(newList, node, extension, fromSize, toSize)
        raise "Invalid operand types" unless loongarch64OperandTypes(node.operands) == [RegisterID, RegisterID]

        if [[:s, :i, :p], [:s, :i, :q]].include? [extension, fromSize, toSize]
            newList << Instruction.new(node.codeOrigin, "addi.w", [node.operands[0], Immediate.new(node.codeOrigin, 0), node.operands[1]])
            return
        end

        source = node.operands[0]
        dest = node.operands[1]

        if [[:z, :i, :p], [:z, :i, :q]].include? [extension, fromSize, toSize]
            newList << Instruction.new(node.codeOrigin, "slli.d", [source, Immediate.new(node.codeOrigin, 32), dest])
            newList << Instruction.new(node.codeOrigin, "srli.d", [dest, Immediate.new(node.codeOrigin, 32), dest])
            return
        end

        raise "Invalid zero extension" unless extension == :s
        case [fromSize, toSize]
        when [:b, :i]
            newList << Instruction.new(node.codeOrigin, "slli.d", [source, Immediate.new(node.codeOrigin, 56), dest])
            newList << Instruction.new(node.codeOrigin, "srai.d", [dest, Immediate.new(node.codeOrigin, 24), dest])
            newList << Instruction.new(node.codeOrigin, "srli.d", [dest, Immediate.new(node.codeOrigin, 32), dest])
        when [:b, :q]
            newList << Instruction.new(node.codeOrigin, "slli.d", [source, Immediate.new(node.codeOrigin, 56), dest])
            newList << Instruction.new(node.codeOrigin, "srai.d", [dest, Immediate.new(node.codeOrigin, 56), dest])
        when [:h, :i]
            newList << Instruction.new(node.codeOrigin, "slli.d", [source, Immediate.new(node.codeOrigin, 48), dest])
            newList << Instruction.new(node.codeOrigin, "srai.d", [dest, Immediate.new(node.codeOrigin, 16), dest])
            newList << Instruction.new(node.codeOrigin, "srli.d", [dest, Immediate.new(node.codeOrigin, 32), dest])
        when [:h, :q]
            newList << Instruction.new(node.codeOrigin, "slli.d", [source, Immediate.new(node.codeOrigin, 48), dest])
            newList << Instruction.new(node.codeOrigin, "srai.d", [dest, Immediate.new(node.codeOrigin, 48), dest])
        else
            raise "Invalid bit-extension combination"
        end
    end

    def emitZeroCountOperation(newList, node, side, size)
        loongarch64ValidateOperands(node.operands, [RegisterID, RegisterID])

        from = node.operands[0]
        to = node.operands[1]

        case size
        when :i
            bits = 32
            suffix = "w"
        when :q
            bits = 64
            suffix = "d"
        end

        count = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "xor", [count, count, count])
        tmp = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, side == :t ? bits : bits - 1), tmp])
        loopLabel = LocalLabel.unique("begin_count_loop")
        newList << loopLabel
        check = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "srl.#{suffix}", [from, side == :t ? count : tmp, check])
        newList << Instruction.new(node.codeOrigin, "andi", [check, Immediate.new(node.codeOrigin, 1), check])
        returnLabel = LocalLabel.unique("return_count")
        newList << Instruction.new(node.codeOrigin, "bgtz", [check, LocalLabelReference.new(node.codeOrigin, returnLabel)])
        newList << Instruction.new(node.codeOrigin, "addi.#{suffix}", [count, Immediate.new(node.codeOrigin, 1), count])
        case side
        when :t
            newList << Instruction.new(node.codeOrigin, "blt", [count, tmp, LocalLabelReference.new(node.codeOrigin, loopLabel)])
        when :l
            newList << Instruction.new(node.codeOrigin, "addi.#{suffix}", [tmp, Immediate.new(node.codeOrigin, -1), tmp])
            newList << Instruction.new(node.codeOrigin, "bgez", [tmp, LocalLabelReference.new(node.codeOrigin, loopLabel)])
        end
        newList << returnLabel
        newList << Instruction.new(node.codeOrigin, "move", [count, to])
    end

    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^load(b|bsi|bsq|h||hsi|hsq|i|is|p|q)$/
                emitLoadOperation(newList, node, $1.to_sym)
            when /^store(b|h|i|p|q)$/
                emitStoreOperation(newList, node, $1.to_sym)
            when "move"
                emitMove(newList, node)
            when "jmp"
                emitJump(newList, node)
            when "call"
                emitCall(newList, node)
            when "push"
                emitPush(newList, node)
            when "pop"
                emitPop(newList, node)
            when /^(add|sub)(i|p|q)$/
                emitAdditionOperation(newList, node, $1.to_sym, $2.to_sym)
            when /^(mul|div|rem)(i|p|q)(s?)$/
                emitMultiplicationOperation(newList, node, $1.to_sym, $2.to_sym, $3.to_sym)
            when /^(l|r)rotate(i|q)$/
                emitRotateOperation(newList, node, $1.to_sym, $2.to_sym)
            when /^(l|r|ur)shift(i|p|q)$/
                emitShiftOperation(newList, node, $1.to_sym, $2.to_sym)
            when /^(and|or|xor)(h|i|p|q)$/
                emitLogicalOperation(newList, node, $1.to_sym, $2.to_sym)
            when /^(neg|not)(i|p|q)$/
                emitComplementOperation(newList, node, $1.to_sym, $2.to_sym)
            when /^(s|z)x(b|h|i)2(i|p|q)$/
                emitBitExtensionOperation(newList, node, $1.to_sym, $2.to_sym, $3.to_sym)
            when /^(t|l)zcnt(i|q)$/
                emitZeroCountOperation(newList, node, $1.to_sym, $2.to_sym)
            when "break"
                # Helpful break 0x5 for gdb.
                newList << Instruction.new(node.codeOrigin, "break", [])
            when "nop", "ret"
                newList << Instruction.new(node.codeOrigin, "#{node.opcode}", [])
            when "memfence"
                newList << Instruction.new(node.codeOrigin, "dbar", [])
            when "fence"
                newList << Instruction.new(node.codeOrigin, "dbar", [])
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def loongarch64LowerTest(list)
    def branchOpcode(test)
        case test
        when :s
            "bltz"
        when :z
            "beqz"
        when :nz
            "bnez"
        else
            raise "Invalid test-branch opcode"
        end
    end

    def setOpcode(test)
        case test
        when :s
            "sltz"
        when :z
            "seqz"
        when :nz
            "snez"
        else
            raise "Invalid test-set opcode"
        end
    end

    def emit(newList, node, size, opcode)
        if node.operands.size == 2
            newList << Instruction.new(node.codeOrigin, "#{opcode}", node.operands)
            return
        end

        if node.operands[0].immediate? and node.operands[0].value == -1
            newList << Instruction.new(node.codeOrigin, "#{opcode}", [node.operands[1], node.operands[2]])
            return
        end

        if node.operands[1].immediate? and node.operands[1].value == -1
            newList << Instruction.new(node.codeOrigin, "#{opcode}", [node.operands[0], node.operands[2]])
            return
        end

        value = node.operands[0]
        mask = node.operands[1]
        if node.operands[0].immediate?
            value = node.operands[1]
            mask = node.operands[0]
        end

        tmp = Tmp.new(node.codeOrigin, :gpr)
        if value.register? and mask.register?
            newList << Instruction.new(node.codeOrigin, "and", [value, mask, tmp])
        else
            newList << Instruction.new(node.codeOrigin, "li.d", [mask, tmp]);
            newList << Instruction.new(node.codeOrigin, "and", [tmp, value, tmp]);
        end

        loongarch64LowerEmitSignExtension(newList, node, size, tmp, tmp)
        newList << Instruction.new(node.codeOrigin, "#{opcode}", [tmp, node.operands[2]])
    end

    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^bt(b|i|p|q)(s|z|nz)$/
                emit(newList, node, $1.to_sym, branchOpcode($2.to_sym))
            when /^t(b|i|p|q)(s|z|nz)$/
                emit(newList, node, $1.to_sym, setOpcode($2.to_sym))
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def loongarch64LowerCompare(list)
    def emit(newList, node, size, comparison)
        lhs = loongarch64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[0], size)
        rhs = loongarch64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[1], size)
        dest = node.operands[2]

        case comparison
        when :eq
            tmp = Tmp.new(node.codeOrigin, :gpr)
            newList << Instruction.new(node.codeOrigin, "sub.d", [lhs, rhs, tmp])
            newList << Instruction.new(node.codeOrigin, "seqz", [tmp, dest])
        when :neq
            tmp = Tmp.new(node.codeOrigin, :gpr)
            newList << Instruction.new(node.codeOrigin, "sub.d", [lhs, rhs, tmp])
            newList << Instruction.new(node.codeOrigin, "snez", [tmp, dest])
        when :a
            newList << Instruction.new(node.codeOrigin, "sltu", [rhs, lhs, dest])
        when :aeq
            newList << Instruction.new(node.codeOrigin, "sltu", [lhs, rhs, dest])
            newList << Instruction.new(node.codeOrigin, "xori", [dest, Immediate.new(node.codeOrigin, 1), dest])
        when :b
            newList << Instruction.new(node.codeOrigin, "sltu", [lhs, rhs, dest])
        when :beq
            newList << Instruction.new(node.codeOrigin, "sltu", [rhs, lhs, dest])
            newList << Instruction.new(node.codeOrigin, "xori", [dest, Immediate.new(node.codeOrigin, 1), dest])
        when :gt
            newList << Instruction.new(node.codeOrigin, "slt", [rhs, lhs, dest])
        when :gteq
            newList << Instruction.new(node.codeOrigin, "slt", [lhs, rhs, dest])
            newList << Instruction.new(node.codeOrigin, "xori", [dest, Immediate.new(node.codeOrigin, 1), dest])
        when :lt
            newList << Instruction.new(node.codeOrigin, "slt", [lhs, rhs, dest])
        when :lteq
            newList << Instruction.new(node.codeOrigin, "slt", [rhs, lhs, dest])
            newList << Instruction.new(node.codeOrigin, "xori", [dest, Immediate.new(node.codeOrigin, 1), dest])
        else
            raise "Invalid comparison #{comparison}"
        end
    end

    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^c(b|i|p|q)(eq|neq|a|aeq|b|beq|gt|gteq|lt|lteq)$/
                emit(newList, node, $1.to_sym, $2.to_sym)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def loongarch64LowerBranch(list)
    def branchOpcode(condition)
        case condition
        # Equal
        when :eq
            "beq"
        # NotEqual
        when :neq
            "bne"
        # Above
        when :a
            "bltu"
        # AboveOrEqual
        when :aeq
            "bgeu"
        # Below
        when :b
            "bltu"
        # BelowOrEqual
        when :beq
            "bgeu"
        # GreaterThan
        when :gt
            "blt"
        # GreaterThanOrEqual
        when :gteq
            "bge"
        # LessThan
        when :lt
            "blt"
        # LessThanOrEqual
        when :lteq
            "bge"
        when :z
            "beqz"
        when :nz
            "bnez"
        when :s
            "bltz"
        else
            raise "Invalid condition #{condition}"
        end
    end

    def emitGeneric(newList, node, size, condition)
        lhs = loongarch64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[0], size)
        rhs = loongarch64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[1], size)
        dest = node.operands[2]

        case condition
        when :a, :beq, :gt, :lteq
            newList << Instruction.new(node.codeOrigin, "#{branchOpcode(condition)}", [rhs, lhs, dest])
        else
            newList << Instruction.new(node.codeOrigin, "#{branchOpcode(condition)}", [lhs, rhs, dest])
        end
    end

    def emitAddition(newList, node, operation, size, condition)
        operands = node.operands
        if operands.size == 3
            operands = [operands[1], operands[0], operands[1], operands[2]]
        end

        loongarch64ValidateOperands(operands,
            [RegisterID, RegisterID, RegisterID, LocalLabelReference],
            [RegisterID, Immediate, RegisterID, LocalLabelReference]);

        case operation
        when :add, :sub
            additionOpcode = operation.to_s + (size == :i ? ".w" : ".d")
        else
            raise "Invalid addition operation"
        end

        lhs = loongarch64LowerOperandIntoRegister(newList, node, operands[0])
        rhs = loongarch64LowerOperandIntoRegister(newList, node, operands[1])
        newList << Instruction.new(node.codeOrigin, "#{additionOpcode}", [lhs, rhs, operands[2]])

        tmp = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "move", [operands[2], tmp])
        loongarch64LowerEmitMask(newList, node, size, operands[2], operands[2])
        newList << Instruction.new(node.codeOrigin, "#{branchOpcode(condition)}", [tmp, operands[3]])
    end

    def emitMultiplication(newList, node, size, condition)
        raise "Invalid size" unless size == :i

        lhs = result = loongarch64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[0], size, :forced_tmp)
        rhs = loongarch64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[1], size, :forced_tmp)
        raise "Invalid lowered-operand type" unless result.is_a? Tmp

        newList << Instruction.new(node.codeOrigin, "mul.d", [lhs, rhs, result])
        loongarch64LowerEmitMask(newList, node, size, result, node.operands[1])
        newList << Instruction.new(node.codeOrigin, "#{branchOpcode(condition)}", [result, node.operands[2]])
    end

    def emitOverflow(newList, node, operation, size)
        raise "Invalid size" unless size == :i

        operands = node.operands
        if operands.size == 3
            operands = [operands[1], operands[0], operands[1], operands[2]]
        end

        loongarch64ValidateOperands(operands,
            [RegisterID, RegisterID, RegisterID, LocalLabelReference],
            [RegisterID, Immediate, RegisterID, LocalLabelReference]);

        case operation
        when :add, :sub, :mul
            operationOpcode = operation.to_s + ".d"
        else
            raise "Invalid operation #{operation}"
        end

        lhs = tmp1 = loongarch64LowerOperandIntoRegisterAndSignExtend(newList, node, operands[0], size, :forced_tmp)
        rhs = tmp2 = loongarch64LowerOperandIntoRegisterAndSignExtend(newList, node, operands[1], size, :forced_tmp)
        raise "Invalid lowered-operand type" unless (tmp1.is_a? Tmp and tmp2.is_a? Tmp)

        newList << Instruction.new(node.codeOrigin, "#{operationOpcode}", [lhs, rhs, tmp1])
        loongarch64LowerEmitMask(newList, node, size, tmp1, operands[2])

        newList << Instruction.new(node.codeOrigin, "addi.w", [tmp1, Immediate.new(node.codeOrigin, 0), tmp2])
        newList << Instruction.new(node.codeOrigin, "bne", [tmp1, tmp2, operands[3]])
    end

    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^b(b|i|p|q)(eq|neq|a|aeq|b|beq|gt|gteq|lt|lteq)$/
                emitGeneric(newList, node, $1.to_sym, $2.to_sym)
            when /^b(add|sub)(i|p|q)(z|nz|s)$/
                emitAddition(newList, node, $1.to_sym, $2.to_sym, $3.to_sym)
            when /^bmul(i)(z|nz|s)$/
                emitMultiplication(newList, node, $1.to_sym, $2.to_sym)
            when /^b(add|sub|mul)(i)o$/
                emitOverflow(newList, node, $1.to_sym, $2.to_sym)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def loongarch64LowerFPOperation(list)
    def emitLoadOperation(newList, node, precision)
        loongarch64ValidateOperands(node.operands, [Address, FPRegisterID])
        case precision
        when :f
            suffix = "s"
        when :d
            suffix = "d"
        else
            raise "Invalid precision #{precision}"
        end

        newList << Instruction.new(node.codeOrigin, "fld.#{suffix}", node.operands)
    end

    def emitStoreOperation(newList, node, precision)
        loongarch64ValidateOperands(node.operands, [FPRegisterID, Address])
        case precision
        when :f
            suffix = "s"
        when :d
          suffix = "d"
        else
            raise "Invalid precision #{precision}"
        end

        newList << Instruction.new(node.codeOrigin, "fst.#{suffix}", node.operands)
    end

    def emitMoveOperation(newList, node, precision)
        loongarch64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID])
        raise "Invalid precision" unless [:f, :d].include? precision
        if precision == :f
            precision = :s
        end

        newList << Instruction.new(node.codeOrigin, "fmov.#{precision.to_s}", node.operands)
    end

    def emitCopyOperation(newList, node, sourceType, destinationType)
        def registerType(type)
            case type
            when :i, :p, :q
                RegisterID
            when :f, :d
                FPRegisterID
            end
        end

        def fpSuffix(type)
            case type
            when :f
                "w"
            when :d
                "d"
            end
        end

        def fr2grSuffix(type)
            case type
            when :f
                "s"
            when :d
                "d"
            end
        end

        loongarch64ValidateOperands(node.operands, [registerType(sourceType), registerType(destinationType)])
        case loongarch64OperandTypes(node.operands)
        when [RegisterID, FPRegisterID]
            fmvOpcode = "movgr2fr.#{fpSuffix(destinationType)}"
        when [FPRegisterID, RegisterID]
            fmvOpcode = "movfr2gr.#{fr2grSuffix(sourceType)}"
        else
            loongarch64RaiseMismatchedOperands
        end

        newList << Instruction.new(node.codeOrigin, fmvOpcode, node.operands)
    end

    def emitComputationalOperation(newList, node, operation, precision)
        loongarch64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID])
        raise "Invalid operation" unless [:add, :sub, :mul, :div, :sqrt, :abs, :neg].include? operation
        raise "Invalid precision" unless [:f, :d].include? precision
        if precision == :f
            precision = :s
        end

        operands = [node.operands[0], node.operands[1]]
        if [:add, :mul].include? operation
            operands = [operands[0], operands[1], operands[1]]
        elsif [:sub, :div].include? operation
            operands = [operands[1], operands[0], operands[1]]
        end
        newList << Instruction.new(node.codeOrigin, "f#{operation.to_s}.#{precision.to_s}", operands)
    end

    def emitBitwiseOperation(newList, node, operation, precision)
        loongarch64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID])
        raise "Invalid operation" unless [:and, :or].include? operation

        case precision
        when :f
            fr2grSuffix = "s"
            gr2frSuffix = "w"
        when :d
            fr2grSuffix = "d"
            gr2frSuffix = "d"
        else
            raise "Invalid precision #{precision}"
        end

        tmp1 = Tmp.new(node.codeOrigin, :gpr)
        tmp2 = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "movfr2gr.#{fr2grSuffix}", [node.operands[0], tmp1])
        newList << Instruction.new(node.codeOrigin, "movfr2gr.#{fr2grSuffix}", [node.operands[1], tmp2])
        newList << Instruction.new(node.codeOrigin, "#{operation.to_s}", [tmp1, tmp2, tmp2])
        newList << Instruction.new(node.codeOrigin, "movgr2fr.#{gr2frSuffix}", [tmp2, node.operands[1]])
    end

    def emitRoundingOperation(newList, node, operation, precision)
        loongarch64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID])

        from = node.operands[0]
        to = node.operands[1]
        case precision
        when :f
            intSuffix = "w"
            fpSuffix = "s"
        when :d
            intSuffix = "l"
            fpSuffix = "d"
        else
            raise "Invalid precision"
        end
        case operation
        when :floor
            roundOpcode = "ftintrm"
        when :ceil
            roundOpcode = "ftintrp"
        when :round
            roundOpcode = "ftintrne"
        when :truncate
            roundOpcode = "ftintrz"
        else
            raise "Invalid rounding mode #{@mode}"
        end

        newList << Instruction.new(node.codeOrigin, "fmov.#{fpSuffix}", [from, to])
        tmp = Tmp.new(node.codeOrigin, :gpr)
        fscratch = SpecialRegister.new("$f23")
        newList << Instruction.new(node.codeOrigin, "fclass.#{fpSuffix}", [from, fscratch])
        newList << Instruction.new(node.codeOrigin, "movfr2gr.#{fpSuffix}", [fscratch, tmp])
        # NaN and infinity
        newList << Instruction.new(node.codeOrigin, "andi", [tmp, Immediate.new(node.codeOrigin, 0x4e), tmp])
        returnLabel = LocalLabel.unique("return_exotic_float")
        newList << Instruction.new(node.codeOrigin, "bnez", [tmp, LocalLabelReference.new(node.codeOrigin, returnLabel)])
        newList << Instruction.new(node.codeOrigin, "#{roundOpcode}.#{intSuffix}.#{fpSuffix}", [from, to])
        newList << Instruction.new(node.codeOrigin, "ffint.#{fpSuffix}.#{intSuffix}", [to, to])
        newList << returnLabel
    end

    def emitConversionOperation(newList, node, sourceType, destinationType, signedness)
        def registerType(type)
            case type
            when :i, :p, :q
                RegisterID
            when :f, :d
                FPRegisterID
            else
                raise "Invalid register type #{type}"
            end
        end

        def fpSuffix(type)
            case type
            when :f
                "s"
            when :d
                "d"
            else
                raise "Invalid type #{type}"
            end
        end

        def intSuffix(type)
            case type
            when :i
                "w"
            when :p
                "l"
            when :q
                "l"
            else
                raise "Invalid type #{type}"
            end
        end

        def gr2frSuffix(type)
            case type
            when :i
                "w"
            when :p
                "d"
            when :q
                "d"
            else
                raise "Invalid type #{type}"
            end
        end

        loongarch64ValidateOperands(node.operands, [registerType(sourceType), registerType(destinationType)])

        zero      = SpecialRegister.new("$r0")
        sp        = RegisterID.forName(node.codeOrigin, 'sp')
        fscratch  = SpecialRegister.new("$f22")
        fscratch2 = SpecialRegister.new("$f23")
        fcc0      = SpecialRegister.new("$fcc0")
        rscratch  = SpecialRegister.new("$r20")
        rscratch2 = SpecialRegister.new("$r19")

        case loongarch64OperandTypes(node.operands)
        when [RegisterID, FPRegisterID]
            fcvtOpcode = "ffint.#{fpSuffix(destinationType)}.#{intSuffix(sourceType)}"

            case signedness
            when :s
                # Int32 or Int64 2 Float or Double
                newList << Instruction.new(node.codeOrigin, "movgr2fr.#{gr2frSuffix(sourceType)}", [node.operands[0], node.operands[1]])
                newList << Instruction.new(node.codeOrigin, fcvtOpcode, [node.operands[1], node.operands[1]])
            else
                # UInt64 to Float or Double
                belowZeroLabel = LocalLabel.unique("uint64_to_float_or_double_below_zero")
                doneLabel = LocalLabel.unique("uint64_to_float_or_double_done")
                newList << Instruction.new(node.codeOrigin, "blt", [node.operands[0], zero, LocalLabelReference.new(node.codeOrigin, belowZeroLabel)])
                newList << Instruction.new(node.codeOrigin, "movgr2fr.#{gr2frSuffix(sourceType)}", [node.operands[0], node.operands[1]])
                newList << Instruction.new(node.codeOrigin, fcvtOpcode, [node.operands[1], node.operands[1]])
                newList << Instruction.new(node.codeOrigin, "b", [LocalLabelReference.new(node.codeOrigin, doneLabel)])
                newList << belowZeroLabel
                newList << Instruction.new(node.codeOrigin, "or", [node.operands[0], zero, rscratch])
                newList << Instruction.new(node.codeOrigin, "andi", [rscratch, Immediate.new(node.codeOrigin, 0x1), rscratch])
                newList << Instruction.new(node.codeOrigin, "or", [node.operands[0], zero, rscratch2])
                newList << Instruction.new(node.codeOrigin, "srli.d", [rscratch2, Immediate.new(node.codeOrigin, 0x1), rscratch2])
                newList << Instruction.new(node.codeOrigin, "or", [rscratch, rscratch2, rscratch])
                newList << Instruction.new(node.codeOrigin, "movgr2fr.#{gr2frSuffix(sourceType)}", [rscratch, node.operands[1]])
                newList << Instruction.new(node.codeOrigin, fcvtOpcode, [node.operands[1], node.operands[1]])
                newList << Instruction.new(node.codeOrigin, "fadd.#{fpSuffix(destinationType)}", [node.operands[1], node.operands[1], node.operands[1]])
                newList << doneLabel
            end

        when [FPRegisterID, RegisterID]
            fcvtOpcode = "ftintrz.#{intSuffix(destinationType)}.#{fpSuffix(sourceType)}"

            case signedness
            when :s
                newList << Instruction.new(node.codeOrigin, fcvtOpcode, [node.operands[0], fscratch])
                newList << Instruction.new(node.codeOrigin, "movfr2gr.#{fpSuffix(sourceType)}", [fscratch, node.operands[1]])
            else
                case sourceType
                when :f
                    case destinationType
                    when :i
                        # Float to Uint32
                        notEqualZeroLabel = LocalLabel.unique("float_to_uint32_not_equal_zero")
                        doneLabel = LocalLabel.unique("float_to_uint32_done")
                        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, -4), sp])
                        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, 0x3b031b014f000000), rscratch])
                        newList << Instruction.new(node.codeOrigin, "st.d", [rscratch, Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, 0))])
                        # fscratch2 = 0x3b031b014f000000 = 2.14748365e+09
                        newList << Instruction.new(node.codeOrigin, "fld.s", [Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, 0)), fscratch2])
                        newList << Instruction.new(node.codeOrigin, "fcmp.sle.s", [fscratch2, node.operands[0], fcc0])
                        newList << Instruction.new(node.codeOrigin, "bcnez", [fcc0, LocalLabelReference.new(node.codeOrigin, notEqualZeroLabel)])
                        newList << Instruction.new(node.codeOrigin, "ftintrz.w.s", [node.operands[0], fscratch])
                        newList << Instruction.new(node.codeOrigin, "movfr2gr.s", [fscratch, node.operands[1]])
                        newList << Instruction.new(node.codeOrigin, "b", [LocalLabelReference.new(node.codeOrigin, doneLabel)])
                        newList << notEqualZeroLabel
                        newList << Instruction.new(node.codeOrigin, "fmov.s", [node.operands[0], fscratch])
                        newList << Instruction.new(node.codeOrigin, "fsub.s", [fscratch, fscratch2, fscratch])
                        newList << Instruction.new(node.codeOrigin, "ftintrz.w.s", [fscratch, fscratch])
                        newList << Instruction.new(node.codeOrigin, "movfr2gr.s", [fscratch, node.operands[1]])
                        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, 0xffffffff80000000), rscratch])
                        newList << Instruction.new(node.codeOrigin, "or", [node.operands[1], rscratch, node.operands[1]])
                        newList << doneLabel
                        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, 4), sp])

                    when :q
                        # Float to Uint64
                        notEqualZeroLabel = LocalLabel.unique("float_to_uint64_not_equal_zero")
                        doneLabel = LocalLabel.unique("float_to_uint64_done")
                        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, -4), sp])
                        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, 0x3b031b015f000000), rscratch])
                        newList << Instruction.new(node.codeOrigin, "st.d", [rscratch, Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, 0))])
                        # fscratch2 = 0x3b031b015f000000 = 9.22337204e+18
                        newList << Instruction.new(node.codeOrigin, "fld.s", [Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, 0)), fscratch2])
                        newList << Instruction.new(node.codeOrigin, "fcmp.sle.s", [fscratch2, node.operands[0], fcc0])
                        newList << Instruction.new(node.codeOrigin, "bcnez", [fcc0, LocalLabelReference.new(node.codeOrigin, notEqualZeroLabel)])
                        newList << Instruction.new(node.codeOrigin, "ftintrz.l.s", [node.operands[0], fscratch])
                        newList << Instruction.new(node.codeOrigin, "movfr2gr.d", [fscratch, node.operands[1]])
                        newList << Instruction.new(node.codeOrigin, "b", [LocalLabelReference.new(node.codeOrigin, doneLabel)])
                        newList << notEqualZeroLabel
                        newList << Instruction.new(node.codeOrigin, "fmov.s", [node.operands[0], fscratch])
                        newList << Instruction.new(node.codeOrigin, "fsub.s", [fscratch, fscratch2, fscratch])
                        newList << Instruction.new(node.codeOrigin, "ftintrz.l.s", [fscratch, fscratch])
                        newList << Instruction.new(node.codeOrigin, "movfr2gr.d", [fscratch, node.operands[1]])
                        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, 0x8000000000000000), rscratch])
                        newList << Instruction.new(node.codeOrigin, "or", [node.operands[1], rscratch, node.operands[1]])
                        newList << doneLabel
                        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, 4), sp])
                    else
                        raise "Invalid type #{destinationType}"
                    end

                when :d
                    case destinationType
                    when :i
                        # Double to Uint32
                        notEqualZeroLabel = LocalLabel.unique("double_to_uint32_not_equal_zero")
                        doneLabel = LocalLabel.unique("double_to_uint32_done")
                        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, -4), sp])
                        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, 0x41e0000000000000), rscratch])
                        newList << Instruction.new(node.codeOrigin, "st.d", [rscratch, Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, 0))])
                        # fscratch2 = 0x41e0000000000000 = 2147483648.0
                        newList << Instruction.new(node.codeOrigin, "fld.d", [Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, 0)), fscratch2])
                        newList << Instruction.new(node.codeOrigin, "fcmp.sle.d", [fscratch2, node.operands[0], fcc0])
                        newList << Instruction.new(node.codeOrigin, "bcnez", [fcc0, LocalLabelReference.new(node.codeOrigin, notEqualZeroLabel)])
                        newList << Instruction.new(node.codeOrigin, "ftintrz.w.d", [node.operands[0], fscratch])
                        newList << Instruction.new(node.codeOrigin, "movfr2gr.s", [fscratch, node.operands[1]])
                        newList << Instruction.new(node.codeOrigin, "b", [LocalLabelReference.new(node.codeOrigin, doneLabel)])
                        newList << notEqualZeroLabel
                        newList << Instruction.new(node.codeOrigin, "fmov.d", [node.operands[0], fscratch])
                        newList << Instruction.new(node.codeOrigin, "fsub.d", [fscratch, fscratch2, fscratch])
                        newList << Instruction.new(node.codeOrigin, "ftintrz.w.d", [fscratch, fscratch])
                        newList << Instruction.new(node.codeOrigin, "movfr2gr.s", [fscratch, node.operands[1]])
                        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, 0xffffffff80000000), rscratch])
                        newList << Instruction.new(node.codeOrigin, "or", [node.operands[1], rscratch, node.operands[1]])
                        newList << doneLabel
                        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, 4), sp])

                    when :q
                        # Double to Uint64
                        notEqualZeroLabel = LocalLabel.unique("double_to_uint64_not_equal_zero")
                        doneLabel = LocalLabel.unique("double_to_uint64_done")
                        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, -4), sp])
                        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, 0x43e0000000000000), rscratch])
                        newList << Instruction.new(node.codeOrigin, "st.d", [rscratch, Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, 0))])
                        # fscratch2 = 0x43e0000000000000 = 9.2233720368547758e+18
                        newList << Instruction.new(node.codeOrigin, "fld.d", [Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, 0)), fscratch2])
                        newList << Instruction.new(node.codeOrigin, "fcmp.sle.d", [fscratch2, node.operands[0], fcc0])
                        newList << Instruction.new(node.codeOrigin, "bcnez", [fcc0, LocalLabelReference.new(node.codeOrigin, notEqualZeroLabel)])
                        newList << Instruction.new(node.codeOrigin, "ftintrz.l.d", [node.operands[0], fscratch])
                        newList << Instruction.new(node.codeOrigin, "movfr2gr.d", [fscratch, node.operands[1]])
                        newList << Instruction.new(node.codeOrigin, "b", [LocalLabelReference.new(node.codeOrigin, doneLabel)])
                        newList << notEqualZeroLabel
                        newList << Instruction.new(node.codeOrigin, "fmov.d", [node.operands[0], fscratch])
                        newList << Instruction.new(node.codeOrigin, "fsub.d", [fscratch, fscratch2, fscratch])
                        newList << Instruction.new(node.codeOrigin, "ftintrz.l.d", [fscratch, fscratch])
                        newList << Instruction.new(node.codeOrigin, "movfr2gr.d", [fscratch, node.operands[1]])
                        newList << Instruction.new(node.codeOrigin, "li.d", [Immediate.new(node.codeOrigin, 0x8000000000000000), rscratch])
                        newList << Instruction.new(node.codeOrigin, "or", [node.operands[1], rscratch, node.operands[1]])
                        newList << doneLabel
                        newList << Instruction.new(node.codeOrigin, "addi.d", [sp, Immediate.new(node.codeOrigin, 4), sp])
                    else
                        raise "Invalid type #{destinationType}"
                    end
                else
                    raise "Invalid type #{sourceType}"
                end
            end

        when [FPRegisterID, FPRegisterID]
            fcvtOpcode = "fcvt.#{fpSuffix(destinationType)}.#{fpSuffix(sourceType)}"
            newList << Instruction.new(node.codeOrigin, fcvtOpcode, [node.operands[0], node.operands[1]])
        else
            loongarch64RaiseMismatchedOperands(node.operands)
        end
    end

    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^load(f|d)$/
                emitLoadOperation(newList, node, $1.to_sym)
            when /^store(f|d)$/
                emitStoreOperation(newList, node, $1.to_sym)
            when /^move(d)$/
                emitMoveOperation(newList, node, $1.to_sym)
            when /^f(i|p|q|f|d)2(i|p|q|f|d)$/
                emitCopyOperation(newList, node, $1.to_sym, $2.to_sym)
            when /^(add|sub|mul|div|sqrt|abs|neg)(f|d)$/
                emitComputationalOperation(newList, node, $1.to_sym, $2.to_sym)
            when /^(and|or)(f|d)$/
                emitBitwiseOperation(newList, node, $1.to_sym, $2.to_sym)
            when /^(floor|ceil|round|truncate)(f|d)$/
                emitRoundingOperation(newList, node, $1.to_sym, $2.to_sym)
            when /^truncate(f|d)2(i|q)(s?)$/
                emitConversionOperation(newList, node, $1.to_sym, $2.to_sym, $3.to_sym)
            when /^c(i|q|f|d)2(f|d)(s?)$/
                emitConversionOperation(newList, node, $1.to_sym, $2.to_sym, $3.to_sym)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def loongarch64LowerFPCompare(list)
    def emitCompare(newList, node, precision, compareOp, lhs, rhs)
        case precision
        when :f
            precisionSuffix = "s"
        when :d
            precisionSuffix = "d"
        else
            raise "Invalid precision #{precision}"
        end

        fcc0 = SpecialRegister.new("$fcc0")
        newList << Instruction.new(node.codeOrigin, "#{compareOp}.#{precisionSuffix}", [lhs, rhs, fcc0])
        newList << Instruction.new(node.codeOrigin, "movcf2gr", [fcc0, node.operands[2]])
    end

    def emit(newList, node, precision, condition)
        loongarch64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID, RegisterID])
        operands = node.operands

        case condition
        when :eq, :equn
            emitCompare(newList, node, precision, "fcmp.ceq", operands[0], operands[1])
        when :neq, :nequn
            emitCompare(newList, node, precision, "fcmp.ceq", operands[0], operands[1])
            newList << Instruction.new(node.codeOrigin, "xori", [operands[2], Immediate.new(node.codeOrigin, 1), operands[2]])
        when :gt
            emitCompare(newList, node, precision, "fcmp.clt", operands[1], operands[0])
        when :gteq
            emitCompare(newList, node, precision, "fcmp.cle", operands[1], operands[0])
        when :lt
            emitCompare(newList, node, precision, "fcmp.clt", operands[0], operands[1])
        when :lteq
            emitCompare(newList, node, precision, "fcmp.cle", operands[0], operands[1])
        else
            raise "Invalid condition #{condition}"
        end
    end

    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^c(f|d)(eq|equn|neq|nequn|gt|gteq|lt|lteq)$/
                emit(newList, node, $1.to_sym, $2.to_sym)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def loongarch64LowerFPBranch(list)
    def precisionSuffix(precision)
        case precision
        when :f
            "s"
        when :d
            "d"
        else
            raise "Invalid precision"
        end
    end

    def emitBranchForUnordered(newList, node, precision)
        tmp1 = Tmp.new(node.codeOrigin, :gpr)
        tmp2 = Tmp.new(node.codeOrigin, :gpr)

        fscratch = SpecialRegister.new("$f23")
        newList << Instruction.new(node.codeOrigin, "fclass.#{precisionSuffix(precision)}", [node.operands[0], fscratch])
        newList << Instruction.new(node.codeOrigin, "movfr2gr.#{precisionSuffix(precision)}", [fscratch, tmp1])
        newList << Instruction.new(node.codeOrigin, "fclass.#{precisionSuffix(precision)}", [node.operands[1], fscratch])
        newList << Instruction.new(node.codeOrigin, "movfr2gr.#{precisionSuffix(precision)}", [fscratch, tmp2])
        newList << Instruction.new(node.codeOrigin, "or", [tmp1, tmp2, tmp2])
        # NaN
        newList << Instruction.new(node.codeOrigin, "andi", [tmp2, Immediate.new(node.codeOrigin, 0x3), tmp2])
        newList << Instruction.new(node.codeOrigin, "bnez", [tmp2, node.operands[2]])
    end

    def emitBranchForTest(newList, node, precision, testOpcode, lhs, rhs, branchOpcode)
        tmp = Tmp.new(node.codeOrigin, :gpr)
        fcc0 = SpecialRegister.new("$fcc0")
        newList << Instruction.new(node.codeOrigin, "#{testOpcode}.#{precisionSuffix(precision)}", [lhs, rhs, fcc0])
        newList << Instruction.new(node.codeOrigin, "movcf2gr", [fcc0, tmp])
        newList << Instruction.new(node.codeOrigin, "#{branchOpcode}", [tmp, node.operands[2]])
    end

    def emit(newList, node, precision, condition)
        loongarch64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID, LocalLabelReference])
        operands = node.operands

        if [:equn, :nequn, :gtun, :gtequn, :ltun, :ltequn].include? condition
            emitBranchForUnordered(newList, node, precision)
        end

        case condition
        when :eq, :equn
            emitBranchForTest(newList, node, precision, "fcmp.ceq", operands[0], operands[1], "bnez")
        when :neq, :nequn
            emitBranchForTest(newList, node, precision, "fcmp.ceq", operands[0], operands[1], "beqz")
        when :gt, :gtun
            emitBranchForTest(newList, node, precision, "fcmp.clt", operands[1], operands[0], "bnez")
        when :gteq, :gtequn
            emitBranchForTest(newList, node, precision, "fcmp.cle", operands[1], operands[0], "bnez")
        when :lt, :ltun
            emitBranchForTest(newList, node, precision, "fcmp.clt", operands[0], operands[1], "bnez")
        when :lteq, :ltequn
            emitBranchForTest(newList, node, precision, "fcmp.cle", operands[0], operands[1], "bnez")
        else
            raise "Invalid condition"
        end
    end

    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^b(f|d)(eq|neq|gt|gteq|lt|lteq|equn|nequn|gtun|gtequn|ltun|ltequn)$/
                emit(newList, node, $1.to_sym, $2.to_sym)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def loongarch64GenerateWASMPlaceholders(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "loadlinkacqb", "loadlinkacqh", "loadlinkacqi", "loadlinkacqq",
                 "storecondrelb", "storecondrelh", "storecondreli", "storecondrelq",
                 "loadv", "storev"
                newList << Instruction.new(node.codeOrigin, "break", [], "WebAssembly placeholder for opcode #{node.opcode}")
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

class Sequence
    def getModifiedListLOONGARCH64
        result = @list

        result = riscDropTags(result)
        result = riscLowerMalformedAddresses(result) {
            | node, address |
            if address.is_a? Address
                !address.loongarch64RequiresLoad
            else
                false
            end
        }
        result = loongarch64LowerMisplacedAddresses(result)
        result = riscLowerMisplacedAddresses(result)
        result = loongarch64LowerAddressLoads(result)

        result = riscLowerMisplacedImmediates(result, ["storeb", "storeh", "storei", "storep", "storeq"])
        result = riscLowerMalformedImmediates(result, -0x800..0x7ff, -0x800..0x7ff)
        result = loongarch64LowerImmediateSubtraction(result)

        result = loongarch64LowerOperation(result)
        result = loongarch64LowerTest(result)
        result = loongarch64LowerCompare(result)
        result = loongarch64LowerBranch(result)

        result = loongarch64LowerFPOperation(result)
        result = loongarch64LowerFPCompare(result)
        result = loongarch64LowerFPBranch(result)

        result = loongarch64GenerateWASMPlaceholders(result)

        result = assignRegistersToTemporaries(result, :gpr, LOONGARCH64_EXTRA_GPRS)
        result = assignRegistersToTemporaries(result, :fpr, LOONGARCH64_EXTRA_FPRS)
        return result
    end
end

class Instruction
    def laop(opcode)
        opcode[/^(.+)/, 1]
    end

    def lowerLOONGARCH64
        case opcode
        when "jr"
            loongarch64ValidateOperands(operands, [RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[0].loongarch64Operand}"
        when "jalr"
            loongarch64ValidateOperands(operands, [RegisterID])
            $asm.puts "jirl $r1, #{operands[0].loongarch64Operand}, 0"
        when /^(bl|b)$/
            loongarch64ValidateOperands(operands, [LabelReference], [LocalLabelReference])
            $asm.puts "#{laop(opcode)} #{operands[0].asmLabel}"
        when /^(la|la.local)$/
            loongarch64ValidateOperands(operands, [LabelReference, RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].asmLabel}"
        when "move"
            loongarch64ValidateOperands(operands, [RegisterID, RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^li.(w|d)$/
            loongarch64ValidateOperands(operands, [Immediate, RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand(:any_immediate)}"
        when /^ld.(b|bu|h|hu|w|wu|d)$/
            loongarch64ValidateOperands(operands, [Address, RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^st.(b|h|w|d)$/
            loongarch64ValidateOperands(operands, [RegisterID, Address])
            $asm.puts "#{laop(opcode)} #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}"
        when /^(add\.(w|d)|sub\.(w|d)|and|or|xor|s(ll|rl|ra)\.(w|d)|mul\.(w|d)|div\.(w|d)(u?)|mod\.(w|d)(u?))$/
            loongarch64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[2].loongarch64Operand}, #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}"
        when /^(and|or|xor)i$/
            loongarch64ValidateOperands(operands, [RegisterID, Immediate, RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[2].loongarch64Operand}, #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}"
        when /^addi.(w|d)$/
            loongarch64ValidateOperands(operands, [RegisterID, Immediate, RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[2].loongarch64Operand}, #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}"
        when /^(sll|srl|sra)i\.(w|d)$/
            loongarch64ValidateOperands(operands, [RegisterID, Immediate, RegisterID])
            validationType = $2 == "w" ? :la32_shift_immediate : :la64_shift_immediate
            raise "Invalid shift-amount immediate" unless loongarch64ValidateImmediate(validationType, operands[1].value)
            $asm.puts "#{laop(opcode)} #{operands[2].loongarch64Operand}, #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}"
        when /^slt|slt(u?)$/
            loongarch64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[2].loongarch64Operand}, #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}"
        when "seqz"
            loongarch64ValidateOperands(operands, [RegisterID, RegisterID])
            $asm.puts "sltui #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}, 1"
        when "snez"
            loongarch64ValidateOperands(operands, [RegisterID, RegisterID])
            $asm.puts "sltu #{operands[1].loongarch64Operand}, $r0, #{operands[0].loongarch64Operand}"
        when "sltz"
            loongarch64ValidateOperands(operands, [RegisterID, RegisterID])
            $asm.puts "slt #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}, $r0"
        when "sgtz"
            loongarch64ValidateOperands(operands, [RegisterID, RegisterID])
            $asm.puts "slt #{operands[1].loongarch64Operand}, $r0, #{operands[0].loongarch64Operand}"
        when /^b(eq|ne|ge|geu|lt|ltu)$/
            loongarch64ValidateOperands(operands, [RegisterID, RegisterID, LocalLabelReference], [RegisterID, RegisterID, LabelReference])
            $asm.puts "#{laop(opcode)} #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}, #{operands[2].asmLabel}"
        when /^b(eqz|nez|lez|ltz|gez|gtz)$/
            loongarch64ValidateOperands(operands, [RegisterID, LocalLabelReference], [RegisterID, LabelReference])
            $asm.puts "#{laop(opcode)} #{operands[0].loongarch64Operand}, #{operands[1].asmLabel}"
        when "bcnez"
            loongarch64ValidateOperands(operands, [FPRegisterID, LocalLabelReference])
            $asm.puts "#{laop(opcode)} #{operands[0].loongarch64Operand}, #{operands[1].asmLabel}"
        when "nop"
            $asm.puts "#{laop(opcode)}"
        when "ret"
            $asm.puts "jr $r1"
        when "break"
            $asm.puts "#{laop(opcode)} 0x5"
        when "dbar"
            $asm.puts "#{laop(opcode)} 0x0"
        when /^fld.(s|d)$/
            loongarch64ValidateOperands(operands, [Address, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^fst.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, Address])
            $asm.puts "#{laop(opcode)} #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}"
        when /^fmov\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^movgr2fr\.(w|d)$/
            loongarch64ValidateOperands(operands, [RegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^movfr2gr\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, RegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when "movcf2gr"
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^f(add|sub|mul|div)\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[2].loongarch64Operand}, #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}"
        when /^f(sqrt|abs|neg)\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^fcmp\.(c(eq|lt|le)|sle)\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[2].loongarch64Operand}, #{operands[0].loongarch64Operand}, #{operands[1].loongarch64Operand}"
        when /^ffint\.(s|d)\.(w|l)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^ftint\.(w|l)\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^ftintrz\.(w|l)\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^ftintr(m|p|ne|z)\.(w|l)\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^fcvt\.(s|d)\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        when /^fclass\.(s|d)$/
            loongarch64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{laop(opcode)} #{operands[1].loongarch64Operand}, #{operands[0].loongarch64Operand}"
        else
            lowerDefault
        end
    end
end
