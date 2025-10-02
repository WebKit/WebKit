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

require "config"
require "ast"

#
# node.resolveSettings(settings)
#
# Construct a new AST that does not have any IfThenElse nodes by
# substituting concrete boolean values for each Setting.
#

class Node
    def resolveSettings(settings)
        mapChildren {
            | child |
            child.resolveSettings(settings)
        }
    end
end

class True
    def resolveSettings(settings)
        self
    end
end

class False
    def resolveSettings(settings)
        self
    end
end

class Setting
    def resolveSettings(settings)
        settings[@name].asNode
    end
end

class And
    def resolveSettings(settings)
        (@left.resolveSettings(settings).value and @right.resolveSettings(settings).value).asNode
    end
end

class Or
    def resolveSettings(settings)
        (@left.resolveSettings(settings).value or @right.resolveSettings(settings).value).asNode
    end
end

class Not
    def resolveSettings(settings)
        (not @child.resolveSettings(settings).value).asNode
    end
end

class IfThenElse
    def resolveSettings(settings)
        if @predicate.resolveSettings(settings).value
            @thenCase.resolveSettings(settings)
        else
            @elseCase.resolveSettings(settings)
        end
    end
end

class Sequence
    def resolveSettings(settings)
        newList = []
        @list.each {
            | item |
            item = item.resolveSettings(settings)
            if item.is_a? Sequence
                newList += item.list
            else
                newList << item
            end
        }
        Sequence.new(codeOrigin, newList)
    end
end

#
# node.demacroify(macros)
# node.substitute(mapping)
#
# demacroify() constructs a new AST that does not have any Macro
# nodes, while substitute() replaces Variable nodes with the given
# nodes in the mapping.
#

class Node
    def demacroify(macros)
        mapChildren {
            | child |
            child.demacroify(macros)
        }
    end

    def freshVariables(mapping)
        mapChildren {
            | child |
            child.freshVariables(mapping)
        }
    end

    def substitute(mapping)
        mapChildren {
            | child |
            child.substitute(mapping)
        }
    end
    
    def substituteLabels(mapping)
        mapChildren {
            | child |
            child.substituteLabels(mapping)
        }
    end
end

$uniqueMacroVarID = 0
class Macro
    def freshVariables(mapping = {})
        myMapping = mapping.dup
        newVars = []
        variables.each do |var|
            $uniqueMacroVarID += 1
            newVar = Variable.forName(var.codeOrigin, "_var#{$uniqueMacroVarID}", var.originalName)
            newVars << newVar
            myMapping[var] = newVar
        end
        Macro.new(codeOrigin, name, newVars, body.freshVariables(myMapping))
    end

    def substitute(mapping)
        myMapping = {}
        mapping.each_pair {
            | key, value |
            unless @variables.include? key
                myMapping[key] = value
            end
        }
        mapChildren {
            | child |
            child.substitute(myMapping)
        }
    end
end

class MacroCall
    def freshVariables(mapping)
        newName = Variable.forName(codeOrigin, name, originalName)
        if mapping[newName]
            newName = mapping[newName]
        end
        newOperands = operands.map { |operand| operand.freshVariables(mapping) }
        MacroCall.new(codeOrigin, newName.name, newOperands, annotation, originalName)
    end
end

$concatenation = /%([a-zA-Z0-9_]+)%/
class Variable
    def freshVariables(mapping)
        if @name =~ $concatenation
            name = @name.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    "%#{mapping[var].name}%"
                else
                    match
                end
            }
            Variable.forName(codeOrigin, name)
        elsif mapping[self]
            mapping[self]
        else
            self
        end
    end

    def substitute(mapping)
        if @name =~ $concatenation
            name = @name.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    mapping[var].name
                else
                    match
                end
            }
            Variable.forName(codeOrigin, name)
        elsif mapping[self]
            mapping[self]
        else
            self
        end
    end
end

class StructOffset
    def freshVariables(mapping)
        if dump =~ $concatenation
            names = dump.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    "%#{mapping[var].name}%"
                else
                    match
                end
            }.split('::')
            StructOffset.forField(codeOrigin, names[0..-2].join('::'), names[-1])
        else
            self
        end
    end

    def substitute(mapping)
        if dump =~ $concatenation
            names = dump.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    mapping[var].name
                else
                    match
                end
            }.split('::')
            StructOffset.forField(codeOrigin, names[0..-2].join('::'), names[-1])
        else
            self
        end
    end
end

class Label
    def freshVariables(mapping)
        if @name =~ $concatenation or @alignTo.is_a? Variable
            name = @name.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    "%#{mapping[var].name}%"
                else
                    match
                end
            }
            result = Label.forName(codeOrigin, name, @definedInFile)
            result.setGlobal() if global?
            result.setUnalignedGlobal() unless aligned?
            result.setAligned(@alignTo.freshVariables(mapping)) if aligned? and @alignTo
            result.clearExtern unless extern?
            result
        else
            self
        end
    end

    def substitute(mapping)
        if @name =~ $concatenation or @alignTo.is_a? Variable
            name = @name.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    mapping[var].name
                else
                    match
                end
            }
            result = Label.forName(codeOrigin, name, @definedInFile)
            result.setGlobal() if global?
            result.setUnalignedGlobal() unless aligned?
            result.setAligned(@alignTo.substitute(mapping)) if aligned? and @alignTo
            result.clearExtern unless extern?
            result
        else
            self
        end
    end
end

class ConstExpr
    def freshVariables(mapping)
        if @value =~ $concatenation
            value = @value.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    "%#{mapping[var].name}%"
                else
                    match
                end
            }
            ConstExpr.forName(codeOrigin, value)
        else
            self
        end
    end

    def substitute(mapping)
        if @value =~ $concatenation
            value = @value.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    mapping[var].name
                else
                    match
                end
            }
            ConstExpr.forName(codeOrigin, value)
        else
            self
        end
    end
end

class Sizeof
    def freshVariables(mapping)
        if struct =~ $concatenation
            value = struct.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    "%#{mapping[var].name}%"
                else
                    match
                end
            }
            Sizeof.forName(codeOrigin, value)
        else
            self
        end
    end

    def substitute(mapping)
        if struct =~ $concatenation
            value = struct.gsub($concatenation) { |match|
                var = Variable.forName(codeOrigin, match[1...-1])
                if mapping[var]
                    mapping[var].name
                else
                    match
                end
            }
            Sizeof.forName(codeOrigin, value)
        else
            self
        end
    end
end

class LocalLabel
    def substituteLabels(mapping)
        if mapping[self]
            mapping[self]
        else
            self
        end
    end
end

class MacroError < StandardError
end

