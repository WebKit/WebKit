# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

require_relative 'Fits'

class Metadata
    @@emitter_local = nil

    def initialize(fields, initializers)
        @fields = fields
        @initializers = initializers
    end

    def empty?
        @fields.nil?
    end

    def length
        empty? ? 0 : @fields.length
    end

    def struct(op)
        return if empty?

        def convertFields(prefix, fields)
            fields.map do |field, type|
                if type.kind_of? Hash
                    "#{prefix}union {\n#{convertFields(prefix + '    ', type)}\n#{prefix}};"
                else
                    "#{prefix}#{type.to_s} m_#{field.to_s};"
                end
            end.join("\n")
        end

        fields = convertFields("        ", @fields)

        inits = nil
        if @initializers && (not @initializers.empty?)
            inits = "\n            : " + @initializers.map do |metadata, arg|
                "m_#{metadata}(__op.m_#{arg})"
            end.join("\n            , ").concat("\n       ");
        end

        <<-EOF

    struct Metadata {
        WTF_MAKE_NONCOPYABLE(Metadata);

    public:
        Metadata(const #{op.capitalized_name}&#{" __op" if inits})#{inits} { }

#{fields}
    };
EOF
    end

    def accessor
        return if empty?

        <<-EOF

    Metadata& metadata(CodeBlock* codeBlock) const
    {
        return codeBlock->metadata<Metadata>(opcodeID, #{Metadata.field_name});
    }

    Metadata& metadata(ExecState* exec) const
    {
        return metadata(exec->codeBlock());
    }
EOF
    end

    def field(prefix)
        return if empty?

        "\n#{prefix}unsigned #{Metadata.field_name};"
    end

    def load_from_stream(index, size)
        return if empty?

        "#{Metadata.field_name}(#{Fits::convert(size, "stream[#{index}]", :unsigned)})"
    end

    def create_emitter_local
        return if empty?

        <<-EOF.chomp

        auto #{emitter_local.name} = gen->addMetadataFor(opcodeID);
        EOF
    end

    def emitter_local
        unless @@emitter_local
            @@emitter_local = Argument.new("__metadataID", :unsigned, -1)
        end

        return @@emitter_local
    end

    def self.field_name
        "m_metadataID"
    end
end
