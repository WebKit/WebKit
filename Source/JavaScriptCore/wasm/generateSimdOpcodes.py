#!/usr/bin/env python3

# Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

# This script generates the opcode data for simd opcodes
# csv source: https://docs.google.com/spreadsheets/d/1mItAPDDyUD97QdtB9yp44utTP9vRRS9XMv0YZ_ePz9s/edit?usp=sharing

import csv
import json
import re

verbose = False

def toCpp(name):
    camelCase = re.sub(r'([^a-z0-9].)', lambda c: c.group(0)[1].upper(), name)
    CamelCase = camelCase[:1].upper() + camelCase[1:]
    return CamelCase

def cppType(type):
    if type == "bool":
        return "I32"
    def capitalizeWithNumbers(s):
        s = s.capitalize()
        m = re.compile("^\d+").search(s)
        if m:
            s = s[0:m.span()[1]] + s[m.span()[1]:].capitalize()
        return s
    return "".join(map(lambda x: capitalizeWithNumbers(x), type.split('_')))


def simdOpcodes(csvDirectory):
    ops = {}

    with open(csvDirectory + "/simd_opcodes.csv", 'r') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            res = convertRow(row)
            if res:
                ops[row["Instruction"]] = res

    if verbose:
        print(json.dumps(ops, sort_keys=True, indent=4))

    return ops

def mapSignExtend(item):
    if item == "S":
        return "Signed"
    if item == "U":
        return "Unsigned"
    return "None"

def fixupCmp(row):
    if row["category"] != "simd.cmp":
        return

    scalarType = row["scalarType"]
    isFloatingPoint = scalarType == "f64" or scalarType == "f32"
    compareOp = row["Instruction"].split(".")[-1]
    airCond = None
    b3op = None
    if isFloatingPoint:
        row["airOp"] = "CompareFloatingPointVector"
        airCond = {
            "eq": "Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered)",
            "ne": "Arg::doubleCond(MacroAssembler::DoubleNotEqualOrUnordered)",
            "lt": "Arg::doubleCond(MacroAssembler::DoubleLessThanAndOrdered)",
            "gt": "Arg::doubleCond(MacroAssembler::DoubleGreaterThanAndOrdered)",
            "le": "Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualAndOrdered)",
            "ge": "Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualAndOrdered)"
        }.get(compareOp)

        b3op = {
            "eq": "VectorEqual",
            "ne": "VectorNotEqual",
            "lt": "VectorLessThan",
            "gt": "VectorGreaterThan",
            "le": "VectorLessThanOrEqual",
            "ge": "VectorGreaterThanOrEqual"
        }.get(compareOp)
    else:
        row["airOp"] = "CompareIntegerVector"
        airCond = {
            "eq": "Arg::relCond(MacroAssembler::Equal)",
            "ne": "Arg::relCond(MacroAssembler::NotEqual)",
            "lt_s": "Arg::relCond(MacroAssembler::LessThan)",
            "lt_u": "Arg::relCond(MacroAssembler::Below)",
            "gt_s": "Arg::relCond(MacroAssembler::GreaterThan)",
            "gt_u": "Arg::relCond(MacroAssembler::Above)",
            "le_s": "Arg::relCond(MacroAssembler::LessThanOrEqual)",
            "le_u": "Arg::relCond(MacroAssembler::BelowOrEqual)",
            "ge_s": "Arg::relCond(MacroAssembler::GreaterThanOrEqual)",
            "ge_u": "Arg::relCond(MacroAssembler::AboveOrEqual)"
        }.get(compareOp)

        b3op = {
            "eq": "VectorEqual",
            "ne": "VectorNotEqual",
            "lt_s": "VectorLessThan",
            "lt_u": "VectorBelow",
            "gt_s": "VectorGreaterThan",
            "gt_u": "VectorAbove",
            "le_s": "VectorLessThanOrEqual",
            "le_u": "VectorBelowOrEqual",
            "ge_s": "VectorGreaterThanOrEqual",
            "ge_u": "VectorAboveOrEqual"
        }.get(compareOp)

    row["extraAirArg"] = "B3::Air::" + airCond
    row["b3op"] = b3op

