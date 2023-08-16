# Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
require_relative 'GeneratedFile'
require_relative 'Section'
require_relative 'Template'
require_relative 'Type'
require_relative 'Wasm'

module DSL
    @sections = []
    @wasm_section = nil
    @current_section = nil
    @context = binding()
    @namespaces = []

    def self.begin_section(name, config={})
        assert("must call `end_section` before beginning a new section") { @current_section.nil? }
        @current_section = Section.new name, config
    end

    def self.end_section(name)
        assert("current section's name is `#{@current_section.name}`, but end_section was called with `#{name}`") { @current_section.name == name }
        if @current_section.config[:preserve_order]
            @current_section.validate
        else
            @current_section.sort!
        end
        @current_section.create_ids!
        @sections << @current_section
        if @current_section.is_wasm?
          assert("Cannot have 2 wasm sections") { @wasm_section.nil? }
          @wasm_section = @current_section
        end
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
    
    def self.simd_op_group(desc, ops, config)
        assert("`simd_op_group` can only be called in between `begin_section` and `end_section`") { not @current_section.nil? }

        for op in ops do
            variants = ["i32x4", "i64x2", "f32x4", "f64x2"]
            if config[:makeSignedVariants] then
                variants += ["i8x16_u", "i8x16_s", "i16x8_u", "i16x8_s"]
            else
                variants += ["i8x16", "i16x8"]
            end

            if config[:makeAllSignedAndUnsignedVariants] then
                variants = ["i8x16_u", "i8x16_s", "i16x8_u", "i16x8_s", "i32x4_u", "i32x4_s", "i64x2_u", "i64x2_s", "f32x4", "f64x2"]
            end

            if config[:makeOnlyFloatingPointVariants] then
                variants = ["f32x4", "f64x2"]
            end

            if config[:makeOnlyV128] then
                variants = ["v128"]
            end

            if config[:makeOnlyI8] then
                variants = ["i8x16"]
            end

            if config[:makeOnlyF32] then
                variants = ["f32x4"]
            end

            if config[:makeOnlyF64] then
                variants = ["f64x2"]
            end
            

            if config[:skipI64Variant] then
                variants = variants.filter do |v| 
                    isI64 = v =~ /i64x2/
                    !isI64
                end
            end

            if config[:skipI32Variant] then
                variants = variants.filter do |v| 
                    isI32 = v =~ /i32x4/
                    !isI32
                end
            end

            if config[:skipI8Variant] then
                variants = variants.filter do |v| 
                    isI8 = v =~ /i8x16/
                    !isI8
                end
            end

            if config[:skipFloatVariants] then
                variants = variants.filter do |v| 
                    isFloat = v =~ /f32x4|f64x2/
                    !isFloat
                end
            end

            if config[:variants] then
                variants = config[:variants]
            end

            opList = []
            for v in variants do
                opList << ((op.to_s + "_" + v).to_sym)
            end
            @current_section.add_opcode_group((desc.to_s + op.to_s).to_sym, opList, config)
        end
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

    def self.autogenerate_wasm_opcodes()
        assert("`autogenerate_wasm_opcodes` can only be called in between `begin_section` and `end_section`") { not @current_section.nil? }
        assert("`autogenerate_wasm_opcodes` can only be called from the `Wasm` section") { @current_section.name == :Wasm }
        Wasm::autogenerate_opcodes(@context, @wasm_json)
    end

    def self.run(options)
        bytecode_list_path = options[:bytecode_list]
        bytecode_list = File.open(bytecode_list_path).read

        @wasm_json = File.open(options[:wasm_json_filename]).read

        @context.eval(bytecode_list, bytecode_list_path)
        assert("must end last section") { @current_section.nil? }

        write_bytecodes(bytecode_list, options[:bytecodes_filename])
        write_bytecode_structs(bytecode_list, options[:bytecode_structs_filename])
        write_bytecode_dumper(bytecode_list, options[:bytecode_dumper_filename])
        write_bytecodes_init(options[:init_asm_filename], bytecode_list)
        write_indices(bytecode_list, options[:bytecode_indices_filename])
        write_llint_generator(options[:wasm_llint_generator_filename], bytecode_list, @wasm_json)
        write_wasm_init(options[:wasm_init_filename], bytecode_list, @wasm_json)
    end

    def self.write_bytecodes(bytecode_list, bytecodes_filename)
        GeneratedFile::create(bytecodes_filename, bytecode_list) do |template|
            template.prefix = "#pragma once\n"
            num_opcodes = @sections.map(&:opcodes).flatten.size
            template.body = [
                @sections.map { |s| s.header_helpers(num_opcodes) },
                @sections.select { |s| s.config[:emit_in_structs_file] }.map(&:for_each_struct)
            ].flatten.join("\n")
        end
    end

    def self.write_bytecode_structs(bytecode_list, bytecode_structs_filename)
        GeneratedFile::create(bytecode_structs_filename, bytecode_list) do |template|
            template.prefix = <<-EOF
