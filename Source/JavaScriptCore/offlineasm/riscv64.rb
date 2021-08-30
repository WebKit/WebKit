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
# x8  => cfr (thought alias fp) (RISC-V frame pointer register)
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
# x28 => not used
# x29 => not used
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
# f28 => not used
# f29 => not used
# f30 => not used
# f31 => not used


def riscv64OperandTypes(operands)
    return operands.map {
        |op|
        op.class
    }
end

def riscv64RaiseMismatchedOperands(operands)
    raise "Unable to match operands #{riscv64OperandTypes(operands)}"
end

def riscv64RaiseUnsupported
    raise "Not supported for RISCV64"
end

def riscv64LoadInstruction(size)
    case size
    when :b
        "lb"
    when :bu
        "lbu"
    when :h
        "lh"
    when :hu
        "lhu"
    when :w
        "lw"
    when :wu
        "lwu"
    when :d
        "ld"
    else
        raise "Unsupported size #{size}"
    end
end

def riscv64ZeroExtendedLoadInstruction(size)
    case size
    when :b
        riscv64LoadInstruction(:bu)
    when :h
        riscv64LoadInstruction(:hu)
    when :w
        riscv64LoadInstruction(:wu)
    when :d
        riscv64LoadInstruction(:d)
    else
        raise "Unsupported size #{size}"
    end
end

def riscv64StoreInstruction(size)
    case size
    when :b
        "sb"
    when :h
        "sh"
    when :w
        "sw"
    when :d
        "sd"
    else
        raise "Unsupported size #{size}"
    end
end

def riscv64EmitRegisterMask(target, size)
    case size
    when :w
        $asm.puts "li x31, 0xffffffff"
        $asm.puts "and #{target.riscv64Operand}, #{target.riscv64Operand}, x31"
    when :d
    else
        raise "Unsupported size"
    end
end

def riscv64ConditionalBranchInstruction(condition)
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
    else
        raise "Unsupported condition #{condition}"
    end
end

def riscv64EmitLoad(operands, type, mask)
    instruction = riscv64LoadInstruction(type)

    case riscv64OperandTypes(operands)
    when [Address, RegisterID]
        if operands[0].riscv64RequiresLoad
            operands[0].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{instruction} #{operands[1].riscv64Operand}, 0(x31)"
        else
            $asm.puts "#{instruction} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        end
    when [BaseIndex, RegisterID]
        operands[0].riscv64Load(RISCV64ScratchRegister.x31, RISCV64ScratchRegister.x30)
        $asm.puts "#{instruction} #{operands[1].riscv64Operand}, 0(x31)"
    else
        riscv64RaiseMismatchedOperands(operands)
    end

    case mask
    when :w
        riscv64EmitRegisterMask(operands[1], :w)
    when :none
    else
        raise "Unsupported mask type"
    end
end

