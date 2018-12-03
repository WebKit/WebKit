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

require_relative 'Assertion'
require_relative 'Section'
require_relative 'Template'
require_relative 'Type'
require_relative 'GeneratedFile'

module DSL
    @sections = []
    @current_section = nil
    @context = binding()
    @namespaces = []

    def self.begin_section(name, config={})
        assert("must call `end_section` before beginning a new section") { @current_section.nil? }
        @current_section = Section.new name, config
    end

    def self.end_section(name)
        assert("current section's name is `#{@current_section.name}`, but end_section was called with `#{name}`") { @current_section.name == name }
        @current_section.sort!
        @sections << @current_section
        @current_section = nil
    end

    def self.op(name, config = {})
        assert("`op` can only be called in between `begin_section` and `end_section`") { not @current_section.nil? }
        @current_section.add_opcode(name, config)
    end

    def self.op_group(desc, ops, config)
        assert("`op_group` can only be called in between `begin_section` and `end_section`") { not @current_section.nil? }
        @current_section.add_opcode_group(desc, ops, config)
    end

    def self.types(types)
        types.map do |type|
            type = (@namespaces + [type]).join "::"
            @context.eval("#{type} = Type.new '#{type}'")
        end
    end

    def self.templates(types)
        types.map do |type|
            type = (@namespaces + [type]).join "::"
            @context.eval("#{type} = Template.new '#{type}'")
        end
    end

    def self.namespace(name)
        @namespaces << name.to_s
        ctx = @context
        @context = @context.eval("
            module #{name}
              def self.get_binding
                binding()
              end
            end
            #{name}.get_binding
         ")
        yield
        @context = ctx
        @namespaces.pop
    end

    def self.run(options)
        bytecodeListPath = options[:bytecodeList]
        bytecodeList = File.open(bytecodeListPath)
        @context.eval(bytecodeList.read, bytecodeListPath)
        assert("must end last section") { @current_section.nil? }

        write_bytecodes(bytecodeList, options[:bytecodesFilename])
        write_bytecode_structs(bytecodeList, options[:bytecodeStructsFilename])
        write_init_asm(bytecodeList, options[:initAsmFilename])
        write_indices(bytecodeList, options[:bytecodeIndicesFilename])
    end

    def self.write_bytecodes(bytecode_list, bytecodes_filename)
        GeneratedFile::create(bytecodes_filename, bytecode_list) do |template|
            template.prefix = "#pragma once\n"
            num_opcodes = @sections.map(&:opcodes).flatten.size
            template.body = <<-EOF
#{@sections.map { |s| s.header_helpers(num_opcodes) }.join("\n")}
#define FOR_EACH_BYTECODE_STRUCT(macro) \\
#{opcodes_for(:emit_in_structs_file).map { |op| "    macro(#{op.capitalized_name}) \\" }.join("\n")}
            EOF
        end
    end

    def self.write_bytecode_structs(bytecode_list, bytecode_structs_filename)
        GeneratedFile::create(bytecode_structs_filename, bytecode_list) do |template|
            opcodes = opcodes_for(:emit_in_structs_file)

            template.prefix = <<-EOF
#pragma once

#include "ArithProfile.h"
#include "BytecodeDumper.h"
#include "BytecodeGenerator.h"
#include "Fits.h"
#include "GetByIdMetadata.h"
#include "Instruction.h"
#include "Opcode.h"
#include "ToThisStatus.h"

namespace JSC {
EOF

            template.body = <<-EOF
#{opcodes.map(&:struct).join("\n")}
#{Opcode.dump_bytecode(opcodes)}
EOF
            template.suffix = "} // namespace JSC"
        end
    end

    def self.write_init_asm(bytecode_list, init_asm_filename)
        opcodes = opcodes_for(:emit_in_asm_file)

        GeneratedFile::create(init_asm_filename, bytecode_list) do |template|
            template.multiline_comment = nil
            template.line_comment = "#"
            template.body = (opcodes.map.with_index(&:set_entry_address) + opcodes.map.with_index(&:set_entry_address_wide)) .join("\n")
        end
    end

    def self.write_indices(bytecode_list, indices_filename)
        opcodes = opcodes_for(:emit_in_structs_file)

        GeneratedFile::create(indices_filename, bytecode_list) do |template|
            template.prefix = "namespace JSC {\n"
            template.body = opcodes.map(&:struct_indices).join("\n")
            template.suffix = "\n} // namespace JSC"
        end
    end

    def self.opcodes_for(file)
        sections = @sections.select { |s| s.config[file] }
        sections.map(&:opcodes).flatten
    end
end