class Sequence
    def substitute(constants)
        newList = []
        myConstants = constants.dup
        @list.each {
            | item |
            if item.is_a? ConstDecl
                myConstants[item.variable] = item.value.substitute(myConstants)
            else
                newList << item.substitute(myConstants)
            end
        }
        Sequence.new(codeOrigin, newList)
    end
    
    def renameLabels(comment)
        mapping = {}
        
        @list.each {
            | item |
            if item.is_a? LocalLabel
                mapping[item] = LocalLabel.unique(codeOrigin, if comment then comment + "_" else "" end + item.cleanName)
            end
        }
        
        substituteLabels(mapping)
    end
    
    @@demacroifyStack = []
    def macroError(msg)
        backtrace = @@demacroifyStack.reverse.map { |macroCall|
            "#{macroCall.codeOrigin} in call to #{macroCall.originalName}"
        }
        raise MacroError, msg, backtrace
    end

    def demacroify(macros)
        myMacros = macros.dup
        # We do an initial pass looking for all macros in order to allow forward references
        @list.each {
            | item |
            if item.is_a? Macro
                myMacros[item.name] = item.freshVariables
            end
        }
        newList = []
        @list.each {
            | item |
            if item.is_a? Macro
                # Ignore. We already looked for macros above and they should not be part of the final output
            elsif item.is_a? MacroCall
                @@demacroifyStack << item
                mapping = {}
                myMyMacros = myMacros.dup
                macro = myMacros[item.name]
                macroError "Could not find macro #{item.originalName}" unless macro
                macroError "Argument count mismatch for call to #{item.originalName} (expected #{macro.variables.size} but got #{item.operands.size} arguments for macro #{item.originalName} defined at #{macro.codeOrigin})" unless item.operands.size == macro.variables.size
                item.operands.size.times {
                    | idx |
                    if item.operands[idx].is_a? Variable and myMacros[item.operands[idx].name]
                        myMyMacros[macro.variables[idx].name] = myMacros[item.operands[idx].name]
                        mapping[macro.variables[idx]] = nil
                    elsif item.operands[idx].is_a? Macro
                        myMyMacros[macro.variables[idx].name] = item.operands[idx].freshVariables
                        mapping[macro.variables[idx]] = nil
                    else
                        myMyMacros[macro.variables[idx]] = nil
                        mapping[macro.variables[idx]] = item.operands[idx]
                    end
                }
                if item.annotation
                    newList << Instruction.new(item.codeOrigin, "localAnnotation", [], item.annotation)
                end
                newList += macro.body.substitute(mapping).demacroify(myMyMacros).renameLabels(item.originalName).list

                @@demacroifyStack.pop
            else
                newList << item.demacroify(myMacros)
            end
        }
        Sequence.new(codeOrigin, newList).substitute({})
    end
end

#
# node.resolveOffsets(offsets, sizes)
#
# Construct a new AST that has offset values instead of symbolic
# offsets.
#

class Node
    def resolveOffsets(constantsMap)
        mapChildren {
            | child |
            child.resolveOffsets(constantsMap)
        }
    end
end

class StructOffset
    def resolveOffsets(constantsMap)
        if constantsMap[self]
            Immediate.new(codeOrigin, constantsMap[self])
        else
            puts "Could not find #{self.inspect} in #{constantsMap.keys.inspect}"
            puts "sizes = #{constantsMap.inspect}"
            raise
        end
    end
end

class Sizeof
    def resolveOffsets(constantsMap)
        if constantsMap[self]
            Immediate.new(codeOrigin, constantsMap[self])
        else
            puts "Could not find #{self.inspect} in #{constantsMap.keys.inspect}"
            puts "sizes = #{constantsMap.inspect}"
            raise
        end
    end
end

class ConstExpr
    def resolveOffsets(constantsMap)
        if constantsMap[self]
            Immediate.new(codeOrigin, constantsMap[self])
        else
            puts "Could not find #{self.inspect} in #{constantsMap.keys.inspect}"
            puts "sizes = #{constantsMap.inspect}"
            raise
        end
    end
end

#
# node.fold
#
# Resolve constant references and compute arithmetic expressions.
#

class Node
    def fold
        mapChildren {
            | child |
            child.fold
        }
    end
end

class AddImmediates
    def fold
        @left = @left.fold
        @right = @right.fold
        
        return right.plusOffset(@left.value) if @left.is_a? Immediate and @right.is_a? LabelReference
        return left.plusOffset(@right.value) if @left.is_a? LabelReference and @right.is_a? Immediate
        
        return self unless @left.is_a? Immediate
        return self unless @right.is_a? Immediate
        Immediate.new(codeOrigin, @left.value + @right.value)
    end
end

class SubImmediates
    def fold
        @left = @left.fold
        @right = @right.fold
        
        return left.plusOffset(-@right.value) if @left.is_a? LabelReference and @right.is_a? Immediate
        
        return self unless @left.is_a? Immediate
        return self unless @right.is_a? Immediate
        Immediate.new(codeOrigin, @left.value - @right.value)
    end
end

class MulImmediates
    def fold
        @left = @left.fold
        @right = @right.fold
        return self unless @left.is_a? Immediate
        return self unless @right.is_a? Immediate
        Immediate.new(codeOrigin, @left.value * @right.value)
    end
end

class NegImmediate
    def fold
        @child = @child.fold
        return self unless @child.is_a? Immediate
        Immediate.new(codeOrigin, -@child.value)
    end
end

class OrImmediates
    def fold
        @left = @left.fold
        @right = @right.fold
        return self unless @left.is_a? Immediate
        return self unless @right.is_a? Immediate
        Immediate.new(codeOrigin, @left.value | @right.value)
    end
end

class AndImmediates
    def fold
        @left = @left.fold
        @right = @right.fold
        return self unless @left.is_a? Immediate
        return self unless @right.is_a? Immediate
        Immediate.new(codeOrigin, @left.value & @right.value)
    end
end

class XorImmediates
    def fold
        @left = @left.fold
        @right = @right.fold
        return self unless @left.is_a? Immediate
        return self unless @right.is_a? Immediate
        Immediate.new(codeOrigin, @left.value ^ @right.value)
    end
end

class BitnotImmediate
    def fold
        @child = @child.fold
        return self unless @child.is_a? Immediate
        Immediate.new(codeOrigin, ~@child.value)
    end
end

#
# node.resolveAfterSettings(offsets, sizes)
#
# Compile assembly against a set of offsets.
#

class Node
    def resolve(constantsMap)
        demacroify({}).resolveOffsets(constantsMap).fold
    end
end

#
# node.validate
#
# Checks that the node is ready for backend compilation.
#

