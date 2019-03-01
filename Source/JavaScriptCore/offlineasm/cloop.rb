# Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

# The CLoop llint backend is initially based on the ARMv7 backend, and
# then further enhanced with a few instructions from the x86 backend to
# support building for X64 targets.  Hence, the shape of the generated
# code and the usage convention of registers will look a lot like the
# ARMv7 backend's.

def cloopMapType(type)
    case type
    when :intptr;         ".i()"
    when :uintptr;        ".u()"
    when :int32;          ".i32()"
    when :uint32;         ".u32()"
    when :int64;          ".i64()"
    when :uint64;         ".u64()"
    when :int8;           ".i8()"
    when :uint8;          ".u8()"
    when :int8Ptr;        ".i8p()"
    when :voidPtr;        ".vp()"
    when :nativeFunc;     ".nativeFunc()"
    when :double;         ".d()"
    when :bitsAsDouble;   ".bitsAsDouble()"
    when :bitsAsInt64;    ".bitsAsInt64()"
    when :opcode;         ".opcode()"
    else;
        raise "Unsupported type"
    end
end


class SpecialRegister < NoChildren
    def clLValue(type=:intptr)
        clDump
    end
    def clDump
        @name
    end
    def clValue(type=:intptr)
        @name + cloopMapType(type)
    end
end

C_LOOP_SCRATCH_FPR = SpecialRegister.new("d6")

class RegisterID
    def clDump
        case name
        # The cloop is modelled on the ARM implementation. Hence, the a0-a3
        # registers are aliases for r0-r3 i.e. t0-t3 in our case.
        when "t0", "a0", "r0"
            "t0"
        when "t1", "a1", "r1"
            "t1"
        when "t2", "a2"
            "t2"
        when "t3", "a3"
            "t3"
        when "t4"
            "pc"
        when "t5"
            "t5"
        when "csr0"
            "pcBase"
        when "csr1"
            "tagTypeNumber"
        when "csr2"
            "tagMask"
        when "csr3"
            "metadataTable"
        when "cfr"
            "cfr"
        when "lr"
            "lr"
        when "sp"
            "sp"
        else
            raise "Bad register #{name} for C_LOOP at #{codeOriginString}"
        end
    end
    def clLValue(type=:intptr)
        clDump
    end
    def clValue(type=:intptr)
        clDump + cloopMapType(type)
    end
end

class FPRegisterID
    def clDump
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
            raise "Bad register #{name} for C_LOOP at #{codeOriginString}"
        end
    end
    def clLValue(type=:intptr)
        clDump
    end
    def clValue(type=:intptr)
        clDump + cloopMapType(type)
    end
end

class Immediate
    def clDump
        "#{value}"
    end
    def clLValue(type=:intptr)
        raise "Immediate cannot be used as an LValue"
    end
    def clValue(type=:intptr)
        # There is a case of a very large unsigned number (0x8000000000000000)
        # which we wish to encode.  Unfortunately, the C/C++ compiler
        # complains if we express that number as a positive decimal integer.
        # Hence, for positive values, we just convert the number into hex form
        # to keep the compiler happy.
        #
        # However, for negative values, the to_s(16) hex conversion method does
        # not strip the "-" sign resulting in a meaningless "0x-..." valueStr.
        # To workaround this, we simply don't encode negative numbers as hex.

        valueStr = (value < 0) ? "#{value}" : "0x#{value.to_s(16)}"

        case type
        when :int8;    "int8_t(#{valueStr})"
        when :int32;   "int32_t(#{valueStr})"
        when :int64;   "int64_t(#{valueStr})"
        when :intptr;  "intptr_t(#{valueStr})"
        when :uint8;   "uint8_t(#{valueStr})"
        when :uint32;  "uint32_t(#{valueStr})"
        when :uint64;  "uint64_t(#{valueStr})"
        when :uintptr; "uintptr_t(#{valueStr})"
        else
            raise "Not implemented immediate of type: #{type}" 
        end
    end
end

class Address
    def clDump
        "[#{base.clDump}, #{offset.value}]"
    end
    def clLValue(type=:intptr)
        clValue(type)
    end
    def clValue(type=:intptr)
        case type
        when :int8;         int8MemRef
        when :int32;        int32MemRef
        when :int64;        int64MemRef
        when :intptr;       intptrMemRef
        when :uint8;        uint8MemRef
        when :uint32;       uint32MemRef
        when :uint64;       uint64MemRef
        when :uintptr;      uintptrMemRef
        when :opcode;       opcodeMemRef
        when :nativeFunc;   nativeFuncMemRef
        else
            raise "Unexpected Address type: #{type}"
        end
    end
    def pointerExpr
        if  offset.value == 0
            "#{base.clValue(:int8Ptr)}"
        elsif offset.value > 0
            "#{base.clValue(:int8Ptr)} + #{offset.value}"
        else
            "#{base.clValue(:int8Ptr)} - #{-offset.value}"
        end
    end
    def int8MemRef
        "*CAST<int8_t*>(#{pointerExpr})"
    end
    def int16MemRef
        "*CAST<int16_t*>(#{pointerExpr})"
    end
    def int32MemRef
        "*CAST<int32_t*>(#{pointerExpr})"
    end
    def int64MemRef
        "*CAST<int64_t*>(#{pointerExpr})"
    end
    def intptrMemRef
        "*CAST<intptr_t*>(#{pointerExpr})"
    end
    def uint8MemRef
        "*CAST<uint8_t*>(#{pointerExpr})"
    end
    def uint16MemRef
        "*CAST<uint16_t*>(#{pointerExpr})"
    end
    def uint32MemRef
        "*CAST<uint32_t*>(#{pointerExpr})"
    end
    def uint64MemRef
        "*CAST<uint64_t*>(#{pointerExpr})"
    end
    def uintptrMemRef
        "*CAST<uintptr_t*>(#{pointerExpr})"
    end
    def nativeFuncMemRef
        "*CAST<NativeFunction*>(#{pointerExpr})"
    end
    def opcodeMemRef
        "*CAST<Opcode*>(#{pointerExpr})"
    end
    def dblMemRef
        "*CAST<double*>(#{pointerExpr})"
    end
