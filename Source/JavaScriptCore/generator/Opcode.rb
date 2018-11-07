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

require_relative 'Argument'
require_relative 'Fits'
require_relative 'Metadata'

class Opcode
    attr_reader :id
    attr_reader :args
    attr_reader :metadata

    module Size
        Narrow = "OpcodeSize::Narrow"
        Wide = "OpcodeSize::Wide"
    end

    @@id = 0

    def self.id
        tid = @@id
        @@id = @@id + 1
        tid
    end

    def initialize(section, name, args, metadata, metadata_initializers)
        @section = section
        @name = name
        @metadata = Metadata.new metadata, metadata_initializers
        @args = args.map.with_index { |(arg_name, type), index| Argument.new arg_name, type, index + 1 } unless args.nil?
    end

    def create_id!
        @id = self.class.id
    end

    def print_args(&block)
        return if @args.nil?

        @args.map(&block).join "\n"
    end

    def capitalized_name
        name.split('_').map(&:capitalize).join
    end

    def typed_args
        return if @args.nil?

        @args.map(&:create_param).unshift("").join(", ")
    end

    def untyped_args
        return if @args.nil?

        @args.map(&:name).unshift("").join(", ")
    end

    def map_fields_with_size(size, &block)
        args = [Argument.new("opcodeID", :unsigned, 0)]
        args += @args.dup if @args
        unless @metadata.empty?
            args << @metadata.emitter_local
        end
        args.map { |arg| block.call(arg, size) }
    end

    def struct
        <<-EOF
        struct #{capitalized_name} : public Instruction {
            #{opcodeID}

            #{emitter}

            #{dumper}

            #{constructors}

            #{setters}

            #{metadata_struct_and_accessor}

            #{members}
        };
        EOF
    end

    def opcodeID
        "static constexpr OpcodeID opcodeID = #{name};"
    end

    def emitter
        op_wide = Argument.new("op_wide", :unsigned, 0)
        metadata_param = @metadata.empty? ? "" : ", #{@metadata.emitter_local.create_param}"
        metadata_arg = @metadata.empty? ? "" : ", #{@metadata.emitter_local.name}"
        <<-EOF
        static void emit(BytecodeGenerator* __generator#{typed_args})
        {
            __generator->recordOpcode(opcodeID);
            #{@metadata.create_emitter_local}
            emit<OpcodeSize::Narrow, NoAssert, false>(__generator#{untyped_args}#{metadata_arg}) || emit<OpcodeSize::Wide, Assert, false>(__generator#{untyped_args}#{metadata_arg});
        }

        #{%{
        template<OpcodeSize size, FitsAssertion shouldAssert = Assert>
        static bool emit(BytecodeGenerator* __generator#{typed_args})
        {
            #{@metadata.create_emitter_local}
            return emit<size, shouldAssert>(__generator#{untyped_args}#{metadata_arg});
        }
        } unless @metadata.empty?}

        template<OpcodeSize size, FitsAssertion shouldAssert = Assert, bool recordOpcode = true>
        static bool emit(BytecodeGenerator* __generator#{typed_args}#{metadata_param})
        {
            if (recordOpcode)
                __generator->recordOpcode(opcodeID);
            bool didEmit = emitImpl<size>(__generator#{untyped_args}#{metadata_arg});
            if (shouldAssert == Assert)
                ASSERT(didEmit);
            return didEmit;
        }

        private:
        template<OpcodeSize size>
        static bool emitImpl(BytecodeGenerator* __generator#{typed_args}#{metadata_param})
        {
            if (size == OpcodeSize::Wide)
                __generator->alignWideOpcode();
            if (#{map_fields_with_size("size", &:fits_check).join " && "} && (size == OpcodeSize::Wide ? #{op_wide.fits_check(Size::Narrow)} : true)) {
                if (size == OpcodeSize::Wide)
                    #{op_wide.fits_write Size::Narrow}
                #{map_fields_with_size("size", &:fits_write).join "\n"}
                return true;
            }
            return false;
        }
        public:
        EOF
    end

    def dumper
        <<-EOF
        template<typename Block>
        void dump(BytecodeDumper<Block>* __dumper, InstructionStream::Offset __location, bool __isWide)
        {
            __dumper->printLocationAndOp(__location, &"*#{@name}"[!__isWide]);
            #{print_args { |arg|
            <<-EOF
                __dumper->dumpOperand(#{arg.name}, #{arg.index == 1});
            EOF
            }}
        }
        EOF
    end

    def constructors
        fields = (@args || []) + (@metadata.empty? ? [] : [@metadata])
        init = ->(size) { fields.empty?  ? "" : ": #{fields.map.with_index { |arg, i| arg.load_from_stream(i + 1, size) }.join ",\n" }" }

        <<-EOF
        #{capitalized_name}(const uint8_t* stream)
            #{init.call("OpcodeSize::Narrow")}
        { ASSERT_UNUSED(stream, stream[0] == opcodeID); }

        #{capitalized_name}(const uint32_t* stream)
            #{init.call("OpcodeSize::Wide")}
        { ASSERT_UNUSED(stream, stream[0] == opcodeID); }

        static #{capitalized_name} decode(const uint8_t* stream)
        {
            if (*stream != op_wide)
                return { stream };

            auto wideStream = reinterpret_cast<const uint32_t*>(stream + 1);
            return { wideStream };
        }

        EOF
    end

    def setters
        print_args(&:setter)
    end

    def metadata_struct_and_accessor
        <<-EOF
        #{@metadata.struct(self)}

        #{@metadata.accessor}
        EOF
    end

    def members
        <<-EOF
        #{print_args(&:field)}
        #{@metadata.field}
        EOF
    end

    def set_entry_address(id)
        "setEntryAddress(#{id}, _#{full_name})"
    end

    def set_entry_address_wide(id)
        "setEntryAddressWide(#{id}, _#{full_name}_wide)"
    end

    def struct_indices
        out = []
        out += @args.map(&:name) unless @args.nil?
        out << Metadata.field_name unless @metadata.empty?
        out.map.with_index do |name, index|
            "const unsigned #{capitalized_name}_#{name}_index = #{index + 1};"
        end
    end

    def full_name
        "#{@section.config[:asm_prefix]}#{@section.config[:op_prefix]}#{@name}"
    end

    def name
        "#{@section.config[:op_prefix]}#{@name}"
    end

    def length
        1 + (@args.nil? ? 0 : @args.length) + (@metadata.empty? ? 0 : 1)
    end

    def self.dump_bytecode(opcodes)
        <<-EOF
        template<typename Block>
        static void dumpBytecode(BytecodeDumper<Block>* __dumper, InstructionStream::Offset __location, const Instruction* __instruction)
        {
            switch (__instruction->opcodeID()) {
            #{opcodes.map { |op|
                <<-EOF
                case #{op.name}:
                    __instruction->as<#{op.capitalized_name}>().dump(__dumper, __location, __instruction->isWide());
                    break;
                EOF
            }.join "\n"}
            default:
                ASSERT_NOT_REACHED();
            }
        }
        EOF
    end
end
