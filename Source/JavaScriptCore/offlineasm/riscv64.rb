# Copyright (C) 2021 Igalia S.L.
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
# x<number> => GPR, used to operate with 32-bit and 64-bit integer values
# f<number> => FPR, used to operate with 32-bit and 64-bit floating-point values
#
# GPR conventions, to match the baseline JIT:
#
# x0  => not used (except where needed for operations) (RISC-V hard-wired zero register)
# x1  => la (through alias ra) (RISC-V return address register)
# x2  => sp (through alias sp) (RISC-V stack pointer register)
# x3  => not used (RISC-V global pointer register)
# x4  => not used (RISC-V thread pointer register)
# x5  => not used
# x6  => ws0
# x7  => ws1
# x8  => cfr (through alias fp) (RISC-V frame pointer register)
# x9  => csr0
# x10 => t0, a0, wa0, r0
# x11 => t1, a1, wa1, r1
# x12 => t2, a2, wa2
# x13 => t3, a3, wa3
# x14 => t4, a4, wa4
# x15 => t5, a5, wa5
# x16 => t6, a6, wa6
# x17 => t7, a7, wa7
# x18 => csr1
# x19 => csr2
# x20 => csr3
# x21 => csr4
# x22 => csr5
# x23 => csr6 (metadataTable)
# x24 => csr7 (PB)
# x25 => csr8 (numberTag)
# x26 => csr9 (notCellMask)
# x27 => csr10
# x28 => scratch register
# x29 => scratch register
# x30 => scratch register
# x31 => scratch register
#
# FPR conventions, to match the baseline JIT:
#
# f0  => ft0
# f1  => ft1
# f2  => ft2
# f3  => ft3
# f4  => ft4
# f5  => ft5
# f6  => not used
# f7  => not used
# f8  => csfr0
# f9  => csfr1
# f10 => fa0, wfa0
# f11 => fa1, wfa1
# f12 => fa2, wfa2
# f13 => fa3, wfa3
# f14 => fa4, wfa4
# f15 => fa5, wfa5
# f16 => fa6, wfa6
# f17 => fa7, wfa7
# f18 => csfr2
# f19 => csfr3
# f20 => csfr4
# f21 => csfr5
# f22 => csfr6
# f23 => csfr7
# f24 => csfr8
# f25 => csfr9
# f26 => csfr10
# f27 => csfr11
# f28 => scratch register
# f29 => scratch register
# f30 => scratch register
# f31 => scratch register

RISCV64_EXTRA_GPRS = [SpecialRegister.new("x28"), SpecialRegister.new("x29"), SpecialRegister.new("x30"), SpecialRegister.new("x31")]
RISCV64_EXTRA_FPRS = [SpecialRegister.new("f28"), SpecialRegister.new("f29"), SpecialRegister.new("f30"), SpecialRegister.new("f31")]


