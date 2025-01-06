#!/usr/bin/env ruby

# Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
require 'optparse'
require "parser"
require "self_hash"
require "settings"
require "shellwords"
require "transform"

class Assembler
    def initialize(outp)
        @outp = outp
        @state = :cpp
        resetAsm
    end

    def resetAsm
        @comment = nil
        @internalComment = nil
        @annotation = nil
        @codeOrigin = nil
        @numLocalLabels = 0
        @numGlobalLabels = 0
        @deferredOSDarwinActions = []
        @deferredNextLabelActions = []
        @count = 0
        @debugAnnotationStr = nil
        @lastDebugAnnotationStr = nil

        @newlineSpacerState = :none
        @lastlabel = ""
    end

    def enterAsm
        @outp.puts ""
        putStr "OFFLINE_ASM_BEGIN"

        @state = :asm
        SourceFile.outputDotFileList(@outp) if $enableDebugAnnotations
    end
    
    def leaveAsm
        putsLastComment
        if not @deferredOSDarwinActions.size.zero?
            putStr("#if OS(DARWIN)")
            (@deferredNextLabelActions + @deferredOSDarwinActions).each {
                | action |
                action.call()
            }
            putStr("#endif // OS(DARWIN)")
        end
        putStr "OFFLINE_ASM_END"
        @state = :cpp
    end
    
    def deferOSDarwinAction(&proc)
        @deferredOSDarwinActions << proc
    end

    def deferNextLabelAction(&proc)
        @deferredNextLabelActions << proc
    end
    
    def newUID
        @count += 1
        @count
    end
    
    def inAsm
        resetAsm
        enterAsm
        yield
        leaveAsm
    end
    
    # Concatenates all the various components of the comment to dump.
    def lastComment
        separator = " "
        result = ""
        result = "#{@comment}" if @comment
        if @annotation and $enableInstrAnnotations
            result += separator if result != ""
            result += "#{@annotation}"
        end
        if @internalComment
            result += separator if result != ""
            result += "#{@internalComment}"
        end
        if $enableCodeOriginComments and @codeOrigin and @codeOrigin != @lastCodeOrigin
            @lastCodeOrigin = @codeOrigin
            result += separator if result != ""
            result += "#{@codeOrigin}"
        end
        if result != ""
            result = "// " + result
        end

        # Reset all the components that we've just sent to be dumped.
        @comment = nil
        @annotation = nil
        @codeOrigin = nil
        @internalComment = nil
        result
    end
    
    # Puts a C Statement in the output stream.
    def putc(*line)
        raise unless @state == :asm
        @outp.puts(formatDump("    " + line.join(''), lastComment))
    end
    
    def formatDump(dumpStr, comment, commentColumns=$preferredCommentStartColumn)
        result = ""
        if comment.length > 0
            result = "%-#{commentColumns}s %s" % [dumpStr, comment]
        else
            result = dumpStr
        end
        if $enableDebugAnnotations
            if @debugAnnotationStr and @debugAnnotationStr != @lastDebugAnnotationStr
                result = "%-#{$preferredDebugAnnotationColumns}s%s" % [@debugAnnotationStr, result]
            else
                result = "%-#{$preferredDebugAnnotationColumns}s%s" % ["", result]
            end
            @lastDebugAnnotationStr = @debugAnnotationStr
            @debugAnnotationStr = nil
        end
        result
    end

    # private method for internal use only.
    def putAnnotation(text)
        raise unless @state == :asm
        if $enableInstrAnnotations
            @outp.puts text
            @annotation = nil
        end
    end

    def putLocalAnnotation()
        putAnnotation "    // #{@annotation}" if @annotation
    end

    def putGlobalAnnotation()
        putsNewlineSpacerIfAppropriate(:annotation)
        putAnnotation "// #{@annotation}" if @annotation
    end

    def putsLastComment
        comment = lastComment
        unless comment.empty?
            @outp.puts comment
        end
    end

    def putStr(str)
        if $enableDebugAnnotations
            @outp.puts "%-#{$preferredDebugAnnotationColumns}s%s" % ["", str]
        else
            @outp.puts str
        end
    end

    def puts(*line)
        raise unless @state == :asm
        @outp.puts(formatDump("    \"" + line.join('') + " \\n\"", lastComment))
    end
    
    def print(line)
        raise unless @state == :asm
        @outp.print("\"" + line + "\"")
    end
    
    def putsNewlineSpacerIfAppropriate(state)
        if @newlineSpacerState != state
            @outp.puts("\n")
            @newlineSpacerState = state
        end
    end

    def putsLabel(labelName, isGlobal, isExport, isAligned, alignTo)
        raise unless @state == :asm
        @deferredNextLabelActions.each {
            | action |
            action.call()
        }
        @deferredNextLabelActions = []
        @numGlobalLabels += 1
        putsNewlineSpacerIfAppropriate(:global)
        @internalComment = $enableLabelCountComments ? "Global Label #{@numGlobalLabels}" : nil
        if isGlobal
            if isAligned
                if isExport
                    @outp.puts(formatDump("OFFLINE_ASM_GLOBAL_EXPORT_LABEL(#{labelName})", lastComment))
                elsif alignTo
                    @outp.puts(formatDump("OFFLINE_ASM_ALIGNED_GLOBAL_LABEL(#{labelName}, #{alignTo})", lastComment))
                else
                    @outp.puts(formatDump("OFFLINE_ASM_GLOBAL_LABEL(#{labelName})", lastComment))
                end
            else
                if isExport
                    @outp.puts(formatDump("OFFLINE_ASM_UNALIGNED_GLOBAL_EXPORT_LABEL(#{labelName})", lastComment))
                else
                    @outp.puts(formatDump("OFFLINE_ASM_UNALIGNED_GLOBAL_LABEL(#{labelName})", lastComment))
                end
            end
        elsif /\Allint_op_/.match(labelName)
            @outp.puts(formatDump("OFFLINE_ASM_OPCODE_LABEL(op_#{$~.post_match})", lastComment))
        else
            if alignTo
                @outp.puts(formatDump("OFFLINE_ASM_ALIGN_TRAP(#{alignTo})", lastComment))
            end
            @outp.puts(formatDump("OFFLINE_ASM_GLUE_LABEL(#{labelName})", lastComment))
        end
        if $emitELFDebugDirectives
            deferNextLabelAction {
                putStr("    \".size #{labelName} , . - #{labelName} \\n\"")
                putStr("    \".type #{labelName} , function \\n\"")
            }
        end
        @newlineSpacerState = :none # After a global label, we can use another spacer.
    end
    
    def putsLocalLabel(labelName)
        raise unless @state == :asm
        @numLocalLabels += 1
        @outp.puts("\n")
        @internalComment = $enableLabelCountComments ? "Local Label #{@numLocalLabels}" : nil
        @outp.puts(formatDump("  OFFLINE_ASM_LOCAL_LABEL(#{labelName})", lastComment))
    end

    def self.externLabelReference(labelName)
        "\" LOCAL_REFERENCE(#{labelName}) \""
    end

    def self.labelReference(labelName)
        "\" LOCAL_LABEL_STRING(#{labelName}) \""
    end
    
    def self.localLabelReference(labelName)
        "\" LOCAL_LABEL_STRING(#{labelName}) \""
    end
    
    def self.cLabelReference(labelName)
        if /\Allint_op_/.match(labelName)
            "op_#{$~.post_match}"  # strip opcodes of their llint_ prefix.
        else
            "#{labelName}"
        end
    end
    
    def self.cLocalLabelReference(labelName)
        "#{labelName}"
    end
    
    def codeOrigin(text)
        @codeOrigin = text
    end

    def comment(text)
        @comment = text
    end

    def annotation(text)
        @annotation = text
    end

    def debugAnnotation(text)
        @debugAnnotationStr = text
    end
