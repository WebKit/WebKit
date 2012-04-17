# Copyright (C) 2011 Apple Inc. All rights reserved.
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

class Node
    def armV7Single
        doubleOperand = armV7Operand
        raise "Bogus register name #{doubleOperand}" unless doubleOperand =~ /^d/
        "s" + ($~.post_match.to_i * 2).to_s
    end
end

class SpecialRegister < NoChildren
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
            $asm.puts "movt #{register.armV7Operand}, \##{value >> 16}"
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
# Lowering of branch ops. For example:
#
# baddiz foo, bar, baz
#
# will become:
#
# addi foo, bar
# bz baz
#

def armV7LowerBranchOps(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when /^b(addi|subi|ori|addp)/
                op = $1
                branch = "b" + $~.post_match
                
                case op
                when "addi", "addp"
                    op = "addis"
                when "subi"
                    op = "subis"
                when "ori"
                    op = "oris"
                end
                
                newList << Instruction.new(node.codeOrigin, op, node.operands[0..-2])
                newList << Instruction.new(node.codeOrigin, branch, [node.operands[-1]])
            when "bmulio"
                tmp1 = Tmp.new(node.codeOrigin, :gpr)
                tmp2 = Tmp.new(node.codeOrigin, :gpr)
                newList << Instruction.new(node.codeOrigin, "smulli", [node.operands[0], node.operands[1], node.operands[1], tmp1])
                newList << Instruction.new(node.codeOrigin, "rshifti", [node.operands[-2], Immediate.new(node.codeOrigin, 31), tmp2])
                newList << Instruction.new(node.codeOrigin, "bineq", [tmp1, tmp2, node.operands[-1]])
            when /^bmuli/
                condition = $~.post_match
                newList << Instruction.new(node.codeOrigin, "muli", node.operands[0..-2])
                newList << Instruction.new(node.codeOrigin, "bti" + condition, [node.operands[-2], node.operands[-1]])
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

#
# Lowering of shift ops. For example:
#
# lshifti foo, bar
#
# will become:
#
# andi foo, 31, tmp
# lshifti tmp, bar
#

def armV7SanitizeShift(operand, list)
    return operand if operand.immediate?
    
    tmp = Tmp.new(operand.codeOrigin, :gpr)
    list << Instruction.new(operand.codeOrigin, "andi", [operand, Immediate.new(operand.codeOrigin, 31), tmp])
    tmp
end

def armV7LowerShiftOps(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "lshifti", "rshifti", "urshifti", "lshiftp", "rshiftp", "urshiftp"
                if node.operands.size == 2
                    newList << Instruction.new(node.codeOrigin, node.opcode, [armV7SanitizeShift(node.operands[0], newList), node.operands[1]])
                else
                    newList << Instruction.new(node.codeOrigin, node.opcode, [node.operands[0], armV7SanitizeShift(node.operands[1], newList), node.operands[2]])
                    raise "Wrong number of operands for shift at #{node.codeOriginString}" unless node.operands.size == 3
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

#
# Lowering of malformed addresses. For example:
#
# loadp 10000[foo], bar
#
# will become:
#
# move 10000, tmp
# addp foo, tmp
# loadp 0[tmp], bar
#

class Node
    def armV7LowerMalformedAddressesRecurse(list)
        mapChildren {
            | node |
            node.armV7LowerMalformedAddressesRecurse(list)
        }
    end
end

class Address
    def armV7LowerMalformedAddressesRecurse(list)
        if offset.value < -0xff or offset.value > 0xfff
            tmp = Tmp.new(codeOrigin, :gpr)
            list << Instruction.new(codeOrigin, "move", [offset, tmp])
            list << Instruction.new(codeOrigin, "addp", [base, tmp])
            Address.new(codeOrigin, tmp, Immediate.new(codeOrigin, 0))
        else
            self
        end
    end
end

class BaseIndex
    def armV7LowerMalformedAddressesRecurse(list)
        if offset.value != 0
            tmp = Tmp.new(codeOrigin, :gpr)
            list << Instruction.new(codeOrigin, "move", [offset, tmp])
            list << Instruction.new(codeOrigin, "addp", [base, tmp])
            BaseIndex.new(codeOrigin, tmp, index, scale, Immediate.new(codeOrigin, 0))
        else
            self
        end
    end
end

class AbsoluteAddress
    def armV7LowerMalformedAddressesRecurse(list)
        tmp = Tmp.new(codeOrigin, :gpr)
        list << Instruction.new(codeOrigin, "move", [address, tmp])
        Address.new(codeOrigin, tmp, Immediate.new(codeOrigin, 0))
    end
end

def armV7LowerMalformedAddresses(list)
    newList = []
    list.each {
        | node |
        newList << node.armV7LowerMalformedAddressesRecurse(newList)
    }
    newList
end

#
# Lowering of malformed addresses in double loads and stores. For example:
#
# loadd [foo, bar, 8], baz
#
# becomes:
#
# leap [foo, bar, 8], tmp
# loadd [tmp], baz
#

class Node
    def armV7DoubleAddress(list)
        self
    end
end

class BaseIndex
    def armV7DoubleAddress(list)
        tmp = Tmp.new(codeOrigin, :gpr)
        list << Instruction.new(codeOrigin, "leap", [self, tmp])
        Address.new(codeOrigin, tmp, Immediate.new(codeOrigin, 0))
    end
end

def armV7LowerMalformedAddressesDouble(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "loadd"
                newList << Instruction.new(node.codeOrigin, "loadd", [node.operands[0].armV7DoubleAddress(newList), node.operands[1]])
            when "stored"
                newList << Instruction.new(node.codeOrigin, "stored", [node.operands[0], node.operands[1].armV7DoubleAddress(newList)])
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

#
# Lowering of misplaced immediates. For example:
#
# storei 0, [foo]
#
# will become:
#
# move 0, tmp
# storei tmp, [foo]
#

def armV7LowerMisplacedImmediates(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "storei", "storep"
                operands = node.operands
                newOperands = []
                operands.each {
                    | operand |
                    if operand.is_a? Immediate
                        tmp = Tmp.new(operand.codeOrigin, :gpr)
                        newList << Instruction.new(operand.codeOrigin, "move", [operand, tmp])
                        newOperands << tmp
                    else
                        newOperands << operand
                    end
                }
                newList << Instruction.new(node.codeOrigin, node.opcode, newOperands)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

#
# Lowering of malformed immediates except when used in a "move" instruction.
# For example:
#
# addp 642641, foo
#
# will become:
#
# move 642641, tmp
# addp tmp, foo
#

class Node
    def armV7LowerMalformedImmediatesRecurse(list)
        mapChildren {
            | node |
            node.armV7LowerMalformedImmediatesRecurse(list)
        }
    end
end

class Address
    def armV7LowerMalformedImmediatesRecurse(list)
        self
    end
end

class BaseIndex
    def armV7LowerMalformedImmediatesRecurse(list)
        self
    end
end

class AbsoluteAddress
    def armV7LowerMalformedImmediatesRecurse(list)
        self
    end
end

class Immediate
    def armV7LowerMalformedImmediatesRecurse(list)
        if value < 0 or value > 255
            tmp = Tmp.new(codeOrigin, :gpr)
            list << Instruction.new(codeOrigin, "move", [self, tmp])
            tmp
        else
            self
        end
    end
end

def armV7LowerMalformedImmediates(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "move"
                newList << node
            when "addi", "addp", "addis", "subi", "subp", "subis"
                if node.operands[0].is_a? Immediate and
                        node.operands[0].value < 0 and
                        node.operands[0].value >= 255 and
                        node.operands.size == 2
                    if node.opcode =~ /add/
                        newOpcode = "sub" + node.opcode[-1..-1]
                    else
                        newOpcode = "add" + node.opcode[-1..-1]
                    end
                    newList << Instruction.new(node.codeOrigin, newOpcode,
                                               [Immediate.new(-node.operands[0].value)] + node.operands[1..-1])
                else
                    newList << node.armV7LowerMalformedImmediatesRecurse(newList)
                end
            when "muli", "mulp"
                if node.operands[0].is_a? Immediate
                    tmp = Tmp.new(codeOrigin, :gpr)
                    newList << Instruction.new(node.codeOrigin, "move", [node.operands[0], tmp])
                    newList << Instruction.new(node.codeOrigin, "muli", [tmp] + node.operands[1..-1])
                else
                    newList << node.armV7LowerMalformedImmediatesRecurse(newList)
                end
            else
                newList << node.armV7LowerMalformedImmediatesRecurse(newList)
            end
        else
            newList << node
        end
    }
    newList
end

#
# Lowering of misplaced addresses. For example:
#
# addi foo, [bar]
#
# will become:
#
# loadi [bar], tmp
# addi foo, tmp
# storei tmp, [bar]
#
# Another example:
#
# addi [foo], bar
#
# will become:
#
# loadi [foo], tmp
# addi tmp, bar
#

def armV7AsRegister(preList, postList, operand, suffix, needStore)
    return operand unless operand.address?
    
    tmp = Tmp.new(operand.codeOrigin, if suffix == "d" then :fpr else :gpr end)
    preList << Instruction.new(operand.codeOrigin, "load" + suffix, [operand, tmp])
    if needStore
        postList << Instruction.new(operand.codeOrigin, "store" + suffix, [tmp, operand])
    end
    tmp
end

def armV7AsRegisters(preList, postList, operands, suffix)
    newOperands = []
    operands.each_with_index {
        | operand, index |
        newOperands << armV7AsRegister(preList, postList, operand, suffix, index == operands.size - 1)
    }
    newOperands
end

def armV7LowerMisplacedAddresses(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            postInstructions = []
            case node.opcode
            when "addi", "addp", "addis", "andi", "andp", "lshifti", "lshiftp", "muli", "mulp", "negi",
                "negp", "noti", "ori", "oris", "orp", "rshifti", "urshifti", "rshiftp", "urshiftp", "subi",
                "subp", "subis", "xori", "xorp", /^bi/, /^bp/, /^bti/, /^btp/, /^ci/, /^cp/, /^ti/
                newList << Instruction.new(node.codeOrigin,
                                           node.opcode,
                                           armV7AsRegisters(newList, postInstructions, node.operands, "i"))
            when "bbeq", "bbneq", "bba", "bbaeq", "bbb", "bbbeq", "btbo", "btbz", "btbnz", "tbz", "tbnz",
                "tbo", "cbeq", "cbneq", "cba", "cbaeq", "cbb", "cbbeq"
                newList << Instruction.new(node.codeOrigin,
                                           node.opcode,
                                           armV7AsRegisters(newList, postInstructions, node.operands, "b"))
            when "bbgt", "bbgteq", "bblt", "bblteq", "btbs", "tbs", "cbgt", "cbgteq", "cblt", "cblteq"
                newList << Instruction.new(node.codeOrigin,
                                           node.opcode,
                                           armV7AsRegisters(newList, postInstructions, node.operands, "bs"))
            when "addd", "divd", "subd", "muld", "sqrtd", /^bd/
                newList << Instruction.new(node.codeOrigin,
                                           node.opcode,
                                           armV7AsRegisters(newList, postInstructions, node.operands, "d"))
            when "jmp", "call"
                newList << Instruction.new(node.codeOrigin,
                                           node.opcode,
                                           [armV7AsRegister(newList, postInstructions, node.operands[0], "p", false)])
            else
                newList << node
            end
            newList += postInstructions
        else
            newList << node
        end
    }
    newList
end

#
# Lowering of register reuse in compare instructions. For example:
#
# cieq t0, t1, t0
#
# will become:
#
# mov tmp, t0
# cieq tmp, t1, t0
#

def armV7LowerRegisterReuse(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "cieq", "cineq", "cia", "ciaeq", "cib", "cibeq", "cigt", "cigteq", "cilt", "cilteq",
                "cpeq", "cpneq", "cpa", "cpaeq", "cpb", "cpbeq", "cpgt", "cpgteq", "cplt", "cplteq",
                "tio", "tis", "tiz", "tinz", "tbo", "tbs", "tbz", "tbnz", "tpo", "tps", "tpz", "tpnz",
                "cbeq", "cbneq", "cba", "cbaeq", "cbb", "cbbeq", "cbgt", "cbgteq", "cblt", "cblteq"
                if node.operands.size == 2
                    if node.operands[0] == node.operands[1]
                        tmp = Tmp.new(node.codeOrigin, :gpr)
                        newList << Instruction.new(node.codeOrigin, "move", [node.operands[0], tmp])
                        newList << Instruction.new(node.codeOrigin, node.opcode, [tmp, node.operands[1]])
                    else
                        newList << node
                    end
                else
                    raise "Wrong number of arguments at #{node.codeOriginString}" unless node.operands.size == 3
                    if node.operands[0] == node.operands[2]
                        tmp = Tmp.new(node.codeOrigin, :gpr)
                        newList << Instruction.new(node.codeOrigin, "move", [node.operands[0], tmp])
                        newList << Instruction.new(node.codeOrigin, node.opcode, [tmp, node.operands[1], node.operands[2]])
                    elsif node.operands[1] == node.operands[2]
                        tmp = Tmp.new(node.codeOrigin, :gpr)
                        newList << Instruction.new(node.codeOrigin, "move", [node.operands[1], tmp])
                        newList << Instruction.new(node.codeOrigin, node.opcode, [node.operands[0], tmp, node.operands[2]])
                    else
                        newList << node
                    end
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
        
        myList = armV7LowerBranchOps(myList)
        myList = armV7LowerShiftOps(myList)
        myList = armV7LowerMalformedAddresses(myList)
        myList = armV7LowerMalformedAddressesDouble(myList)
        myList = armV7LowerMisplacedImmediates(myList)
        myList = armV7LowerMalformedImmediates(myList)
        myList = armV7LowerMisplacedAddresses(myList)
        myList = armV7LowerRegisterReuse(myList)
        myList = assignRegistersToTemporaries(myList, :gpr, ARMv7_EXTRA_GPRS)
        myList = assignRegistersToTemporaries(myList, :fpr, ARMv7_EXTRA_FPRS)
        
        return myList
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
        raise unless operands[1].is_a? RegisterID
        if operands[0].is_a? Immediate
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
    
    if mask.is_a? Immediate and mask.value == -1
        $asm.puts "tst #{value.armV7Operand}, #{value.armV7Operand}"
    elsif mask.is_a? Immediate
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
        $asm.comment codeOriginString
        case opcode
        when "addi", "addp", "addis"
            if opcode == "addis"
                suffix = "s"
            else
                suffix = ""
            end
            if operands.size == 3 and operands[0].is_a? Immediate
                raise unless operands[1].is_a? RegisterID
                raise unless operands[2].is_a? RegisterID
                if operands[0].value == 0 and suffix.empty?
                    unless operands[1] == operands[2]
                        $asm.puts "mov #{operands[2].armV7Operand}, #{operands[1].armV7Operand}"
                    end
                else
                    $asm.puts "adds #{operands[2].armV7Operand}, #{operands[1].armV7Operand}, #{operands[0].armV7Operand}"
                end
            elsif operands.size == 3 and operands[0].is_a? RegisterID
                raise unless operands[1].is_a? RegisterID
                raise unless operands[2].is_a? RegisterID
                $asm.puts "adds #{armV7FlippedOperands(operands)}"
            else
                if operands[0].is_a? Immediate
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
            $asm.puts "bvs #{LabelReference.new(codeOrigin, isUnordered).asmLabel}"
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
        when "move", "sxi2p", "zxi2p"
            if operands[0].is_a? Immediate
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
        when "btio", "btpo", "btbo"
            emitArmV7Test(operands)
            $asm.puts "bvs #{operands[-1].asmLabel}"
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
        when "tio", "tbo", "tpo"
            emitArmV7TestSet(operands, "vs")
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
            raise "Unhandled opcode #{opcode} at #{codeOriginString}"
        end
    end
end