def riscv64EmitStore(operands, type)
    instruction = riscv64StoreInstruction(type)

    case riscv64OperandTypes(operands)
    when [RegisterID, Address]
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{instruction} #{operands[0].riscv64Operand}, 0(x31)"
        else
            $asm.puts "#{instruction} #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        end
    when [RegisterID, BaseIndex]
        operands[1].riscv64Load(RISCV64ScratchRegister.x31, RISCV64ScratchRegister.x30)
        $asm.puts "#{instruction} #{operands[0].riscv64Operand}, 0(x31)"
    when [Immediate, Address]
        $asm.puts "li x30, #{operands[0].riscv64Operand}"
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{instruction} x30, 0(x31)"
        else
            $asm.puts "#{instruction} x30, #{operands[1].riscv64Operand}"
        end
    when [Immediate, BaseIndex]
        operands[1].riscv64Load(RISCV64ScratchRegister.x31, RISCV64ScratchRegister.x30)
        $asm.puts "li x30, #{operands[0].riscv64Operand}"
        $asm.puts "#{instruction} x30, 0(x31)"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitAdditionOperation(operands, size, operation)
    raise "Unsupported size" unless [:w, :d].include? size

    def additionInstruction(size, operation)
        case operation
        when :add
            size == :w ? "addw" : "add"
        when :sub
            size == :w ? "subw" : "sub"
        else
            raise "Unsupported arithmetic operation"
        end
    end

    instruction = additionInstruction(size, operation)
    loadInstruction = riscv64LoadInstruction(size)
    storeInstruction = riscv64StoreInstruction(size)

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID]
        operands = [operands[1], operands[0], operands[1]]
    when [Immediate, RegisterID]
        operands = [operands[1], operands[0], operands[1]]
    when [Address, RegisterID]
        operands = [operands[1], operands[0], operands[1]]
    end

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, RegisterID]
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        riscv64EmitRegisterMask(operands[2], size)
    when [RegisterID, Immediate, RegisterID]
        operands[1].riscv64Load(RISCV64ScratchRegister.x31)
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, x31"
        riscv64EmitRegisterMask(operands[2], size)
    when [Immediate, RegisterID, RegisterID]
        operands[0].riscv64Load(RISCV64ScratchRegister.x31)
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, x31, #{operands[1].riscv64Operand}"
        riscv64EmitRegisterMask(operands[2], size)
    when [RegisterID, Address, RegisterID]
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{loadInstruction} x31, 0(x31)"
        else
            $asm.puts "#{loadInstruction} x31, #{operands[1].riscv64Operand}"
        end
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, x31"
        riscv64EmitRegisterMask(operands[2], size)
    when [Immediate, Address]
        $asm.puts "li x30, #{operands[0].riscv64Operand}"
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{loadInstruction} x31, 0(x31)"
        else
            $asm.puts "#{loadInstruction} x31, #{operands[1].riscv64Operand}"
        end
        $asm.puts "#{instruction} x30, x30, x31"
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{storeInstruction} x30, 0(x31)"
        else
            $asm.puts "#{storeInstruction} x30, #{operands[1].riscv64Operand}"
        end
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitMulDivArithmetic(operands, size, operation)
    raise "Unsupported size" unless [:w, :d].include? size

    def arithmeticInstruction(size, operation)
        case operation
        when :mul
            size == :w ? "mulw" : "mul"
        when :div
            size == :w ? "divuw" : "divu"
        else
            raise "Unsupported arithmetic operation"
        end
    end

    instruction = arithmeticInstruction(size, operation)

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID]
        operands = [operands[0], operands[1], operands[1]]
    when [Immediate, RegisterID]
        operands = [operands[1], operands[0], operands[1]]
    end

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, RegisterID]
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
    when [RegisterID, Immediate, RegisterID]
        $asm.puts "li x31, #{operands[1].riscv64Operand}"
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, x31"
    when [Immediate, RegisterID, RegisterID]
        $asm.puts "li x31, #{operands[0].riscv64Operand}"
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, x31, #{operands[1].riscv64Operand}"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitConditionalBranch(operands, size, condition)
    instruction = riscv64ConditionalBranchInstruction(condition)

    def signExtendForSize(register, target, size)
        case size
        when :b
            $asm.puts "slli #{target}, #{register.riscv64Operand}, 24"
            $asm.puts "sext.w #{target}, #{target}"
            $asm.puts "srai #{target}, #{target}, 24"
        when :w
            $asm.puts "sext.w #{target}, #{register.riscv64Operand}"
        when :d
            $asm.puts "mv #{target}, #{register.riscv64Operand}"
        end
    end

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, LocalLabelReference]
        signExtendForSize(operands[0], 'x30', size)
        signExtendForSize(operands[1], 'x31', size)
        $asm.puts "#{instruction} x30, x31, #{operands[2].asmLabel}"
    when [RegisterID, Immediate, LocalLabelReference]
        signExtendForSize(operands[0], 'x30', size)
        $asm.puts "li x31, #{operands[1].riscv64Operand}"
        $asm.puts "#{instruction} x30, x31, #{operands[2].asmLabel}"
    when [RegisterID, Address, LocalLabelReference]
        signExtendForSize(operands[0], 'x30', size)
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{riscv64LoadInstruction(size)} x31, 0(x31)"
        else
            $asm.puts "#{riscv64LoadInstruction(size)} x31, #{operands[1].riscv64Operand}"
        end
        $asm.puts "#{instruction} x30, x31, #{operands[2].asmLabel}"
    when [RegisterID, BaseIndex, LocalLabelReference]
        operands[1].riscv64Load(RISCV64ScratchRegister.x31, RISCV64ScratchRegister.x30)
        $asm.puts "#{riscv64LoadInstruction(size)} x31, 0(x31)"
        signExtendForSize(operands[0], 'x30', size)
        $asm.puts "#{instruction} x30, x31, #{operands[2].asmLabel}"
    when [Address, RegisterID, LocalLabelReference]
        if operands[0].riscv64RequiresLoad
            operands[0].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{riscv64LoadInstruction(size)} x30, 0(x31)"
        else
            $asm.puts "#{riscv64LoadInstruction(size)} x30, #{operands[0].riscv64Operand}"
        end
        signExtendForSize(operands[1], 'x31', size)
        $asm.puts "#{instruction} x30, x31, #{operands[2].asmLabel}"
    when [Address, Immediate, LocalLabelReference]
        if operands[0].riscv64RequiresLoad
            operands[0].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{riscv64LoadInstruction(size)} x30, 0(x31)"
        else
            $asm.puts "#{riscv64LoadInstruction(size)} x30, #{operands[0].riscv64Operand}"
        end
        $asm.puts "li x31, #{operands[1].riscv64Operand}"
        $asm.puts "#{instruction} x30, x31, #{operands[2].asmLabel}"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitConditionalBranchForTest(operands, size, test)
    def branchInstruction(test)
        case test
        when :z
            "beqz"
        when :nz
            "bnez"
        when :s
            "bltz"
        else
        end
    end

    def signExtendForSize(target, size)
        case size
        when :b
            $asm.puts "slli #{target}, #{target}, 24"
            $asm.puts "sext.w #{target}, #{target}"
            $asm.puts "srai #{target}, #{target}, 24"
        when :h
            $asm.puts "slli #{target}, #{target}, 16"
            $asm.puts "sext.w #{target}, #{target}"
            $asm.puts "srai #{target}, #{target}, 16"
        when :w
            $asm.puts "sext.w #{target}, #{target}"
        when :d
            return
        end
    end

    bInstruction = branchInstruction(test)
    loadInstruction = riscv64LoadInstruction(size)

    case riscv64OperandTypes(operands)
    when [RegisterID, LocalLabelReference]
        case test
        when :s
            $asm.puts "mv x31, #{operands[0].riscv64Operand}"
            signExtendForSize('x31', size)
            $asm.puts "#{bInstruction} x31, #{operands[1].asmLabel}"
        else
            $asm.puts "#{bInstruction} #{operands[0].riscv64Operand}, #{operands[1].asmLabel}"
        end
    when [RegisterID, RegisterID, LocalLabelReference]
        $asm.puts "and x31, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        signExtendForSize('x31', size)
        $asm.puts "#{bInstruction} x31, #{operands[2].asmLabel}"
    when [RegisterID, Immediate, LocalLabelReference]
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "and x31, #{operands[0].riscv64Operand}, x31"
        else
            $asm.puts "andi x31, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        end
        signExtendForSize('x31', size)
        $asm.puts "#{bInstruction} x31, #{operands[2].asmLabel}"
    when [Address, LocalLabelReference]
        if operands[0].riscv64RequiresLoad
            operands[0].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{loadInstruction} x31, 0(x31)"
        else
            $asm.puts "#{loadInstruction} x31, #{operands[0].riscv64Operand}"
        end
        $asm.puts "#{bInstruction} x31, #{operands[1].asmLabel}"
    when [Address, Immediate, LocalLabelReference]
        if operands[0].riscv64RequiresLoad
            operands[0].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{loadInstruction} x30, 0(x31)"
        else
            $asm.puts "#{loadInstruction} x30, #{operands[0].riscv64Operand}"
        end
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "and x31, x30, x31"
        else
            $asm.puts "andi x31, x30, #{operands[1].riscv64Operand}"
        end
        signExtendForSize('x31', size)
        $asm.puts "#{bInstruction} x31, #{operands[2].asmLabel}"
    when [BaseIndex, LocalLabelReference]
        operands[0].riscv64Load(RISCV64ScratchRegister.x31, RISCV64ScratchRegister.x30)
        $asm.puts "#{loadInstruction} x31, 0(x31)"
        $asm.puts "#{bInstruction} x31, #{operands[1].asmLabel}"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitConditionalBranchForAdditionOperation(operands, size, operation, test)
    def additionInstruction(size, operation)
        case operation
        when :add
            size == :w ? "addw" : "add"
        when :sub
            size == :w ? "subw" : "sub"
        else
            raise "Unsupported arithmetic operation"
        end
    end

    def emitBranchForTest(test, target, label)
        case test
        when :z
            $asm.puts "beqz #{target.riscv64Operand}, #{label.asmLabel}"
        when :nz
            $asm.puts "bnez #{target.riscv64Operand}, #{label.asmLabel}"
        when :s
            $asm.puts "bltz #{target.riscv64Operand}, #{label.asmLabel}"
        else
            raise "Unsupported test"
        end
    end

    instruction = additionInstruction(size, operation)
    loadInstruction = riscv64LoadInstruction(size)
    storeInstruction = riscv64StoreInstruction(size)

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, LocalLabelReference]
        operands = [operands[1], operands[0], operands[1], operands[2]]
    when [Immediate, RegisterID, LocalLabelReference]
        operands = [operands[1], operands[0], operands[1], operands[2]]
    end

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, RegisterID, LocalLabelReference]
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        $asm.puts "mv x30, #{operands[2].riscv64Operand}"
        riscv64EmitRegisterMask(operands[2], size)
        emitBranchForTest(test, RISCV64ScratchRegister.x30, operands[3])
    when [RegisterID, Immediate, RegisterID, LocalLabelReference]
        $asm.puts "li x31, #{operands[1].riscv64Operand}"
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, x31"
        $asm.puts "mv x30, #{operands[2].riscv64Operand}"
        riscv64EmitRegisterMask(operands[2], size)
        emitBranchForTest(test, RISCV64ScratchRegister.x30, operands[3])
    when [Immediate, Address, LocalLabelReference]
        $asm.puts "li x30, #{operands[0].riscv64Operand}"
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{loadInstruction} x31, 0(x31)"
        else
            $asm.puts "#{loadInstruction} x31, #{operands[1].riscv64Operand}"
        end
        $asm.puts "#{instruction} x30, x30, x31"
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{storeInstruction} x30, 0(x31)"
        else
            $asm.puts "#{storeInstruction} x30, #{operands[1].riscv64Operand}"
        end
        emitBranchForTest(test, RISCV64ScratchRegister.x30, operands[2])
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitConditionalBranchForMultiplicationOperation(operands, size, test)
    raise "Unsupported size" unless size == :w

    def emitMultiplication(lhs, rhs)
        $asm.puts "sext.w x30, #{lhs.riscv64Operand}"
        $asm.puts "sext.w x31, #{rhs.riscv64Operand}"
        $asm.puts "mul x30, x30, x31"
    end

    def emitBranchForTest(test, label)
        case test
        when :z
            $asm.puts "beqz x30, #{label.asmLabel}"
        when :nz
            $asm.puts "bnez x30, #{label.asmLabel}"
        when :s
            $asm.puts "bltz x30, #{label.asmLabel}"
        else
            raise "Unsupported test"
        end
    end

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, LocalLabelReference]
        emitMultiplication(operands[0], operands[1])
        $asm.puts "mv #{operands[1].riscv64Operand}, x30"
        riscv64EmitRegisterMask(operands[1], size)

        emitBranchForTest(test, operands[2])
    when [Immediate, RegisterID, LocalLabelReference]
        $asm.puts "li x30, #{operands[0].riscv64Operand}"
        emitMultiplication(RISCV64ScratchRegister.x30, operands[1])
        $asm.puts "mv #{operands[1].riscv64Operand}, x30"
        riscv64EmitRegisterMask(operands[1], size)

        emitBranchForTest(test, operands[2])
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitOverflowBranchForOperation(operands, size, operation)
    raise "Unsupported size" unless size == :w

    def operationInstruction(operation)
        case operation
        when :add
            "add"
        when :sub
            "sub"
        when :mul
            "mul"
        else
            raise "Unsupported operation"
        end
    end

    instruction = operationInstruction(operation)

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, LocalLabelReference]
        operands = [operands[1], operands[0], operands[1], operands[2]]
    when [Immediate, RegisterID, LocalLabelReference]
        operands = [operands[1], operands[0], operands[1], operands[2]]
    end

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, RegisterID, LocalLabelReference]
        $asm.puts "sext.w x30, #{operands[0].riscv64Operand}"
        $asm.puts "sext.w x31, #{operands[1].riscv64Operand}"
        $asm.puts "#{instruction} x30, x30, x31"

        $asm.puts "mv #{operands[2].riscv64Operand}, x30"
        riscv64EmitRegisterMask(operands[2], size)

        $asm.puts "sext.w x31, x30"
        $asm.puts "bne x30, x31, #{operands[3].asmLabel}"
    when [RegisterID, Immediate, RegisterID, LocalLabelReference]
        $asm.puts "sext.w x30, #{operands[0].riscv64Operand}"
        $asm.puts "li x31, #{operands[1].riscv64Operand}"
        $asm.puts "sext.w x31, x31"
        $asm.puts "#{instruction} x30, x30, x31"

        $asm.puts "mv #{operands[2].riscv64Operand}, x30"
        riscv64EmitRegisterMask(operands[2], size)

        $asm.puts "sext.w x31, x30"
        $asm.puts "bne x30, x31, #{operands[3].asmLabel}"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitCompare(operands, size, condition)
    def signExtendRegisterForSize(register, target, size)
        case size
        when :b
            $asm.puts "slli #{target}, #{register.riscv64Operand}, 24"
            $asm.puts "sext.w #{target}, #{target}"
            $asm.puts "srai #{target}, #{target}, 24"
        when :w
            $asm.puts "sext.w #{target}, #{register.riscv64Operand}"
        when :d
            $asm.puts "mv #{target}, #{register.riscv64Operand}"
        else
            raise "Unsupported size"
        end
    end

    def signExtendImmediateForSize(immediate, target, size)
        $asm.puts "li #{target}, #{immediate.riscv64Operand}"
        case size
        when :b
            $asm.puts "slli #{target}, #{target}, 24"
            $asm.puts "sext.w #{target}, #{target}"
            $asm.puts "srai #{target}, #{target}, 24"
        when :w
            $asm.puts "sext.w #{target}, #{target}"
        when :d
        else
            raise "Unsupported size"
        end
    end

    def loadAndSignExtendAddressForSize(address, target, size)
        if address.riscv64RequiresLoad
            address.riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{riscv64LoadInstruction(size)} #{target}, 0(x31)"
        else
            $asm.puts "#{riscv64LoadInstruction(size)} #{target}, #{address.riscv64Operand}"
        end
    end

    def setForCondition(lhs, rhs, target, condition)
        case condition
        when :eq
            $asm.puts "sub x31, #{lhs}, #{rhs}"
            $asm.puts "seqz #{operands[2].riscv64Operand}, x31"
        when :neq
            $asm.puts "sub x31, #{lhs}, #{rhs}"
            $asm.puts "snez #{operands[2].riscv64Operand}, x31"
        when :a
            $asm.puts "sltu #{operands[2].riscv64Operand}, #{rhs}, #{lhs}"
        when :aeq
            $asm.puts "sltu #{operands[2].riscv64Operand}, #{lhs}, #{rhs}"
            $asm.puts "xori #{operands[2].riscv64Operand}, #{operands[2].riscv64Operand}, 1"
        when :b
            $asm.puts "sltu #{operands[2].riscv64Operand}, #{lhs}, #{rhs}"
        when :beq
            $asm.puts "sltu #{operands[2].riscv64Operand}, #{rhs}, #{lhs}"
            $asm.puts "xori #{operands[2].riscv64Operand}, #{operands[2].riscv64Operand}, 1"
        when :lt
            $asm.puts "slt #{target.riscv64Operand}, #{lhs}, #{rhs}"
        when :lteq
            $asm.puts "slt #{target.riscv64Operand}, #{rhs}, #{lhs}"
            $asm.puts "xori #{target.riscv64Operand}, #{target.riscv64Operand}, 1"
        when :gt
            $asm.puts "slt #{target.riscv64Operand}, #{rhs}, #{lhs}"
        when :gteq
            $asm.puts "slt #{target.riscv64Operand}, #{lhs}, #{rhs}"
            $asm.puts "xori #{target.riscv64Operand}, #{target.riscv64Operand}, 1"
        else
            raise "Unsupported condition"
        end
    end

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, RegisterID]
        signExtendRegisterForSize(operands[0], 'x30', size)
        signExtendRegisterForSize(operands[1], 'x31', size)
        setForCondition('x30', 'x31', operands[2], condition)
    when [RegisterID, Immediate, RegisterID]
        signExtendRegisterForSize(operands[0], 'x30', size)
        signExtendImmediateForSize(operands[1], 'x31', size)
        setForCondition('x30', 'x31', operands[2], condition)
    when [Address, RegisterID, RegisterID]
        loadAndSignExtendAddressForSize(operands[0], 'x30', size)
        signExtendRegisterForSize(operands[1], 'x31', size)
        setForCondition('x30', 'x31', operands[2], condition)
    when [Address, Immediate, RegisterID]
        loadAndSignExtendAddressForSize(operands[0], 'x30', size)
        signExtendImmediateForSize(operands[1], 'x31', size)
        setForCondition('x30', 'x31', operands[2], condition)
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitTest(operands, size, test)
    def testInstruction(test)
        case test
        when :z
            "seqz"
        when :nz
            "snez"
        else
            raise "Unknown test type"
        end
    end

    instruction = testInstruction(test)
    loadInstruction = riscv64ZeroExtendedLoadInstruction(size)

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID, RegisterID]
        $asm.puts "and x31, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, x31"
    when [RegisterID, Immediate, RegisterID]
        if operands[1].riscv64RequiresLoad
            $asm.puts "li x31, #{operands[1].riscv64Operand}"
            $asm.puts "and x31, #{operands[0].riscv64Operand}, x31"
        else
            $asm.puts "andi x31, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        end
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, x31"
    when [Address, Immediate, RegisterID]
        if operands[0].riscv64RequiresLoad
            operands[0].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{loadInstruction} x31, 0(x31)"
        else
            $asm.puts "#{loadInstruction} x31, #{operands[0].riscv64Operand}"
        end
        if operands[1].riscv64RequiresLoad
            $asm.puts "li x30, #{operands[1].riscv64Operand}"
            $asm.puts "and x31, x30, x31"
        else
            $asm.puts "andi x31, x31, #{operands[1].riscv64Operand}"
        end
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, x31"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitLogicalOperation(operands, size, operation)
    def opInstruction(operation)
        case operation
        when :and
            "and"
        when :or
            "or"
        when :xor
            "xor"
        else
            raise "Unsupported logical operation"
        end
    end

    instruction = opInstruction(operation)
    loadInstruction = riscv64ZeroExtendedLoadInstruction(size)
    storeInstruction = riscv64StoreInstruction(size)

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID]
        $asm.puts "#{instruction} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        riscv64EmitRegisterMask(operands[1], size)
    when [RegisterID, RegisterID, RegisterID]
        $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        riscv64EmitRegisterMask(operands[2], size)
    when [RegisterID, Immediate, RegisterID]
        if operands[1].riscv64RequiresLoad
            $asm.puts "li x31, #{operands[1].riscv64Operand}"
            $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, x31"
        else
            $asm.puts "#{instruction}i #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        end
        riscv64EmitRegisterMask(operands[2], size)
    when [Immediate, RegisterID]
        if operands[0].riscv64RequiresLoad
            $asm.puts "li x31, #{operands[0].riscv64Operand}"
            $asm.puts "#{instruction} #{operands[1].riscv64Operand}, x31, #{operands[1].riscv64Operand}"
        else
            $asm.puts "#{instruction}i #{operands[1].riscv64Operand}, #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        end
        riscv64EmitRegisterMask(operands[1], size)
    when [Immediate, Address]
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{loadInstruction} x30, 0(x31)"
        else
            $asm.puts "#{loadInstruction} x30, #{operands[1].riscv64Operand}"
        end
        $asm.puts "li x31, #{operands[0].riscv64Operand}"
        $asm.puts "#{instruction} x30, x31, x30"
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{storeInstruction} x30, 0(x31)"
        else
            $asm.puts "#{storeInstruction} x30, #{operands[1].riscv64Operand}"
        end
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitComplementOperation(operands, size, operation)
    def complementInstruction(size, operation)
        case operation
        when :not
            "not"
        when :neg
            size == :w ? "negw" : "neg"
        else
            raise "Unsupported complement operation"
        end
    end

    instruction = complementInstruction(size, operation)

    case riscv64OperandTypes(operands)
    when [RegisterID]
        $asm.puts "#{instruction} #{operands[0].riscv64Operand}, #{operands[0].riscv64Operand}"
        riscv64EmitRegisterMask(operands[0], size)
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitShift(operands, size, shift)
    raise "Unsupported size" unless [:w, :d].include? size

    def shiftInstruction(size, shift)
        case shift
        when :lleft
            size == :w ? "sllw" : "sll"
        when :lright
            size == :w ? "srlw" : "srl"
        when :aright
            size == :w ? "sraw" : "sra"
        else
            raise "Unsupported shift type"
        end
    end

    case riscv64OperandTypes(operands)
    when [RegisterID, RegisterID]
        $asm.puts "#{shiftInstruction(size, shift)} #{operands[1].riscv64Operand}, #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        riscv64EmitRegisterMask(operands[1], size)
    when [Immediate, RegisterID]
        $asm.puts "li x31, #{operands[0].riscv64Operand}"
        $asm.puts "#{shiftInstruction(size, shift)} #{operands[1].riscv64Operand}, #{operands[1].riscv64Operand}, x31"
        riscv64EmitRegisterMask(operands[1], size)
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitBitExtension(operands, fromSize, toSize, extensionType)
    raise "Unsupported operand types" unless riscv64OperandTypes(operands) == [RegisterID, RegisterID]

    def emitShifts(operands, shiftCount)
        $asm.puts "slli #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}, #{shiftCount}"
        $asm.puts "sext.w #{operands[1].riscv64Operand}, #{operands[1].riscv64Operand}"
        $asm.puts "srai #{operands[1].riscv64Operand}, #{operands[1].riscv64Operand}, #{shiftCount}"
    end

    case [fromSize, toSize, extensionType]
    when [:b, :w, :sign], [:b, :d, :sign]
        emitShifts(operands, 24)
        riscv64EmitRegisterMask(operands[1], toSize)
    when [:h, :w, :sign], [:h, :d, :sign]
        emitShifts(operands, 16)
        riscv64EmitRegisterMask(operands[1], toSize)
    when [:w, :d, :sign]
        $asm.puts "sext.w #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
    when [:w, :d, :zero]
        $asm.puts "slli #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}, 32"
        $asm.puts "srli #{operands[1].riscv64Operand}, #{operands[1].riscv64Operand}, 32"
    else
        raise "Unsupported bit-extension operation"
    end