end

IncludeFile.processIncludeOptions()

asmFile = ARGV.shift
offsetsFile = ARGV.shift
outputFlnm = ARGV.shift
variants = ARGV.shift.split(/[,\s]+/)

$options = {}
OptionParser.new do |opts|
    opts.banner = "Usage: asm.rb asmFile offsetsFile outputFileName [--platform=<OS>] [--webkit-additions-path=<path>] [--binary-format=<format>] [--depfile=<depfile>]"
    # This option is currently only used to specify Windows for label lowering
    opts.on("--platform=[Windows]", "Specify a specific platform for lowering.") do |platform|
        $options[:platform] = platform
    end
    opts.on("--webkit-additions-path=PATH", "WebKitAdditions path.") do |path|
        $options[:webkit_additions_path] = path
    end
    opts.on("--binary-format=FORMAT", "Specify the binary format used by the target system.") do |format|
        $options[:binary_format] = format
    end
    opts.on("--depfile=DEPFILE", "Path to write Makefile-style discovered dependencies to.") do |path|
        $options[:depfile] = path
    end
end.parse!

begin
    configurationList = offsetsAndConfigurationIndexForVariants(offsetsFile, variants)
rescue MissingMagicValuesException
    $stderr.puts "offlineasm: No magic values found in #{offsetsFile}. Skipping assembly file generation."
    exit 1