def riscv64OperandTypes(operands)
    return operands.map {
        |op|
        if op.is_a? SpecialRegister
            case op.name
            when /^x/
                RegisterID
            when /^f/
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

def riscv64RaiseMismatchedOperands(operands)
    raise "Unable to match operands #{riscv64OperandTypes(operands)}"
end

def riscv64ValidateOperands(operands, *expected)
    riscv64RaiseMismatchedOperands(operands) unless expected.include? riscv64OperandTypes(operands)
end

def riscv64ValidateImmediate(validation, value)
    case validation
    when :i_immediate
        (-0x800..0x7ff).include? value
    when :any_immediate
        true
    when :rv32_shift_immediate
        (0..31).include? value
    when :rv64_shift_immediate
        (0..63).include? value
    else
        raise "Invalid immediate validation #{validation}"
    end
end

class RegisterID
    def riscv64Operand
        case @name
        when 't0', 'a0', 'wa0', 'r0'
            'x10'
        when 't1', 'a1', 'wa1', 'r1'
            'x11'
        when 't2', 'a2', 'wa2'
            'x12'
        when 't3', 'a3', 'wa3'
            'x13'
        when 't4', 'a4', 'wa4'
            'x14'
        when 't5', 'a5', 'wa5'
            'x15'
        when 't6', 'a6', 'wa6'
            'x16'
        when 't7', 'a7', 'wa7'
            'x17'
        when 'ws0'
            'x6'
        when 'ws1'
            'x7'
        when 'csr0'
            'x9'
        when 'csr1'
            'x18'
        when 'csr2'
            'x19'
        when 'csr3'
            'x20'
        when 'csr4'
            'x21'
        when 'csr5'
            'x22'
        when 'csr6'
            'x23'
        when 'csr7'
            'x24'
        when 'csr8'
            'x25'
        when 'csr9'
            'x26'
        when 'csr10'
            'x27'
        when 'lr'
            'ra'
        when 'sp'
            'sp'
        when 'cfr'
            'fp'
        else
            raise "Bad register name #{@name} at #{codeOriginString}"
        end
    end
end

class FPRegisterID
    def riscv64Operand
        case @name
        when 'ft0'
            'f0'
        when 'ft1'
            'f1'
        when 'ft2'
            'f2'
        when 'ft3'
            'f3'
        when 'ft4'
            'f4'
        when 'ft5'
            'f5'
        when 'csfr0'
            'f8'
        when 'csfr1'
            'f9'
        when 'fa0', 'wfa0'
            'f10'
        when 'fa1', 'wfa1'
            'f11'
        when 'fa2', 'wfa2'
            'f12'
        when 'fa3', 'wfa3'
            'f13'
        when 'fa4', 'wfa4'
            'f14'
        when 'fa5', 'wfa5'
            'f15'
        when 'fa6', 'wfa6'
            'f16'
        when 'fa7', 'wfa7'
            'f17'
        when 'csfr2'
            'f18'
        when 'csfr3'
            'f19'
        when 'csfr4'
            'f20'
        when 'csfr5'
            'f21'
        when 'csfr6'
            'f22'
        when 'csfr7'
            'f23'
        when 'csfr8'
            'f24'
        when 'csfr9'
            'f25'
        when 'csfr10'
            'f26'
        when 'csfr11'
            'f27'
        else
            raise "Bad register name #{@name} at #{codeOriginString}"
        end
    end
end

class SpecialRegister
    def riscv64Operand
        @name
    end
end

class Immediate
    def riscv64Operand(validation = :i_immediate)
        raise "Invalid immediate value #{value} at #{codeOriginString}" if riscv64RequiresLoad(validation)
        "#{value}"
    end

    def riscv64RequiresLoad(validation = :i_immediate)
        not riscv64ValidateImmediate(validation, value)
    end
end

class Address
    def riscv64Operand
        raise "Invalid offset #{offset.value} at #{codeOriginString}" if riscv64RequiresLoad
        "#{offset.value}(#{base.riscv64Operand})"
    end

    def riscv64RequiresLoad
        not riscv64ValidateImmediate(:i_immediate, offset.value)
    end
end

class RISCV64RoundingMode < NoChildren
    def initialize(mode)
        @mode = mode
    end

    def riscv64RoundingMode
        case @mode
        when :floor
            "rdn"
        when :ceil
            "rup"
        when :round
            "rne"
        when :truncate
            "rtz"
        else
            raise "Invalid rounding mode #{@mode}"
        end
    end
end

class RISCV64MemoryOrdering < NoChildren
    def initialize(ordering)
        @ordering = ordering
    end

    def riscv64MemoryOrdering
        case @ordering
        when :rw, :iorw
            @ordering.to_s
        else
            raise "Invalid memory ordering #{@ordering}"
        end
    end
end

def riscv64LowerEmitMask(newList, node, size, source, destination)
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
        newList << Instruction.new(node.codeOrigin, "rv_slli", [source, Immediate.new(node.codeOrigin, shiftSize), destination])
        newList << Instruction.new(node.codeOrigin, "rv_srli", [destination, Immediate.new(node.codeOrigin, shiftSize), destination])
    when :p, :q
    else
        raise "Invalid masking size"
    end
end

def riscv64LowerEmitSignExtension(newList, node, size, source, destination)
    case size
    when :b, :h
        case size
        when :b
            shiftSize = 56
        when :h
            shiftSize = 32
        end
        newList << Instruction.new(node.codeOrigin, "rv_slli", [source, Immediate.new(node.codeOrigin, shiftSize), destination])
        newList << Instruction.new(node.codeOrigin, "rv_srai", [destination, Immediate.new(node.codeOrigin, shiftSize), destination])
    when :i
        newList << Instruction.new(node.codeOrigin, "rv_sext.w", [source, destination])
    when :p, :q
    else
        raise "Invalid extension size"
    end
end

def riscv64LowerOperandIntoRegister(newList, node, operand)
    register = operand
    if operand.immediate?
        register = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_li", [operand, register])
    end

    raise "Invalid register type" unless riscv64OperandTypes([register]) == [RegisterID]
    register
end

def riscv64LowerOperandIntoRegisterAndSignExtend(newList, node, operand, size, forcedTmp = :none)
    source = riscv64LowerOperandIntoRegister(newList, node, operand)
    destination = source

    if ([:b, :h, :i].include? size or forcedTmp == :forced_tmp) and not destination.is_a? Tmp
        destination = Tmp.new(node.codeOrigin, :gpr)
    end

    riscv64LowerEmitSignExtension(newList, node, size, source, destination)
    destination
end

def riscv64LowerMisplacedAddresses(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^b(add|sub)i(z|nz|s)$/
                case riscv64OperandTypes(node.operands)
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

def riscv64LowerAddressLoads(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "leap", "leaq"
                case riscv64OperandTypes(node.operands)
                when [Address, RegisterID]
                    address, dest = node.operands[0], node.operands[1]
                    raise "Invalid address" if address.riscv64RequiresLoad
                    newList << Instruction.new(node.codeOrigin, "rv_addi", [address.base, address.offset, dest])
                when [BaseIndex, RegisterID]
                    bi, dest = node.operands[0], node.operands[1]
                    newList << Instruction.new(node.codeOrigin, "rv_slli", [bi.index, Immediate.new(node.codeOrigin, bi.scaleShift), dest])
                    newList << Instruction.new(node.codeOrigin, "rv_add", [dest, bi.base, dest])
                    if bi.offset.value != 0
                        offset = Immediate.new(node.codeOrigin, bi.offset.value)
                        if offset.riscv64RequiresLoad
                            tmp = Tmp.new(node.codeOrigin, :gpr)
                            newList << Instruction.new(node.codeOrigin, "rv_li", [offset, tmp])
                            newList << Instruction.new(node.codeOrigin, "rv_add", [dest, tmp, dest])
                        else
                            newList << Instruction.new(node.codeOrigin, "rv_addi", [dest, offset, dest])
                        end
                    end
                when [LabelReference, RegisterID]
                    label, dest = node.operands[0], node.operands[1]
                    newList << Instruction.new(node.codeOrigin, "rv_la", [label, dest])
                    if label.offset != 0
                        offset = Immediate.new(node.codeOrigin, label.offset)
                        if offset.riscv64RequiresLoad
                            tmp = Tmp.new(node.codeOrigin, :gpr)
                            newList << Instruction.new(node.codeOrigin, "rv_li", [offset, tmp])
                            newList << Instruction.new(node.codeOrigin, "rv_add", [dest, tmp, dest])
                        else
                            newList << Instruction.new(node.codeOrigin, "rv_addi", [dest, offset, dest])
                        end
                    end
                else
                    riscv64RaiseMismatchedOperands(node.operands)
                end
            when "globaladdr"
                riscv64ValidateOperands(node.operands, [LabelReference, RegisterID])
                newList << Instruction.new(node.codeOrigin, "rv_la", node.operands)
            when "pcrtoaddr"
                riscv64ValidateOperands(node.operands, [LabelReference, RegisterID])
                newList << Instruction.new(node.codeOrigin, "rv_lla", node.operands)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def riscv64LowerImmediateSubtraction(list)
    def emit(newList, node, size, operands)
        riscv64ValidateOperands(operands, [RegisterID, Immediate, RegisterID])
        nimmediate = Immediate.new(node.codeOrigin, -operands[1].value)
        if nimmediate.riscv64RequiresLoad
            tmp = Tmp.new(node.codeOrigin, :gpr)
            newList << Instruction.new(node.codeOrigin, "rv_li", [operands[1], tmp])
            newList << Instruction.new(node.codeOrigin, "rv_sub", [operands[0], tmp, operands[2]])
        else
            newList << Instruction.new(node.codeOrigin, "rv_addi", [operands[0], nimmediate, operands[2]])
        end
        riscv64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^sub(i|p|q)$/
                case riscv64OperandTypes(node.operands)
                when [RegisterID, Immediate, RegisterID]
                    emit(newList, node, $1.to_sym, node.operands)
                when [Immediate, RegisterID]
                    emit(newList, node, $1.to_sym, [node.operands[1], node.operands[0], node.operands[1]])
                else
                    raise "Invalid immediate subtraction pattern" if riscv64OperandTypes(node.operands).include? Immediate
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

def riscv64LowerOperation(list)
    def emitLoadOperation(newList, node, size)
        riscv64ValidateOperands(node.operands, [Address, RegisterID])

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

        newList << Instruction.new(node.codeOrigin, "rv_l#{suffix}", node.operands)

        case size
        when :bsi, :hsi
            riscv64LowerEmitMask(newList, node, :i, node.operands[1], node.operands[1])
        when :bsq, :hsq
            # Nothing to do
        end
    end

    def emitStoreOperation(newList, node, size)
        riscv64ValidateOperands(node.operands, [RegisterID, Address])

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

        newList << Instruction.new(node.codeOrigin, "rv_s#{suffix}", node.operands)
    end

    def emitMove(newList, node)
        case riscv64OperandTypes(node.operands)
        when [RegisterID, RegisterID]
            moveOpcode = "mv"
        when [Immediate, RegisterID]
            moveOpcode = "li"
        else
            riscv64RaiseMismatchedOperands(node.operands)
        end

        newList << Instruction.new(node.codeOrigin, "rv_#{moveOpcode}", node.operands)
    end

    def emitJump(newList, node)
        case riscv64OperandTypes(node.operands)
        when [RegisterID]
            jumpOpcode = "jr"
        when [LabelReference], [LocalLabelReference]
            jumpOpcode = "tail"
        else
            riscv64RaiseMismatchedOperands(node.operands)
        end

        newList << Instruction.new(node.codeOrigin, "rv_#{jumpOpcode}", node.operands)
    end

    def emitCall(newList, node)
        case riscv64OperandTypes(node.operands)
        when [RegisterID]
            callOpcode = "jalr"
        when [LabelReference]
            callOpcode = "call"
        else
            riscv64RaiseMismatchedOperands(node.operands)
        end

        newList << Instruction.new(node.codeOrigin, "rv_#{callOpcode}", node.operands)
    end

    def emitPush(newList, node)
        sp = RegisterID.forName(node.codeOrigin, 'sp')
        size = 8 * node.operands.size
        newList << Instruction.new(node.codeOrigin, "rv_addi", [sp, Immediate.new(node.codeOrigin, -size), sp])
        node.operands.reverse.each_with_index {
            | op, index |
            offset = size - 8 * (index + 1)
            newList << Instruction.new(node.codeOrigin, "rv_sd", [op, Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, offset))])
        }
    end

    def emitPop(newList, node)
        sp = RegisterID.forName(node.codeOrigin, 'sp')
        size = 8 * node.operands.size
        node.operands.each_with_index {
            | op, index |
            offset = size - 8 * (index + 1)
            newList << Instruction.new(node.codeOrigin, "rv_ld", [Address.new(node.codeOrigin, sp, Immediate.new(node.codeOrigin, offset)), op])
        }
        newList << Instruction.new(node.codeOrigin, "rv_addi", [sp, Immediate.new(node.codeOrigin, size), sp])
    end

    def emitAdditionOperation(newList, node, operation, size)
        operands = node.operands
        if operands.size == 2
            operands = [operands[1], operands[0], operands[1]]
        end
        if riscv64OperandTypes(operands) == [Immediate, RegisterID, RegisterID]
            raise "Invalid subtraction pattern" if operation == :sub
            operands = [operands[1], operands[0], operands[2]]
        end
        riscv64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID], [RegisterID, Immediate, RegisterID])

        case operation
        when :add, :sub
            additionOpcode = operation.to_s
        else
            raise "Invalid operation #{operation}"
        end

        raise "Invalid subtraction of immediate" if operands[1].is_a? Immediate and operation == :sub
        additionOpcode += ((operands[1].is_a? Immediate) ? "i" : "") + (size == :i ? "w" : "")
        newList << Instruction.new(node.codeOrigin, "rv_#{additionOpcode}", operands)
        riscv64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    def emitMultiplicationOperation(newList, node, operation, size, signedness)
        operands = node.operands
        if operands.size == 2
            operands = [operands[1], operands[0], operands[1]]
        end
        if riscv64OperandTypes(operands) == [Immediate, RegisterID, RegisterID]
            raise "Invalid division/remainder pattern" if [:div, :rem].include? operation
            operands = [operands[1], operands[0], operands[2]]
        end
        riscv64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID], [RegisterID, Immediate, RegisterID])

        case operation
        when :mul
            multiplicationOpcode = "mul"
        when :div, :rem
            multiplicationOpcode = operation.to_s + (signedness != :s ? "u" : "")
        else
            raise "Invalid operation #{operation}"
        end

        multiplicationOpcode += (size == :i ? "w" : "")
        newList << Instruction.new(node.codeOrigin, "rv_#{multiplicationOpcode}", operands)
        riscv64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    def emitShiftOperation(newList, node, operation, size)
        operands = node.operands
        if operands.size == 2
            operands = [operands[1], operands[0], operands[1]]
        end
        riscv64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID], [RegisterID, Immediate, RegisterID])

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

        shiftOpcode += ((operands[1].is_a? Immediate) ? "i" : "") + (size == :i ? "w" : "")
        newList << Instruction.new(node.codeOrigin, "rv_#{shiftOpcode}", operands)
        riscv64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    def emitRotateOperation(newList, node, direction, size)
        riscv64ValidateOperands(node.operands, [RegisterID, RegisterID])

        lhs = node.operands[1]
        rhs = node.operands[0]

        case size
        when :i
            bits = 32
            suffix = "w"
        when :q
            bits = 64
            suffix = ""
        end

        inverseAmount = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_li", [Immediate.new(node.codeOrigin, bits), inverseAmount])
        realAmount = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_rem#{suffix}", [rhs, inverseAmount, realAmount])
        newList << Instruction.new(node.codeOrigin, "rv_sub#{suffix}", [inverseAmount, realAmount, inverseAmount])
        leftRegister = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_sll#{suffix}", [lhs, direction == :l ? realAmount : inverseAmount, leftRegister])
        rightRegister = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_srl#{suffix}", [lhs, direction == :l ? inverseAmount : realAmount, rightRegister])
        newList << Instruction.new(node.codeOrigin, "rv_or", [leftRegister, rightRegister, lhs])
    end

    def emitLogicalOperation(newList, node, operation, size)
        operands = node.operands
        if operands.size == 2
            operands = [operands[1], operands[0], operands[1]]
        end
        riscv64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID], [RegisterID, Immediate, RegisterID])

        case operation
        when :and, :or, :xor
            logicalOpcode = operation.to_s
        else
            raise "Invalid operation #{operation}"
        end

        if operands[1].is_a? Immediate
            logicalOpcode += "i"
        end
        newList << Instruction.new(node.codeOrigin, "rv_#{logicalOpcode}", operands)
        riscv64LowerEmitMask(newList, node, size, operands[2], operands[2])
    end

    def emitComplementOperation(newList, node, operation, size)
        riscv64ValidateOperands(node.operands, [RegisterID])

        case operation
        when :neg
            complementOpcode = size == :i ? "negw" : "neg"
        when :not
            complementOpcode = "not"
        else
            raise "Invalid operation #{operation}"
        end

        newList << Instruction.new(node.codeOrigin, "rv_#{complementOpcode}", [node.operands[0], node.operands[0]])
        riscv64LowerEmitMask(newList, node, size, node.operands[0], node.operands[0])
    end

    def emitBitExtensionOperation(newList, node, extension, fromSize, toSize)
        raise "Invalid operand types" unless riscv64OperandTypes(node.operands) == [RegisterID, RegisterID]

        if [[:s, :i, :p], [:s, :i, :q]].include? [extension, fromSize, toSize]
            newList << Instruction.new(node.codeOrigin, "rv_sext.w", node.operands)
            return
        end

        source = node.operands[0]
        dest = node.operands[1]

        if [[:z, :i, :p], [:z, :i, :q]].include? [extension, fromSize, toSize]
            newList << Instruction.new(node.codeOrigin, "rv_slli", [source, Immediate.new(node.codeOrigin, 32), dest])
            newList << Instruction.new(node.codeOrigin, "rv_srli", [dest, Immediate.new(node.codeOrigin, 32), dest])
            return
        end

        raise "Invalid zero extension" unless extension == :s
        case [fromSize, toSize]
        when [:b, :i]
            newList << Instruction.new(node.codeOrigin, "rv_slli", [source, Immediate.new(node.codeOrigin, 56), dest])
            newList << Instruction.new(node.codeOrigin, "rv_srai", [dest, Immediate.new(node.codeOrigin, 24), dest])
            newList << Instruction.new(node.codeOrigin, "rv_srli", [dest, Immediate.new(node.codeOrigin, 32), dest])
        when [:b, :q]
            newList << Instruction.new(node.codeOrigin, "rv_slli", [source, Immediate.new(node.codeOrigin, 56), dest])
            newList << Instruction.new(node.codeOrigin, "rv_srai", [dest, Immediate.new(node.codeOrigin, 56), dest])
        when [:h, :i]
            newList << Instruction.new(node.codeOrigin, "rv_slli", [source, Immediate.new(node.codeOrigin, 48), dest])
            newList << Instruction.new(node.codeOrigin, "rv_srai", [dest, Immediate.new(node.codeOrigin, 16), dest])
            newList << Instruction.new(node.codeOrigin, "rv_srli", [dest, Immediate.new(node.codeOrigin, 32), dest])
        when [:h, :q]
            newList << Instruction.new(node.codeOrigin, "rv_slli", [source, Immediate.new(node.codeOrigin, 48), dest])
            newList << Instruction.new(node.codeOrigin, "rv_srai", [dest, Immediate.new(node.codeOrigin, 48), dest])
        else
            raise "Invalid bit-extension combination"
        end
    end

    def emitZeroCountOperation(newList, node, side, size)
        riscv64ValidateOperands(node.operands, [RegisterID, RegisterID])

        from = node.operands[0]
        to = node.operands[1]

        case size
        when :i
            bits = 32
            suffix = "w"
        when :q
            bits = 64
            suffix = ""
        end

        count = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_xor", [count, count, count])
        tmp = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_li", [Immediate.new(node.codeOrigin, side == :t ? bits : bits - 1), tmp])
        loopLabel = LocalLabel.unique("begin_count_loop")
        newList << loopLabel
        check = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_srl#{suffix}", [from, side == :t ? count : tmp, check])
        newList << Instruction.new(node.codeOrigin, "rv_andi", [check, Immediate.new(node.codeOrigin, 1), check])
        returnLabel = LocalLabel.unique("return_count")
        newList << Instruction.new(node.codeOrigin, "rv_bgtz", [check, LocalLabelReference.new(node.codeOrigin, returnLabel)])
        newList << Instruction.new(node.codeOrigin, "rv_addi#{suffix}", [count, Immediate.new(node.codeOrigin, 1), count])
        case side
        when :t
            newList << Instruction.new(node.codeOrigin, "rv_blt", [count, tmp, LocalLabelReference.new(node.codeOrigin, loopLabel)])
        when :l
            newList << Instruction.new(node.codeOrigin, "rv_addi#{suffix}", [tmp, Immediate.new(node.codeOrigin, -1), tmp])
            newList << Instruction.new(node.codeOrigin, "rv_bgez", [tmp, LocalLabelReference.new(node.codeOrigin, loopLabel)])
        end
        newList << returnLabel
        newList << Instruction.new(node.codeOrigin, "rv_mv", [count, to])
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
                newList << Instruction.new(node.codeOrigin, "rv_ebreak", [])
            when "nop", "ret"
                newList << Instruction.new(node.codeOrigin, "rv_#{node.opcode}", [])
            when "memfence"
                newList << Instruction.new(node.codeOrigin, "rv_fence", [RISCV64MemoryOrdering.new(:rw), RISCV64MemoryOrdering.new(:rw)])
            when "fence"
                newList << Instruction.new(node.codeOrigin, "rv_fence", [RISCV64MemoryOrdering.new(:iorw), RISCV64MemoryOrdering.new(:iorw)])
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def riscv64LowerTest(list)
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
            newList << Instruction.new(node.codeOrigin, "rv_#{opcode}", node.operands)
            return
        end

        if node.operands[0].immediate? and node.operands[0].value == -1
            newList << Instruction.new(node.codeOrigin, "rv_#{opcode}", [node.operands[1], node.operands[2]])
            return
        end

        if node.operands[1].immediate? and node.operands[1].value == -1
            newList << Instruction.new(node.codeOrigin, "rv_#{opcode}", [node.operands[0], node.operands[2]])
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
            newList << Instruction.new(node.codeOrigin, "rv_and", [value, mask, tmp])
        else
            newList << Instruction.new(node.codeOrigin, "rv_li", [mask, tmp]);
            newList << Instruction.new(node.codeOrigin, "rv_and", [tmp, value, tmp]);
        end

        riscv64LowerEmitSignExtension(newList, node, size, tmp, tmp)
        newList << Instruction.new(node.codeOrigin, "rv_#{opcode}", [tmp, node.operands[2]])
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