end

def riscv64EmitFPLoad(operands, loadInstruction)
    case riscv64OperandTypes(operands)
    when [Address, FPRegisterID]
        if operands[0].riscv64RequiresLoad
            operands[0].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{loadInstruction} #{operands[1].riscv64Operand}, 0(x31)"
        else
            $asm.puts "#{loadInstruction} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        end
    when [BaseIndex, FPRegisterID]
        operands[0].riscv64Load(RISCV64ScratchRegister.x31, RISCV64ScratchRegister.x30)
        $asm.puts "#{loadInstruction} #{operands[1].riscv64Operand}, 0(x31)"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitFPStore(operands, storeInstruction)
    case riscv64OperandTypes(operands)
    when [FPRegisterID, Address]
        if operands[1].riscv64RequiresLoad
            operands[1].riscv64Load(RISCV64ScratchRegister.x31)
            $asm.puts "#{storeInstruction} #{operands[0].riscv64Operand}, 0(x31)"
        else
            $asm.puts "#{storeInstruction} #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        end
    when [FPRegisterID, BaseIndex]
        operands[1].riscv64Load(RISCV64ScratchRegister.x31, RISCV64ScratchRegister.x30)
        $asm.puts "#{storeInstruction} #{operands[0].riscv64Operand}, 0(x31)"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitFPOperation(operands, operation)
    case riscv64OperandTypes(operands)
    when [FPRegisterID, FPRegisterID, FPRegisterID]
        $asm.puts "#{operation} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
    when [FPRegisterID, FPRegisterID]
        $asm.puts "#{operation} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitFPCompare(operands, precision, condition)
    def suffixForPrecision(precision)
        case precision
        when :s
            "s"
        when :d
            "d"
        else
            raise "Unsupported precision"
        end
    end

    def instructionForCondition(condition, precision)
        suffix = suffixForPrecision(precision)
        case condition
        when :eq, :neq
            "feq.#{suffix}"
        when :lt, :gt
            "flt.#{suffix}"
        when :lteq, :gteq
            "fle.#{suffix}"
        else
            raise "Unsupported condition"
        end
    end

    def setForCondition(operands, precision, condition)
        instruction = instructionForCondition(condition, precision)
        case condition
        when :eq
            $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when :neq
            $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
            $asm.puts "xori #{operands[2].riscv64Operand}, #{operands[2].riscv64Operand}, 1"
        when :lt, :lteq
            $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[0].riscv64Operand}, #{operands[1].riscv64Operand}"
        when :gt, :gteq
            $asm.puts "#{instruction} #{operands[2].riscv64Operand}, #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
        else
            raise "Unsupported condition"
        end
    end

    case riscv64OperandTypes(operands)
    when [FPRegisterID, FPRegisterID, RegisterID]
        setForCondition(operands, precision, condition)
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitFPBitwiseOperation(operands, precision, operation)
    def suffixForPrecision(precision)
        case precision
        when :s
            "w"
        when :d
            "d"
        else
            raise "Unsupported precision"
        end
    end

    suffix = suffixForPrecision(precision)

    case riscv64OperandTypes(operands)
    when [FPRegisterID, FPRegisterID]
        $asm.puts "fmv.x.#{suffix} x30, #{operands[0].riscv64Operand}"
        $asm.puts "fmv.x.#{suffix} x31, #{operands[1].riscv64Operand}"
        $asm.puts "#{operation} x31, x30, x31"
        $asm.puts "fmv.#{suffix}.x #{operands[1].riscv64Operand}, x31"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitFPCopy(operands, precision)
    def suffixForPrecision(precision)
        case precision
        when :s
            "w"
        when :d
            "d"
        else
            raise "Unsupported precision"
        end
    end

    suffix = suffixForPrecision(precision)

    case riscv64OperandTypes(operands)
    when [RegisterID, FPRegisterID]
        $asm.puts "fmv.#{suffix}.x #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
    when [FPRegisterID, RegisterID]
        $asm.puts "fmv.x.#{suffix} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitFPConditionalBranchForTest(operands, precision, test)
    def suffixForPrecision(precision)
        case precision
        when :s
            "s"
        when :d
            "d"
        else
            raise "Unsupported precision"
        end
    end

    def emitBranchForUnordered(lhs, rhs, label, precision)
        suffix = suffixForPrecision(precision)

        $asm.puts "fclass.d x30, #{lhs.riscv64Operand}"
        $asm.puts "fclass.d x31, #{rhs.riscv64Operand}"
        $asm.puts "or x31, x30, x31"
        $asm.puts "li x30, 0x300"
        $asm.puts "and x31, x31, x30"
        $asm.puts "bnez x31, #{label.asmLabel}"
    end

    def emitBranchForTest(test, lhs, rhs, branch, label, precision)
        suffix = suffixForPrecision(precision)

        $asm.puts "#{test}.#{suffix} x31, #{lhs.riscv64Operand}, #{rhs.riscv64Operand}"
        $asm.puts "#{branch} x31, #{label.asmLabel}"
    end

    suffix = suffixForPrecision(precision)

    case riscv64OperandTypes(operands)
    when [FPRegisterID, FPRegisterID, LocalLabelReference]
        case test
        when :eq
            emitBranchForTest("feq", operands[0], operands[1], "bnez", operands[2], precision)
        when :neq
            emitBranchForTest("feq", operands[0], operands[1], "beqz", operands[2], precision)
        when :lt
            emitBranchForTest("flt", operands[0], operands[1], "bnez", operands[2], precision)
        when :lteq
            emitBranchForTest("fle", operands[0], operands[1], "bnez", operands[2], precision)
        when :gt
            emitBranchForTest("flt", operands[1], operands[0], "bnez", operands[2], precision)
        when :gteq
            emitBranchForTest("fle", operands[1], operands[0], "bnez", operands[2], precision)
        when :equn
            emitBranchForUnordered(operands[0], operands[1], operands[2], precision)
            emitBranchForTest("feq", operands[0], operands[1], "bnez", operands[2], precision)
        when :nequn
            emitBranchForUnordered(operands[0], operands[1], operands[2], precision)
            emitBranchForTest("feq", operands[0], operands[1], "beqz", operands[2], precision)
        when :ltun
            emitBranchForUnordered(operands[0], operands[1], operands[2], precision)
            emitBranchForTest("flt", operands[0], operands[1], "bnez", operands[2], precision)
        when :ltequn
            emitBranchForUnordered(operands[0], operands[1], operands[2], precision)
            emitBranchForTest("fle", operands[0], operands[1], "bnez", operands[2], precision)
        when :gtun
            emitBranchForUnordered(operands[0], operands[1], operands[2], precision)
            emitBranchForTest("flt", operands[1], operands[0], "bnez", operands[2], precision)
        when :gtequn
            emitBranchForUnordered(operands[0], operands[1], operands[2], precision)
            emitBranchForTest("fle", operands[1], operands[0], "bnez", operands[2], precision)
        else
            raise "Unsupported test"
        end
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitFPRoundOperation(operands, precision, roundingMode)
    def intSuffixForPrecision(precision)
        case precision
        when :s
            "w"
        when :d
            "l"
        else
            raise "Unsupported precision"
        end
    end

    def fpSuffixForPrecision(precision)
        case precision
        when :s
            "s"
        when :d
            "d"
        else
            raise "Unsupported precision"
        end
    end

    intSuffix = intSuffixForPrecision(precision)
    fpSuffix = fpSuffixForPrecision(precision)

    case riscv64OperandTypes(operands)
    when [FPRegisterID, FPRegisterID]
        $asm.puts "fcvt.#{intSuffix}.#{fpSuffix} x31, #{operands[0].riscv64Operand}, #{roundingMode}"
        $asm.puts "fcvt.#{fpSuffix}.#{intSuffix} #{operands[1].riscv64Operand}, x31, #{roundingMode}"
    else
        riscv64RaiseMismatchedOperands(operands)
    end
