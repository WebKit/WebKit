#!/usr/bin/env ruby

# Copyright (C) 2015 Apple Inc. All rights reserved.
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

require "pathname"

$varIndex = 0
def newVar
    $varIndex += 1
    "_tmp#{$varIndex}"
end

class Production
    attr_reader :origin, :name, :varName, :anonymous, :opcode, :children

    def initialize(origin, name, opcode, children)
        @origin = origin
        @name = name
        @anonymous = (name == nil or name =~ /\A\$([0-9]+)\Z/)
        @varName = @anonymous ? newVar : name =~ /\A\$/ ? $' : name
        @opcode = opcode
        @children = children ? children : []
    end

    def hasOpcode
        opcode != nil
    end

    def eachVariable(&proc)
        children.each_with_index {
            | child, index |
            yield varName,
                  child.name,
                  child.varName,
                  child.anonymous ? (child.opcode ? :anonInternal : :anonOperand) : (child.opcode ? :internal : :operand),
                  index
            child.eachVariable(&proc)
        }
    end
end

class Matcher
    attr_reader :name, :productions

    def initialize(name, productions)
        @name = name
        @productions = productions
    end
end

class Origin
    attr_reader :fileName, :lineNumber
    
    def initialize(fileName, lineNumber)
        @fileName = fileName
        @lineNumber = lineNumber
    end
    
    def to_s
        "#{fileName}:#{lineNumber}"
    end
end

class Token
    attr_reader :origin, :string
    
    def initialize(origin, string)
        @origin = origin
        @string = string
    end
    
    def ==(other)
        if other.is_a? Token
            @string == other.string
        else
            @string == other
        end
    end
    
    def =~(other)
        @string =~ other
    end
    
    def to_s
        "#{@string.inspect} at #{origin}"
    end
    
    def parseError(*comment)
        if comment.empty?
            raise "Parse error: #{to_s}"
        else
            raise "Parse error: #{to_s}: #{comment[0]}"
        end
    end
end

def lex(str, fileName)
    fileName = Pathname.new(fileName)
    result = []
    lineNumber = 1
    while not str.empty?
        case str
        when /\A\#([^\n]*)/
            # comment, ignore
        when /\A\n/
            # newline, ignore
            lineNumber += 1
        when /\A[a-zA-Z0-9$]([a-zA-Z0-9_]*)/
            result << Token.new(Origin.new(fileName, lineNumber), $&)
        when /\A([ \t\r]+)/
            # whitespace, ignore
        when /\A[=(),]/
            result << Token.new(Origin.new(fileName, lineNumber), $&)
        else
            raise "Lexer error at #{Origin.new(fileName, lineNumber).to_s}, unexpected sequence #{str[0..20].inspect}"
        end
        str = $~.post_match
    end
    result
end

def isKeyword(token)
    # We can extend this as we add more keywords. Like, we might want "include" eventually.
    token =~ /\A((matcher)|(include))\Z/
end

def isIdentifier(token)
    token =~ /\A[a-zA-Z0-9$]([a-zA-Z0-9_]*)\Z/ and not isKeyword(token)
end

class Parser
    def initialize(data, fileName)
        @tokens = lex(data, fileName)
        @idx = 0
    end

    def token
        @tokens[@idx]
    end

    def advance
        @idx += 1
    end

    def parseError(*comment)
        if token
            token.parseError(*comment)
        else
            if comment.empty?
                raise "Parse error at end of file"
            else
                raise "Parse error at end of file: #{comment[0]}"
            end
        end
    end

    def consume(string)
        parseError("Expected #{string}") unless token == string
        advance
    end

    def consumeIdentifier
        result = token.string
        parseError("Expected identifier") unless isIdentifier(result)
        advance
        result
    end

    def addUnique(name, productions, production)
        if name == production.varName
            parseError("Child production has same name as parent production")
        end
        if productions.detect { | entry | entry.name == production.name }
            parseError("Duplicate production")
        end
        productions << production
    end

    def parse
        consume("matcher")

        matcherName = consumeIdentifier

        productions = []

        loop {
            break if @idx >= @tokens.length

            # We might want to support "include". If we did that, it would go here.
            production = parseProduction
            
            if productions.detect { | entry | entry.name == production.name }
                parseError("Duplicate production")
            end

            productions << production
        }

        Matcher.new(matcherName, productions)
    end

    def parseProduction
        origin = token.origin
        name = consumeIdentifier

        if token == "="
            consume("=")
            opcode = consumeIdentifier
            consume("(")
        elsif token == "("
            consume("(")
            opcode = name
            name = nil
        else
            return Production.new(origin, name, nil, nil)
        end
        
        productions = []
        loop {
            break if token == ")"

            addUnique(name, productions, parseProduction)

            break if token == ")"
            consume(",")
        }

        # Advance after the ")"
        advance

        Production.new(origin, name, opcode, productions)
    end
end

fileName = ARGV[0]