def riscv64LowerCompare(list)
    def emit(newList, node, size, comparison)
        lhs = riscv64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[0], size)
        rhs = riscv64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[1], size)
        dest = node.operands[2]

        case comparison
        when :eq
            tmp = Tmp.new(node.codeOrigin, :gpr)
            newList << Instruction.new(node.codeOrigin, "rv_sub", [lhs, rhs, tmp])
            newList << Instruction.new(node.codeOrigin, "rv_seqz", [tmp, dest])
        when :neq
            tmp = Tmp.new(node.codeOrigin, :gpr)
            newList << Instruction.new(node.codeOrigin, "rv_sub", [lhs, rhs, tmp])
            newList << Instruction.new(node.codeOrigin, "rv_snez", [tmp, dest])
        when :a
            newList << Instruction.new(node.codeOrigin, "rv_sltu", [rhs, lhs, dest])
        when :aeq
            newList << Instruction.new(node.codeOrigin, "rv_sltu", [lhs, rhs, dest])
            newList << Instruction.new(node.codeOrigin, "rv_xori", [dest, Immediate.new(node.codeOrigin, 1), dest])
        when :b
            newList << Instruction.new(node.codeOrigin, "rv_sltu", [lhs, rhs, dest])
        when :beq
            newList << Instruction.new(node.codeOrigin, "rv_sltu", [rhs, lhs, dest])
            newList << Instruction.new(node.codeOrigin, "rv_xori", [dest, Immediate.new(node.codeOrigin, 1), dest])
        when :gt
            newList << Instruction.new(node.codeOrigin, "rv_slt", [rhs, lhs, dest])
        when :gteq
            newList << Instruction.new(node.codeOrigin, "rv_slt", [lhs, rhs, dest])
            newList << Instruction.new(node.codeOrigin, "rv_xori", [dest, Immediate.new(node.codeOrigin, 1), dest])
        when :lt
            newList << Instruction.new(node.codeOrigin, "rv_slt", [lhs, rhs, dest])
        when :lteq
            newList << Instruction.new(node.codeOrigin, "rv_slt", [rhs, lhs, dest])
            newList << Instruction.new(node.codeOrigin, "rv_xori", [dest, Immediate.new(node.codeOrigin, 1), dest])
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