class Node
    def validate
        raise "Unresolved '#{dump}' at #{codeOriginString}"
    end
    
    def validateChildren
        children.each {
            | node |
            node.validate
        }
    end
end

class Sequence
    def validate
        validateChildren
        
        # Further verify that this list contains only instructions, labels, and skips.
        @list.each {
            | node |
            unless node.is_a? Instruction or
                    node.is_a? Label or
                    node.is_a? LocalLabel or
                    node.is_a? Skip
                raise "Unexpected #{node.inspect} at #{node.codeOrigin}" 
            end
        }
    end
end

class Immediate
    def validate
    end
end

class StringLiteral
    def validate
    end
end

class RegisterPair
    def validate
    end
end

class RegisterID
    def validate
    end
end

class FPRegisterID
    def validate
    end
end

class VecRegisterID
    def validate
    end
end

class Address
    def validate
        validateChildren
    end
end

class BaseIndex
    def validate
        validateChildren
    end
end

class AbsoluteAddress
    def validate
        validateChildren
    end
end

class Instruction
    def validate
        validateChildren
    end
end

class SubImmediates
    def validate
      raise "Invalid operand #{left.dump} to immediate subtraction" unless left.immediateOperand?
      raise "Invalid operand #{right.dump} to immediate subtraction" unless right.immediateOperand?
    end
end

class Error
    def validate
    end
end

class Label
    def validate
        raise "Unresolved substitution in Label #{name} at #{codeOrigin}" if name =~ /%/
    end
end

class LocalLabel
    def validate
    end
end

class LabelReference
    def validate
    end
end

class LocalLabelReference
    def validate
    end
end

class Skip
    def validate
    end
end

GPR_TMP0 = 'x15' # todo: lr
GPR_TMP1 = 'x14' # todo: csr2

def jsrClobbers(register)
    if JSR_MAPPING.include?(register.to_sym)
        [
            JSR_MAPPING[register.to_sym][:gpr] == :spill ? [GPR_TMP0, GPR_TMP1] : JSR_MAPPING[register.to_sym][:gpr],
            JSR_MAPPING[register.to_sym][:fpr],
        ].flatten
    else
        raise "Unknown jsr: #{register}"
    end
end

# Lower JSR spills/fills, and add debug clobbering on 64-bit platforms
# to ensure that code remains portable
def lowerJSRUse(instruction)
    origin = instruction.codeOrigin
    ptrSize =  $currentSettings["ADDRESS64"] ? :quad : :word
    operandSizes = instruction.operandSizes.map { |s| if s == :ptr then ptrSize else s end }
    operandDefs = instruction.operandDefs
    usedJSRs = instruction.children.filter.with_index { |o, i| (operandSizes[i] == :quad or operandDefs[i]) && isJSR?(o.dump) }
    usedJSRs += instruction.children.map { |a| a.children.filter { |o| o.is_a?(RegisterID) } }.flatten.filter { |r| isJSR?(r.dump) }
    extraClobbers = usedJSRs.map { |o| jsrClobbers(o.dump) }.flatten

    return instruction if extraClobbers.empty?

    if instruction.opcode == 'move'
        result = []
        src = JSR_MAPPING[instruction.children[0].dump.to_sym]
        dst = JSR_MAPPING[instruction.children[1].dump.to_sym]
        return instruction if src.nil? or dst.nil?
        result << Instruction.new(origin, "moved", [
            FPRegisterID.new(origin, src[:fpr]),
            FPRegisterID.new(origin, dst[:fpr])])
        if src[:gpr] != :spill and dst[:grp] != :spill
            result << instruction
            return result
        end
        if src[:gpr] == :spill and dst[:grp] == :spill
            return result
        end
        # Let's just do it the long way then.
    end

    if instruction.children.any? { |o| extraClobbers.include?(o.dump.to_sym) }
        raise "Operand list contains a JSR and an implementation-clobbered register: #{instruction.dump} at #{origin}"
    end

    operandMappings = { }
    availableRegisters = ['t0', 't1', 't2', 't3', 't4', 't5', 't6', 't7'] - extraClobbers
    
    usedJSRs.filter { |operand| JSR_MAPPING[operand.dump.to_sym][:gpr] == :spill }.each do |operand|
        mapping = JSR_MAPPING[operand.dump.to_sym]
        operandMappings[operand.dump.to_sym] = { gpr: availableRegisters.pop, gpr2: availableRegisters.pop, fpr: mapping[:fpr] }
    end

    if instruction.opcode.start_with?('b') or instruction.opcode.start_with?('j') or instruction.opcode.start_with?('call')
        raise if operandDefs.any?
        $stderr.puts "(ARMv7-compat) Cannot spill jsr registers for branch instruction at #{origin}" if operandMappings.size > 1
    end

    # Avoid needing to fill again after
    if operandMappings.size == 1 and not operandDefs.any?
        operand = operandMappings.keys.first
        operandMappings[operand][:gpr] = GPR_TMP0
        operandMappings[operand][:gpr2] = GPR_TMP1
    end

    usedJSRs.filter { |operand| JSR_MAPPING[operand.dump.to_sym][:gpr] != :spill }.each do |operand|
        mapping = JSR_MAPPING[operand.dump.to_sym]
        operandMappings[operand.dump.to_sym] = { gpr: mapping[:gpr], gpr2: nil, fpr: mapping[:fpr] }
    end

    result = []

    instruction.children.each.with_index { |child, i|
        next if !child.is_a?(RegisterID) || !isJSR?(child.dump.to_sym)
        if operandSizes[i] != :quad and operandDefs[i]
            gpr, gpr2, fpr = operandMappings[child.dump.to_sym].values_at(:gpr, :gpr2, :fpr)
            if not gpr2.nil?
                result << Instruction.new(origin, "move", [
                    Immediate.new(origin, 0),
                    RegisterID.new(origin, gpr2)])
            else
                result << Instruction.new(origin, "moved", [
                    Immediate.new(origin, 0),
                    FPRegisterID.new(origin, fpr)])
            end
        end
    }
    
    swap = lambda do
        for spill in operandMappings.values
            gpr, gpr2, fpr = spill.values_at(:gpr, :gpr2, :fpr)
            next if gpr2.nil?
            result << Instruction.new(origin, "fd2ii", [
                FPRegisterID.new(origin, fpr),
                RegisterID.new(origin, GPR_TMP0),
                RegisterID.new(origin, GPR_TMP1)])
            next if gpr == GPR_TMP0 && gpr2 == GPR_TMP1
            raise if [gpr, gpr2].any? { | o | [GPR_TMP0, GPR_TMP1].include? o }
            result << Instruction.new(origin, "fii2d", [
                FPRegisterID.new(origin, fpr),
                RegisterID.new(origin, gpr),
                RegisterID.new(origin, gpr2)])
            result << Instruction.new(origin, "move", [
                RegisterID.new(origin, GPR_TMP0),
                RegisterID.new(origin, gpr)])
            result << Instruction.new(origin, "move", [
                RegisterID.new(origin, GPR_TMP1),
                RegisterID.new(origin, gpr2)])
        end
    end

    pairForReg = lambda do |child|
        next child if !child.is_a?(RegisterID) || !isJSR?(child.dump.to_sym)
        gpr, gpr2, fpr = operandMappings[child.dump.to_sym].values_at(:gpr, :gpr2, :fpr)
        next RegisterPair.new(RegisterID.new(origin, gpr), FPRegisterID.new(origin, fpr)) if gpr2.nil?
        RegisterPair.new(RegisterID.new(origin, gpr), RegisterID.new(origin, gpr2))
    end

    swap[]
    result << instruction.to_enum(:mapChildren).with_index do |child, index| 
        child = child.mapChildren(&pairForReg)
        if operandSizes[index] == :quad
            pairForReg[child]
        else
            child
        end
    end
    swap[]

    result
