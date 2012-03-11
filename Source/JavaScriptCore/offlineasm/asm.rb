#!/usr/bin/env ruby

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

$: << File.dirname(__FILE__)

require "backends"
require "digest/sha1"
require "offsets"
require "parser"
require "self_hash"
require "settings"
require "transform"

class Assembler
    def initialize(outp)
        @outp = outp
        @state = :cpp
        @commentState = :none
        @comment = nil
    end
    
    def enterAsm
        @outp.puts "asm ("
        @state = :asm
    end
    
    def leaveAsm
        putsLastComment
        @outp.puts ");"
        @state = :cpp
    end
    
    def inAsm
        enterAsm
        yield
        leaveAsm
    end
    
    def lastComment
        if @comment
            result = "// #{@comment}"
        else
            result = ""
        end
        @commentState = :none
        @comment = nil
        result
    end
    
    def putsLastComment
        comment = lastComment
        unless comment.empty?
            @outp.puts comment
        end
    end
    
    def puts(*line)
        raise unless @state == :asm
        @outp.puts("\"\\t" + line.join('') + "\\n\" #{lastComment}")
    end
    
    def print(line)
        raise unless @state == :asm
        @outp.print("\"" + line + "\"")
    end
    
    def putsLabel(labelName)
        raise unless @state == :asm
        @outp.puts("OFFLINE_ASM_GLOBAL_LABEL(#{labelName}) #{lastComment}")
    end
    
    def putsLocalLabel(labelName)
        raise unless @state == :asm
        @outp.puts("LOCAL_LABEL_STRING(#{labelName}) \":\\n\" #{lastComment}")
    end
    
    def self.labelReference(labelName)
        "\" SYMBOL_STRING(#{labelName}) \""
    end
    
    def self.localLabelReference(labelName)
        "\" LOCAL_LABEL_STRING(#{labelName}) \""
    end
    
    def comment(text)
        case @commentState
        when :none
            @comment = text
            @commentState = :one
        when :one
            @outp.puts "// #{@comment}"
            @outp.puts "// #{text}"
            @comment = nil
            @commentState = :many
        when :many
            @outp.puts "// #{text}"
        else
            raise
        end
    end
end

asmFile = ARGV.shift
offsetsFile = ARGV.shift
outputFlnm = ARGV.shift

$stderr.puts "offlineasm: Parsing #{asmFile} and #{offsetsFile} and creating assembly file #{outputFlnm}."

begin
    configurationList = offsetsAndConfigurationIndex(offsetsFile)
rescue MissingMagicValuesException
    $stderr.puts "offlineasm: No magic values found. Skipping assembly file generation assuming the classic interpreter is enabled."
    exit 0
end

inputHash =
    "// offlineasm input hash: " + parseHash(asmFile) +
    " " + Digest::SHA1.hexdigest(configurationList.map{|v| (v[0] + [v[1]]).join(' ')}.join(' ')) +
    " " + selfHash

if FileTest.exist? outputFlnm
    File.open(outputFlnm, "r") {
        | inp |
        firstLine = inp.gets
        if firstLine and firstLine.chomp == inputHash
            $stderr.puts "offlineasm: Nothing changed."
            exit 0
        end
    }
end

File.open(outputFlnm, "w") {
    | outp |
    $output = outp
    $output.puts inputHash
    
    $asm = Assembler.new($output)
    
    ast = parse(asmFile)
    
    configurationList.each {
        | configuration |
        offsetsList = configuration[0]
        configIndex = configuration[1]
        forSettings(computeSettingsCombinations(ast)[configIndex], ast) {
            | concreteSettings, lowLevelAST, backend |
            lowLevelAST = lowLevelAST.resolve(*buildOffsetsMap(lowLevelAST, offsetsList))
            lowLevelAST.validate
            emitCodeInConfiguration(concreteSettings, lowLevelAST, backend) {
                $asm.inAsm {
                    lowLevelAST.lower(backend)
                }
            }
        }
    }
}

$stderr.puts "offlineasm: Assembly file #{outputFlnm} successfully generated."