end

class BaseIndex
    def clDump
        "[#{base.clDump}, #{offset.clDump}, #{index.clDump} << #{scaleShift}]"
    end
    def clLValue(type=:intptr)
        clValue(type)
    end
    def clValue(type=:intptr)
        case type
        when :int8;       int8MemRef
        when :int32;      int32MemRef
        when :int64;      int64MemRef
        when :intptr;     intptrMemRef
        when :uint8;      uint8MemRef
        when :uint32;     uint32MemRef
        when :uint64;     uint64MemRef
        when :uintptr;    uintptrMemRef
        when :opcode;     opcodeMemRef
        else
            raise "Unexpected BaseIndex type: #{type}"
        end
    end
    def pointerExpr
        if offset.value == 0
            "#{base.clValue(:int8Ptr)} + (#{index.clValue} << #{scaleShift})"
        else
            "#{base.clValue(:int8Ptr)} + (#{index.clValue} << #{scaleShift}) + #{offset.clValue}"
        end
    end
    def int8MemRef
        "*CAST<int8_t*>(#{pointerExpr})"
    end
    def int16MemRef
        "*CAST<int16_t*>(#{pointerExpr})"
    end
    def int32MemRef
        "*CAST<int32_t*>(#{pointerExpr})"
    end
    def int64MemRef
        "*CAST<int64_t*>(#{pointerExpr})"
    end
    def intptrMemRef
        "*CAST<intptr_t*>(#{pointerExpr})"
    end
    def uint8MemRef
        "*CAST<uint8_t*>(#{pointerExpr})"
    end
    def uint16MemRef
        "*CAST<uint16_t*>(#{pointerExpr})"
    end
    def uint32MemRef
        "*CAST<uint32_t*>(#{pointerExpr})"
    end
    def uint64MemRef
        "*CAST<uint64_t*>(#{pointerExpr})"
    end
    def uintptrMemRef
        "*CAST<uintptr_t*>(#{pointerExpr})"
    end
    def opcodeMemRef
        "*CAST<Opcode*>(#{pointerExpr})"
    end
    def dblMemRef
        "*CAST<double*>(#{pointerExpr})"
    end
end

class AbsoluteAddress
    def clDump
        "#{codeOriginString}"
    end
    def clLValue(type=:intptr)
        clValue(type)
    end
    def clValue
        clDump
    end
end

class LabelReference
    def intptrMemRef
        "*CAST<intptr_t*>(&#{cLabel})"
    end
    def cloopEmitLea(destination, type)
        $asm.putc "#{destination.clLValue(:voidPtr)} = CAST<void*>(&#{cLabel});"
    end
end


#
# Lea support.
#

class Address
    def cloopEmitLea(destination, type)
        if destination == base
            $asm.putc "#{destination.clLValue(:int8Ptr)} += #{offset.clValue(type)};"
        else
            $asm.putc "#{destination.clLValue(:int8Ptr)} = #{base.clValue(:int8Ptr)} + #{offset.clValue(type)};"
        end
    end
end

class BaseIndex
    def cloopEmitLea(destination, type)
        raise "Malformed BaseIndex, offset should be zero at #{codeOriginString}" unless offset.value == 0
        $asm.putc "#{destination.clLValue(:int8Ptr)} = #{base.clValue(:int8Ptr)} + (#{index.clValue} << #{scaleShift});"
    end
end

#
# Actual lowering code follows.
#

class Sequence
    def getModifiedListC_LOOP
        myList = @list
        
        # Verify that we will only see instructions and labels.
        myList.each {
            | node |
            unless node.is_a? Instruction or
                    node.is_a? Label or
                    node.is_a? LocalLabel or
                    node.is_a? Skip
                raise "Unexpected #{node.inspect} at #{node.codeOrigin}" 
            end
        }
        
        return myList
    end
end

def clOperands(operands)
    operands.map{|v| v.clDump}.join(", ")
end


def cloopEmitOperation(operands, type, operator)
    raise unless type == :intptr || type == :uintptr || type == :int32 || type == :uint32 || \
        type == :int64 || type == :uint64 || type == :double
    if operands.size == 3
        op1 = operands[0]
        op2 = operands[1]
        dst = operands[2]
    else
        raise unless operands.size == 2
        op1 = operands[1]
        op2 = operands[0]
        dst = operands[1]
    end
    raise unless not dst.is_a? Immediate
    if dst.is_a? RegisterID and (type == :int32 or type == :uint32)
        truncationHeader = "(uint32_t)("
        truncationFooter = ")"
    else
        truncationHeader = ""
        truncationFooter = ""
    end
    $asm.putc "#{dst.clLValue(type)} = #{truncationHeader}#{op1.clValue(type)} #{operator} #{op2.clValue(type)}#{truncationFooter};"
end

