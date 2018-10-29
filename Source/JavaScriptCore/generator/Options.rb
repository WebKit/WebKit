# Copyright (C) 2018 Apple Inc. All rights reserved.
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

require 'optparse'

$config = {
    bytecodesFilename: {
        short: "-b",
        long: "--bytecodes_h FILE",
        desc: "generate bytecodes macro .h FILE",
    },
    bytecodeStructsFilename: {
        short: "-s",
        long: "--bytecode_structs_h FILE",
        desc: "generate bytecode structs .h FILE",
    },
    initAsmFilename: {
        short: "-a",
        long: "--init_bytecodes_asm FILE",
        desc: "generate ASM bytecodes init FILE",
    },
    bytecodeIndicesFilename: {
        short: "-i",
        long: "--bytecode_indices_h FILE",
        desc: "generate indices of bytecode structs .h FILE",
    },
};

module Options
    def self.optparser(options)
        OptionParser.new do |opts|
            opts.banner = "usage: #{opts.program_name} [options] <bytecode-list-file>"
            $config.map do |key, option|
                opts.on(option[:short], option[:long], option[:desc]) do |v|
                    options[key] = v
                end
            end
        end
    end

    def self.check(argv, options)
        missing = $config.keys.select{ |param| options[param].nil? }
        unless missing.empty?
            raise OptionParser::MissingArgument.new(missing.join(', '))
        end
        unless argv.length == 1
            raise OptionParser::MissingArgument.new("<bytecode-list-file>")
        end
    end

    def self.parse(argv)
        options = {}
        parser = optparser(options)

        begin
            parser.parse!(argv)
            check(argv, options)
        rescue OptionParser::MissingArgument, OptionParser::InvalidOption
            puts $!.to_s
            puts parser
            exit 1
        end

        options[:bytecodeList] = argv[0]
        options
    end
end