end

def riscv64EmitFPConvertOperation(operands, fromType, toType, roundingMode)
    def intSuffixForType(type)
        case type
        when :w
            "w"
        when :wu
            "wu"
        when :l
            "l"
        when :lu
            "lu"
        else
            raise "Unsupported precision"
        end
    end

    def fpSuffixForType(type)
        case type
        when :s
            "s"
        when :d
            "d"
        else
            raise "Unsupported precision"
        end
    end

    case riscv64OperandTypes(operands)
    when [FPRegisterID, RegisterID]
        fpSuffix = fpSuffixForType(fromType)
        intSuffix = intSuffixForType(toType)

        $asm.puts "fcvt.#{intSuffix}.#{fpSuffix} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}, #{roundingMode}"
    when [RegisterID, FPRegisterID]
        raise "Unsupported rounding mode" unless roundingMode == :none
        intSuffix = intSuffixForType(fromType)
        fpSuffix = fpSuffixForType(toType)

        $asm.puts "fcvt.#{fpSuffix}.#{intSuffix} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
    when [FPRegisterID, FPRegisterID]
        raise "Unsupported rounding mode" unless roundingMode == :none
        fpFromSuffix = fpSuffixForType(fromType)
        fpToSuffix = fpSuffixForType(toType)

        $asm.puts "fcvt.#{fpToSuffix}.#{fpFromSuffix} #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
    else
        riscv64RaiseMismatchedOperands(operands)
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

