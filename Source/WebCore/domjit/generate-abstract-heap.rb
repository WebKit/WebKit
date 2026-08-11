#!/usr/bin/env ruby
# -*- coding: utf-8 -*-
# Copyright (C) 2016 Apple Inc. All rights reserved.
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

require "yaml"

class HeapRange
    attr_reader :first, :last
    def initialize(first, last)
        @first = first
        @last = last
    end
end

class AbstractHeap
    attr_reader :range, :name, :parent
    def initialize(name, tree)
        @name = name
        @parent = nil
        if tree.nil?
            @children = []
        else
            @children = tree.map {|key, value| AbstractHeap.new(key, value) }
        end
        @range = nil
    end

    def setParent(parent)
        parent.children.push(self)
        @parent = parent
    end

    def compute(start)
        current = start
        if @children.empty?
            @range = HeapRange.new(start, current + 1)
            return
        end

        @children.each {|child|
            child.compute(current)
            current = child.range.last
        }

        @range = HeapRange.new(start, current)
    end

    def dump output
        shallowDump(output)
        if @parent
            output.print "-> "
            @parent.dump(output)
        end
    end

    def shallowDump(output)
        output.print "#{@name}<#{@range.first},#{@range.last}>"
    end

    def deepDump output, indent
        printIndent(output, indent)
        shallowDump(output)
        if @children.empty?
            output.print "\n"
            return
        end

        output.print ":\n"
        @children.each {|child|
            child.deepDump(output, indent + 1)
        }
    end

    def generate output
        output.puts "constexpr JSC::DOMJIT::HeapRange #{@name}(JSC::DOMJIT::HeapRange::ConstExpr, #{@range.first}, #{@range.last});"
        @children.each {|child|
            child.generate(output)
        }
    end

private
    def printIndent output, indent
        indent.times {
            output.print "    "
        }
    end
end

header = <<-EOS
/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// Auto-generated file. Do not modify.

#pragma once

#include <JavaScriptCore/DOMJITHeapRange.h>

namespace WebCore { namespace DOMJIT { namespace AbstractHeapRepository {
EOS

footer = <<-EOS
} } }
EOS

$inputFileName = ARGV.shift
$outputFileName = ARGV.shift
File.open($outputFileName, "w") {|output|
    File.open($inputFileName, "rb") {|file|
        tree = YAML::load(file.read())
        heap = tree.map {|key, value|
            AbstractHeap.new(key, value)
        }.first
        heap.compute(0)

        output.print(header)
        output.puts("/* DOMJIT Abstract Heap Tree.")
        heap.deepDump(output, 0)
        output.puts("*/")
        heap.generate(output)
        output.print(footer)
    }
}