def riscv64LowerBranch(list)
    def branchOpcode(condition)
        case condition
        when :eq
            "beq"
        when :neq
            "bne"
        when :a
            "bgtu"
        when :aeq
            "bgeu"
        when :b
            "bltu"
        when :beq
            "bleu"
        when :gt
            "bgt"
        when :gteq
            "bge"
        when :lt
            "blt"
        when :lteq
            "ble"
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
        lhs = riscv64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[0], size)
        rhs = riscv64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[1], size)
        dest = node.operands[2]

        newList << Instruction.new(node.codeOrigin, "rv_#{branchOpcode(condition)}", [lhs, rhs, dest])
    end

    def emitAddition(newList, node, operation, size, condition)
        operands = node.operands
        if operands.size == 3
            operands = [operands[1], operands[0], operands[1], operands[2]]
        end

        riscv64ValidateOperands(operands,
            [RegisterID, RegisterID, RegisterID, LocalLabelReference],
            [RegisterID, Immediate, RegisterID, LocalLabelReference]);

        case operation
        when :add, :sub
            additionOpcode = operation.to_s + (size == :i ? "w" : "")
        else
            raise "Invalid addition operation"
        end

        lhs = riscv64LowerOperandIntoRegister(newList, node, operands[0])
        rhs = riscv64LowerOperandIntoRegister(newList, node, operands[1])
        newList << Instruction.new(node.codeOrigin, "rv_#{additionOpcode}", [lhs, rhs, operands[2]])

        tmp = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_mv", [operands[2], tmp])
        riscv64LowerEmitMask(newList, node, size, operands[2], operands[2])
        newList << Instruction.new(node.codeOrigin, "rv_#{branchOpcode(condition)}", [tmp, operands[3]])
    end

    def emitMultiplication(newList, node, size, condition)
        raise "Invalid size" unless size == :i

        lhs = result = riscv64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[0], size, :forced_tmp)
        rhs = riscv64LowerOperandIntoRegisterAndSignExtend(newList, node, node.operands[1], size, :forced_tmp)
        raise "Invalid lowered-operand type" unless result.is_a? Tmp

        newList << Instruction.new(node.codeOrigin, "rv_mul", [lhs, rhs, result])
        riscv64LowerEmitMask(newList, node, size, result, node.operands[1])
        newList << Instruction.new(node.codeOrigin, "rv_#{branchOpcode(condition)}", [result, node.operands[2]])
    end

    def emitOverflow(newList, node, operation, size)
        raise "Invalid size" unless size == :i

        operands = node.operands
        if operands.size == 3
            operands = [operands[1], operands[0], operands[1], operands[2]]
        end

        riscv64ValidateOperands(operands,
            [RegisterID, RegisterID, RegisterID, LocalLabelReference],
            [RegisterID, Immediate, RegisterID, LocalLabelReference]);

        case operation
        when :add, :sub, :mul
            operationOpcode = operation.to_s
        else
            raise "Invalid operation #{operation}"
        end

        lhs = tmp1 = riscv64LowerOperandIntoRegisterAndSignExtend(newList, node, operands[0], size, :forced_tmp)
        rhs = tmp2 = riscv64LowerOperandIntoRegisterAndSignExtend(newList, node, operands[1], size, :forced_tmp)
        raise "Invalid lowered-operand type" unless (tmp1.is_a? Tmp and tmp2.is_a? Tmp)

        newList << Instruction.new(node.codeOrigin, "rv_#{operationOpcode}", [lhs, rhs, tmp1])
        riscv64LowerEmitMask(newList, node, size, tmp1, operands[2])

        newList << Instruction.new(node.codeOrigin, "rv_sext.w", [tmp1, tmp2])
        newList << Instruction.new(node.codeOrigin, "rv_bne", [tmp1, tmp2, operands[3]])
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