def cloopEmitShiftOperation(operands, type, operator)
    raise unless type == :intptr || type == :uintptr || type == :int32 || type == :uint32 || type == :int64 || type == :uint64
    if operands.size == 3
        op1 = operands[0]
        op2 = operands[1]
        dst = operands[2]
    else
        op1 = operands[1]
        op2 = operands[0]
        dst = operands[1]
    end
    if dst.is_a? RegisterID and (type == :int32 or type == :uint32)
        truncationHeader = "(uint32_t)("
        truncationFooter = ")"
    else
        truncationHeader = ""
        truncationFooter = ""
    end
    shiftMask = "((sizeof(uintptr_t) == 8) ? 0x3f : 0x1f)" if type == :intptr || type == :uintptr
    shiftMask = "0x3f" if type == :int64 || type == :uint64
    shiftMask = "0x1f" if type == :int32 || type == :uint32
    $asm.putc "#{dst.clLValue(type)} = #{truncationHeader}#{operands[1].clValue(type)} #{operator} (#{operands[0].clValue(:intptr)} & #{shiftMask})#{truncationFooter};"
end

def cloopEmitUnaryOperation(operands, type, operator)
    raise unless type == :intptr || type == :uintptr || type == :int32 || type == :uint32 || type == :int64 || type == :uint64
    raise unless operands.size == 1
    raise unless not operands[0].is_a? Immediate
    op = operands[0]
    dst = operands[0]
    if dst.is_a? RegisterID and (type == :int32 or type == :uint32)
        truncationHeader = "(uint32_t)("
        truncationFooter = ")"
    else
        truncationHeader = ""
        truncationFooter = ""
    end
    $asm.putc "#{dst.clLValue(type)} = #{truncationHeader}#{operator}#{op.clValue(type)}#{truncationFooter};"
end

def cloopEmitCompareDoubleWithNaNCheckAndBranch(operands, condition)
    $asm.putc "if (std::isnan(#{operands[0].clValue(:double)}) || std::isnan(#{operands[1].clValue(:double)})"
    $asm.putc "    || (#{operands[0].clValue(:double)} #{condition} #{operands[1].clValue(:double)}))"
    $asm.putc "    goto #{operands[2].cLabel};"
end


def cloopEmitCompareAndSet(operands, type, comparator)
    # The result is a boolean.  Hence, it doesn't need to be based on the type
    # of the arguments being compared.
    $asm.putc "#{operands[2].clLValue(type)} = (#{operands[0].clValue(type)} #{comparator} #{operands[1].clValue(type)});"
end


def cloopEmitCompareAndBranch(operands, type, comparator)
    $asm.putc "if (#{operands[0].clValue(type)} #{comparator} #{operands[1].clValue(type)})"
    $asm.putc "    goto #{operands[2].cLabel};"
end


# conditionTest should contain a string that provides a comparator and a RHS
# value e.g. "< 0".
def cloopGenerateConditionExpression(operands, type, conditionTest)
    op1 = operands[0].clValue(type)

    # The operands must consist of 2 or 3 values.
    case operands.size
    when 2 # Just test op1 against the conditionTest.
        lhs = op1
    when 3 # Mask op1 with op2 before testing against the conditionTest.
        lhs = "(#{op1} & #{operands[1].clValue(type)})"
    else
        raise "Expected 2 or 3 operands but got #{operands.size} at #{codeOriginString}"
    end
    
    "#{lhs} #{conditionTest}"
end

# conditionTest should contain a string that provides a comparator and a RHS
# value e.g. "< 0".
def cloopEmitTestAndBranchIf(operands, type, conditionTest, branchTarget)
    conditionExpr = cloopGenerateConditionExpression(operands, type, conditionTest)
    $asm.putc "if (#{conditionExpr})"
    $asm.putc "    goto #{branchTarget};"
end

def cloopEmitTestSet(operands, type, conditionTest)
    # The result is a boolean condition.  Hence, the result type is always an
    # int.  The passed in type is only used for the values being tested in
    # the condition test.
    conditionExpr = cloopGenerateConditionExpression(operands, type, conditionTest)
    $asm.putc "#{operands[-1].clLValue} = (#{conditionExpr});"
end

def cloopEmitOpAndBranch(operands, operator, type, conditionTest)
    case type
    when :intptr; tempType = "intptr_t"
    when :int32; tempType = "int32_t"
    when :int64; tempType = "int64_t"
    else
        raise "Unimplemented type"
    end

    $asm.putc "{"
    $asm.putc "    #{tempType} temp = #{operands[1].clValue(type)} #{operator} #{operands[0].clValue(type)};"
    $asm.putc "    #{operands[1].clLValue(type)} = temp;"
    $asm.putc "    if (temp #{conditionTest})"
    $asm.putc "        goto  #{operands[2].cLabel};"
    $asm.putc "}"
end

def cloopEmitOpAndBranchIfOverflow(operands, operator, type)
    case type
    when :int32
        tempType = "int32_t"
        truncationHeader = "(uint32_t)("
        truncationFooter = ")"
    else
        raise "Unimplemented type"
    end

    $asm.putc "{"

    # Emit the overflow test based on the operands and the type:
    case operator
    when "+"; operation = "add"
    when "-"; operation = "sub"
    when "*"; operation = "multiply"
    else
        raise "Unimplemented opeartor"
    end

    $asm.putc "    #{tempType} result;"
    $asm.putc "    bool success = WTF::ArithmeticOperations<#{tempType}, #{tempType}, #{tempType}>::#{operation}(#{operands[1].clValue(type)}, #{operands[0].clValue(type)}, result);"
    $asm.putc "    #{operands[1].clLValue(type)} = #{truncationHeader}result#{truncationFooter};"
    $asm.putc "    if (!success)"
    $asm.putc "        goto #{operands[2].cLabel};"
    $asm.putc "}"