def fixupArithOrBitwise(row):
    category = row["category"] 
    if category == "simd.cmp":
        return
    row["b3op"] = "Vector" + cppType(row["op"])
    if len(row["airOp"]) == 0:
        row["airOp"] = cppType(row["op"])
    row["airOp"] = "Vector" + row["airOp"]

def convertRow(row): 
    res = {}

    if not len(row["op"]):
        raise Exception("No op in row: " + str(row))

    fixupCmp(row)
    fixupArithOrBitwise(row)

    res["category"] = row["category"]
    res["value"] = 0xFD
    res["extendedOp"] = int(row["extendedOp"],0)
    scalarType = row["scalarType"]
    res["simd_scalar_type"] = scalarType
    res['simd_operation'] = row["op"]
    res['simd_sign_extend_mode'] = mapSignExtend(row["signExtend"])
    res['simd_lane_interpretation'] = row["interpretation"]
    res["is_simd_special_op"] = (row["special"] == 'y')

    def fixParams(p):
        p = p.strip()
        p = p.split(",")
        p = [t.strip() for t in p if t != '']
        return [fixType(t) for t in p]


    def fixType(t):
        t = t.split(":")
        t = t[-1].strip()

        if t == "v":
            return "v128"
        if t == "s":
            if scalarType not in ['void', "i32", "i64", "f32", "f64"]:
                raise Exception("invalid scalar type: " + scalarType)
            if scalarType == 'void':
                raise Exception("instruction " + str(res) + " does not have a scalar type")
            return scalarType
        if t not in ["i32", "i64", "f32", "f64"]:
            raise Exception("Bad type: " + t)
        return t


    def fixImms(p):
        p = p.strip()
        p = p.split(",")
        p = [t.strip() for t in p if t != '']
        p = [fixImm(t) for t in p]
        return [j for i in p for j in i]


    def fixImm(t):
        t = t.split(":")
        t = t[-1].strip()

        if t == "memarg":
            return [{"name": "offset", "type": "varuint32"}, {"name": "algin", "type": "varuint32"}]
        if t == "ImmByte[16]" or t == "ImmLaneIdx32[16]":
            return [{"name": "immbyte[0:8]", "type": "varuint64"}, {"name": "immbyte[8:16]", "type": "varuint64"}]
        if t in ["ImmLaneIdx2", "ImmLaneIdx4", "ImmLaneIdx8", "ImmLaneIdx16", "ImmLaneIdx"]:
            return [{"name": "laneidx", "type": "varuint8", "laneSize": t[len("ImmLaneIdx"):]}]

        raise Exception("error: type " + t + " not found, update generateSimdOpcodes.py")


    res["immediate"] = fixImms(row["Imm"])
    res["parameter"] = fixParams(row["params"])
    res["return"] = [fixType(row["return"])] if row["return"] != '' else []

    wasmOp = cppType(res["simd_operation"]) + cppType(res["simd_lane_interpretation"])
    if res["simd_sign_extend_mode"] == "Unsigned":
        wasmOp += "U"
    elif res["simd_sign_extend_mode"] == "Signed":
        wasmOp += "S"

    res["b3op"] = row["b3op"] if "b3op" in row else cppType(res["simd_operation"])
    if not ("B3::Opcode::" in res["b3op"]):
        res["b3op"] = "B3::Opcode::" + res["b3op"]

    if not res["is_simd_special_op"]:
        res["airOp"] = "B3::Air::" + (row["airOp"] or res["b3op"])
    res["wasmOp"] = "WasmSimd" + wasmOp

    if "extraAirArg" in row:
        res["extraAirArg"] = row["extraAirArg"]

    return res


def functionGeneratorDecl(key, op, generator):
    s = ""
    s = s + """
template<>
auto """ + generator + "::addSimdOp<"

    s = s + "ExtSimdOpType::" + toCpp(key) + ">("

    assert(len(op["parameter"]))
    assert(len(op["return"]) == 1)

    hasImm = len(op["immediate"]) != 0

    if hasImm:
        assert(len(op["immediate"]) == 1)
        s = s + "uint8_t imm, "

    i = 0
    for x in op["parameter"]:
        s = s + "ExpressionType arg" + str(i) + ", "
        i = i + 1

    s = s + "ExpressionType& result) -> PartialResult\n{\n"
    return s

