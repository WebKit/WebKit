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

class Argument
    attr_reader :name
    attr_reader :index

    def initialize(name, type, index)
        @optional = name[-1] == "?"
        @name = @optional ? name[0...-1] : name
        @type = type
        @index = index
    end

    def field
        "#{@type.to_s} #{field_name};"
    end

    def create_param
        "#{@type.to_s} #{@name}"
    end

    def create_reference_param
        "#{@type.to_s}& #{@name}"
    end

    def field_name
        "m_#{@name}"
    end

    def fits_check(size)
        Fits::check size, @name, @type
    end

    def fits_write(size)
        Fits::write size, @name, @type
    end

    def assert_fits(size)
        "ASSERT((#{fits_check size}));"
    end

    def load_from_stream(index, size)
        "#{field_name}(#{Fits::convert(size, "stream[#{index}]", @type)})"
    end

    def setter
        <<-EOF
    template<typename Functor>
    void set#{capitalized_name}(#{@type.to_s} value, Functor func)
    {
        if (isWide32())
            set#{capitalized_name}<OpcodeSize::Wide32>(value, func);
        else if (isWide16())
            set#{capitalized_name}<OpcodeSize::Wide16>(value, func);
        else
            set#{capitalized_name}<OpcodeSize::Narrow>(value, func);
    }

    template <OpcodeSize size, typename Functor>
    void set#{capitalized_name}(#{@type.to_s} value, Functor func)
    {
        if (!#{Fits::check "size", "value", @type})
            value = func();
        auto* stream = bitwise_cast<typename TypeBySize<size>::unsignedType*>(reinterpret_cast<uint8_t*>(this) + #{@index} * size + PaddingBySize<size>::value);
        *stream = #{Fits::convert "size", "value", @type};
    }
EOF
    end

    def capitalized_name
        @capitalized_name ||= @name.to_s.split('_').map do |word|
            letters = word.split('')
            letters.first.upcase!
            letters.join
        end.join
    end
end