parser = Parser.new(IO::read(fileName), fileName)
matcher = parser.parse

class SubMatch
    attr_reader :indexMap, :productions
    
    def initialize(indexList, productions)
        @indexMap = []
        @productions = []
        indexList.each {
            | index |
            @indexMap << index
            @productions << productions[index]
        }
    end

    def mapIndexList(indexList)
        indexList.map {
            | index |
            @indexMap[index]
        }
    end
end

$stderr.puts "Generating code for #{fileName}."

File.open("B3" + matcher.name + ".h", "w") {
    | outp |

    outp.puts "// Generated by generate_pattern_matcher.rb from #{fileName} -- do not edit!"

    outp.puts "#ifndef B3#{matcher.name}_h"
    outp.puts "#define B3#{matcher.name}_h"
    
    outp.puts "#include \"B3Value.h\""
    
    outp.puts "namespace JSC { namespace B3 {"

    outp.puts "template<typename Adaptor>"
    outp.puts "bool run#{matcher.name}(Value* _value, Adaptor& adaptor)"
    outp.puts "{"

    # Note that this does not emit properly indented code, because it's a recursive emitter. If you
    # want to see the code nicely formatted, open it in a good text editor and ask it to format it
    # for you.

    # In odd situations, we may not use the input value. Tell the compiler to chill out about it.
    outp.puts "UNUSED_PARAM(_value);"
        
    # This just protects the caller from having to worry about productions having different lengths.
    def matchLength(outp, valueName, productions)
        indexLists = []
        productions.each_with_index {
            | production, index |
            if indexLists[-1] and productions[indexLists[-1][0]].children.length == production.children.length
                indexLists[-1] << index
            else
                indexLists << [index]
            end
        }

        indexLists.each {
            | indexList |
            length = productions[indexList[0]].children.length
            if length > 0
                outp.puts "if (#{valueName}->children().size() >= #{length}) {"
                yield indexList
                outp.puts "}"
            else
                yield indexList
            end
        }
    end

    # This efficiently selects from the given set of productions. It assumes that productions
    # are not duplicated. It yields the index of the found production, and the coroutine is
    # responsible for emitting specific code for that production. The coroutine is given lists of
    # indices of possibly-matching productions.
    def matchProductions(outp, valueName, productions)
        # First, split the productions list into runs of productions with an opcode and productions
        # without one.
        indexLists = []
        productions.each_with_index {
            | production, index |
            if indexLists[-1] and productions[indexLists[-1][0]].hasOpcode == production.hasOpcode
                indexLists[-1] << index
            else
                indexLists << [index]
            end
        }
        
        # Now, emit pattern matching code. We do it differently for lists with an opcode than for
        # lists without one.
        indexLists.each {
            | indexList |
            if productions[indexList[0]].hasOpcode
                # Now, we split this index list into groups for each opcode.
                groups = {}
                indexList.each {
                    | index |
                    production = productions[index]
                    if groups[production.opcode]
                        groups[production.opcode] << index
                    else
                        groups[production.opcode] = [index]
                    end
                }

                # Emit a switch statement.
                outp.puts "switch (#{valueName}->opcode()) {"
                groups.each_pair {
                    | opcode, subIndexList |
                    outp.puts "case #{opcode}:"
                    subMatch = SubMatch.new(subIndexList, productions)
                    matchLength(outp, valueName, subMatch.productions) {
                        | indexList |
                        yield subMatch.mapIndexList(indexList)
                    }
                    outp.puts "break;"
                }
                outp.puts "default:"
                outp.puts "break;"
                outp.puts "}"
            else
                subMatch = SubMatch.new(indexList, productions)
                matchLength(outp, valueName, subMatch.productions) {
                    | indexList |
                    yield subMatch.mapIndexList(indexList)
                }
            end
        }
    end

    # Takes productions that have the same opcode and the same length and selects from them based on
    # the productions at the given column.
    def matchColumn(outp, valueName, productions, columnIndex)
        if columnIndex >= productions[0].children.length
            yield (0...(productions.size)).to_a
            return
        end

        subValue = newVar
        
        outp.puts "{"
        outp.puts "Value* #{subValue} = #{valueName}->child(#{columnIndex});"

        # We may not use this value. Tell the compiler to chill out about it.
        outp.puts "UNUSED_PARAM(#{subValue});"
        
        subProductions = productions.map {
            | production |
            production.children[columnIndex]
        }

        matchRecursively(outp, subValue, subProductions) {
            | indexList |
            subMatch = SubMatch.new(indexList, productions)
            matchColumn(outp, valueName, subMatch.productions, columnIndex + 1) {
                | indexList |
                yield subMatch.mapIndexList(indexList)
            }
        }

        outp.puts "}"
    end

    def matchRecursively(outp, valueName, productions)
        matchProductions(outp, valueName, productions) {
            | indexList |
            # We are guaranteed that this index list contains productions with the same opcode and
            # the same length.
            subMatch = SubMatch.new(indexList, productions)
            matchColumn(outp, valueName, subMatch.productions, 0) {
                | indexList |
                yield subMatch.mapIndexList(indexList)
            }
        }
    end

    matchRecursively(outp, "_value", matcher.productions) {
        | indexList |
        indexList.each {
            | index |
            production = matcher.productions[index]
            outp.puts "{"
            outp.puts "Value* #{production.varName} = _value;"
            outp.puts "UNUSED_PARAM(#{production.varName});"
            internalArguments = []
            operandArguments = []
            tryArguments = []
            numScopes = 0
            seen = {}

            # FIXME: We want to be able to combine pattern matchers, like have the pattern match rule
            # for Branch in the lowering patcher delegate to a matcher that can deduce the kind of
            # comparison that we're doing. We could then reuse that latter matcher for Check and
            # unfused compare. The key to making such a delegation work is to have the inner matcher
            # (the comparison matcher in this case) keep cascading if it encounters a rule that
            # produces something that the outer matcher cannot handle. It's not obvious that the
            # comparison matcher would need this, but the address matcher probably will. For example,
            # we may have a StoreAddLoad rule that delegates to the address matcher, and the address
            # matcher may produce an address that the lowering matcher cannot use because while the
            # Add form does accept addresses it may not accept the particular address that the
            # address matcher produced. In that case, instead of giving up on the StoreAddLoad rule,
            # we want the address matcher to just try a different address. The comparison matcher
            # might want this just for better controlling when to pin variables. Currently, if it
            # constructed some code, it would also pin the variables that it used. If the lowering
            # matcher then decided to reject the code created by the comparison matcher, it would
            # have a hard time "unpinning" those variables. But if we had some kind of delegation, we
            # could have the pinning of the comparison matcher happen only if the lowering matcher
            # accepted.
            #
            # This could all be accomplished by having tryBlah() return something, and have the
            # matcher also take a functor argument that accepts what tryBlah() returns. So instead of
            #
            # if (tryBlah()) ok;
            #
            # We would have:
            #
            # if (functor(tryBlah()) ok;
            #
            # When lowering decided to delegate to the address or comparison matcher, it could supply
            # a functor that decides whether the thing that the sub-matcher selected is indeed
            # useful.
            #
            # In the near term, we probably won't need this. But we will definitely need it as we
            # expand to efficiently support more platforms. On X86_64, any branching instruction is
            # usable in any context, and any address should be usable in any context (and the only
            # cases where it wouldn't be is if we have holes in the MacroAssembler).
            #
            # https://bugs.webkit.org/show_bug.cgi?id=150559
            
            production.eachVariable {
                | parentVarName, childName, childVarName, kind, index |
                loadExpr = "#{parentVarName}->child(#{index})"
                
                if seen[childVarName]
                    tmp = newVar
                    outp.puts "Value* #{tmp} = #{loadExpr};"
                    outp.puts "if (#{tmp} == #{childVarName}) {"
                    numScopes += 1
                else
                    seen[childVarName] = true
                    
                    outp.puts "Value* #{childVarName} = #{loadExpr};"

                    # FIXME: Consider getting rid of the $ prefix.
                    # https://bugs.webkit.org/show_bug.cgi?id=150527
                    if childName =~ /\A\$/
                        if childName =~ /\A\$([0-9]+)\Z/
                            outp.puts "if (#{childVarName}->isInt(#{$1})) {"
                        else
                            outp.puts "if (#{childVarName}->hasInt()) {"
                        end
                        numScopes += 1
                    end
                    
                    internalArguments << childVarName if kind == :internal or kind == :anonInternal
                    operandArguments << childVarName if kind == :operand or kind == :anonOperand
                    tryArguments << childVarName if kind == :internal or kind == :operand
                end
            }
            outp.puts "if (adaptor.acceptRoot(_value)"
            unless internalArguments.empty?
                outp.puts("&& adaptor.acceptInternals(" + internalArguments.join(", ") + ")")
            end
            unless operandArguments.empty?
                outp.puts("&& adaptor.acceptOperands(" + operandArguments.join(", ") + ")")
            end
            outp.puts("&& adaptor.try#{production.name}(" + tryArguments.join(", ") + ")) {")
            outp.puts "adaptor.acceptRootLate(_value);"
            unless internalArguments.empty?
                outp.puts "adaptor.acceptInternalsLate(" + internalArguments.join(", ") + ");"
            end
            unless operandArguments.empty?
                outp.puts "adaptor.acceptOperandsLate(" + operandArguments.join(", ") + ");"
            end
            outp.puts "return true;"
            outp.puts "}"
            numScopes.times {
                outp.puts "}"
            }
            outp.puts "}"
        }
    }
    
    outp.puts "    return false;"
    outp.puts "}"
    
    outp.puts "} } // namespace JSC::B3"

    outp.puts "#endif // B3#{matcher.name}_h"
}
