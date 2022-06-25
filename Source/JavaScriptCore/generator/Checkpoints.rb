# Copyright (C) 2019 Apple Inc. All rights reserved.
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

class Checkpoint
    attr_reader :uses
    attr_reader :defs
    attr_reader :name
    attr_reader :index

    def initialize(name, index, tmps)
        @name = name
        @index = index
        @uses = tmps.filter { |(tmp, useDef)| useDef == :use }
        @defs = tmps.filter { |(tmp, useDef)| useDef == :define }
    end

    def liveness(namespace)
        EOF<--
    case #{namespace}::#{name}:
EOF
    end
end

class Checkpoints
    def initialize(opcode, tmps, checkpoints)
        @opcode = opcode
        @tmps = tmps
        @checkpoints = checkpoints.map_with_index { |(name, tmps), index| Checkpoint.new(name, index, tmps) }
    end

    def liveness
        @checkpoints.each do |checkpoint|

        end
        EOF<--
BitMap<maxNumTmps> tmpLivenessFor#{@opcode.capitalized_name}(uint8_t checkpoint)
{
    switch (checkpoint) {

    }
}
EOF
    end
end
