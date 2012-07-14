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

require "config"
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
        @internalComment = nil
        @annotation = nil
        @codeOrigin = nil
        @numLocalLabels = 0
        @numGlobalLabels = 0
    end
    
    def enterAsm
        @outp.puts "OFFLINE_ASM_BEGIN"
        @state = :asm
    end
    
    def leaveAsm
        putsLastComment
        @outp.puts "OFFLINE_ASM_END"
        @state = :cpp
    end
    
    def inAsm
        enterAsm
        yield
        leaveAsm
    end
    
    # Concatenates all the various components of the comment to dump.
    def lastComment
        result = ""
        result = " #{@comment} ." if @comment
        result += " #{@annotation} ." if @annotation and $enableTrailingInstrAnnotations
        result += " #{@internalComment} ." if @internalComment
        result += " #{@codeOrigin} ." if @codeOrigin and $enableCodeOriginComments
        if result != ""
            result = "  //" + result
        end

        # Reset all the components that we've just sent to be dumped.
        @commentState = :none
        @comment = nil
        @internalComment = nil
        @annotation = nil
        @codeOrigin = nil
        result
    end
    
    # Dumps the current instruction annotation in interlaced mode if appropriate.
    def putInterlacedAnnotation()
        raise unless @state == :asm
        if $enableInterlacedInstrAnnotations
            @outp.puts("    // #{@annotation}") if @annotation
            @annotation = nil
        end
    end

    def putsLastComment
        comment = lastComment
        unless comment.empty?
            @outp.puts comment
        end
    end
    
    def puts(*line)
        raise unless @state == :asm
        putInterlacedAnnotation
        @outp.puts("    \"\\t" + line.join('') + "\\n\"#{lastComment}")
    end
    
    def print(line)
        raise unless @state == :asm
        @outp.print("\"" + line + "\"")
    end
    
    def putsLabel(labelName)
        raise unless @state == :asm
        @numGlobalLabels += 1
        @outp.puts("\n")
        @internalComment = $enableLabelCountComments ? "Global Label #{@numGlobalLabels}" : nil
        @outp.puts("OFFLINE_ASM_GLOBAL_LABEL(#{labelName})#{lastComment}")
    end
    
    def putsLocalLabel(labelName)
        raise unless @state == :asm
        @numLocalLabels += 1
        @outp.puts("\n")
        @internalComment = $enableLabelCountComments ? "Local Label #{@numLocalLabels}" : nil
        @outp.puts("OFFLINE_ASM_LOCAL_LABEL(#{labelName})#{lastComment}")
    end
    
    def self.labelReference(labelName)
        "\" LOCAL_REFERENCE(#{labelName}) \""
    end
    
    def self.localLabelReference(labelName)
        "\" LOCAL_LABEL_STRING(#{labelName}) \""
    end
    
    def codeOrigin(text)
        case @commentState
        when :none
            @codeOrigin = text
            @commentState = :one
        when :one
            if $enableCodeOriginComments
                @outp.puts "    // #{@codeOrigin}"
                @outp.puts "    // #{text}"
            end
            @codeOrigin = nil
            @commentState = :many
        when :many
            @outp.puts "// #{text}" if $enableCodeOriginComments
        else
            raise
        end
    end

    def comment(text)
        @comment = text
    end
    def annotation(text)
        @annotation = text
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