class RISCV64ScratchRegister
    def initialize(name)
        @name = name
    end

    def riscv64Operand
        case @name
        when :x30
            'x30'
        when :x31
            'x31'
        else
            raise "Unsupported scratch register"
        end
    end

    def self.x30
        RISCV64ScratchRegister.new(:x30)
    end

    def self.x31
        RISCV64ScratchRegister.new(:x31)
    end
end

class Immediate
    def riscv64Operand
        "#{value}"
    end

    def riscv64RequiresLoad
        value > 0x7ff or value < -0x800
    end

    def riscv64Load(target)
        $asm.puts "li #{target.riscv64Operand}, #{value}"
    end
end

class Address
    def riscv64Operand
        raise "Invalid offset #{offset.value} at #{codeOriginString}" if offset.value > 0x7ff or offset.value < -0x800
        "#{offset.value}(#{base.riscv64Operand})"
    end

    def riscv64RequiresLoad
        offset.value > 0x7ff or offset.value < -0x800
    end

    def riscv64Load(target)
        $asm.puts "li #{target.riscv64Operand}, #{offset.value}"
        $asm.puts "add #{target.riscv64Operand}, #{base.riscv64Operand}, #{target.riscv64Operand}"
    end
end

class BaseIndex
    def riscv64Load(target, scratch)
        case riscv64OperandTypes([base, index])
        when [RegisterID, RegisterID]
            $asm.puts "slli #{target.riscv64Operand}, #{index.riscv64Operand}, #{scaleShift}"
            $asm.puts "add #{target.riscv64Operand}, #{base.riscv64Operand}, #{target.riscv64Operand}"
            if offset.value != 0
                $asm.puts "li #{scratch.riscv64Operand}, #{offset.value}"
                $asm.puts "add #{target.riscv64Operand}, #{target.riscv64Operand}, #{scratch.riscv64Operand}"
            end
        else
            riscv64RaiseMismatchedOperands([base, index])
        end
    end
end

