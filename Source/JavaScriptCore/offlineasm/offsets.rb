# Copyright (C) 2011 Apple Inc. All rights reserved.
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

require "ast"
require "offset_extractor_constants"

OFFSET_MAGIC_NUMBERS = [ 0xec577ac7, 0x0ff5e755 ]

#
# offsetsList(ast)
# sizesList(ast)
#
# Returns a list of offsets and sizes used by the AST.
#

def offsetsList(ast)
    ast.filter(StructOffset).uniq.sort
end

def sizesList(ast)
    ast.filter(Sizeof).uniq.sort
end

#
# offsetsAndConfigurationIndex(ast, file) ->
#     [offsets, index]
#
# Parses the offsets from a file and returns a list of offsets and the
# index of the configuration that is valid in this build target.
#

def offsetsAndConfigurationIndex(file)
    index = nil
    offsets = []
    endiannessMarkerBytes = nil
    
    def readInt(endianness, inp)
        bytes = []
        4.times {
            bytes << inp.getbyte
        }
        
        if endianness == :little
            # Little endian
            (bytes[0] << 0 |
             bytes[1] << 8 |
             bytes[2] << 16 |
             bytes[3] << 24)
        else
            # Big endian
            (bytes[0] << 24 |
             bytes[1] << 16 |
             bytes[2] << 8 |
             bytes[3] << 0)
        end
    end
    
    [:little, :big].each {
        | endianness |
        magicBytes = []
        MAGIC_NUMBERS.each {
            | number |
            currentBytes = []
            4.times {
                currentBytes << (number & 0xff)
                number >>= 8
            }
            if endianness == :big
                currentBytes.reverse!
            end
            magicBytes += currentBytes
        }
        
        File.open(file, "r") {
            | inp |
            whereInMarker = 0
            loop {
                byte = inp.getbyte
                break unless byte
                if byte == magicBytes[whereInMarker]
                    whereInMarker += 1
                    if whereInMarker == magicBytes.size
                        # We have a match! If we have not yet read the endianness marker and index,
                        # then read those now; otherwise read an offset.
                        
                        if not index
                            index = readInt(endianness, inp)
                        else
                            offsets << readInt(endianness, inp)
                        end
                        
                        whereInMarker = 0
                    end
                else
                    whereInMarker = 0
                end
            }
        }
        
        break if index
    }
    
    raise unless index
    
    [offsets, index]
end

#
# buildOffsetsMap(ast, offsetsList) -> [offsets, sizes]
#
# Builds a mapping between StructOffset nodes and their values.
#

def buildOffsetsMap(ast, offsetsList)
    offsetsMap = {}
    sizesMap = {}
    astOffsetsList = offsetsList(ast)
    astSizesList = sizesList(ast)
    raise unless astOffsetsList.size + astSizesList.size == offsetsList.size
    offsetsList(ast).each_with_index {
        | structOffset, index |
        offsetsMap[structOffset] = offsetsList.shift
    }
    sizesList(ast).each_with_index {
        | sizeof, index |
        sizesMap[sizeof] = offsetsList.shift
    }
    [offsetsMap, sizesMap]
end

