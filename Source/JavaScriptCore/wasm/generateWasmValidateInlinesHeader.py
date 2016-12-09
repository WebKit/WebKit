#!/usr/bin/env python

# Copyright (C) 2016 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This tool has a couple of helpful macros to process Wasm files from the wasm.json.

from generateWasm import *
import optparse
import sys

parser = optparse.OptionParser(usage="usage: %prog <wasm.json> <WasmOps.h>")
(options, args) = parser.parse_args(sys.argv[0:])
if len(args) != 3:
    parser.error(parser.usage)

wasm = Wasm(args[0], args[1])
opcodes = wasm.opcodes
wasmValidateInlinesHFile = open(args[2], "w")


def cppType(name):
    result = {
        "bool": "I32",
        "addr": "I32",
        "i32": "I32",
        "i64": "I64",
        "f32": "F32",
        "f64": "F64",
    }.get(name, None)
    if result == None:
        raise ValueError("Unknown type name: " + name)
    return result


def toCpp(name):
    return wasm.toCpp(name)


def unaryMacro(name):
    op = opcodes[name]
    return """
template<> bool Validate::addOp<OpType::""" + toCpp(name) + """>(ExpressionType value, ExpressionType& result)
{
    if (value != """ + cppType(op["parameter"][0]) + """) {
        m_errorMessage = makeString(\"""" + name + """ expects the value to be of type: ", toString(""" + cppType(op["parameter"][0]) + """), " but got a value with type: ", toString(value));
        return false;
    }

    result = """ + cppType(op["return"][0]) + """;
    return true;
}
"""


def binaryMacro(name):
    op = opcodes[name]
    return """
template<> bool Validate::addOp<OpType::""" + toCpp(name) + """>(ExpressionType left, ExpressionType right, ExpressionType& result)
{
    if (left != """ + cppType(op["parameter"][0]) + """) {
        m_errorMessage = makeString(\"""" + name + """ expects the left value to be of type: ", toString(""" + cppType(op["parameter"][0]) + """), " but got a value with type: ", toString(left));
        return false;
    }

    if (right != """ + cppType(op["parameter"][1]) + """) {
        m_errorMessage = makeString(\"""" + name + """ expects the right value to be of type: ", toString(""" + cppType(op["parameter"][0]) + """), " but got a value with type: ", toString(right));
        return false;
    }

    result = """ + cppType(op["return"][0]) + """;
    return true;
}
"""

def loadMacro(name):
    op = opcodes[name]
    return """
    case LoadOpType::""" + toCpp(name) + """: {
        if (pointer != """ + cppType(op["parameter"][0]) + """) {
            m_errorMessage = makeString(\"""" + name + """ expects the pointer to be of type: ", toString(""" + cppType(op["parameter"][0]) + """), " but got a value with type: ", toString(pointer));
            return false;
        }

        result = """ + cppType(op["return"][0]) + """;
        return true;
    }"""


def storeMacro(name):
    op = opcodes[name]
    return """
    case StoreOpType::""" + toCpp(name) + """: {
        if (pointer != """ + cppType(op["parameter"][0]) + """) {
            m_errorMessage = makeString(\"""" + name + """ expects the pointer to be of type: ", toString(""" + cppType(op["parameter"][0]) + """), " but got a value with type: ", toString(pointer));
            return false;
        }

        if (value != """ + cppType(op["parameter"][1]) + """) {
            m_errorMessage = makeString(\"""" + name + """ expects the value to be of type: ", toString(""" + cppType(op["parameter"][0]) + """), " but got a value with type: ", toString(value));
            return false;
        }

        return true;
    }"""


unarySpecializations = "".join([op for op in wasm.opcodeIterator(isUnary, unaryMacro)])
binarySpecializations = "".join([op for op in wasm.opcodeIterator(isBinary, binaryMacro)])
loadCases = "".join([op for op in wasm.opcodeIterator(lambda op: op["category"] == "memory" and len(op["return"]) == 1, loadMacro)])
storeCases = "".join([op for op in wasm.opcodeIterator(lambda op: op["category"] == "memory" and len(op["return"]) == 0, storeMacro)])

contents = wasm.header + """
// This file is intended to be inlined by WasmValidate.cpp only! It should not be included elsewhere.

#pragma once

#if ENABLE(WEBASSEMBLY)

namespace JSC { namespace Wasm {

""" + unarySpecializations + binarySpecializations + """

bool Validate::load(LoadOpType op, ExpressionType pointer, ExpressionType& result, uint32_t)
{
    if (!hasMemory())
        return false;

    switch (op) {
""" + loadCases + """
    }
}

bool Validate::store(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t)
{
    if (!hasMemory())
        return false;

    switch (op) {
""" + storeCases + """
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

"""

wasmValidateInlinesHFile.write(contents)
wasmValidateInlinesHFile.close()
