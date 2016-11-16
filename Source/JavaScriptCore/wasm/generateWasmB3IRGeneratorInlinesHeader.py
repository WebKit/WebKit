#!/usr/bin/env python

# Copyright (C) 2016 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:n
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
wasmB3IRGeneratorHFile = open(args[2], "w")


def generateSimpleCode(op):
    opcode = op["opcode"]
    b3op = opcode["b3op"]
    args = ["ExpressionType arg" + str(param) for param in range(len(opcode["parameter"]))]
    args.append("ExpressionType& result")
    params = ["arg" + str(param) for param in range(len(opcode["parameter"]))]
    return """
template<> bool B3IRGenerator::addOp<OpType::""" + wasm.toCpp(op["name"]) + ">(" + ", ".join(args) + """)
{
    result = m_currentBlock->appendNew<Value>(m_proc, B3::""" + opcode["b3op"] + ", Origin(), " + ", ".join(params) + """);
    return true;
}
"""

definitions = [generateSimpleCode(op) for op in wasm.opcodeIterator(lambda op: isSimple(op) and (isBinary(op) or isUnary(op)))]
contents = wasm.header + """

#pragma once

#if ENABLE(WEBASSEMBLY)

namespace JSC { namespace Wasm {

""" + "".join(definitions) + """

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

"""

wasmB3IRGeneratorHFile.write(contents)
wasmB3IRGeneratorHFile.close()