def riscv64LowerFPOperation(list)
    def emitLoadOperation(newList, node, precision)
        riscv64ValidateOperands(node.operands, [Address, FPRegisterID])
        case precision
        when :f
            suffix = "w"
        when :d
            suffix = "d"
        else
            raise "Invalid precision #{precision}"
        end

        newList << Instruction.new(node.codeOrigin, "rv_fl#{suffix}", node.operands)
    end

    def emitStoreOperation(newList, node, precision)
        riscv64ValidateOperands(node.operands, [FPRegisterID, Address])
        case precision
        when :f
            suffix = "w"
        when :d
            suffix = "d"
        else
            raise "Invalid precision #{precision}"
        end

        newList << Instruction.new(node.codeOrigin, "rv_fs#{suffix}", node.operands)
    end

    def emitMoveOperation(newList, node, precision)
        riscv64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID])
        raise "Invalid precision" unless [:f, :d].include? precision
        if precision == :f
            precision = :s
        end

        newList << Instruction.new(node.codeOrigin, "rv_fmv.#{precision.to_s}", node.operands)
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

        riscv64ValidateOperands(node.operands, [registerType(sourceType), registerType(destinationType)])
        case riscv64OperandTypes(node.operands)
        when [RegisterID, FPRegisterID]
            fmvOpcode = "rv_fmv.#{fpSuffix(destinationType)}.x"
        when [FPRegisterID, RegisterID]
            fmvOpcode = "rv_fmv.x.#{fpSuffix(sourceType)}"
        else
            riscv64RaiseMismatchedOperands
        end

        newList << Instruction.new(node.codeOrigin, fmvOpcode, node.operands)
    end

    def emitComputationalOperation(newList, node, operation, precision)
        riscv64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID])
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
        newList << Instruction.new(node.codeOrigin, "rv_f#{operation.to_s}.#{precision.to_s}", operands)
    end

    def emitBitwiseOperation(newList, node, operation, precision)
        riscv64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID])
        raise "Invalid operation" unless [:and, :or].include? operation

        case precision
        when :f
            suffix = "w"
        when :d
            suffix = "d"
        else
            raise "Invalid precision #{precision}"
        end

        tmp1 = Tmp.new(node.codeOrigin, :gpr)
        tmp2 = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_fmv.x.#{suffix}", [node.operands[0], tmp1])
        newList << Instruction.new(node.codeOrigin, "rv_fmv.x.#{suffix}", [node.operands[1], tmp2])
        newList << Instruction.new(node.codeOrigin, "rv_#{operation.to_s}", [tmp1, tmp2, tmp2])
        newList << Instruction.new(node.codeOrigin, "rv_fmv.#{suffix}.x", [tmp2, node.operands[1]])
    end

    def emitRoundingOperation(newList, node, operation, precision)
        riscv64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID])

        from = node.operands[0]
        to = node.operands[1]
        roundingMode = RISCV64RoundingMode.new(operation)
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

        newList << Instruction.new(node.codeOrigin, "rv_fmv.#{fpSuffix}", [from, to])
        tmp = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_fclass.#{fpSuffix}", [from, tmp])
        newList << Instruction.new(node.codeOrigin, "rv_andi", [tmp, Immediate.new(node.codeOrigin, 0x381), tmp])
        returnLabel = LocalLabel.unique("return_exotic_float")
        newList << Instruction.new(node.codeOrigin, "rv_bnez", [tmp, LocalLabelReference.new(node.codeOrigin, returnLabel)])
        newList << Instruction.new(node.codeOrigin, "rv_fcvt.#{intSuffix}.#{fpSuffix}", [from, tmp, roundingMode])
        newList << Instruction.new(node.codeOrigin, "rv_fcvt.#{fpSuffix}.#{intSuffix}", [tmp, to])
        newList << returnLabel
    end

    def emitConversionOperation(newList, node, sourceType, destinationType, signedness, roundingMode)
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
            end
        end

        def intSuffix(type, signedness)
            case type
            when :i
                signedness == :s ? "w" : "wu"
            when :q
                signedness == :s ? "l" : "lu"
            end
        end

        riscv64ValidateOperands(node.operands, [registerType(sourceType), registerType(destinationType)])

        case riscv64OperandTypes(node.operands)
        when [RegisterID, FPRegisterID]
            raise "Invalid rounding mode" unless roundingMode == :none
            fcvtOpcode = "rv_fcvt.#{fpSuffix(destinationType)}.#{intSuffix(sourceType, signedness)}"
        when [FPRegisterID, RegisterID]
            fcvtOpcode = "rv_fcvt.#{intSuffix(destinationType, signedness)}.#{fpSuffix(sourceType)}"
        when [FPRegisterID, FPRegisterID]
            raise "Invalid rounding mode" unless roundingMode == :none
            fcvtOpcode = "rv_fcvt.#{fpSuffix(destinationType)}.#{fpSuffix(sourceType)}"
        else
            riscv64RaiseMismatchedOperands(node.operands)
        end

        operands = [node.operands[0], node.operands[1]]
        if roundingMode != :none
            operands += [RISCV64RoundingMode.new(roundingMode)]
        end
        newList << Instruction.new(node.codeOrigin, fcvtOpcode, operands)
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
                emitConversionOperation(newList, node, $1.to_sym, $2.to_sym, $3.to_sym, :truncate)
            when /^c(i|q|f|d)2(f|d)(s?)$/
                emitConversionOperation(newList, node, $1.to_sym, $2.to_sym, $3.to_sym, :none)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def riscv64LowerFPCompare(list)
    def emitCompare(newList, node, precision, compareOp, lhs, rhs)
        case precision
        when :f
            precisionSuffix = "s"
        when :d
            precisionSuffix = "d"
        else
            raise "Invalid precision #{precision}"
        end

        newList << Instruction.new(node.codeOrigin, "rv_#{compareOp}.#{precisionSuffix}", [lhs, rhs, node.operands[2]])
    end

    def emit(newList, node, precision, condition)
        riscv64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID, RegisterID])
        operands = node.operands

        case condition
        when :eq, :equn
            emitCompare(newList, node, precision, "feq", operands[0], operands[1])
        when :neq, :nequn
            emitCompare(newList, node, precision, "feq", operands[0], operands[1])
            newList << Instruction.new(node.codeOrigin, "rv_xori", [operands[2], Immediate.new(node.codeOrigin, 1), operands[2]])
        when :gt
            emitCompare(newList, node, precision, "flt", operands[1], operands[0])
        when :gteq
            emitCompare(newList, node, precision, "fle", operands[1], operands[0])
        when :lt
            emitCompare(newList, node, precision, "flt", operands[0], operands[1])
        when :lteq
            emitCompare(newList, node, precision, "fle", operands[0], operands[1])
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

