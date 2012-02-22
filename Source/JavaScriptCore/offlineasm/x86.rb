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

class RegisterID
    def supports8BitOnX86
        case name
        when "t0", "a0", "r0", "t1", "a1", "r1", "t2", "t3"
            true
        when "t4", "cfr"
            false
        else
            raise
        end
    end
    
    def x86Operand(kind)
        case name
        when "t0", "a0", "r0"
            case kind
            when :byte
                "%al"
            when :half
                "%ax"
            when :int
                "%eax"
            else
                raise
            end
        when "t1", "a1", "r1"
            case kind
            when :byte
                "%dl"
            when :half
                "%dx"
            when :int
                "%edx"
            else
                raise
            end
        when "t2"
            case kind
            when :byte
                "%cl"
            when :half
                "%cx"
            when :int
                "%ecx"
            else
                raise
            end
        when "t3"
            case kind
            when :byte
                "%bl"
            when :half
                "%bx"
            when :int
                "%ebx"
            else
                raise
            end
        when "t4"
            case kind
            when :byte
                "%sil"
            when :half
                "%si"
            when :int
                "%esi"
            else
                raise
            end
        when "cfr"
            case kind
            when :byte
                "%dil"
            when :half
                "%di"
            when :int
                "%edi"
            else
                raise
            end
        when "sp"
            case kind
            when :byte
                "%spl"
            when :half
                "%sp"
            when :int
                "%esp"
            else
                raise
            end
        else
            raise "Bad register #{name} for X86 at #{codeOriginString}"
        end
    end
    def x86CallOperand(kind)
        "*#{x86Operand(kind)}"
    end
end

class FPRegisterID
    def x86Operand(kind)
        raise unless kind == :double
        case name
        when "ft0", "fa0", "fr"
            "%xmm0"
        when "ft1", "fa1"
            "%xmm1"
        when "ft2", "fa2"
            "%xmm2"
        when "ft3", "fa3"
            "%xmm3"
        when "ft4"
            "%xmm4"
        when "ft5"
            "%xmm5"
        else
            raise "Bad register #{name} for X86 at #{codeOriginString}"
        end
    end
    def x86CallOperand(kind)
        "*#{x86Operand(kind)}"
    end
end

class Immediate
    def x86Operand(kind)
        "$#{value}"
    end
    def x86CallOperand(kind)
        "#{value}"
    end
end

class Address
    def supports8BitOnX86
        true
    end
    
    def x86Operand(kind)
        "#{offset.value}(#{base.x86Operand(:int)})"
    end
    def x86CallOperand(kind)
        "*#{x86Operand(kind)}"
    end
end

class BaseIndex
    def supports8BitOnX86
        true
    end
    
    def x86Operand(kind)
        "#{offset.value}(#{base.x86Operand(:int)}, #{index.x86Operand(:int)}, #{scale})"
    end

    def x86CallOperand(kind)
        "*#{x86operand(kind)}"
    end
end

class AbsoluteAddress
    def supports8BitOnX86
        true
    end
    
    def x86Operand(kind)
        "#{address.value}"
    end

    def x86CallOperand(kind)
        "*#{address.value}"
    end
end

class LabelReference
    def x86CallOperand(kind)
        asmLabel
    end
end

class LocalLabelReference
    def x86CallOperand(kind)
        asmLabel
    end
end

