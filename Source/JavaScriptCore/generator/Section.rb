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

require 'stringio'

require_relative 'Opcode'
require_relative 'OpcodeGroup'

class Section
  attr_reader :name
  attr_reader :config
  attr_reader :opcodes

  def initialize(name, config)
    @name = name
    @config = config
    @opcodes = []
    @opcode_groups = []
  end

  def add_opcode(name, config)
      @opcodes << create_opcode(name, config)
  end

  def create_opcode(name, config)
      Opcode.new(self, name, config[:extras], config[:args], config[:metadata], config[:metadata_initializers], config[:tmps], config[:checkpoints])
  end

  def add_opcode_group(name, opcodes, config)
      opcodes = opcodes.map { |opcode| create_opcode(opcode, config) }
      @opcode_groups << OpcodeGroup.new(self, name, opcodes, config)
      @opcodes += opcodes
  end

  def sort!
      @opcodes = @opcodes.sort { |a, b|
          result = nil
          if a.checkpoints or b.checkpoints
              raise "Bytecodes with checkpoints should have metadata: #{a.name}" if a.checkpoints and a.metadata.empty?
              raise "Bytecodes with checkpoints should have metadata: #{b.name}" if b.checkpoints and b.metadata.empty?
              result = a.checkpoints ? b.checkpoints ? 0 : -1 : 1
          # SIMD opcodes should always use higher bytecode ids since they are more rare.
          elsif a.name.downcase.include?("simd") or b.name.downcase.include?("simd")
              result = a.name.include?("simd") ? b.name.include?("simd") ? 0 : 1 : -1 
          else
              result = a.metadata.empty? ? b.metadata.empty? ? 0 : 1 : -1 
          end
          result
      }
      @opcodes.each(&:create_id!)
  end

  def is_wasm?
    @name == :Wasm
  end

  def header_helpers(num_opcodes)
      out = StringIO.new
      if config[:emit_in_h_file]
          out.write("#define FOR_EACH_#{config[:macro_name_component]}_ID(macro) \\\n")
          opcodes.each { |opcode| out.write("    macro(#{opcode.name}, #{opcode.length}) \\\n") }
          out << "\n"

          out.write("#define NUMBER_OF_#{config[:macro_name_component]}_IDS #{opcodes.length}\n")
          out.write("#define MAX_LENGTH_OF_#{config[:macro_name_component]}_IDS #{(opcodes.max {|a, b| a.length <=> b.length }).length}\n")
      end

      if config[:emit_in_structs_file]
          i = 0
          out.write("static constexpr unsigned #{config[:macro_name_component].downcase}CheckpointCountTable[] = {\n")
          while true
              if !opcodes[i].checkpoints
                  out << "    0, // this unused entry is needed since MSVC won't compile empty arrays\n"
                  out << "};\n\n"
                  out << "#define NUMBER_OF_#{config[:macro_name_component]}_WITH_CHECKPOINTS #{i}\n"
                  break
              end

              out.write("    #{opcodes[i].checkpoints.length},\n")
              i += 1
          end
          out << "\n"

          out.write("#define FOR_EACH_#{config[:macro_name_component]}_METADATA_SIZE(macro) \\\n")
          i = 0
          while true
              if opcodes[i].metadata.empty?
                  out << "\n"
                  out << "#define NUMBER_OF_#{config[:macro_name_component]}_WITH_METADATA #{i}\n"
                  break
              end

              out.write("    macro(sizeof(#{opcodes[i].capitalized_name}::Metadata))\\\n")
              i += 1
          end
          out << "\n"

          out.write("#define FOR_EACH_#{config[:macro_name_component]}_METADATA_ALIGNMENT(macro) \\\n")
          i = 0
          while true
              if opcodes[i].metadata.empty?
                  out << "\n"
                  break
              end

              out.write("    macro(alignof(#{opcodes[i].capitalized_name}::Metadata))\\\n")
              i += 1
          end
      end

      if config[:emit_opcode_id_string_values_in_h_file]
          opcodes.each { |opcode|
              out.write("#define #{opcode.name}_value_string \"#{opcode.id}\"\n")
          }
          opcodes.each { |opcode|
              out.write("#define #{opcode.name}_wide16_value_string \"#{num_opcodes + opcode.id}\"\n")
          }
          opcodes.each { |opcode|
              out.write("#define #{opcode.name}_wide32_value_string \"#{num_opcodes * 2 + opcode.id}\"\n")
          }
      end
      out.string
  end

    def for_each_struct
        <<-EOF
#define FOR_EACH_#{config[:macro_name_component]}_STRUCT(macro) \\
#{opcodes.map do |op|
    "    macro(#{op.capitalized_name}) \\"
end.join("\n")}
EOF
    end

end