end

# operands: callTarget, currentFrame, currentPC
def cloopEmitCallSlowPath(operands)
    $asm.putc "{"
    $asm.putc "    cloopStack.setCurrentStackPointer(sp.vp());"
    $asm.putc "    SlowPathReturnType result = #{operands[0].cLabel}(#{operands[1].clDump}, #{operands[2].clDump});"
    $asm.putc "    decodeResult(result, t0, t1);"
    $asm.putc "}"
end

def cloopEmitCallSlowPathVoid(operands)
    $asm.putc "cloopStack.setCurrentStackPointer(sp.vp());"
    $asm.putc "#{operands[0].cLabel}(#{operands[1].clDump}, #{operands[2].clDump});"
end

class Instruction
    def lowerC_LOOP
        case opcode
        when "addi"
            cloopEmitOperation(operands, :int32, "+")
        when "addq"
            cloopEmitOperation(operands, :int64, "+")
        when "addp"
            cloopEmitOperation(operands, :intptr, "+")

        when "andi"
            cloopEmitOperation(operands, :int32, "&")
        when "andq"
            cloopEmitOperation(operands, :int64, "&")
        when "andp"
            cloopEmitOperation(operands, :intptr, "&")

        when "ori"
            cloopEmitOperation(operands, :int32, "|")
        when "orq"
            cloopEmitOperation(operands, :int64, "|")
        when "orp"
            cloopEmitOperation(operands, :intptr, "|")

        when "xori"
            cloopEmitOperation(operands, :int32, "^")
        when "xorq"
            cloopEmitOperation(operands, :int64, "^")
        when "xorp"
            cloopEmitOperation(operands, :intptr, "^")

        when "lshifti"
            cloopEmitShiftOperation(operands, :int32, "<<")
        when "lshiftq"
            cloopEmitShiftOperation(operands, :int64, "<<")
        when "lshiftp"
            cloopEmitShiftOperation(operands, :intptr, "<<")

        when "rshifti"
            cloopEmitShiftOperation(operands, :int32, ">>")
        when "rshiftq"
            cloopEmitShiftOperation(operands, :int64, ">>")
        when "rshiftp"
            cloopEmitShiftOperation(operands, :intptr, ">>")

        when "urshifti"
            cloopEmitShiftOperation(operands, :uint32, ">>")
        when "urshiftq"
            cloopEmitShiftOperation(operands, :uint64, ">>")
        when "urshiftp"
            cloopEmitShiftOperation(operands, :uintptr, ">>")

        when "muli"
            cloopEmitOperation(operands, :int32, "*")
        when "mulq"
            cloopEmitOperation(operands, :int64, "*")
        when "mulp"
            cloopEmitOperation(operands, :intptr, "*")

        when "subi"
            cloopEmitOperation(operands, :int32, "-")
        when "subq"
            cloopEmitOperation(operands, :int64, "-")
        when "subp"
            cloopEmitOperation(operands, :intptr, "-")

        when "negi"
            cloopEmitUnaryOperation(operands, :int32, "-")
        when "negq"
            cloopEmitUnaryOperation(operands, :int64, "-")
        when "negp"
            cloopEmitUnaryOperation(operands, :intptr, "-")

        when "noti"
            cloopEmitUnaryOperation(operands, :int32, "~")

        when "loadi"
            $asm.putc "#{operands[1].clLValue(:uint32)} = #{operands[0].uint32MemRef};"
            # There's no need to call clearHighWord() here because the above will
            # automatically take care of 0 extension.
        when "loadis"
            $asm.putc "#{operands[1].clLValue(:int32)} = #{operands[0].int32MemRef};"
        when "loadq"
            $asm.putc "#{operands[1].clLValue(:int64)} = #{operands[0].int64MemRef};"
        when "loadp"
            $asm.putc "#{operands[1].clLValue} = #{operands[0].intptrMemRef};"
        when "storei"
            $asm.putc "#{operands[1].int32MemRef} = #{operands[0].clValue(:int32)};"
        when "storeq"
            $asm.putc "#{operands[1].int64MemRef} = #{operands[0].clValue(:int64)};"
        when "storep"
            $asm.putc "#{operands[1].intptrMemRef} = #{operands[0].clValue(:intptr)};"
        when "loadb"
            $asm.putc "#{operands[1].clLValue(:intptr)} = #{operands[0].uint8MemRef};"
        when "loadbs"
            $asm.putc "#{operands[1].clLValue(:intptr)} = (uint32_t)(#{operands[0].int8MemRef});"
        when "loadbsp"
            $asm.putc "#{operands[1].clLValue(:intptr)} = #{operands[0].int8MemRef};"
        when "storeb"
            $asm.putc "#{operands[1].uint8MemRef} = #{operands[0].clValue(:int8)};"
        when "loadh"
            $asm.putc "#{operands[1].clLValue(:intptr)} = #{operands[0].uint16MemRef};"
        when "loadhs"
            $asm.putc "#{operands[1].clLValue(:intptr)} = (uint32_t)(#{operands[0].int16MemRef});"
        when "storeh"
            $asm.putc "*#{operands[1].uint16MemRef} = #{operands[0].clValue(:int16)};"
        when "loadd"
            $asm.putc "#{operands[1].clLValue(:double)} = #{operands[0].dblMemRef};"
        when "stored"
            $asm.putc "#{operands[1].dblMemRef} = #{operands[0].clValue(:double)};"

        when "addd"
            cloopEmitOperation(operands, :double, "+")
        when "divd"
            cloopEmitOperation(operands, :double, "/")
        when "subd"
            cloopEmitOperation(operands, :double, "-")
        when "muld"
            cloopEmitOperation(operands, :double, "*")

        # Convert an int value to its double equivalent, and store it in a double register.
        when "ci2d"
            $asm.putc "#{operands[1].clLValue(:double)} = (double)#{operands[0].clValue(:int32)}; // ci2d"

        when "bdeq"
            cloopEmitCompareAndBranch(operands, :double, "==")
        when "bdneq"
            cloopEmitCompareAndBranch(operands, :double, "!=")
        when "bdgt"
            cloopEmitCompareAndBranch(operands, :double, ">");
        when "bdgteq"
            cloopEmitCompareAndBranch(operands, :double, ">=");
        when "bdlt"
            cloopEmitCompareAndBranch(operands, :double, "<");
        when "bdlteq"
            cloopEmitCompareAndBranch(operands, :double, "<=");

        when "bdequn"
            cloopEmitCompareDoubleWithNaNCheckAndBranch(operands, "==")
        when "bdnequn"
            cloopEmitCompareDoubleWithNaNCheckAndBranch(operands, "!=")
        when "bdgtun"
            cloopEmitCompareDoubleWithNaNCheckAndBranch(operands, ">")
        when "bdgtequn"
            cloopEmitCompareDoubleWithNaNCheckAndBranch(operands, ">=")
        when "bdltun"
            cloopEmitCompareDoubleWithNaNCheckAndBranch(operands, "<")
        when "bdltequn"
            cloopEmitCompareDoubleWithNaNCheckAndBranch(operands, "<=")

        when "td2i"
            $asm.putc "#{operands[1].clLValue(:intptr)} = (uint32_t)(intptr_t)#{operands[0].clValue(:double)}; // td2i"

        when "bcd2i"  # operands: srcDbl dstInt slowPath
            $asm.putc "{ // bcd2i"
            $asm.putc "    double d = #{operands[0].clValue(:double)};"
            $asm.putc "    const int32_t asInt32 = int32_t(d);"
            $asm.putc "    if (asInt32 != d || (!asInt32 && std::signbit(d))) // true for -0.0"
            $asm.putc "        goto  #{operands[2].cLabel};"
            $asm.putc "    #{operands[1].clLValue} = (uint32_t)asInt32;"
            $asm.putc "}"

        when "move"
            $asm.putc "#{operands[1].clLValue(:intptr)} = #{operands[0].clValue(:intptr)};"
        when "sxi2q"
            $asm.putc "#{operands[1].clLValue(:int64)} = #{operands[0].clValue(:int32)};"
        when "zxi2q"
            $asm.putc "#{operands[1].clLValue(:uint64)} = #{operands[0].clValue(:uint32)};"
        when "nop"
            $asm.putc "// nop"
        when "bbeq"
            cloopEmitCompareAndBranch(operands, :int8, "==")
        when "bieq"
            cloopEmitCompareAndBranch(operands, :int32, "==")
        when "bqeq"
            cloopEmitCompareAndBranch(operands, :int64, "==")
        when "bpeq"
            cloopEmitCompareAndBranch(operands, :intptr, "==")

        when "bbneq"
            cloopEmitCompareAndBranch(operands, :int8, "!=")
        when "bineq"
            cloopEmitCompareAndBranch(operands, :int32, "!=")
        when "bqneq"
            cloopEmitCompareAndBranch(operands, :int64, "!=")
        when "bpneq"
            cloopEmitCompareAndBranch(operands, :intptr, "!=")

        when "bba"
            cloopEmitCompareAndBranch(operands, :uint8, ">")
        when "bia"
            cloopEmitCompareAndBranch(operands, :uint32, ">")
        when "bqa"
            cloopEmitCompareAndBranch(operands, :uint64, ">")
        when "bpa"
            cloopEmitCompareAndBranch(operands, :uintptr, ">")

        when "bbaeq"
            cloopEmitCompareAndBranch(operands, :uint8, ">=")
        when "biaeq"
            cloopEmitCompareAndBranch(operands, :uint32, ">=")
        when "bqaeq"
            cloopEmitCompareAndBranch(operands, :uint64, ">=")
        when "bpaeq"
            cloopEmitCompareAndBranch(operands, :uintptr, ">=")

        when "bbb"
            cloopEmitCompareAndBranch(operands, :uint8, "<")
        when "bib"
            cloopEmitCompareAndBranch(operands, :uint32, "<")
        when "bqb"
            cloopEmitCompareAndBranch(operands, :uint64, "<")
        when "bpb"
            cloopEmitCompareAndBranch(operands, :uintptr, "<")

        when "bbbeq"
            cloopEmitCompareAndBranch(operands, :uint8, "<=")
        when "bibeq"
            cloopEmitCompareAndBranch(operands, :uint32, "<=")
        when "bqbeq"
            cloopEmitCompareAndBranch(operands, :uint64, "<=")
        when "bpbeq"
            cloopEmitCompareAndBranch(operands, :uintptr, "<=")

        when "bbgt"
            cloopEmitCompareAndBranch(operands, :int8, ">")
        when "bigt"
            cloopEmitCompareAndBranch(operands, :int32, ">")
        when "bqgt"
            cloopEmitCompareAndBranch(operands, :int64, ">")
        when "bpgt"
            cloopEmitCompareAndBranch(operands, :intptr, ">")

        when "bbgteq"
            cloopEmitCompareAndBranch(operands, :int8, ">=")
        when "bigteq"
            cloopEmitCompareAndBranch(operands, :int32, ">=")
        when "bqgteq"
            cloopEmitCompareAndBranch(operands, :int64, ">=")
        when "bpgteq"
            cloopEmitCompareAndBranch(operands, :intptr, ">=")

        when "bblt"
            cloopEmitCompareAndBranch(operands, :int8, "<")
        when "bilt"
            cloopEmitCompareAndBranch(operands, :int32, "<")
        when "bqlt"
            cloopEmitCompareAndBranch(operands, :int64, "<")
        when "bplt"
            cloopEmitCompareAndBranch(operands, :intptr, "<")

        when "bblteq"
            cloopEmitCompareAndBranch(operands, :int8, "<=")
        when "bilteq"
            cloopEmitCompareAndBranch(operands, :int32, "<=")
        when "bqlteq"
            cloopEmitCompareAndBranch(operands, :int64, "<=")
        when "bplteq"
            cloopEmitCompareAndBranch(operands, :intptr, "<=")

        when "btbz"
            cloopEmitTestAndBranchIf(operands, :int8, "== 0", operands[-1].cLabel)
        when "btiz"
            cloopEmitTestAndBranchIf(operands, :int32, "== 0", operands[-1].cLabel)
        when "btqz"
            cloopEmitTestAndBranchIf(operands, :int64, "== 0", operands[-1].cLabel)
        when "btpz"
            cloopEmitTestAndBranchIf(operands, :intptr, "== 0", operands[-1].cLabel)

        when "btbnz"
            cloopEmitTestAndBranchIf(operands, :int8, "!= 0", operands[-1].cLabel)
        when "btinz"
            cloopEmitTestAndBranchIf(operands, :int32, "!= 0", operands[-1].cLabel)
        when "btqnz"
            cloopEmitTestAndBranchIf(operands, :int64, "!= 0", operands[-1].cLabel)
        when "btpnz"
            cloopEmitTestAndBranchIf(operands, :intptr, "!= 0", operands[-1].cLabel)

        when "btbs"
            cloopEmitTestAndBranchIf(operands, :int8, "< 0", operands[-1].cLabel)
        when "btis"
            cloopEmitTestAndBranchIf(operands, :int32, "< 0", operands[-1].cLabel)
        when "btqs"
            cloopEmitTestAndBranchIf(operands, :int64, "< 0", operands[-1].cLabel)
        when "btps"
            cloopEmitTestAndBranchIf(operands, :intptr, "< 0", operands[-1].cLabel)

        # For jmp, we do not want to assume that we have COMPUTED_GOTO support.
        # Fortunately, the only times we should ever encounter indirect jmps is
        # when the jmp target is a CLoop opcode (by design).
        #
        # Hence, we check if the jmp target is a known label reference. If so,
        # we can emit a goto directly. If it is not a known target, then we set
        # the target in the opcode, and dispatch to it via whatever dispatch
        # mechanism is in used.
        when "jmp"
            if operands[0].is_a? LocalLabelReference or operands[0].is_a? LabelReference
                # Handles jumps local or global labels.
                $asm.putc "goto #{operands[0].cLabel};"
            else
                # Handles jumps to some computed target.
                # NOTE: must be an opcode handler or a llint glue helper.
                $asm.putc "opcode = #{operands[0].clValue(:opcode)};"
                $asm.putc "DISPATCH_OPCODE();"
            end

        when "call"
            $asm.putc "CRASH(); // generic call instruction not supported by design!"
        when "break"
            $asm.putc "CRASH(); // break instruction not implemented."
        when "ret"
            $asm.putc "opcode = lr.opcode();"
            $asm.putc "DISPATCH_OPCODE();"

        when "cbeq"
            cloopEmitCompareAndSet(operands, :uint8, "==")
        when "cieq"
            cloopEmitCompareAndSet(operands, :uint32, "==")
        when "cqeq"
            cloopEmitCompareAndSet(operands, :uint64, "==")
        when "cpeq"
            cloopEmitCompareAndSet(operands, :uintptr, "==")

        when "cbneq"
            cloopEmitCompareAndSet(operands, :uint8, "!=")
        when "cineq"
            cloopEmitCompareAndSet(operands, :uint32, "!=")
        when "cqneq"
            cloopEmitCompareAndSet(operands, :uint64, "!=")
        when "cpneq"
            cloopEmitCompareAndSet(operands, :uintptr, "!=")

        when "cba"
            cloopEmitCompareAndSet(operands, :uint8, ">")
        when "cia"
            cloopEmitCompareAndSet(operands, :uint32, ">")
        when "cqa"
            cloopEmitCompareAndSet(operands, :uint64, ">")
        when "cpa"
            cloopEmitCompareAndSet(operands, :uintptr, ">")

        when "cbaeq"
            cloopEmitCompareAndSet(operands, :uint8, ">=")
        when "ciaeq"
            cloopEmitCompareAndSet(operands, :uint32, ">=")
        when "cqaeq"
            cloopEmitCompareAndSet(operands, :uint64, ">=")
        when "cpaeq"
            cloopEmitCompareAndSet(operands, :uintptr, ">=")

        when "cbb"
            cloopEmitCompareAndSet(operands, :uint8, "<")
        when "cib"
            cloopEmitCompareAndSet(operands, :uint32, "<")
        when "cqb"
            cloopEmitCompareAndSet(operands, :uint64, "<")
        when "cpb"
            cloopEmitCompareAndSet(operands, :uintptr, "<")

        when "cbbeq"
            cloopEmitCompareAndSet(operands, :uint8, "<=")
        when "cibeq"
            cloopEmitCompareAndSet(operands, :uint32, "<=")
        when "cqbeq"
            cloopEmitCompareAndSet(operands, :uint64, "<=")
        when "cpbeq"
            cloopEmitCompareAndSet(operands, :uintptr, "<=")

        when "cbgt"
            cloopEmitCompareAndSet(operands, :int8, ">")
        when "cigt"
            cloopEmitCompareAndSet(operands, :int32, ">")
        when "cqgt"
            cloopEmitCompareAndSet(operands, :int64, ">")
        when "cpgt"
            cloopEmitCompareAndSet(operands, :intptr, ">")

        when "cbgteq"
            cloopEmitCompareAndSet(operands, :int8, ">=")
        when "cigteq"
            cloopEmitCompareAndSet(operands, :int32, ">=")
        when "cqgteq"
            cloopEmitCompareAndSet(operands, :int64, ">=")
        when "cpgteq"
            cloopEmitCompareAndSet(operands, :intptr, ">=")

        when "cblt"
            cloopEmitCompareAndSet(operands, :int8, "<")
        when "cilt"
            cloopEmitCompareAndSet(operands, :int32, "<")
        when "cqlt"
            cloopEmitCompareAndSet(operands, :int64, "<")
        when "cplt"
            cloopEmitCompareAndSet(operands, :intptr, "<")

        when "cblteq"
            cloopEmitCompareAndSet(operands, :int8, "<=")
        when "cilteq"
            cloopEmitCompareAndSet(operands, :int32, "<=")
        when "cqlteq"
            cloopEmitCompareAndSet(operands, :int64, "<=")
        when "cplteq"
            cloopEmitCompareAndSet(operands, :intptr, "<=")

        when "tbs"
            cloopEmitTestSet(operands, :int8, "< 0")
        when "tis"
            cloopEmitTestSet(operands, :int32, "< 0")
        when "tqs"
            cloopEmitTestSet(operands, :int64, "< 0")
        when "tps"
            cloopEmitTestSet(operands, :intptr, "< 0")

        when "tbz"
            cloopEmitTestSet(operands, :int8, "== 0")
        when "tiz"
            cloopEmitTestSet(operands, :int32, "== 0")
        when "tqz"
            cloopEmitTestSet(operands, :int64, "== 0")
        when "tpz"
            cloopEmitTestSet(operands, :intptr, "== 0")

        when "tbnz"
            cloopEmitTestSet(operands, :int8, "!= 0")
        when "tinz"
            cloopEmitTestSet(operands, :int32, "!= 0")
        when "tqnz"
            cloopEmitTestSet(operands, :int64, "!= 0")
        when "tpnz"
            cloopEmitTestSet(operands, :intptr, "!= 0")

        # 64-bit instruction: cdqi (based on X64)
        # Sign extends the lower 32 bits of t0, but put the sign extension into
        # the lower 32 bits of t1. Leave the upper 32 bits of t0 and t1 unchanged.
        when "cdqi"
            $asm.putc "{ // cdqi"
            $asm.putc "    int64_t temp = t0.i32(); // sign extend the low 32bit"
            $asm.putc "    t0 = (uint32_t)temp; // low word"
            $asm.putc "    t1 = (uint32_t)(temp >> 32); // high word"
            $asm.putc "}"

        # 64-bit instruction: idivi op1 (based on X64)
        # Divide a 64-bit integer numerator by the specified denominator.
        # The numerator is specified in t0 and t1 as follows:
        #     1. low 32 bits of the numerator is in the low 32 bits of t0.
        #     2. high 32 bits of the numerator is in the low 32 bits of t1.
        #
        # The resultant quotient is a signed 32-bit int, and is to be stored
        # in the lower 32 bits of t0.
        # The resultant remainder is a signed 32-bit int, and is to be stored
        # in the lower 32 bits of t1.
        when "idivi"
            # Divide t1,t0 (EDX,EAX) by the specified arg, and store the remainder in t1,
            # and quotient in t0:
            $asm.putc "{ // idivi"
            $asm.putc "    int64_t dividend = (int64_t(t1.u32()) << 32) | t0.u32();"
            $asm.putc "    int64_t divisor = #{operands[0].clValue(:intptr)};"
            $asm.putc "    t1 = (uint32_t)(dividend % divisor); // remainder"
            $asm.putc "    t0 = (uint32_t)(dividend / divisor); // quotient"
            $asm.putc "}"

        # 32-bit instruction: fii2d int32LoOp int32HiOp dblOp (based on ARMv7)
        # Decode 2 32-bit ints (low and high) into a 64-bit double.
        when "fii2d"
            $asm.putc "#{operands[2].clLValue(:double)} = ints2Double(#{operands[0].clValue(:uint32)}, #{operands[1].clValue(:uint32)}); // fii2d"

        # 32-bit instruction: f2dii dblOp int32LoOp int32HiOp (based on ARMv7)
        # Encode a 64-bit double into 2 32-bit ints (low and high).
        when "fd2ii"
            $asm.putc "double2Ints(#{operands[0].clValue(:double)}, #{operands[1].clDump}, #{operands[2].clDump}); // fd2ii"

        # 64-bit instruction: fq2d int64Op dblOp (based on X64)
        # Copy a bit-encoded double in a 64-bit int register to a double register.
        when "fq2d"
            $asm.putc "#{operands[1].clLValue(:double)} = #{operands[0].clValue(:bitsAsDouble)}; // fq2d"

        # 64-bit instruction: fd2q dblOp int64Op (based on X64 instruction set)
        # Copy a double as a bit-encoded double into a 64-bit int register.
        when "fd2q"
            $asm.putc "#{operands[1].clLValue(:int64)} = #{operands[0].clValue(:bitsAsInt64)}; // fd2q"

        when "leai"
            operands[0].cloopEmitLea(operands[1], :int32)
        when "leap"
            operands[0].cloopEmitLea(operands[1], :intptr)

        when "baddio"
            cloopEmitOpAndBranchIfOverflow(operands, "+", :int32)
        when "bsubio"
            cloopEmitOpAndBranchIfOverflow(operands, "-", :int32)
        when "bmulio"
            cloopEmitOpAndBranchIfOverflow(operands, "*", :int32)

        when "baddis"
            cloopEmitOpAndBranch(operands, "+", :int32, "< 0")
        when "baddiz"
            cloopEmitOpAndBranch(operands, "+", :int32, "== 0")
        when "baddinz"
            cloopEmitOpAndBranch(operands, "+", :int32, "!= 0")

        when "baddqs"
            cloopEmitOpAndBranch(operands, "+", :int64, "< 0")
        when "baddqz"
            cloopEmitOpAndBranch(operands, "+", :int64, "== 0")
        when "baddqnz"
            cloopEmitOpAndBranch(operands, "+", :int64, "!= 0")

        when "baddps"
            cloopEmitOpAndBranch(operands, "+", :intptr, "< 0")
        when "baddpz"
            cloopEmitOpAndBranch(operands, "+", :intptr, "== 0")
        when "baddpnz"
            cloopEmitOpAndBranch(operands, "+", :intptr, "!= 0")

        when "bsubis"
            cloopEmitOpAndBranch(operands, "-", :int32, "< 0")
        when "bsubiz"
            cloopEmitOpAndBranch(operands, "-", :int32, "== 0")
        when "bsubinz"
            cloopEmitOpAndBranch(operands, "-", :int32, "!= 0")

        when "borris"
            cloopEmitOpAndBranch(operands, "|", :int32, "< 0")
        when "borriz"
            cloopEmitOpAndBranch(operands, "|", :int32, "== 0")
        when "borrinz"
            cloopEmitOpAndBranch(operands, "|", :int32, "!= 0")
            
        when "memfence"

        when "push"
            operands.each {
                | op |
                $asm.putc "PUSH(#{op.clDump});"
            }
        when "pop"
            operands.each {
                | op |
                $asm.putc "POP(#{op.clDump});"
            }


        # A convenience and compact call to crash because we don't want to use
        # the generic llint crash mechanism which relies on the availability
        # of the call instruction (which cannot be implemented in a generic
        # way, and can be abused if we made it just work for this special case).
        # Using a special cloopCrash instruction is cleaner.
        when "cloopCrash"
            $asm.putc "CRASH();"

        # We can't rely on the llint JS call mechanism which actually makes
        # use of the call instruction. Instead, we just implement JS calls
        # as an opcode dispatch.
        when "cloopCallJSFunction"
            uid = $asm.newUID
            $asm.putc "lr = getOpcode(llint_cloop_did_return_from_js_#{uid});"
            $asm.putc "opcode = #{operands[0].clValue(:opcode)};"
            $asm.putc "DISPATCH_OPCODE();"
            $asm.putsLabel("llint_cloop_did_return_from_js_#{uid}", false)

        # We can't do generic function calls with an arbitrary set of args, but
        # fortunately we don't have to here. All native function calls always
        # have a fixed prototype of 1 args: the passed ExecState.
        when "cloopCallNative"
            $asm.putc "cloopStack.setCurrentStackPointer(sp.vp());"
            $asm.putc "nativeFunc = #{operands[0].clValue(:nativeFunc)};"
            $asm.putc "functionReturnValue = JSValue::decode(nativeFunc(t0.execState()));"
            $asm.putc "#if USE(JSVALUE32_64)"
            $asm.putc "    t1 = functionReturnValue.tag();"
            $asm.putc "    t0 = functionReturnValue.payload();"
            $asm.putc "#else // USE_JSVALUE64)"
            $asm.putc "    t0 = JSValue::encode(functionReturnValue);"
            $asm.putc "#endif // USE_JSVALUE64)"

        # We can't do generic function calls with an arbitrary set of args, but
        # fortunately we don't have to here. All slow path function calls always
        # have a fixed prototype too. See cloopEmitCallSlowPath() for details.
        when "cloopCallSlowPath"
            cloopEmitCallSlowPath(operands)

        when "cloopCallSlowPathVoid"
            cloopEmitCallSlowPathVoid(operands)

        # For debugging only. This is used to insert instrumentation into the
        # generated LLIntAssembly.h during llint development only. Do not use
        # for production code.
        when "cloopDo"
            $asm.putc "#{annotation}"

        else
            lowerDefault
        end
    end

    def recordMetaDataC_LOOP
        $asm.codeOrigin codeOriginString if $enableCodeOriginComments
        $asm.annotation annotation if $enableInstrAnnotations && (opcode != "cloopDo")
        $asm.debugAnnotation codeOrigin.debugDirective if $enableDebugAnnotations
    end
end