class Instruction
    def x86Operands(*kinds)
        raise unless kinds.size == operands.size
        result = []
        kinds.size.times {
            | idx |
            result << operands[idx].x86Operand(kinds[idx])
        }
        result.join(", ")
    end

    def x86Suffix(kind)
        case kind
        when :byte
            "b"
        when :half
            "w"
        when :int
            "l"
        when :double
            "sd"
        else
            raise
        end
    end
    
    def handleX86OpWithNumOperands(opcode, kind, numOperands)
        if numOperands == 3
            if operands[0] == operands[2]
                $asm.puts "#{opcode} #{operands[1].x86Operand(kind)}, #{operands[2].x86Operand(kind)}"
            elsif operands[1] == operands[2]
                $asm.puts "#{opcode} #{operands[0].x86Operand(kind)}, #{operands[2].x86Operand(kind)}"
            else
                $asm.puts "mov#{x86Suffix(kind)} #{operands[0].x86Operand(kind)}, #{operands[2].x86Operand(kind)}"
                $asm.puts "#{opcode} #{operands[1].x86Operand(kind)}, #{operands[2].x86Operand(kind)}"
            end
        else
            $asm.puts "#{opcode} #{operands[0].x86Operand(kind)}, #{operands[1].x86Operand(kind)}"
        end
    end
    
    def handleX86Op(opcode, kind)
        handleX86OpWithNumOperands(opcode, kind, operands.size)
    end
    
    def handleX86Shift(opcode, kind)
        if operands[0].is_a? Immediate or operands[0] == RegisterID.forName(nil, "t2")
            $asm.puts "#{opcode} #{operands[0].x86Operand(:byte)}, #{operands[1].x86Operand(kind)}"
        else
            $asm.puts "xchgl #{operands[0].x86Operand(:int)}, %ecx"
            $asm.puts "#{opcode} %cl, #{operands[1].x86Operand(kind)}"
            $asm.puts "xchgl #{operands[0].x86Operand(:int)}, %ecx"
        end
    end
    
    def handleX86DoubleBranch(branchOpcode, mode)
        case mode
        when :normal
            $asm.puts "ucomisd #{operands[1].x86Operand(:double)}, #{operands[0].x86Operand(:double)}"
        when :reverse
            $asm.puts "ucomisd #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        else
            raise mode.inspect
        end
        $asm.puts "#{branchOpcode} #{operands[2].asmLabel}"
    end
    
    def handleX86IntCompare(opcodeSuffix, kind)
        if operands[0].is_a? Immediate and operands[0].value == 0 and operands[1].is_a? RegisterID and (opcodeSuffix == "e" or opcodeSuffix == "ne")
            $asm.puts "test#{x86Suffix(kind)} #{operands[1].x86Operand(kind)}"
        elsif operands[1].is_a? Immediate and operands[1].value == 0 and operands[0].is_a? RegisterID and (opcodeSuffix == "e" or opcodeSuffix == "ne")
            $asm.puts "test#{x86Suffix(kind)} #{operands[0].x86Operand(kind)}"
        else
            $asm.puts "cmp#{x86Suffix(kind)} #{operands[1].x86Operand(kind)}, #{operands[0].x86Operand(kind)}"
        end
    end
    
    def handleX86IntBranch(branchOpcode, kind)
        handleX86IntCompare(branchOpcode[1..-1], kind)
        $asm.puts "#{branchOpcode} #{operands[2].asmLabel}"
    end
    
    def handleX86Set(setOpcode, operand)
        if operand.supports8BitOnX86
            $asm.puts "#{setOpcode} #{operand.x86Operand(:byte)}"
            $asm.puts "movzbl #{operand.x86Operand(:byte)}, #{operand.x86Operand(:int)}"
        else
            $asm.puts "xchgl #{operand.x86Operand(:int)}, %eax"
            $asm.puts "#{setOpcode} %al"
            $asm.puts "movzbl %al, %eax"
            $asm.puts "xchgl #{operand.x86Operand(:int)}, %eax"
        end
    end
    
    def handleX86IntCompareSet(setOpcode, kind)
        handleX86IntCompare(setOpcode[3..-1], kind)
        handleX86Set(setOpcode, operands[2])
    end
    
    def handleX86Test(kind)
        value = operands[0]
        case operands.size
        when 2
            mask = Immediate.new(codeOrigin, -1)
        when 3
            mask = operands[1]
        else
            raise "Expected 2 or 3 operands, but got #{operands.size} at #{codeOriginString}"
        end
        
        if mask.is_a? Immediate and mask.value == -1
            if value.is_a? RegisterID
                $asm.puts "test#{x86Suffix(kind)} #{value.x86Operand(kind)}, #{value.x86Operand(kind)}"
            else
                $asm.puts "cmp#{x86Suffix(kind)} $0, #{value.x86Operand(kind)}"
            end
        else
            $asm.puts "test#{x86Suffix(kind)} #{mask.x86Operand(kind)}, #{value.x86Operand(kind)}"
        end
    end
    
    def handleX86BranchTest(branchOpcode, kind)
        handleX86Test(kind)
        $asm.puts "#{branchOpcode} #{operands.last.asmLabel}"
    end
    
    def handleX86SetTest(setOpcode, kind)
        handleX86Test(kind)
        handleX86Set(setOpcode, operands.last)
    end
    
    def handleX86OpBranch(opcode, branchOpcode, kind)
        handleX86OpWithNumOperands(opcode, kind, operands.size - 1)
        case operands.size
        when 4
            jumpTarget = operands[3]
        when 3
            jumpTarget = operands[2]
        else
            raise self.inspect
        end
        $asm.puts "#{branchOpcode} #{jumpTarget.asmLabel}"
    end
    
    def handleX86SubBranch(branchOpcode, kind)
        if operands.size == 4 and operands[1] == operands[2]
            $asm.puts "negl #{operands[2].x86Operand(:int)}"
            $asm.puts "addl #{operands[0].x86Operand(:int)}, #{operands[2].x86Operand(:int)}"
        else
            handleX86OpWithNumOperands("sub#{x86Suffix(kind)}", kind, operands.size - 1)
        end
        case operands.size
        when 4
            jumpTarget = operands[3]
        when 3
            jumpTarget = operands[2]
        else
            raise self.inspect
        end
        $asm.puts "#{branchOpcode} #{jumpTarget.asmLabel}"
    end
    
    def lowerX86
        $asm.comment codeOriginString
        case opcode
        when "addi", "addp"
            if operands.size == 3 and operands[0].is_a? Immediate
                raise unless operands[1].is_a? RegisterID
                raise unless operands[2].is_a? RegisterID
                if operands[0].value == 0
                    unless operands[1] == operands[2]
                        $asm.puts "movl #{operands[1].x86Operand(:int)}, #{operands[2].x86Operand(:int)}"
                    end
                else
                    $asm.puts "leal #{operands[0].value}(#{operands[1].x86Operand(:int)}), #{operands[2].x86Operand(:int)}"
                end
            elsif operands.size == 3 and operands[0].is_a? RegisterID
                raise unless operands[1].is_a? RegisterID
                raise unless operands[2].is_a? RegisterID
                $asm.puts "leal (#{operands[0].x86Operand(:int)}, #{operands[1].x86Operand(:int)}), #{operands[2].x86Operand(:int)}"
            else
                unless Immediate.new(nil, 0) == operands[0]
                    $asm.puts "addl #{x86Operands(:int, :int)}"
                end
            end
        when "andi", "andp"
            handleX86Op("andl", :int)
        when "lshifti"
            handleX86Shift("sall", :int)
        when "muli"
            if operands.size == 3 and operands[0].is_a? Immediate
                $asm.puts "imull #{x86Operands(:int, :int, :int)}"
            else
                # FIXME: could do some peephole in case the left operand is immediate and it's
                # a power of two.
                handleX86Op("imull", :int)
            end
        when "negi"
            $asm.puts "negl #{x86Operands(:int)}"
        when "noti"
            $asm.puts "notl #{x86Operands(:int)}"
        when "ori", "orp"
            handleX86Op("orl", :int)
        when "rshifti"
            handleX86Shift("sarl", :int)
        when "urshifti"
            handleX86Shift("shrl", :int)
        when "subi", "subp"
            if operands.size == 3 and operands[1] == operands[2]
                $asm.puts "negl #{operands[2].x86Operand(:int)}"
                $asm.puts "addl #{operands[0].x86Operand(:int)}, #{operands[2].x86Operand(:int)}"
            else
                handleX86Op("subl", :int)
            end
        when "xori", "xorp"
            handleX86Op("xorl", :int)
        when "loadi", "storei", "loadp", "storep"
            $asm.puts "movl #{x86Operands(:int, :int)}"
        when "loadb"
            $asm.puts "movzbl #{operands[0].x86Operand(:byte)}, #{operands[1].x86Operand(:int)}"
        when "loadbs"
            $asm.puts "movsbl #{operands[0].x86Operand(:byte)}, #{operands[1].x86Operand(:int)}"
        when "loadh"
            $asm.puts "movzwl #{operands[0].x86Operand(:half)}, #{operands[1].x86Operand(:int)}"
        when "loadhs"
            $asm.puts "movswl #{operands[0].x86Operand(:half)}, #{operands[1].x86Operand(:int)}"
        when "storeb"
            $asm.puts "movb #{x86Operands(:byte, :byte)}"
        when "loadd", "moved", "stored"
            $asm.puts "movsd #{x86Operands(:double, :double)}"
        when "addd"
            $asm.puts "addsd #{x86Operands(:double, :double)}"
        when "divd"
            $asm.puts "divsd #{x86Operands(:double, :double)}"
        when "subd"
            $asm.puts "subsd #{x86Operands(:double, :double)}"
        when "muld"
            $asm.puts "mulsd #{x86Operands(:double, :double)}"
        when "sqrtd"
            $asm.puts "sqrtsd #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        when "ci2d"
            $asm.puts "cvtsi2sd #{operands[0].x86Operand(:int)}, #{operands[1].x86Operand(:double)}"
        when "bdeq"
            isUnordered = LocalLabel.unique("bdeq")
            $asm.puts "ucomisd #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
            $asm.puts "jp #{LabelReference.new(codeOrigin, isUnordered).asmLabel}"
            $asm.puts "je #{LabelReference.new(codeOrigin, operands[2]).asmLabel}"
            isUnordered.lower("X86")
        when "bdneq"
            handleX86DoubleBranch("jne", :normal)
        when "bdgt"
            handleX86DoubleBranch("ja", :normal)
        when "bdgteq"
            handleX86DoubleBranch("jae", :normal)
        when "bdlt"
            handleX86DoubleBranch("ja", :reverse)
        when "bdlteq"
            handleX86DoubleBranch("jae", :reverse)
        when "bdequn"
            handleX86DoubleBranch("je", :normal)
        when "bdnequn"
            isUnordered = LocalLabel.unique("bdnequn")
            isEqual = LocalLabel.unique("bdnequn")
            $asm.puts "ucomisd #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
            $asm.puts "jp #{LabelReference.new(codeOrigin, isUnordered).asmLabel}"
            $asm.puts "je #{LabelReference.new(codeOrigin, isEqual).asmLabel}"
            isUnordered.lower("X86")
            $asm.puts "jmp #{operands[2].asmLabel}"
            isEqual.lower("X86")
        when "bdgtun"
            handleX86DoubleBranch("jb", :reverse)
        when "bdgtequn"
            handleX86DoubleBranch("jbe", :reverse)
        when "bdltun"
            handleX86DoubleBranch("jb", :normal)
        when "bdltequn"
            handleX86DoubleBranch("jbe", :normal)
        when "btd2i"
            $asm.puts "cvttsd2si #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:int)}"
            $asm.puts "cmpl $0x80000000 #{operands[1].x86Operand(:int)}"
            $asm.puts "je #{operands[2].asmLabel}"
        when "td2i"
            $asm.puts "cvttsd2si #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:int)}"
        when "bcd2i"
            $asm.puts "cvttsd2si #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:int)}"
            $asm.puts "testl #{operands[1].x86Operand(:int)}, #{operands[1].x86Operand(:int)}"
            $asm.puts "je #{operands[2].asmLabel}"
            $asm.puts "cvtsi2sd #{operands[1].x86Operand(:int)}, %xmm7"
            $asm.puts "ucomisd #{operands[0].x86Operand(:double)}, %xmm7"
            $asm.puts "jp #{operands[2].asmLabel}"
            $asm.puts "jne #{operands[2].asmLabel}"
        when "movdz"
            $asm.puts "xorpd #{operands[0].x86Operand(:double)}, #{operands[0].x86Operand(:double)}"
        when "pop"
            $asm.puts "pop #{operands[0].x86Operand(:int)}"
        when "push"
            $asm.puts "push #{operands[0].x86Operand(:int)}"
        when "move", "sxi2p", "zxi2p"
            if Immediate.new(nil, 0) == operands[0] and operands[1].is_a? RegisterID
                $asm.puts "xorl #{operands[1].x86Operand(:int)}, #{operands[1].x86Operand(:int)}"
            elsif operands[0] != operands[1]
                $asm.puts "movl #{x86Operands(:int, :int)}"
            end
        when "nop"
            $asm.puts "nop"
        when "bieq", "bpeq"
            handleX86IntBranch("je", :int)
        when "bineq", "bpneq"
            handleX86IntBranch("jne", :int)
        when "bia", "bpa"
            handleX86IntBranch("ja", :int)
        when "biaeq", "bpaeq"
            handleX86IntBranch("jae", :int)
        when "bib", "bpb"
            handleX86IntBranch("jb", :int)
        when "bibeq", "bpbeq"
            handleX86IntBranch("jbe", :int)
        when "bigt", "bpgt"
            handleX86IntBranch("jg", :int)
        when "bigteq", "bpgteq"
            handleX86IntBranch("jge", :int)
        when "bilt", "bplt"
            handleX86IntBranch("jl", :int)
        when "bilteq", "bplteq"
            handleX86IntBranch("jle", :int)
        when "bbeq"
            handleX86IntBranch("je", :byte)
        when "bbneq"
            handleX86IntBranch("jne", :byte)
        when "bba"
            handleX86IntBranch("ja", :byte)
        when "bbaeq"
            handleX86IntBranch("jae", :byte)
        when "bbb"
            handleX86IntBranch("jb", :byte)
        when "bbbeq"
            handleX86IntBranch("jbe", :byte)
        when "bbgt"
            handleX86IntBranch("jg", :byte)
        when "bbgteq"
            handleX86IntBranch("jge", :byte)
        when "bblt"
            handleX86IntBranch("jl", :byte)
        when "bblteq"
            handleX86IntBranch("jlteq", :byte)
        when "btio", "btpo"
            handleX86BranchTest("jo", :int)
        when "btis", "btps"
            handleX86BranchTest("js", :int)
        when "btiz", "btpz"
            handleX86BranchTest("jz", :int)
        when "btinz", "btpnz"
            handleX86BranchTest("jnz", :int)
        when "btbo"
            handleX86BranchTest("jo", :byte)
        when "btbs"
            handleX86BranchTest("js", :byte)
        when "btbz"
            handleX86BranchTest("jz", :byte)
        when "btbnz"
            handleX86BranchTest("jnz", :byte)
        when "jmp"
            $asm.puts "jmp #{operands[0].x86CallOperand(:int)}"
        when "baddio", "baddpo"
            handleX86OpBranch("addl", "jo", :int)
        when "baddis", "baddps"
            handleX86OpBranch("addl", "js", :int)
        when "baddiz", "baddpz"
            handleX86OpBranch("addl", "jz", :int)
        when "baddinz", "baddpnz"
            handleX86OpBranch("addl", "jnz", :int)
        when "bsubio"
            handleX86SubBranch("jo", :int)
        when "bsubis"
            handleX86SubBranch("js", :int)
        when "bsubiz"
            handleX86SubBranch("jz", :int)
        when "bsubinz"
            handleX86SubBranch("jnz", :int)
        when "bmulio"
            handleX86OpBranch("imull", "jo", :int)
        when "bmulis"
            handleX86OpBranch("imull", "js", :int)
        when "bmuliz"
            handleX86OpBranch("imull", "jz", :int)
        when "bmulinz"
            handleX86OpBranch("imull", "jnz", :int)
        when "borio"
            handleX86OpBranch("orl", "jo", :int)
        when "boris"
            handleX86OpBranch("orl", "js", :int)
        when "boriz"
            handleX86OpBranch("orl", "jz", :int)
        when "borinz"
            handleX86OpBranch("orl", "jnz", :int)
        when "break"
            $asm.puts "int $3"
        when "call"
            $asm.puts "call #{operands[0].x86CallOperand(:int)}"
        when "ret"
            $asm.puts "ret"
        when "cieq", "cpeq"
            handleX86IntCompareSet("sete", :int)
        when "cineq", "cpneq"
            handleX86IntCompareSet("setne", :int)
        when "cia", "cpa"
            handleX86IntCompareSet("seta", :int)
        when "ciaeq", "cpaeq"
            handleX86IntCompareSet("setae", :int)
        when "cib", "cpb"
            handleX86IntCompareSet("setb", :int)
        when "cibeq", "cpbeq"
            handleX86IntCompareSet("setbe", :int)
        when "cigt", "cpgt"
            handleX86IntCompareSet("setg", :int)
        when "cigteq", "cpgteq"
            handleX86IntCompareSet("setge", :int)
        when "cilt", "cplt"
            handleX86IntCompareSet("setl", :int)
        when "cilteq", "cplteq"
            handleX86IntCompareSet("setle", :int)
        when "tio"
            handleX86SetTest("seto", :int)
        when "tis"
            handleX86SetTest("sets", :int)
        when "tiz"
            handleX86SetTest("setz", :int)
        when "tinz"
            handleX86SetTest("setnz", :int)
        when "tbo"
            handleX86SetTest("seto", :byte)
        when "tbs"
            handleX86SetTest("sets", :byte)
        when "tbz"
            handleX86SetTest("setz", :byte)
        when "tbnz"
            handleX86SetTest("setnz", :byte)
        when "peek"
            $asm.puts "movl #{operands[0].value * 4}(%esp), #{operands[1].x86Operand(:int)}"
        when "poke"
            $asm.puts "movl #{operands[0].x86Operand(:int)}, #{operands[1].value * 4}(%esp)"
        when "cdqi"
            $asm.puts "cdq"
        when "idivi"
            $asm.puts "idivl #{operands[0].x86Operand(:int)}"
        when "fii2d"
            $asm.puts "movd #{operands[0].x86Operand(:int)}, #{operands[2].x86Operand(:double)}"
            $asm.puts "movd #{operands[1].x86Operand(:int)}, %xmm7"
            $asm.puts "psllq $32, %xmm7"
            $asm.puts "por %xmm7, #{operands[2].x86Operand(:double)}"
        when "fd2ii"
            $asm.puts "movd #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:int)}"
            $asm.puts "movsd #{operands[0].x86Operand(:double)}, %xmm7"
            $asm.puts "psrlq $32, %xmm7"
            $asm.puts "movsd %xmm7, #{operands[2].x86Operand(:int)}"
        when "bo"
            $asm.puts "jo #{operands[0].asmLabel}"
        when "bs"
            $asm.puts "js #{operands[0].asmLabel}"
        when "bz"
            $asm.puts "jz #{operands[0].asmLabel}"
        when "bnz"
            $asm.puts "jnz #{operands[0].asmLabel}"
        when "leai", "leap"
            $asm.puts "leal #{operands[0].x86Operand(:int)}, #{operands[1].x86Operand(:int)}"
        else
            raise "Bad opcode: #{opcode}"
        end
    end
end