def riscv64LowerFPBranch(list)
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

        newList << Instruction.new(node.codeOrigin, "rv_fclass.#{precisionSuffix(precision)}", [node.operands[0], tmp1])
        newList << Instruction.new(node.codeOrigin, "rv_fclass.#{precisionSuffix(precision)}", [node.operands[1], tmp2])
        newList << Instruction.new(node.codeOrigin, "rv_or", [tmp1, tmp2, tmp2])
        newList << Instruction.new(node.codeOrigin, "rv_andi", [tmp2, Immediate.new(node.codeOrigin, 0x300), tmp2])
        newList << Instruction.new(node.codeOrigin, "rv_bnez", [tmp2, node.operands[2]])
    end

    def emitBranchForTest(newList, node, precision, testOpcode, lhs, rhs, branchOpcode)
        tmp = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, "rv_#{testOpcode}.#{precisionSuffix(precision)}", [lhs, rhs, tmp])
        newList << Instruction.new(node.codeOrigin, "rv_#{branchOpcode}", [tmp, node.operands[2]])
    end

    def emit(newList, node, precision, condition)
        riscv64ValidateOperands(node.operands, [FPRegisterID, FPRegisterID, LocalLabelReference])
        operands = node.operands

        if [:equn, :nequn, :gtun, :gtequn, :ltun, :ltequn].include? condition
            emitBranchForUnordered(newList, node, precision)
        end

        case condition
        when :eq, :equn
            emitBranchForTest(newList, node, precision, "feq", operands[0], operands[1], "bnez")
        when :neq, :nequn
            emitBranchForTest(newList, node, precision, "feq", operands[0], operands[1], "beqz")
        when :gt, :gtun
            emitBranchForTest(newList, node, precision, "flt", operands[1], operands[0], "bnez")
        when :gteq, :gtequn
            emitBranchForTest(newList, node, precision, "fle", operands[1], operands[0], "bnez")
        when :lt, :ltun
            emitBranchForTest(newList, node, precision, "flt", operands[0], operands[1], "bnez")
        when :lteq, :ltequn
            emitBranchForTest(newList, node, precision, "fle", operands[0], operands[1], "bnez")
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