end

# We want this in all ELF systems we support, except for C_LOOP (we'll disable it later on if we are building cloop)
$emitELFDebugDirectives = $options.has_key?(:binary_format) && $options[:binary_format] == "ELF"

inputHash =
    "// offlineasm input hash: " + parseHash(asmFile, $options) +
    " " + Digest::SHA1.hexdigest(configurationList.map{|v| (v[0] + [v[1]]).join(' ')}.join(' ')) +
    " " + selfHash +
    " " + Digest::SHA1.hexdigest($options.has_key?(:platform) ? $options[:platform] : "")

if FileTest.exist?(outputFlnm) and (not $options[:depfile] or FileTest.exist?($options[:depfile]))
    lastLine = nil
    File.open(outputFlnm, "r") {
        | file |
        file.each_line {
            | line |
            line = line.chomp
            unless line.empty?
                lastLine = line
            end
        }
    }
    if lastLine and lastLine == inputHash
        # Nothing changed.
        exit 0
    end
end

File.open(outputFlnm, "w") {
    | outp |
    $output = outp

    $asm = Assembler.new($output)

    sources = Set.new
    ast = parse(asmFile, $options, sources)
    settingsCombinations = computeSettingsCombinations(ast)
    
    if $options[:depfile]
        depfile = File.open($options[:depfile], "w")
        depfile.print(Shellwords.escape(outputFlnm), ": ")
        depfile.puts(Shellwords.join(sources.sort))
    end

    configurationList.each {
        | configuration |
        offsetsList = configuration[0]
        configIndex = configuration[1]
        forSettings(settingsCombinations[configIndex], ast) {
            | concreteSettings, lowLevelAST, backend |

            # There could be multiple backends we are generating for, but the C_LOOP is
            # always by itself so this check to turn off $enableDebugAnnotations won't
            # affect the generation for any other backend.
            if backend == "C_LOOP"
                $enableDebugAnnotations = false
                $preferredCommentStartColumn = 60
                $emitELFDebugDirectives = false
            end

            lowLevelAST = lowLevelAST.demacroify({})
            lowLevelAST = lowLevelAST.resolve(buildOffsetsMap(lowLevelAST, offsetsList))
            lowLevelAST.validate
            emitCodeInConfiguration(concreteSettings, lowLevelAST, backend) {
                $currentSettings = concreteSettings
                $asm.inAsm {
                    lowLevelAST.lower(backend)
                }
            }
        }
    }

    $output.fsync
    $output.puts inputHash
}