#pragma once

#include "ArithProfile.h"
#include "ArrayAllocationProfile.h"
#include "BytecodeDumper.h"
#include "Fits.h"
#include "GetByIdMetadata.h"
#include "GetByValHistory.h"
#include "Instruction.h"
#include "IterationModeMetadata.h"
#include "JSPropertyNameEnumerator.h"
#include "Opcode.h"
#include "PrivateFieldPutKind.h"
#include "PutByStatus.h"
#include "PutByIdFlags.h"
#include "ToThisStatus.h"

namespace JSC {

class BasicBlockLocation;

void dumpBytecode(BytecodeDumperBase<JSInstructionStream>* dumper, JSInstructionStream::Offset, const JSInstruction*);

#if ENABLE(WEBASSEMBLY)
void dumpWasm(BytecodeDumperBase<WasmInstructionStream>* dumper, WasmInstructionStream::Offset, const WasmInstruction*);
#endif // ENABLE(WEBASSEMBLY)

EOF

            template.body = <<-EOF
#{opcodes_filter { |s| s.config[:emit_in_structs_file] && !s.is_wasm? }.map(&:struct).join("\n")}

#if ENABLE(WEBASSEMBLY)
#{opcodes_filter { |s| s.config[:emit_in_structs_file] && s.is_wasm? }.map(&:struct).join("\n")}
#endif // ENABLE(WEBASSEMBLY)
EOF
            template.suffix = "} // namespace JSC"
        end
    end

    def self.write_bytecode_dumper(bytecode_list, bytecode_dumper_filename)
        GeneratedFile::create(bytecode_dumper_filename, bytecode_list) do |template|
            template.prefix = <<-EOF
#include "config.h"
#include "BytecodeDumper.h"

#include "BytecodeStructs.h"

namespace JSC {
EOF

            template.body = <<-EOF
#{Opcode.dump_bytecode(:Bytecode, :JSOpcodeTraits, opcodes_filter { |s| s.config[:emit_in_structs_file] && !s.is_wasm? })}

#if ENABLE(WEBASSEMBLY)
#{Opcode.dump_bytecode(:Wasm, :WasmOpcodeTraits, opcodes_filter { |s| s.is_wasm? })}
#endif // ENABLE(WEBASSEMBLY)
EOF
            template.suffix = "} // namespace JSC"
        end
    end

    def self.write_init_asm(opcodes, filename, *dependencies)
        GeneratedFile::create(filename, *dependencies) do |template|
            template.multiline_comment = nil
            template.line_comment = "#"
            template.body = (opcodes.map.with_index(&:set_entry_address) + opcodes.map.with_index(&:set_entry_address_wide16) + opcodes.map.with_index(&:set_entry_address_wide32)) .join("\n")
        end
    end

    def self.write_bytecodes_init(bytecodes_init_filename, *dependencies)
        write_init_asm(opcodes_for(:emit_in_asm_file), bytecodes_init_filename, *dependencies)
    end

    def self.write_wasm_init(wasm_init_filename, *dependencies)
        write_init_asm(@wasm_section.opcodes, wasm_init_filename, *dependencies)
    end

    def self.write_llint_generator(generator_filename, *dependencies)
        GeneratedFile::create(generator_filename, *dependencies) do |template|
            template.body = Wasm::generate_llint_generator(@wasm_section)
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

    def self.opcodes_filter
        sections = @sections.select { |s| yield s }
        sections.map(&:opcodes).flatten
    end
end
