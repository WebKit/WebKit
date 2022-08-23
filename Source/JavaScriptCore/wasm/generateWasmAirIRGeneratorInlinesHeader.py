#!/usr/bin/env python3

# Copyright (C) 2022 Apple Inc. All rights reserved.
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

from generateWasm import *
from generateSimdOpcodes import *
import optparse
import sys
import re

parser = optparse.OptionParser(usage="usage: %prog <wasm.json> <WasmOps.h>")
(options, args) = parser.parse_args(sys.argv[0:])
if len(args) != 3:
    parser.error(parser.usage)

jsonPath = args[1]
jsonDirectory = os.path.dirname(os.path.abspath(jsonPath))
headerFile = open(args[2], "w")

contents = ""
opcodes = simdOpcodes(jsonDirectory)

def isValidFormArgs(hasImm, hasSimdInfo, op):
    s = op["airOp"] + ", "
    if hasImm:
        s = s + "Arg::Imm, "

    # these are used for conds
    if "extraAirArg" in op:
        cond = op["extraAirArg"]
        cond = cond[0 : cond.index("(")]
        if "Arg::doubleCond" in cond:
            s = s + "Arg::DoubleCond, "
        elif "Arg::relCond" in cond:
            s = s + "Arg::RelCond, "
        else:
            print(cond)
            assert(0)

    if hasSimdInfo:
        s = s + "Arg::SimdInfo, "

    for x in op["parameter"]:
        s = s + "Arg::Tmp, "
    # Return
    s = s + "Arg::Tmp"

    return s

def callArgs(hasImm, hasSimdInfo, op):
    s =  op['airOp'] + ", "
    if hasImm:
        s = s + "Arg::imm(imm), "

    if "extraAirArg" in op:
        s = s + op["extraAirArg"] + ", "

    if hasSimdInfo:
        s = s + "Arg::simdInfo(SimdInfo { SimdLane::" + op["simd_lane_interpretation"] + ", SimdSignMode::" + op["simd_sign_extend_mode"] + " }), "

    i = 0
    for x in op["parameter"]:
        s = s + "arg" + str(i) + ", "
        i = i + 1
    s = s + "result"
    return s


for key in opcodes:
    op = opcodes[key]
    if op["is_simd_special_op"]:
        continue
    

    hasImm = len(op["immediate"]) != 0
    assert(len(op["parameter"]))
    assert(len(op["return"]) == 1)

    s = ""
    s = s + functionGeneratorDecl(key, op, "AirIRGenerator")
    s = s + f"    result = tmpForType(Types::{cppType(op['return'][0])});\n"

    s = s + "    if (isValidForm(" + isValidFormArgs(hasImm, False, op) + ")) {\n"
    s = s + "        append(" + callArgs(hasImm, False, op) + ");\n"
    s = s + "        return { };"
    s = s + "\n    }\n"

    s = s + "    if (isValidForm(" + isValidFormArgs(hasImm, True, op) + ")) {\n"
    s = s + "        append(" + callArgs(hasImm, True, op) + ");\n"
    s = s + "        return { };"
    s = s + "\n    }\n"

    s = s + "    RELEASE_ASSERT_NOT_REACHED();\n"
    s = s + "    return { };"

    s = s + "\n}"

    contents = contents + s + "\n\n" 

contents = """/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// DO NO EDIT! - This file was generated by generateWasmAirIRGeneratorInlinesHeader.py

""" + contents + "\n\n"


headerFile.write(contents)
headerFile.close()
