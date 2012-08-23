# Copyright (C) 2012 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms(with or without
# modification(are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice(this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice(this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES(INCLUDING(BUT NOT LIMITED TO(THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT(INDIRECT(INCIDENTAL(SPECIAL,
# EXEMPLARY(OR CONSEQUENTIAL DAMAGES (INCLUDING(BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE(DATA(OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY(WHETHER IN CONTRACT(STRICT LIABILITY(OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE(EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import re


class PrimitiveType:
    def __init__(self, name, unlinkedType, linkedType):
        self.name = name
        self.unlinkedType = unlinkedType
        self.linkedType = linkedType

    def __repr__(self):
        return self.name


class RegisterType(PrimitiveType):
    def __init__(self, name):
        PrimitiveType.__init__(self, name, "int", "RegisterID")

    def specialise(self, specialisation):
        return PrimitiveType("%s<%s>" % (self.name, specialisation), "int", "RegisterID")

types = {
    "int": PrimitiveType("int", "int", "int"),
    "unsigned": PrimitiveType("unsigned", "unsigned", "unsigned"),
    "bool": PrimitiveType("bool", "bool", "bool"),
    "void": PrimitiveType("void", None, "void*"),
    "Register": RegisterType("Register"),
    "OperandTypes": PrimitiveType("OperandTypes", "short", "OperandTypes"),
    "Identifier": PrimitiveType("Identifier", "int", "int"),
    "ValueProfile": PrimitiveType("ValueProfile", None, "ValueProfile*"),
    "Structure": PrimitiveType("Structure", None, "Structure*"),
    "StructureChain": PrimitiveType("Structure", None, "StructureChain*"),
    "ValuePointer": PrimitiveType("ValuePointer", None, "WriteBarrier<Unknown>*"),
    "WatchPointer": PrimitiveType("WatchPointer", None, "bool*"),
    "ArrayProfile": PrimitiveType("ArrayProfile", None, "ArrayProfile*"),
    "Label": PrimitiveType("Label", "int", "int"),
    "JSFunction": PrimitiveType("JSFunction", "UnlinkedFunctionReference", "JSFunction*"),
    "JumpTable": PrimitiveType("JumpTable", "int", "int"),
    "Function": PrimitiveType("Function", "int", "int"),
    "FunctionExpression": PrimitiveType("FunctionExpression", "int", "int"),
    "LLIntCallLinkInfo": PrimitiveType("LLIntCallLinkInfo", None, "LLIntCallLinkInfo*"),
}

parameterString = r"((void)|((?!void\s*<)\w+(\s*<\w+>)?))(\s+\w+)?"
parameterString = parameterString + r"(\s*,\s*" + parameterString + ")*"
opcodeRegex = re.compile(r"(?P<isUncached>uncached\s+)?(?P<opcodeName>\w+)\(\s*(?P<parameters>" + parameterString + ")?\s*\)")
parameterRegex = re.compile(r"(?P<baseType>\w+)\s*(<(?P<specialisation>\w+)>)?(\s+(?P<name>\w+))?")


def makeType(base, specialisation):
    result = types[base]
    if specialisation:
        result = result.specialise(specialisation)
    return result


def parseParameter(parameter):
    match = parameterRegex.match(parameter)
    return (makeType(match.group("baseType"), match.group("specialisation")), match.group("name"))


def parseOpcode(line, lineNumber):
    try:
        match = opcodeRegex.match(line)
        isUncached = match.group("isUncached") != None
        name = match.group("opcodeName").strip()
        if match.group("parameters"):
            parameters = [parameter.strip() for parameter in match.group("parameters").split(",")]
        else:
            parameters = []
        return (isUncached, name, [parseParameter(parameter) for parameter in parameters])
    except:
        print("Error on line %d" % lineNumber)
        raise


def parseOpcodes(lines):
    # Strip comments and white space
    commentStripper = re.compile("#.*")
    i = 0
    lines = [l.strip() for l in [commentStripper.sub("", l) for l in lines]]
    lines = zip(lines, range(1, len(lines) + 1))
    opcodes = [parseOpcode(opcode, line) for (opcode, line) in lines if len(opcode) != 0]
    return opcodes