def riscv64GenerateWASMPlaceholders(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "loadlinkacqb", "loadlinkacqh", "loadlinkacqi", "loadlinkacqq",
                 "storecondrelb", "storecondrelh", "storecondreli", "storecondrelq"
                newList << Instruction.new(node.codeOrigin, "rv_ebreak", [], "WebAssembly placeholder for opcode #{node.opcode}")
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
    def getModifiedListRISCV64
        result = @list

        result = riscLowerMalformedAddresses(result) {
            | node, address |
            if address.is_a? Address
                !address.riscv64RequiresLoad
            else
                false
            end
        }
        result = riscv64LowerMisplacedAddresses(result)
        result = riscLowerMisplacedAddresses(result)
        result = riscv64LowerAddressLoads(result)

        result = riscLowerMisplacedImmediates(result, ["storeb", "storeh", "storei", "storep", "storeq"])
        result = riscLowerMalformedImmediates(result, -0x800..0x7ff, -0x800..0x7ff)
        result = riscv64LowerImmediateSubtraction(result)

        result = riscv64LowerOperation(result)
        result = riscv64LowerTest(result)
        result = riscv64LowerCompare(result)
        result = riscv64LowerBranch(result)

        result = riscv64LowerFPOperation(result)
        result = riscv64LowerFPCompare(result)
        result = riscv64LowerFPBranch(result)

        result = riscv64GenerateWASMPlaceholders(result)

        result = assignRegistersToTemporaries(result, :gpr, RISCV64_EXTRA_GPRS)
        result = assignRegistersToTemporaries(result, :fpr, RISCV64_EXTRA_FPRS)
        return result
    end
end

class Instruction
    def rvop(opcode)
        opcode[/^rv_(.+)/, 1]
    end

    def lowerRISCV64
        case opcode
        # I and M instructions
        when /^rv_(jr|jalr)$/
            riscv64ValidateOperands(operands, [RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[0].riscv64Operand}"
        when /^rv_(call|tail)$/
            riscv64ValidateOperands(operands, [LabelReference], [LocalLabelReference])
            $asm.puts "#{rvop(opcode)} #{operands[0].asmLabel}"
        when /^rv_(la|lla)$/
            riscv64ValidateOperands(operands, [LabelReference, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].asmLabel}"
        when "rv_mv"
            riscv64ValidateOperands(operands, [RegisterID, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_l(u?)i$/
            riscv64ValidateOperands(operands, [Immediate, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand(:any_immediate)}"
        when /^rv_l(b|bu|h|hu|w|wu|d)$/
            riscv64ValidateOperands(operands, [Address, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_s(b|h|w|d)$/
            riscv64ValidateOperands(operands, [RegisterID, Address])
            $asm.puts "#{rvop(opcode)} #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when /^rv_(add(w?)|sub(w?)|and|or|xor|s(ll|rl|ra)(w?)|mul(w?)|div(u?)(w?)|rem(u?)(w?))$/ # all M instructions
            riscv64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when /^rv_addi(w?)$/, /^rv_(and|or|xor)i$/
            riscv64ValidateOperands(operands, [RegisterID, Immediate, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when /^rv_(sll|srl|sra)i(w?)$/
            riscv64ValidateOperands(operands, [RegisterID, Immediate, RegisterID])
            validationType = $2 == "w" ? :rv32_shift_immediate : :rv64_shift_immediate
            raise "Invalid shift-amount immediate" unless riscv64ValidateImmediate(validationType, operands[1].value)
            $asm.puts "#{rvop(opcode)} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when /^rv_neg(w?)$/, "rv_not", "rv_sext.w"
            riscv64ValidateOperands(operands, [RegisterID, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_slt|slt(u?)$/
            riscv64ValidateOperands(operands, [RegisterID, RegisterID, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when /^rv_(seqz|snez|sltz|sgtz)$/
            riscv64ValidateOperands(operands, [RegisterID, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_b(eq|ne|gt|ge|gtu|geu|lt|le|ltu|leu)$/
            riscv64ValidateOperands(operands, [RegisterID, RegisterID, LocalLabelReference])
            $asm.puts "#{rvop(opcode)} #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}, #{operands[2].asmLabel}"
        when /^rv_b(eqz|nez|lez|ltz|gez|gtz)$/
            riscv64ValidateOperands(operands, [RegisterID, LocalLabelReference])
            $asm.puts "#{rvop(opcode)} #{operands[0].riscv64Operand}, #{operands[1].asmLabel}"
        when "rv_nop", "rv_ret", "rv_ebreak"
            $asm.puts "#{rvop(opcode)}"
        when "rv_fence"
            riscv64ValidateOperands(operands, [RISCV64MemoryOrdering, RISCV64MemoryOrdering])
            $asm.puts "#{rvop(opcode)} #{operands[0].riscv64MemoryOrdering}, #{operands[1].riscv64MemoryOrdering}"
        # D and F instructions
        when /^rv_fl(w|d)$/
            riscv64ValidateOperands(operands, [Address, FPRegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_fs(w|d)$/
            riscv64ValidateOperands(operands, [FPRegisterID, Address])
            $asm.puts "#{rvop(opcode)} #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when /^rv_fmv\.(s|d)$/
            riscv64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_fmv\.(x|w|d)\.(x|w|d)$/
            riscv64ValidateOperands(operands, [RegisterID, FPRegisterID], [FPRegisterID, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_f(add|sub|mul|div)\.(s|d)$/
            riscv64ValidateOperands(operands,
                                    [FPRegisterID, FPRegisterID, FPRegisterID], [FPRegisterID, FPRegisterID, FPRegisterID, RISCV64RoundingMode])
            if operands.size == 4
                $asm.puts "#{rvop(opcode)} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}, #{operands[3].riscv64RoundingMode}"
            else
                $asm.puts "#{rvop(opcode)} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
            end
        when /^rv_f(sqrt|abs|neg)\.(s|d)$/
            riscv64ValidateOperands(operands, [FPRegisterID, FPRegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_f(eq|lt|le)\.(s|d)$/
            riscv64ValidateOperands(operands, [FPRegisterID, FPRegisterID, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when /^rv_fcvt\.(w|wu|l|lu|s|d)\.(w|wu|l|lu|s|d)$/
            riscv64ValidateOperands(operands,
                [RegisterID, FPRegisterID], [FPRegisterID, RegisterID], [FPRegisterID, FPRegisterID],
                [RegisterID, FPRegisterID, RISCV64RoundingMode], [FPRegisterID, RegisterID, RISCV64RoundingMode])
            if operands.size == 3
                riscv64RaiseMismatchedOperands(operands) unless operands[2].is_a? RISCV64RoundingMode
                $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[2].riscv64RoundingMode}"
            else
                $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
            end
        when /^rv_fclass\.(s|d)$/
            riscv64ValidateOperands(operands, [FPRegisterID, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_fsgn(n|x)?j\.(s|d)$/
            riscv64ValidateOperands(operands, [FPRegisterID, FPRegisterID, FPRegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        # A instructions
        when /^rv_amo(add|and|max(u)?|min(u)?|or|swap|xor)\.(d|w)(\.(aq|rl))?$/
            riscv64ValidateOperands(operands, [RegisterID, Address, RegisterID])
            $asm.puts "#{rvop(opcode)}, #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when /^rv_lr\.(d|w)(\.(aq|rl))?$/
            riscv64ValidateOperands(operands, [Address, RegisterID])
            $asm.puts "#{rvop(opcode)} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        when /^rv_sc\.(d|w)(\.(aq|rl))?$/
            riscv64ValidateOperands(operands, [RegisterID, RegisterID, Address])
            $asm.puts "#{rvop(opcode)} #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}, #{operands[2].riscv64Operand}"
        else
            lowerDefault
        end
    end
end