end

class Instruction
    def operandSizes
        x86_only_opcodes = [
          "andf", "andd",
          "orf", "ord",
          "noti",
          "cdqi",
          "cqoq",
          "idivi", "udivi",
          "idivq", "udivq",
          "btd2i",
          "bcd2i",
          "movdz",
          "atomicxchgsubb", "atomicxchgsubh", "atomicxchgsubi", "atomicxchgsubq",
          "baddio", "baddpo", "baddqo",
          "baddis", "baddps", "baddqs",
          "baddiz", "baddpz", "baddqz",
          "baddinz", "baddpnz", "baddqnz",
          "bsubio", "bsubis", "bsubiz", "bsubinz",
          "bmulio", "bmulis", "bmuliz", "bmulinz",
          "borio", "boris", "boriz", "borinz",
          "btis", "btps", "btqs",
          "btiz", "btpz", "btqz",
          "btinz", "btpnz", "btqnz",
          "btbs", "btbz", "btbnz",
          "tis", "tiz", "tinz",
          "tps", "tpz", "tpnz",
          "tqs", "tqz", "tqnz",
          "tbs", "tbz", "tbnz",
          "popcnti", "popcntq",
          "transferi", "transferp", "transferq",
          "batomicweakcasb", "batomicweakcash", "batomicweakcasi", "batomicweakcasq",
          "bfgteq", "bflteq"
        ]
        armv7_only_opcodes = [
          "adci",
          "sbci", 
          "noti",
          "load2ia",
          "store2ia",
          "transferq",
          "loadlinkb",
          "loadlinkh", 
          "loadlinki",
          "loadlink2i",
          "storecondb",
          "storecondh", 
          "storecondi",
          "storecond2i",
          "andf",
          "orf",
          "andd",
          "ord",
          "moveii",
          "mvlbl",
          "btd2i",
          "movdz",
          "bcs",
          "smulli",
          "umulli", 
          "writefence",
          "clrbp",
        ]
        if x86_only_opcodes.include?(opcode) or armv7_only_opcodes.include?(opcode)
            return Array.new(operands.length, :word)
        end

        case opcode
        when 'emit', 'localAnnotation', 'globalAnnotation'
            []
        when "tagCodePtr","tagReturnAddress","untagReturnAddress","removeCodePtrTag","untagArrayPtr""removeArrayPtrTag", "btis", "btiz", "btinz", "btps", "btpz", "btpnz", "btqs", "btqz", "btqnz", "btbs", "btbz", "btbnz", "tis", "tiz", "tinz", "tps", "tpz", "tpnz", "tqs", "tqz", "tqnz", "tbs", "tbz", "tbnz", "fii2d", "fd2ii", "storepairv", "loadpairv"
            Array.new(operands.length, :word)
        when 'addi'
            Array.new(operands.length, :word)
        when 'addis'
            Array.new(operands.length, :word)
        when 'addp', 'addlshiftp'
            Array.new(operands.length, :ptr)
        when 'addps'
            Array.new(operands.length, :ptr)
        when 'addq'
            Array.new(operands.length, :quad)
        when "andi"
            Array.new(operands.length, :word)
        when "andp"
            Array.new(operands.length, :ptr)
        when "andq"
            Array.new(operands.length, :quad)
        when "ori"
            Array.new(operands.length, :word)
        when "orp"
            Array.new(operands.length, :ptr)
        when "orq"
            Array.new(operands.length, :quad)
        when "orh"
            Array.new(operands.length, :word)
        when "xori"
            Array.new(operands.length, :word)
        when "xorp"
            Array.new(operands.length, :ptr)
        when "xorq"
            Array.new(operands.length, :quad)
        when 'divi'
            Array.new(operands.length, :word)
        when 'divis'
            Array.new(operands.length, :word)
        when 'divq'
            Array.new(operands.length, :quad)
        when 'divqs'
            Array.new(operands.length, :quad)
        when "lshifti"
            Array.new(operands.length, :word)
        when "lshiftp"
            Array.new(operands.length, :ptr)
        when "lshiftq"
            Array.new(operands.length, :quad)
        when "rshifti"
            Array.new(operands.length, :word)
        when "rshiftp"
            Array.new(operands.length, :ptr)
        when "rshiftq"
            Array.new(operands.length, :quad)
        when "urshifti"
            Array.new(operands.length, :word)
        when "urshiftp"
            Array.new(operands.length, :ptr)
        when "urshiftq"
            Array.new(operands.length, :quad)
        when "muli"
            Array.new(operands.length, :word)
        when "mulp"
            Array.new(operands.length, :ptr)
        when "mulq"
            Array.new(operands.length, :quad)
        when "subi"
            Array.new(operands.length, :word)
        when "subp"
            Array.new(operands.length, :ptr)
        when "subq"
            Array.new(operands.length, :quad)
        when "subis"
            Array.new(operands.length, :word)
        when "negi"
            [:word]
        when "negp"
            [:ptr]
        when "negq"
            [:quad]
        when "notq"
            [:quad]
        when "loadi"
            [:word, :word]
        when "loadis"
            [:quad, :quad]
        when "loadp"
            [:ptr, :ptr]
        when "loadq"
            [:quad, :quad]
        when "loadqinc"
            [:quad, :quad, :quad]
        when "storei"
            [:word, :word]
        when "storep"
            [:ptr, :ptr]
        when "storeq"
            [:quad, :quad]
        when "loadb"
            [:word, :word]
        when "loadbsi"
            [:word, :word]
        when "loadbsq"
            [:quad, :word]
        when "storeb"
            [:word, :word]
        when "transferi"
            [:word, :word]
        when "transferq"
            [:quad, :quad]
        when "transferp"
            [:ptr, :ptr]
        when "loadh"
            [:word, :word]
        when "loadhsi"
            [:word, :word]
        when "loadhsq"
            [:quad, :word]
        when "storeh"
            [:word, :word]
        when "loadd"
            [:double, :double]
        when "stored"
            [:double, :double]
        when "loadv"
            [:vector_with_interpretation, :vector_with_interpretation]
        when "storev"
            [:vector_with_interpretation, :vector_with_interpretation]
        when "addd"
            Array.new(operands.length, :double)
        when "divd"
            Array.new(operands.length, :double)
        when "subd"
            Array.new(operands.length, :double)
        when "muld"
            Array.new(operands.length, :double)
        when "sqrtd"
            Array.new(operands.length, :double)
        when "bdeq"
            [:double, :double, nil]
        when "bdneq"
            [:double, :double, nil]
        when "bdgt"
            [:double, :double, nil]
        when "bdgteq"
            [:double, :double, nil]
        when "bdlt"
            [:double, :double, nil]
        when "bdlteq"
            [:double, :double, nil]
        when "bdequn"
            [:double, :double, nil]
        when "bdnequn"
            [:double, :double, nil]
        when "bdgtun"
            [:double, :double, nil]
        when "bdgtequn"
            [:double, :double, nil]
        when "bdltun"
            [:double, :double, nil]
        when "bdltequn"
            [:double, :double, nil]
        when "btd2i"
            raise "ARM64 does not support this opcode yet, #{codeOriginString}"
        when "td2i"
            [:double, :word]
        when "bcd2i"
            raise "ARM64 does not support this opcode yet, #{codeOriginString}"
        when "movdz"
            raise "ARM64 does not support this opcode yet, #{codeOriginString}"
        when "pop"
            Array.new(operands.length, :quad)
        when "popv"
            Array.new(operands.length, :vector_with_interpretation)
        when "push"
            Array.new(operands.length, :quad)
        when "pushv"
            Array.new(operands.length, :vector_with_interpretation)
        when "move"
            [:quad, :quad]
        when "moved"
            [:double, :double]
        when "sxi2p"
            [:word, :ptr]
        when "sxi2q"
            [:word, :quad]
        when "zxi2p"
            [:word, :ptr]
        when "zxi2q"
            [:word, :quad]
        when "sxb2i"
            [:word, :word]
        when "sxh2i"
            [:word, :word]
        when "sxb2q", "sxb2p"
            [:word, :quad]
        when "sxh2q"
            [:word, :quad]
        when "nop"
            []
        when "bieq", "bbeq"
            [:word, :word, nil]
        when "bpeq"
            [:ptr, :ptr, nil]
        when "bqeq"
            [:quad, :quad, nil]
        when "bineq", "bbneq"
            [:word, :word, nil]
        when "bpneq"
            [:ptr, :ptr, nil]
        when "bqneq"
            [:quad, :quad, nil]
        when "bia", "bba"
            [:word, :word, nil]
        when "bpa"
            [:ptr, :ptr, nil]
        when "bqa"
            [:quad, :quad, nil]
        when "biaeq", "bbaeq"
            [:word, :word, nil]
        when "bpaeq"
            [:ptr, :ptr, nil]
        when "bqaeq"
            [:quad, :quad, nil]
        when "bib", "bbb"
            [:word, :word, nil]
        when "bpb"
            [:ptr, :ptr, nil]
        when "bqb"
            [:quad, :quad, nil]
        when "bibeq", "bbbeq"
            [:word, :word, nil]
        when "bpbeq"
            [:ptr, :ptr, nil]
        when "bqbeq"
            [:quad, :quad, nil]
        when "bigt", "bbgt"
            [:word, :word, nil]
        when "bpgt"
            [:ptr, :ptr, nil]
        when "bqgt"
            [:quad, :quad, nil]
        when "bigteq", "bbgteq"
            [:word, :word, nil]
        when "bpgteq"
            [:ptr, :ptr, nil]
        when "bqgteq"
            [:quad, :quad, nil]
        when "bilt", "bblt"
            [:word, :word, nil]
        when "bplt"
            [:ptr, :ptr, nil]
        when "bqlt"
            [:quad, :quad, nil]
        when "bilteq", "bblteq"
            [:word, :word, nil]
        when "bplteq"
            [:ptr, :ptr, nil]
        when "bqlteq"
            [:quad, :quad, nil]
        when "jmp"
            [nil]
        when "call"
            [nil]
        when "break"
            []
        when "ret"
            []
        when "cieq", "cbeq"
            [:word, :word, :word]
        when "cpeq"
            [:ptr, :ptr, :ptr]
        when "cqeq"
            [:quad, :quad, :quad]
        when "cineq", "cbneq"
            [:word, :word, :word]
        when "cpneq"
            [:ptr, :ptr, :ptr]
        when "cqneq"
            [:quad, :quad, :quad]
        when "cia", "cba"
            [:word, :word, :word]
        when "cpa"
            [:ptr, :ptr, :ptr]
        when "cqa"
            [:quad, :quad, :quad]
        when "ciaeq", "cbaeq"
            [:word, :word, :word]
        when "cpaeq"
            [:ptr, :ptr, :ptr]
        when "cqaeq"
            [:quad, :quad, :quad]
        when "cib", "cbb"
            [:word, :word, :word]
        when "cpb"
            [:ptr, :ptr, :ptr]
        when "cqb"
            [:quad, :quad, :quad]
        when "cibeq", "cbbeq"
            [:word, :word, :word]
        when "cpbeq"
            [:ptr, :ptr, :ptr]
        when "cqbeq"
            [:quad, :quad, :quad]
        when "cilt", "cblt"
            [:word, :word, :word]
        when "cplt"
            [:ptr, :ptr, :ptr]
        when "cqlt"
            [:quad, :quad, :quad]
        when "cilteq", "cblteq"
            [:word, :word, :word]
        when "cplteq"
            [:ptr, :ptr, :ptr]
        when "cqlteq"
            [:quad, :quad, :quad]
        when "cigt", "cbgt"
            [:word, :word, :word]
        when "cpgt"
            [:ptr, :ptr, :ptr]
        when "cqgt"
            [:quad, :quad, :quad]
        when "cigteq", "cbgteq"
            [:word, :word, :word]
        when "cpgteq"
            [:ptr, :ptr, :ptr]
        when "cqgteq"
            [:quad, :quad, :quad]
        when "peek"
            [nil, :quad]
        when "poke"
            [nil, :quad]
        when "fp2d"
            [:ptr, :double]
        when "fq2d"
            [:quad, :double]
        when "fd2p"
            [:double, :ptr]
        when "fd2q"
            [:double, :quad]
        when "bo"
            [nil]
        when "bs"
            [nil]
        when "bz"
            [nil]
        when "bnz"
            [nil]
        when "leai"
            [:word, :word]
        when "leap"
            [:ptr, :ptr]
        when "leaq"
            [:quad, :quad]
        when "smulli"
            [:word, :word, :quad]
        when "memfence"
            []
        when "fence"
            []
        when "bfiq"
            [:quad, nil, nil, :quad]
        when "pcrtoaddr"
            [nil, :quad]
        when "globaladdr"
            [nil, :quad]

        when "andf", "andd"
            Array.new(operands.length, :double)
        when "orf", "ord"
            Array.new(operands.length, :double)
        when "lrotatei"
            [:word, :word]
        when "lrotateq"
            [:quad, :quad]
        when "rrotatei"
            Array.new(operands.length, :word)
        when "rrotateq"
            Array.new(operands.length, :quad)
        when "loadf"
            [:float, :float]
        when "storef"
            [:float, :float]
        when "addf"
            Array.new(operands.length, :float)
        when "divf"
            Array.new(operands.length, :float)
        when "subf"
            Array.new(operands.length, :float)
        when "mulf"
            Array.new(operands.length, :float)
        when "sqrtf"
            Array.new(operands.length, :float)
        when "floorf"
            Array.new(operands.length, :float)
        when "floord"
            Array.new(operands.length, :double)
        when "roundf"
            Array.new(operands.length, :float)
        when "roundd"
            Array.new(operands.length, :double)
        when "truncatef"
            Array.new(operands.length, :float)
        when "truncated"
            Array.new(operands.length, :double)
        when "truncatef2i"
            [:float, :word]
        when "truncatef2q"
            [:float, :quad]
        when "truncated2q"
            [:double, :quad]
        when "truncated2i"
            [:double, :word]
        when "truncatef2is"
            [:float, :word]
        when "truncated2is"
            [:double, :word]
        when "truncatef2qs"
            [:float, :quad]
        when "truncated2qs"
            [:double, :quad]
        when "ci2d"
            [:word, :double]
        when "ci2ds"
            [:word, :double]
        when "ci2f"
            [:word, :float]
        when "ci2fs"
            [:word, :float]
        when "cq2f"
            [:quad, :float]
        when "cq2fs"
            [:quad, :float]
        when "cq2d"
            [:quad, :double]
        when "cq2ds"
            [:quad, :double]
        when "cd2f"
            [:double, :float]
        when "cf2d"
            [:float, :double]
        when "bfeq"
            [:float, :float, nil]
        when "bfgt"
            [:float, :float, nil]
        when "bflt"
            [:float, :float, nil]
        when "bfgtun"
            [:float, :float, nil]
        when "bfgtequn"
            [:float, :float, nil]
        when "bfltun"
            [:float, :float, nil]
        when "bfltequn"
            [:float, :float, nil]
        when "tzcnti"
            [:word, :word]
        when "tzcntq"
            [:quad, :quad]
        when "lzcnti"
            Array.new(operands.length, :word)
        when "lzcntq"
            Array.new(operands.length, :quad)
        when "absf"
            Array.new(operands.length, :float)
        when "absd"
            Array.new(operands.length, :double)
        when "negf"
            Array.new(operands.length, :float)
        when "negd"
            Array.new(operands.length, :double)
        when "ceilf"
            Array.new(operands.length, :float)
        when "ceild"
            Array.new(operands.length, :double)
        when "cfeq"
            [:float, :float, :word]
        when "cdeq"
            [:double, :double, :word]
        when "cfneq"
            [:float, :float, :word]
        when "cdneq"
            [:double, :double, :word]
        when "cfnequn"
            [:float, :float, :word]
        when "cdnequn"
            [:double, :double, :word]
        when "cflt"
            [:float, :float, :word]
        when "cdlt"
            [:double, :double, :word]
        when "cflteq"
            [:float, :float, :word]
        when "cdlteq"
            [:double, :double, :word]
        when "cfgt"
            [:float, :float, :word]
        when "cdgt"
            [:double, :double, :word]
        when "cfgteq"
            [:float, :float, :word]
        when "cdgteq"
            [:double, :double, :word]
        when "fi2f"
            [:word, :float]
        when "ff2i"
            [:float, :word]
        when "tls_loadp"
            [nil, :ptr]
        when "tls_storep"
            [:ptr, nil]
        when "loadlinkacqb"
            [:word, :word]
        when "loadlinkacqh"
            [:word, :word]
        when "loadlinkacqi"
            [:word, :word]
        when "loadlinkacqq"
            [:quad, :quad]
        when "storecondrelb"
            [:word, :word, :word]
        when "storecondrelh"
            [:word, :word, :word]
        when "storecondreli"
            [:word, :word, :word]
        when "storecondrelq"
            [:word, :word, :word]
        when "atomicxchgaddb"
            [:word, :word, :word]
        when "atomicxchgaddh"
            [:word, :word, :word]
        when "atomicxchgaddi"
            [:word, :word, :word]
        when "atomicxchgaddq"
            [:quad, :quad, :quad]
        when "atomicxchgclearb"
            [:word, :word, :word]
        when "atomicxchgclearh"
            [:word, :word, :word]
        when "atomicxchgcleari"
            [:word, :word, :word]
        when "atomicxchgclearq"
            [:quad, :quad, :quad]
        when "atomicxchgorb"
            [:word, :word, :word]
        when "atomicxchgorh"
            [:word, :word, :word]
        when "atomicxchgori"
            [:word, :word, :word]
        when "atomicxchgorq"
            [:quad, :quad, :quad]
        when "atomicxchgxorb"
            [:word, :word, :word]
        when "atomicxchgxorh"
            [:word, :word, :word]
        when "atomicxchgxori"
            [:word, :word, :word]
        when "atomicxchgxorq"
            [:quad, :quad, :quad]
        when "atomicxchgb"
            [:word, :word, :word]
        when "atomicxchgh"
            [:word, :word, :word]
        when "atomicxchgi"
            [:word, :word, :word]
        when "atomicxchgq"
            [:quad, :quad, :quad]
        when "atomicweakcasb"
            [:word, :word, :word]
        when "atomicweakcash"
            [:word, :word, :word]
        when "atomicweakcasi"
            [:word, :word, :word]
        when "atomicweakcasq"
            [:quad, :quad, :quad]
        when "atomicloadb"
            [:word, :word]
        when "atomicloadh"
            [:word, :word]
        when "atomicloadi"
            [:word, :word]
        when "atomicloadq"
            [:quad, :quad]
        when "loadpairq", "loadpairp"
            [:quad, :quad, :quad]
        when "loadpairi"
            [:quad, :word, :word]
        when "storepairq", "storepairp"
            [:quad, :quad, :quad]
        when "storepairi"
            [:word, :word, :quad]
        when "loadpaird"
            [:double, :double, :double]
        when "storepaird"
            [:double, :double, :double]
        when "umovb"
            [:word, :vector, nil]
        when "umovh"
            [:word, :vector, nil]
        when "umovi"
            [:word, :vector, nil]
        when "umovq"
            [:quad, :vector, nil]
        else
            raise "Unknown instruction: #{dump}"
        end
    end

    def operandDefs
        x86_only_opcodes = [
          "jccb", "jcci",
          "callf", "jmpf",
          "andnotd", "andnotf",
          "cmovz", "cmovnz", "cmovb", "cmovbe", "cmovl", "cmovle", "cmovs", "cmovns",
          "cmova", "cmovae", "cmovg", "cmovge",
          "baddio", "baddis", "baddi",
          "baddiz", "baddpz", "baddqz",
          "baddinz", "baddpnz", "baddqnz",
          "bsubio", "bsubis", "bsubiz", "bsubinz",
          "bmulio", "bmulis", "bmuliz", "bmulinz",
          "borio", "boris", "boriz", "borinz",
          "btis", "btps", "btqs",
          "btiz", "btpz", "btqz",
          "btinz", "btpnz", "btqnz",
          "btbs", "btbz", "btbnz",
          "tis", "tiz", "tinz",
          "tps", "tpz", "tpnz",
          "tqs", "tqz", "tqnz",
          "tbs", "tbz", "tbnz",
          "popcnti", "popcntq",
          "transferi", "transferp", "transferq",
          "batomicweakcasb", "batomicweakcash", "batomicweakcasi", "batomicweakcasq",
          "bfgteq", "bflteq"
        ]
        if x86_only_opcodes.include?(opcode)
            return Array.new(operands.length, false)
        end

        case opcode
        when 'emit', 'localAnnotation', 'globalAnnotation'
            []
        when "tagCodePtr", "tagReturnAddress", "untagReturnAddress", "removeCodePtrTag", "untagArrayPtr", "removeArrayPtrTag", "fii2d", "fd2ii", "storepairv", "loadpairv"
            # Most of these are loads/moves - typically [false, true] pattern but using generic pattern
            if operands.length <= 1
                Array.new(operands.length, true)  # single operand usually modified
            else
                # Multi-operand: usually src, dst pattern
                result = Array.new(operands.length, false)
                result[-1] = true if operands.length > 0  # last operand is usually destination
                result
            end
        when 'addi', 'addis', 'addp', 'addps', 'addq', 'addlshiftp'
            if operands.length == 2
                [false, true]  # src, dst (in-place)
            else
                [false, false, true]  # src1, src2, dst
            end
        when "andi", "andp", "andq", "ori", "orp", "orq", "orh", "xori", "xorp", "xorq"
            if operands.length == 2
                [false, true]  # src, dst (in-place)
            else
                [false, false, true]  # src1, src2, dst
            end
        when 'divi', 'divis', 'divq', 'divqs'
            if operands.length == 2
                [false, true]  # src, dst (in-place)
            else
                [false, false, true]  # src1, src2, dst
            end
        when "lshifti", "lshiftp", "lshiftq", "rshifti", "rshiftp", "rshiftq", "urshifti", "urshiftp", "urshiftq"
            if operands.length == 2
                [false, true]  # src, dst (in-place)
            else
                [false, false, true]  # src1, src2, dst
            end
        when "muli", "mulp", "mulq"
            if operands.length == 2
                [false, true]  # src, dst (in-place)
            else
                [false, false, true]  # src1, src2, dst
            end
        when "subi", "subp", "subq", "subis"
            if operands.length == 2
                [false, true]  # src, dst (in-place)
            else
                [false, false, true]  # src1, src2, dst
            end
        when "negi", "negp", "negq"
            [true]  # dst modified in-place
        when "notq"
            [true]  # dst modified in-place
        when "loadi", "loadis", "loadp", "loadq", "loadb", "loadbsi", "loadbsq", "loadh", "loadhsi", "loadhsq", "loadd", "loadv", "loadf"
            [false, true]  # addr, dst
        when "loadqinc"
            [false, true, false]  # addr, dst, offset
        when "storei", "storep", "storeq", "storeb", "storeh", "stored", "storev", "storef"
            [false, false]  # src, addr
        when "transferi", "transferq", "transferp"
            [false, false]  # src, dst (memory-to-memory, operands not modified)
        when "addd", "divd", "subd", "muld", "sqrtd", "addf", "divf", "subf", "mulf", "sqrtf", "floorf", "floord", "roundf", "roundd", "truncatef", "truncated"
            if operands.length == 2
                [false, true]  # src, dst (in-place)
            else
                [false, false, true]  # src1, src2, dst
            end
        when "bdeq", "bdneq", "bdgt", "bdgteq", "bdlt", "bdlteq", "bdequn", "bdnequn", "bdgtun", "bdgtequn", "bdltun", "bdltequn"
            [false, false, false]  # src1, src2, label
        when "btd2i", "bcd2i", "movdz"
            raise "ARM64 does not support this opcode yet, #{codeOriginString}"
        when "td2i", "truncatef2i", "truncatef2q", "truncated2q", "truncated2i", "truncatef2is", "truncated2is", "truncatef2qs", "truncated2qs"
            [false, true]  # src, dst
        when "pop"
            Array.new(operands.length, true)  # all destinations
        when "popv"
            Array.new(operands.length, true)  # all destinations
        when "push", "pushv"
            Array.new(operands.length, false)  # all sources
        when /^b.*/
            Array.new(operands.length, false)  # all sources
        when "move", "moved"
            [false, true]  # src, dst
        when "sxi2p", "sxi2q", "zxi2p", "zxi2q", "sxb2i", "sxh2i", "sxb2q", "sxb2p", "sxh2q"
            [false, true]  # src, dst
        when "nop", "break", "ret", "memfence", "fence"
            []  # no operands modified
        when "jmp", "call"
            [false]  # target
        when "cieq", "cbeq", "cpeq", "cqeq", "cineq", "cbneq", "cpneq", "cqneq"
            [false, false, true]  # src1, src2, dst
        when "cia", "cba", "cpa", "cqa", "ciaeq", "cbaeq", "cpaeq", "cqaeq", "cib", "cbb", "cpb", "cqb", "cibeq", "cbbeq", "cpbeq", "cqbeq"
            [false, false, true]  # src1, src2, dst
        when "cilt", "cblt", "cplt", "cqlt", "cilteq", "cblteq", "cplteq", "cqlteq", "cigt", "cbgt", "cpgt", "cqgt", "cigteq", "cbgteq", "cpgteq", "cqgteq"
            [false, false, true]  # src1, src2, dst
        when "peek"
            [false, true]  # offset, dst
        when "poke"
            [false, false]  # offset, src
        when "fp2d", "fq2d", "fd2p", "fd2q", "fi2f", "ff2i"
            [false, true]  # src, dst
        when "bo", "bs", "bz", "bnz"
            [false]  # label
        when "leai", "leap", "leaq"
            [false, true]  # addr, dst
        when "smulli"
            if operands.length == 3
                [false, false, true]  # src1, src2, dst (ARM64 variant)
            else
                [false, false, true, true]  # src1, src2, dst_low, dst_high (ARM v7 variant)
            end
        when "bfiq"
            [false, false, false, true]  # src, pos, len, dst
        when "pcrtoaddr", "globaladdr"
            [false, true]  # src, dst
        when "andf", "andd", "orf", "ord"
            if operands.length == 2
                [false, true]  # src, dst (in-place)
            else
                [false, false, true]  # src1, src2, dst
            end
        when "lrotatei", "lrotateq", "rrotatei", "rrotateq"
            if operands.length == 2
                [false, true]  # src, dst (in-place)
            else
                [false, false, true]  # src1, src2, dst
            end
        when "ci2d", "ci2ds", "ci2f", "ci2fs", "cq2f", "cq2fs", "cq2d", "cq2ds", "cd2f", "cf2d"
            [false, true]  # src, dst
        when "bfeq", "bfgt", "bflt", "bfgtun", "bfgtequn", "bfltun", "bfltequn"
            [false, false, false]  # src1, src2, label
        when "tzcnti", "tzcntq", "lzcnti", "lzcntq", "absf", "absd", "negf", "negd", "ceilf", "ceild"
            [false, true]  # src, dst
        when "cfeq", "cdeq", "cfneq", "cdneq", "cfnequn", "cdnequn", "cflt", "cdlt", "cflteq", "cdlteq", "cfgt", "cdgt", "cfgteq", "cdgteq"
            [false, false, true]  # src1, src2, dst
        when "tls_loadp"
            [false, true]  # offset, dst
        when "tls_storep"
            [false, false]  # src, offset
        when "loadlinkacqb", "loadlinkacqh", "loadlinkacqi", "loadlinkacqq"
            [false, true]  # addr, dst
        when "storecondrelb", "storecondrelh", "storecondreli", "storecondrelq"
            [true, false, false]  # result, src, addr
        when "atomicxchgaddb", "atomicxchgaddh", "atomicxchgaddi", "atomicxchgaddq"
            [false, false, true]  # val, addr, old_val
        when "atomicxchgclearb", "atomicxchgclearh", "atomicxchgcleari", "atomicxchgclearq"
            [false, false, true]  # val, addr, old_val
        when "atomicxchgorb", "atomicxchgorh", "atomicxchgori", "atomicxchgorq"
            [false, false, true]  # val, addr, old_val
        when "atomicxchgxorb", "atomicxchgxorh", "atomicxchgxori", "atomicxchgxorq"
            [false, false, true]  # val, addr, old_val
        when "atomicxchgb", "atomicxchgh", "atomicxchgi", "atomicxchgq"
            [false, false, true]  # val, addr, old_val
        when "atomicweakcasb", "atomicweakcash", "atomicweakcasi", "atomicweakcasq"
            [true, false, false]  # expected (modified), new_val, addr
        when "atomicloadb", "atomicloadh", "atomicloadi", "atomicloadq"
            [false, true]  # addr, dst
        when "loadpairq", "loadpairp", "loadpairi", "loadpaird"
            [false, true, true]  # addr, dst1, dst2
        when "storepairq", "storepairp", "storepairi", "storepaird"
            [false, false, false]  # src1, src2, addr
        when "umovb", "umovh", "umovi", "umovq"
            [true, false, false]  # dst, src, index
        # ARMv7-specific instructions
        when "adci"
            if operands.length == 2
                [false, true]  # src, dst (in-place with carry)
            else
                [false, false, true]  # src1, src2, dst (with carry)
            end
        when "sbci"
            if operands.length == 2
                [false, true]  # src, dst (in-place subtract with carry)
            else
                [false, false, true]  # src1, src2, dst (subtract with carry)
            end
        when "noti"
            [true]  # dst modified in-place (bitwise NOT)
        when "load2ia"
            [false, true, true]  # addr, dst1, dst2 (load two 32-bit values)
        when "store2ia"
            [false, false, false]  # src1, src2, addr (store two 32-bit values)
        when "loadlinkb", "loadlinkh", "loadlinki"
            [false, true]  # addr, dst (load-link operations)
        when "loadlink2i"
            [false, true, true]  # addr, dst1, dst2 (load-link two words)
        when "storecondb", "storecondh", "storecondi"
            [true, false, false]  # result, src, addr (store-conditional, returns success)
        when "storecond2i"
            [true, false, false, false]  # result, src1, src2, addr (store-conditional two words)
        when "andf", "orf"
            if operands.length == 2
                [false, true]  # src, dst (in-place float bitwise op)
            else
                [false, false, true]  # src1, src2, dst (float bitwise op)
            end
        when "andd", "ord"
            if operands.length == 2
                [false, true]  # src, dst (in-place double bitwise op)
            else
                [false, false, true]  # src1, src2, dst (double bitwise op)
            end
        when "moveii"
            [false, true, true]  # src, dst1, dst2 (move immediate to two registers)
        when "mvlbl"
            [false, true]  # label, dst (move label address)
        when "btd2i"
            [false, false, true]  # src, branch_target, dst (branch and convert)
        when "movdz"
            [false, true]  # src, dst (move double with zero extension)
        when "bcs"
            [false]  # label (branch on carry set)
        when "umulli"
            [false, false, true, true]  # src1, src2, dst_low, dst_high (64-bit multiply result)
        when "writefence"
            []  # no operands (memory fence)
        when "clrbp"
            []  # no operands (clear breakpoint)
        else
            raise "Unknown instruction: #{dump}"
        end
    end
end

# Lower JSR registers into their gpr backing register.

class Node
    def lowerJSRs()
        mapChildren { | node | node.lowerJSRs }
    end
end

class Sequence
    def lowerJSRs()
        newInstrs = []
        children.each {
            | node |
            unless node.is_a? Instruction then
                newInstrs << node.lowerJSRs
                next
            end

            newInstrs << lowerJSRUse(node)

        }
        Sequence.new(codeOrigin, newInstrs.flatten)
    end
end
