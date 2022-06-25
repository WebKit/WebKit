#!/usr/bin/env ruby

# Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

IncludeFile.processIncludeOptions()

inputFlnm = ARGV.shift
settingsFlnm = ARGV.shift
outputFlnm = ARGV.shift

validBackends = canonicalizeBackendNames(ARGV.shift.split(/[,\s]+/))
includeOnlyBackends(validBackends)

variants = ARGV.shift.split(/[,\s]+/)

$options = {}
OptionParser.new do |opts|
    opts.banner = "Usage: generate_offset_extractor.rb asmFile settingFile outputFileName backends variants [--webkit-additions-path=<path>] [--depfile=<depfile>]"
    opts.on("--webkit-additions-path=PATH", "WebKitAdditions path.") do |path|
        $options[:webkit_additions_path] = path
    end
    opts.on("--depfile=DEPFILE", "path to write Makefile-style discovered dependencies to.") do |path|
        $options[:depfile] = path
    end
end.parse!

begin
    configurationList = configurationIndicesForVariants(settingsFlnm, variants)
rescue MissingMagicValuesException
    $stderr.puts "OffsetExtractor: No magic values found. Skipping offsets extractor file generation."
    exit 1
end

def emitMagicNumber
    OFFSET_MAGIC_NUMBERS.each {
        | number |
        $output.puts "unsigned(#{number}),"
    }
end

configurationHash = Digest::SHA1.hexdigest(configurationList.join(' '))
inputHash = "// OffsetExtractor input hash: #{parseHash(inputFlnm, $options)} #{configurationHash} #{selfHash}"

if FileTest.exist?(outputFlnm) and (not $options[:depfile] or FileTest.exist?($options[:depfile]))
    File.open(outputFlnm, "r") {
        | inp |
        firstLine = inp.gets
        if firstLine and firstLine.chomp == inputHash
            # Nothing changed.
            exit 0
        end
    }
end

sources = Set.new
ast = parse(inputFlnm, $options, sources)
settingsCombinations = computeSettingsCombinations(ast)

if $options[:depfile]
    depfile = File.open($options[:depfile], "w")
    depfile.print(Shellwords.escape(outputFlnm), ": ")
    depfile.puts(Shellwords.join(sources.sort))
end

File.open(outputFlnm, "w") {
    | outp |
    $output = outp
    outp.puts inputHash

    configurationList.each {
        | configIndex |
        forSettings(settingsCombinations[configIndex], ast) {
            | concreteSettings, lowLevelAST, backend |

            lowLevelAST = lowLevelAST.demacroify({})
            offsetsList = offsetsList(lowLevelAST)
            sizesList = sizesList(lowLevelAST)
            constsList = constsList(lowLevelAST)

            emitCodeInConfiguration(concreteSettings, lowLevelAST, backend) {

                # Windows complains about signed integers being cast to unsigned but we just want the bits.
                outp.puts "\#if COMPILER(MSVC)"
                outp.puts "\#pragma warning(disable:4308)"
                outp.puts "\#endif"
                constsList.each_with_index {
                    | const, index |
                    outp.puts "constexpr int64_t constValue#{index} = static_cast<int64_t>(#{const.value});"
                }
                outp.puts "static const int64_t offsetExtractorTable[] = {"
                OFFSET_HEADER_MAGIC_NUMBERS.each {
                    | number |
                    outp.puts "unsigned(#{number}),"
                }

                emitMagicNumber
                outp.puts "#{configIndex},"
                offsetsList.each {
                    | offset |
                    emitMagicNumber
                    outp.puts "OFFLINE_ASM_OFFSETOF(#{offset.struct}, #{offset.field}),"
                }
                sizesList.each {
                    | sizeof |
                    emitMagicNumber
                    outp.puts "sizeof(#{sizeof.struct}),"
                }
                constsList.each_with_index {
                    | const, index |
                    emitMagicNumber
                    outp.puts "constValue#{index},"
                }
                outp.puts "};"
            }
        }
    }

    outp.puts "static const int64_t offsetExtractorTable[] = { };" if not configurationList.size

}
