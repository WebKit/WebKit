#!/usr/bin/env ruby

# Copyright (C) 2013 Apple Inc. All rights reserved.
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

class Reference
    attr_reader :kind, :number
    
    def initialize(kind, number)
        @kind = kind
        @number = number
    end
    
    def resolve(hashTable, bangTable)
        case @kind
        when "#"
            result = hashTable[@number]
        when "!"
            result = bangTable[@number]
        else
            raise
        end
        raise unless result
        result
    end
end

def parse(string)
    result = []
    until string.empty?
        before, match, string = string.partition(/[!#]([0-9]+)/)
        result << before
        if match.empty?
            result << string
            break
        end
        result << Reference.new(match[0..0], match[1..-1].to_i)
    end
    result
end

class MetaData
    attr_reader :index, :name, :parent
    
    def initialize(index, name, parent)
        @index = index
        @name = name
        @parent = parent
    end
end

$definitions = []
$declarations = {}
$attributes = []
$metaData = {}
$attributesBackMap = {}
$count = 0

loop {
    line = $stdin.readline
    if line =~ /^; NOTE: THIS IS A COMBINED MODULE/
        puts line
        puts $stdin.read
        exit 0
    end
    break if line =~ /^define/
}

puts "; NOTE: THIS IS A COMBINED MODULE"

# Loop over all definitions.
shouldContinue = true
while shouldContinue
    # We're starting a new definition.
    body = ""
    loop {
        line = $stdin.readline
        break if line.chomp == "}"
        body += line
    }
    
    body = parse(body)
    
    declarations=[]
    metaDataMap=[]
    attributeMap = []
    unresolvedMetaData = []
    
    loop {
        line = $stdin.gets
        
        if not line
            shouldContinue = false
            break
        elsif line =~ /^define/
            break
        elsif line =~ /^declare/
            declarations << parse(line)
        elsif line =~ /!([0-9]+) = metadata !{metadata !\"([a-zA-Z0-9_]+)\"}/
            index = $1.to_i
            name = $2
            unless $metaData[name]
                $metaData[name] = MetaData.new($metaData.size, name, nil)
            end
            metaDataMap[index] = $metaData[$2].index
        elsif line =~ /!([0-9]+) = metadata !{metadata !\"([a-zA-Z0-9_]+)\", metadata !([0-9]+)/
            metaData = MetaData.new($1.to_i, $2, $3.to_i)
            unresolvedMetaData << metaData
        elsif line =~ /attributes #([0-9]+) = /
            attributeNumber = $1.to_i
            attributeBody = $~.post_match
            if $attributesBackMap[attributeBody]
                attributeMap[attributeNumber] = $attributesBackMap[attributeBody]
            else
                attributeMap[attributeNumber] = $attributes.size
                $attributesBackMap[attributeBody] = $attributes.size
                $attributes << attributeBody
            end
        end
    }
    
    # Iteratively resolve meta-data references
    until unresolvedMetaData.empty?
        index = 0
        while index < unresolvedMetaData.size
            metaData = unresolvedMetaData[index]
            if $metaData[metaData.name]
                metaDataMap[metaData.index] = $metaData[metaData.name].index
                unresolvedMetaData[index] = unresolvedMetaData[-1]
                unresolvedMetaData.pop
            elsif metaDataMap[metaData.parent]
                metaDataMap[metaData.index] = $metaData.size
                $metaData[metaData.name] = MetaData.new($metaData.size, metaData.name, metaDataMap[metaData.parent])
                unresolvedMetaData[index] = unresolvedMetaData[-1]
                unresolvedMetaData.pop
            else
                index += 1
            end
        end
    end
    
    # Output the body with all of the things remapped.
    puts "define i64 @jsBody_#{$count += 1}(i64) {"
    body.each {
        | thing |
        if thing.is_a? Reference
            print(thing.kind + thing.resolve(attributeMap, metaDataMap).to_s)
        else
            print(thing)
        end
    }
    puts "}"
    
    # Figure out what to do with declarations.
    declarations.each {
        | declaration |
        declaration = declaration.map {
            | thing |
            if thing.is_a? Reference
                thing.kind + thing.resolve(attributeMap, metaDataMap).to_s
            else
                thing
            end
        }
        declaration = declaration.join('')
        
        next if $declarations[declaration]
        
        $declarations[declaration] = true
    }
end

$declarations.each_key {
    | declaration |
    puts declaration
}

$attributes.each_with_index {
    | attribute, index |
    puts "attributes ##{index} = #{attribute}"
}

$metaData.each_value {
    | metaData |
    print "!#{metaData.index} = metadata !{metadata !\"#{metaData.name}\""
    if metaData.parent
        print ", metadata !#{metaData.parent}"
    end
    puts "}"
}

