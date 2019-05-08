# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

class ARM64E
    # FIXME: This is fragile and needs to match the enum value in PtrTag.h.
    CFunctionPtrTag = 2
end

class Sequence
    def getModifiedListARM64E
        result = riscLowerMisplacedAddresses(@list)
        getModifiedListARM64(result)
    end
end

class Instruction
    def self.lowerMisplacedAddressesARM64E(node, newList)
        wasHandled = false
        if node.is_a? Instruction
            postInstructions = []
            annotation = node.annotation
            codeOrigin = node.codeOrigin
            case node.opcode
            when "jmp", "call"
                if node.operands.size > 1
                    if node.operands[1].is_a? RegisterID
                        tag = riscAsRegister(newList, postInstructions, node.operands[1], "p", false)
                    else
                        tag = Tmp.new(codeOrigin, :gpr)
                        newList << Instruction.new(codeOrigin, "move", [node.operands[1], tag], annotation)
                    end
                    operands = [riscAsRegister(newList, postInstructions, node.operands[0], "p", false), tag]
                    newList << Instruction.new(codeOrigin, node.opcode, operands, annotation)
                    wasHandled = true
                end
            when "untagArrayPtr"
                newOperands = node.operands.map {
                    | operand |
                    if operand.address?
                        tmp = Tmp.new(codeOrigin, :gpr)
                        newList << Instruction.new(codeOrigin, "loadp", [operand, tmp], annotation)
                        tmp
                    else
                        operand
                    end
                }
                newList << Instruction.new(codeOrigin, node.opcode, newOperands, annotation)
                wasHandled = true
            end
            newList += postInstructions if wasHandled
        end
        return wasHandled, newList
    end

    def lowerARM64E
        case opcode
        when "call"
            if operands.size == 1 or operands[0].label?
                lowerARM64
            elsif operands[1] == ARM64E::CFunctionPtrTag
                emitARM64Unflipped("blraaz", [operands[0]], :ptr)
            else
                emitARM64Unflipped("blrab", operands, :ptr)
            end
        when "jmp"
            if operands[0].label?
                lowerARM64
            else
                emitARM64Unflipped("brab", operands, :ptr)
            end
        when "tagReturnAddress"
            raise if operands.size < 1 or not operands[0].is_a? RegisterID
            if operands[0].is_a? RegisterID and operands[0].name == "sp"
                $asm.puts "pacibsp"
            else
                emitARM64Unflipped("pacib lr,", operands, :ptr)
            end
        when "untagReturnAddress"
            raise if operands.size < 1 or not operands[0].is_a? RegisterID
            if operands[0].is_a? RegisterID and operands[0].name == "sp"
                $asm.puts "autibsp"
            else
                emitARM64Unflipped("autib lr,", operands, :ptr)
            end
        when "removeCodePtrTag"
            raise unless operands[0].is_a? RegisterID
            emitARM64Unflipped("xpaci ", operands, :ptr)
        when "untagArrayPtr"
            raise if operands.size != 2 or not operands.each { |operand| operand.is_a? RegisterID or operand.is_a? Tmp }
            emitARM64("autdb ", operands, :ptr)
        when "ret"
            $asm.puts "retab"
        else
            lowerARM64
        end
    end
end