class Instruction
    def lowerRISCV64
        case opcode
        when "addi"
            riscv64EmitAdditionOperation(operands, :w, :add)
        when "addp", "addq"
            riscv64EmitAdditionOperation(operands, :d, :add)
        when "addis", "addps"
            riscv64RaiseUnsupported
        when "subi"
            riscv64EmitAdditionOperation(operands, :w, :sub)
        when "subp", "subq"
            riscv64EmitAdditionOperation(operands, :d, :sub)
        when "subis"
            riscv64RaiseUnsupported
        when "andi"
            riscv64EmitLogicalOperation(operands, :w, :and)
        when "andp", "andq"
            riscv64EmitLogicalOperation(operands, :d, :and)
        when "orh"
            riscv64EmitLogicalOperation(operands, :h, :or)
        when "ori"
            riscv64EmitLogicalOperation(operands, :w, :or)
        when "orp", "orq"
            riscv64EmitLogicalOperation(operands, :d, :or)
        when "xori"
            riscv64EmitLogicalOperation(operands, :w, :xor)
        when "xorp", "xorq"
            riscv64EmitLogicalOperation(operands, :d, :xor)
        when "lshifti"
            riscv64EmitShift(operands, :w, :lleft)
        when "lshiftp", "lshiftq"
            riscv64EmitShift(operands, :d, :lleft)
        when "rshifti"
            riscv64EmitShift(operands, :w, :aright)
        when "rshiftp", "rshiftq"
            riscv64EmitShift(operands, :d, :aright)
        when "urshifti"
            riscv64EmitShift(operands, :w, :lright)
        when "urshiftp", "urshiftq"
            riscv64EmitShift(operands, :d, :lright)
        when "muli"
            riscv64EmitMulDivArithmetic(operands, :w, :mul)
        when "mulp", "mulq"
            riscv64EmitMulDivArithmetic(operands, :d, :mul)
        when "divi"
            riscv64EmitMulDivArithmetic(operands, :w, :div)
        when "divq"
            riscv64EmitMulDivArithmetic(operands, :d, :div)
        when "divis", "divqs"
            riscv64RaiseUnsupported
        when "negi"
            riscv64EmitComplementOperation(operands, :w, :neg)
        when "negp", "negq"
            riscv64EmitComplementOperation(operands, :d, :neg)
        when "noti"
            riscv64EmitComplementOperation(operands, :w, :not)
        when "notq"
            riscv64EmitComplementOperation(operands, :d, :not)
        when "storeb"
            riscv64EmitStore(operands, :b)
        when "storeh"
            riscv64EmitStore(operands, :h)
        when "storei"
            riscv64EmitStore(operands, :w)
        when "storep", "storeq"
            riscv64EmitStore(operands, :d)
        when "loadb"
            riscv64EmitLoad(operands, :bu, :none)
        when "loadh"
            riscv64EmitLoad(operands, :hu, :none)
        when "loadi"
            riscv64EmitLoad(operands, :wu, :none)
        when "loadis"
            riscv64EmitLoad(operands, :w, :none)
        when "loadp", "loadq"
            riscv64EmitLoad(operands, :d, :none)
        when "loadbsi"
            riscv64EmitLoad(operands, :b, :w)
        when "loadbsq"
            riscv64EmitLoad(operands, :b, :none)
        when "loadhsi"
            riscv64EmitLoad(operands, :h, :w)
        when "loadhsq"
            riscv64EmitLoad(operands, :h, :none)
        when "bfeq"
            riscv64EmitFPConditionalBranchForTest(operands, :s, :eq)
        when "bflt"
            riscv64EmitFPConditionalBranchForTest(operands, :s, :lt)
        when "bfgt"
            riscv64EmitFPConditionalBranchForTest(operands, :s, :gt)
        when "bfltun"
            riscv64EmitFPConditionalBranchForTest(operands, :s, :ltun)
        when "bfltequn"
            riscv64EmitFPConditionalBranchForTest(operands, :s, :ltequn)
        when "bfgtun"
            riscv64EmitFPConditionalBranchForTest(operands, :s, :gtun)
        when "bfgtequn"
            riscv64EmitFPConditionalBranchForTest(operands, :s, :gtequn)
        when "bdeq"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :eq)
        when "bdneq"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :neq)
        when "bdlt"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :lt)
        when "bdlteq"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :lteq)
        when "bdgt"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :gt)
        when "bdgteq"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :gteq)
        when "bdequn"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :equn)
        when "bdnequn"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :nequn)
        when "bdltun"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :ltun)
        when "bdltequn"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :ltequn)
        when "bdgtun"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :gtun)
        when "bdgtequn"
            riscv64EmitFPConditionalBranchForTest(operands, :d, :gtequn)
        when "td2i", "bcd2i", "btd2i"
            riscv64RaiseUnsupported
        when "movdz"
            riscv64RaiseUnsupported
        when "pop"
            size = 8 * operands.size
            operands.each_with_index {
                | op, index |
                $asm.puts "ld #{op.riscv64Operand}, #{size - 8 * (index + 1)}(sp)"
            }
            $asm.puts "addi sp, sp, #{size}"
        when "push"
            size = 8 * operands.size
            $asm.puts "addi sp, sp, #{-size}"
            operands.reverse.each_with_index {
                | op, index |
                $asm.puts "sd #{op.riscv64Operand}, #{size - 8 * (index + 1)}(sp)"
            }
        when "move"
            case riscv64OperandTypes(operands)
            when [RegisterID, RegisterID]
                $asm.puts "mv #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
            when [Immediate, RegisterID]
                $asm.puts "li #{operands[1].riscv64Operand}, #{operands[0].riscv64Operand}"
            else
                riscv64RaiseMismatchedOperands(operands)
            end
        when "sxb2i"
            riscv64EmitBitExtension(operands, :b, :w, :sign)
        when "sxb2q"
            riscv64EmitBitExtension(operands, :b, :d, :sign)
        when "sxh2i"
            riscv64EmitBitExtension(operands, :h, :w, :sign)
        when "sxh2q"
            riscv64EmitBitExtension(operands, :h, :d, :sign)
        when "sxi2p", "sxi2q"
            riscv64EmitBitExtension(operands, :w, :d, :sign)
        when "zxi2p", "zxi2q"
            riscv64EmitBitExtension(operands, :w, :d, :zero)
        when "nop"
            $asm.puts "nop"
        when "bbeq"
            riscv64EmitConditionalBranch(operands, :b, :eq)
        when "bieq"
            riscv64EmitConditionalBranch(operands, :w, :eq)
        when "bpeq", "bqeq"
            riscv64EmitConditionalBranch(operands, :d, :eq)
        when "bbneq"
            riscv64EmitConditionalBranch(operands, :b, :neq)
        when "bineq"
            riscv64EmitConditionalBranch(operands, :w, :neq)
        when "bpneq", "bqneq"
            riscv64EmitConditionalBranch(operands, :d, :neq)
        when "bba"
            riscv64EmitConditionalBranch(operands, :b, :a)
        when "bia"
            riscv64EmitConditionalBranch(operands, :w, :a)
        when "bpa", "bqa"
            riscv64EmitConditionalBranch(operands, :d, :a)
        when "bbaeq"
            riscv64EmitConditionalBranch(operands, :b, :aeq)
        when "biaeq"
            riscv64EmitConditionalBranch(operands, :w, :aeq)
        when "bpaeq", "bqaeq"
            riscv64EmitConditionalBranch(operands, :d, :aeq)
        when "bbb"
            riscv64EmitConditionalBranch(operands, :b, :b)
        when "bib"
            riscv64EmitConditionalBranch(operands, :w, :b)
        when "bpb", "bqb"
            riscv64EmitConditionalBranch(operands, :d, :b)
        when "bbbeq"
            riscv64EmitConditionalBranch(operands, :b, :beq)
        when "bibeq"
            riscv64EmitConditionalBranch(operands, :w, :beq)
        when "bpbeq", "bqbeq"
            riscv64EmitConditionalBranch(operands, :d, :beq)
        when "bbgt"
            riscv64EmitConditionalBranch(operands, :b, :gt)
        when "bigt"
            riscv64EmitConditionalBranch(operands, :w, :gt)
        when "bpgt", "bqgt"
            riscv64EmitConditionalBranch(operands, :d, :gt)
        when "bbgteq"
            riscv64EmitConditionalBranch(operands, :b, :gteq)
        when "bigteq"
            riscv64EmitConditionalBranch(operands, :w, :gteq)
        when "bpgteq", "bqgteq"
            riscv64EmitConditionalBranch(operands, :d, :gteq)
        when "bblt"
            riscv64EmitConditionalBranch(operands, :b, :lt)
        when "bilt"
            riscv64EmitConditionalBranch(operands, :w, :lt)
        when "bplt", "bqlt"
            riscv64EmitConditionalBranch(operands, :d, :lt)
        when "bblteq"
            riscv64EmitConditionalBranch(operands, :b, :lteq)
        when "bilteq"
            riscv64EmitConditionalBranch(operands, :w, :lteq)
        when "bplteq", "bqlteq"
            riscv64EmitConditionalBranch(operands, :d, :lteq)
        when "btbz"
            riscv64EmitConditionalBranchForTest(operands, :b, :z)
        when "btbnz"
            riscv64EmitConditionalBranchForTest(operands, :b, :nz)
        when "btbs"
            riscv64EmitConditionalBranchForTest(operands, :b, :s)
        when "btiz"
            riscv64EmitConditionalBranchForTest(operands, :w, :z)
        when "btinz"
            riscv64EmitConditionalBranchForTest(operands, :w, :nz)
        when "btis"
            riscv64EmitConditionalBranchForTest(operands, :w, :s)
        when "btpz", "btqz"
            riscv64EmitConditionalBranchForTest(operands, :d, :z)
        when "btpnz", "btqnz"
            riscv64EmitConditionalBranchForTest(operands, :d, :nz)
        when "btps", "btqs"
            riscv64EmitConditionalBranchForTest(operands, :d, :s)
        when "baddiz"
            riscv64EmitConditionalBranchForAdditionOperation(operands, :w, :add, :z)
        when "baddinz"
            riscv64EmitConditionalBranchForAdditionOperation(operands, :w, :add, :nz)
        when "baddis"
            riscv64EmitConditionalBranchForAdditionOperation(operands, :w, :add, :s)
        when "baddio"
            riscv64EmitOverflowBranchForOperation(operands, :w, :add)
        when "baddpz", "baddqz"
            riscv64EmitConditionalBranchForAdditionOperation(operands, :d, :add, :z)
        when "baddpnz", "baddqnz"
            riscv64EmitConditionalBranchForAdditionOperation(operands, :d, :add, :nz)
        when "baddps", "baddqs"
            riscv64EmitConditionalBranchForAdditionOperation(operands, :d, :add, :s)
        when "baddpo", "baddqo"
            riscv64RaiseUnsupported
        when "bsubiz"
            riscv64EmitConditionalBranchForAdditionOperation(operands, :w, :sub, :z)
        when "bsubinz"
            riscv64EmitConditionalBranchForAdditionOperation(operands, :w, :sub, :nz)
        when "bsubis"
            riscv64EmitConditionalBranchForAdditionOperation(operands, :w, :sub, :s)
        when "bsubio"
            riscv64EmitOverflowBranchForOperation(operands, :w, :sub)
        when "bmuliz"
            riscv64EmitConditionalBranchForMultiplicationOperation(operands, :w, :z)
        when "bmulinz"
            riscv64EmitConditionalBranchForMultiplicationOperation(operands, :w, :nz)
        when "bmulis"
            riscv64EmitConditionalBranchForMultiplicationOperation(operands, :w, :s)
        when "bmulio"
            riscv64EmitOverflowBranchForOperation(operands, :w, :mul)
        when "boriz", "borinz", "boris", "borio"
            riscv64RaiseUnsupported
        when "jmp"
            case riscv64OperandTypes(operands)
            when [RegisterID, Immediate]
                $asm.puts "jr #{operands[0].riscv64Operand}"
            when [BaseIndex, Immediate, Immediate]
                operands[0].riscv64Load(RISCV64ScratchRegister.x31, RISCV64ScratchRegister.x30)
                $asm.puts "ld x31, 0(x31)"
                $asm.puts "jr x31"
            when [Address, Immediate]
                if operands[0].riscv64RequiresLoad
                    operands[0].riscv64Load(RISCV64ScratchRegister.x31)
                    $asm.puts "ld x31, 0(x31)"
                    $asm.puts "jr x31"
                else
                    $asm.puts "ld x31, #{operands[0].riscv64Operand}"
                    $asm.puts "jr x31"
                end
            when [LabelReference]
                $asm.puts "tail #{operands[0].asmLabel}"
            when [LocalLabelReference]
                $asm.puts "tail #{operands[0].asmLabel}"
            else
                riscv64RaiseMismatchedOperands(operands)
            end
        when "call"
            case riscv64OperandTypes(operands)
            when [RegisterID, Immediate]
                $asm.puts "jalr #{operands[0].riscv64Operand}"
            when [Address, Immediate]
                if operands[0].riscv64RequiresLoad
                    operands[0].riscv64Load(RISCV64ScratchRegister.x31)
                    $asm.puts "ld x31, 0(x31)"
                    $asm.puts "jalr x31"
                else
                    $asm.puts "ld x31, #{operands[0].riscv64Operand}"
                    $asm.puts "jalr x31"
                end
            when [LabelReference]
                $asm.puts "call #{operands[0].asmLabel}"
            else
                riscv64RaiseMismatchedOperands(operands)
            end
        when "break"
            $asm.puts "ebreak"
        when "ret"
            $asm.puts "ret"
        when "cbeq"
            riscv64EmitCompare(operands, :b, :eq)
        when "cieq"
            riscv64EmitCompare(operands, :w, :eq)
        when "cpeq", "cqeq"
            riscv64EmitCompare(operands, :d, :eq)
        when "cbneq"
            riscv64EmitCompare(operands, :b, :neq)
        when "cineq"
            riscv64EmitCompare(operands, :w, :neq)
        when "cpneq", "cqneq"
            riscv64EmitCompare(operands, :d, :neq)
        when "cba"
            riscv64EmitCompare(operands, :b, :a)
        when "cia"
            riscv64EmitCompare(operands, :w, :a)
        when "cpa", "cqa"
            riscv64EmitCompare(operands, :d, :a)
        when "cbaeq"
            riscv64EmitCompare(operands, :b, :aeq)
        when "ciaeq"
            riscv64EmitCompare(operands, :w, :aeq)
        when "cpaeq", "cqaeq"
            riscv64EmitCompare(operands, :d, :aeq)
        when "cbb"
            riscv64EmitCompare(operands, :b, :b)
        when "cib"
            riscv64EmitCompare(operands, :w, :b)
        when "cpb", "cqb"
            riscv64EmitCompare(operands, :d, :b)
        when "cbbeq"
            riscv64EmitCompare(operands, :b, :beq)
        when "cibeq"
            riscv64EmitCompare(operands, :w, :beq)
        when "cpbeq", "cqbeq"
            riscv64EmitCompare(operands, :d, :beq)
        when "cblt"
            riscv64EmitCompare(operands, :b, :lt)
        when "cilt"
            riscv64EmitCompare(operands, :w, :lt)
        when "cplt", "cqlt"
            riscv64EmitCompare(operands, :d, :lt)
        when "cblteq"
            riscv64EmitCompare(operands, :b, :lteq)
        when "cilteq"
            riscv64EmitCompare(operands, :w, :lteq)
        when "cplteq", "cqlteq"
            riscv64EmitCompare(operands, :d, :lteq)
        when "cbgt"
            riscv64EmitCompare(operands, :b, :gt)
        when "cigt"
            riscv64EmitCompare(operands, :w, :gt)
        when "cpgt", "cqgt"
            riscv64EmitCompare(operands, :d, :gt)
        when "cbgteq"
            riscv64EmitCompare(operands, :b, :gteq)
        when "cigteq"
            riscv64EmitCompare(operands, :w, :gteq)
        when "cpgteq", "cqgteq"
            riscv64EmitCompare(operands, :d, :gteq)
        when "tbz"
            riscv64EmitTest(operands, :b, :z)
        when "tbnz"
            riscv64EmitTest(operands, :b, :nz)
        when "tiz"
            riscv64EmitTest(operands, :w, :z)
        when "tinz"
            riscv64EmitTest(operands, :w, :nz)
        when "tpz", "tqz"
            riscv64EmitTest(operands, :d, :z)
        when "tpnz", "tqnz"
            riscv64EmitTest(operands, :d, :nz)
        when "tbs", "tis", "tps", "tqs"
            riscv64RaiseUnsupported
        when "peek", "poke"
            riscv64RaiseUnsupported
        when "bo", "bs", "bz", "bnz"
            riscv64RaiseUnsupported
        when "leap", "leaq"
            case riscv64OperandTypes(operands)
            when [Address, RegisterID]
                if operands[0].riscv64RequiresLoad
                    operands[0].riscv64Load(RISCV64ScratchRegister.x31)
                    $asm.puts "mv #{operands[1].riscv64Operand}, x31"
                else
                    $asm.puts "addi #{operands[1].riscv64Operand}, #{operands[0].base.riscv64Operand}, #{operands[0].offset.value}"
                end
            when [BaseIndex, RegisterID]
                operands[0].riscv64Load(RISCV64ScratchRegister.x31, RISCV64ScratchRegister.x30)
                $asm.puts "mv #{operands[1].riscv64Operand}, x31"
            when [LabelReference, RegisterID]
                $asm.puts "lla #{operands[1].riscv64Operand}, #{operands[0].asmLabel}"
            else
                riscv64RaiseMismatchedOperands(operands)
            end
        when "smulli"
            riscv64RaiseUnsupported
        when "memfence"
            $asm.puts "fence rw, rw"
        when "fence"
            $asm.puts "fence"
        when "bfiq"
            riscv64RaiseUnsupported
        when "pcrtoaddr"
            case riscv64OperandTypes(operands)
            when [LabelReference, RegisterID]
                $asm.puts "lla #{operands[1].riscv64Operand}, #{operands[0].asmLabel}"
            else
                riscv64RaiseMismatchedOperands(operands)
            end
        when "globaladdr"
            case riscv64OperandTypes(operands)
            when [LabelReference, RegisterID]
                $asm.puts "la #{operands[1].riscv64Operand}, #{operands[0].asmLabel}"
            else
                riscv64RaiseMismatchedOperands(operands)
            end
        when "lrotatei", "lrotateq"
            riscv64RaiseUnsupported
        when "rrotatei", "rrotateq"
            riscv64RaiseUnsupported
        when "moved"
            riscv64EmitFPOperation(operands, "fmv.d")
        when "loadf"
            riscv64EmitFPLoad(operands, "flw")
        when "loadd"
            riscv64EmitFPLoad(operands, "fld")
        when "storef"
            riscv64EmitFPStore(operands, "fsw")
        when "stored"
            riscv64EmitFPStore(operands, "fsd")
        when "addf"
            riscv64EmitFPOperation([operands[0], operands[1], operands[1]], "fadd.s")
        when "addd"
            riscv64EmitFPOperation([operands[0], operands[1], operands[1]], "fadd.d")
        when "subf"
            riscv64EmitFPOperation([operands[1], operands[0], operands[1]], "fsub.s")
        when "subd"
            riscv64EmitFPOperation([operands[1], operands[0], operands[1]], "fsub.d")
        when "mulf"
            riscv64EmitFPOperation([operands[0], operands[1], operands[1]], "fmul.s")
        when "muld"
            riscv64EmitFPOperation([operands[0], operands[1], operands[1]], "fmul.d")
        when "divf"
            riscv64EmitFPOperation([operands[1], operands[0], operands[1]], "fdiv.s")
        when "divd"
            riscv64EmitFPOperation([operands[1], operands[0], operands[1]], "fdiv.d")
        when "sqrtf"
            riscv64EmitFPOperation(operands, "fsqrt.s")
        when "sqrtd"
            riscv64EmitFPOperation(operands, "fsqrt.d")
        when "absf"
            riscv64EmitFPOperation(operands, "fabs.s")
        when "absd"
            riscv64EmitFPOperation(operands, "fabs.d")
        when "negf"
            riscv64EmitFPOperation(operands, "fneg.s")
        when "negd"
            riscv64EmitFPOperation(operands, "fneg.d")
        when "floorf"
            riscv64EmitFPRoundOperation(operands, :s, "rdn")
        when "floord"
            riscv64EmitFPRoundOperation(operands, :d, "rdn")
        when "ceilf"
            riscv64EmitFPRoundOperation(operands, :s, "rup")
        when "ceild"
            riscv64EmitFPRoundOperation(operands, :d, "rup")
        when "roundf"
            riscv64EmitFPRoundOperation(operands, :s, "rne")
        when "roundd"
            riscv64EmitFPRoundOperation(operands, :d, "rne")
        when "truncatef"
            riscv64EmitFPRoundOperation(operands, :s, "rtz")
        when "truncated"
            riscv64EmitFPRoundOperation(operands, :d, "rtz")
        when "truncatef2i"
            riscv64EmitFPConvertOperation(operands, :s, :wu, "rtz")
        when "truncated2i"
            riscv64EmitFPConvertOperation(operands, :d, :wu, "rtz")
        when "truncatef2q"
            riscv64EmitFPConvertOperation(operands, :s, :lu, "rtz")
        when "truncated2q"
            riscv64EmitFPConvertOperation(operands, :d, :lu, "rtz")
        when "truncatef2is"
            riscv64EmitFPConvertOperation(operands, :s, :w, "rtz")
        when "truncated2is"
            riscv64EmitFPConvertOperation(operands, :d, :w, "rtz")
        when "truncatef2qs"
            riscv64EmitFPConvertOperation(operands, :s, :l, "rtz")
        when "truncated2qs"
            riscv64EmitFPConvertOperation(operands, :d, :l, "rtz")
        when "ci2f"
            riscv64EmitFPConvertOperation(operands, :wu, :s, :none)
        when "ci2d"
            riscv64EmitFPConvertOperation(operands, :wu, :d, :none)
        when "ci2fs"
            riscv64EmitFPConvertOperation(operands, :w, :s, :none)
        when "ci2ds"
            riscv64EmitFPConvertOperation(operands, :w, :d, :none)
        when "cq2f"
            riscv64EmitFPConvertOperation(operands, :lu, :s, :none)
        when "cq2d"
            riscv64EmitFPConvertOperation(operands, :lu, :d, :none)
        when "cq2fs"
            riscv64EmitFPConvertOperation(operands, :l, :s, :none)
        when "cq2ds"
            riscv64EmitFPConvertOperation(operands, :l, :d, :none)
        when "cf2d"
            riscv64EmitFPConvertOperation(operands, :s, :d, :none)
        when "cd2f"
            riscv64EmitFPConvertOperation(operands, :d, :s, :none)
        when "tzcnti", "tzcntq"
            riscv64RaiseUnsupported
        when "lzcnti", "lzcntq"
            riscv64RaiseUnsupported
        when "andf"
            riscv64EmitFPBitwiseOperation(operands, :s, "and")
        when "andd"
            riscv64EmitFPBitwiseOperation(operands, :d, "and")
        when "orf"
            riscv64EmitFPBitwiseOperation(operands, :s, "or")
        when "ord"
            riscv64EmitFPBitwiseOperation(operands, :d, "or")
        when "cfeq"
            riscv64EmitFPCompare(operands, :s, :eq)
        when "cfneq"
            riscv64EmitFPCompare(operands, :s, :neq)
        when "cflt"
            riscv64EmitFPCompare(operands, :s, :lt)
        when "cflteq"
            riscv64EmitFPCompare(operands, :s, :lteq)
        when "cfgt"
            riscv64EmitFPCompare(operands, :s, :gt)
        when "cfgteq"
            riscv64EmitFPCompare(operands, :s, :gteq)
        when "cfnequn"
            riscv64EmitFPCompare(operands, :s, :nequn)
        when "cdeq"
            riscv64EmitFPCompare(operands, :d, :eq)
        when "cdneq"
            riscv64EmitFPCompare(operands, :d, :neq)
        when "cdlt"
            riscv64EmitFPCompare(operands, :d, :lt)
        when "cdlteq"
            riscv64EmitFPCompare(operands, :d, :lteq)
        when "cdgt"
            riscv64EmitFPCompare(operands, :d, :gt)
        when "cdgteq"
            riscv64EmitFPCompare(operands, :d, :gteq)
        when "cdnequn"
            riscv64EmitFPCompare(operands, :d, :nequn)
        when "fi2f"
            riscv64EmitFPCopy(operands, :s)
        when "ff2i"
            riscv64EmitFPCopy(operands, :s)
        when "fp2d", "fq2d"
            riscv64EmitFPCopy(operands, :d)
        when "fd2p", "fd2q"
            riscv64EmitFPCopy(operands, :d)
        when "tls_loadp", "tls_storep"
            riscv64RaiseUnsupported
        when "loadlinkacqb", "loadlinkacqh", "loadlinkacqi", "loadlinkacqq"
            riscv64RaiseUnsupported
        when "storecondrelb", "storecondrelh", "storecondreli", "storecondrelq"
            riscv64RaiseUnsupported
        when "atomicxchgaddb", "atomicxchgaddh", "atomicxchgaddi", "atomicxchgaddq"
            riscv64RaiseUnsupported
        when "atomicxchgclearb", "atomicxchgclearh", "atomicxchgcleari", "atomicxchgclearq"
            riscv64RaiseUnsupported
        when "atomicxchgorb", "atomicxchgorh", "atomicxchgori", "atomicxchgorq"
            riscv64RaiseUnsupported
        when "atomicxchgxorb", "atomicxchgxorh", "atomicxchgxori", "atomicxchgxorq"
            riscv64RaiseUnsupported
        when "atomicxchgb", "atomicxchgh", "atomicxchgi", "atomicxchgq"
            riscv64RaiseUnsupported
        when "atomicweakcasb", "atomicweakcash", "atomicweakcasi", "atomicweakcasq"
            riscv64RaiseUnsupported
        when "atomicloadb", "atomicloadh", "atomicloadi", "atomicloadq"
            riscv64RaiseUnsupported
        else
            lowerDefault
        end
    end
end
