# Copyright (C) 2011-2019 Apple Inc. All rights reserved.
# Copyright (C) 2012 Sony Network Entertainment. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import sys
import string
import operator

keywordsText = open(sys.argv[1]).read()

# A second argument signifies that the output
# should be redirected to a file
redirect_to_file = len(sys.argv) > 2

# Change stdout to point to the file if requested
if redirect_to_file:
    file_output = open(sys.argv[-1], "w")
    sys.stdout = file_output

# Observed weights of the most common keywords, rounded to 2.s.d
keyWordWeights = {
    "catch": 0.01,
    "try": 0.01,
    "while": 0.01,
    "case": 0.01,
    "break": 0.01,
    "new": 0.01,
    "in": 0.01,
    "typeof": 0.02,
    "true": 0.02,
    "false": 0.02,
    "for": 0.03,
    "null": 0.03,
    "else": 0.03,
    "return": 0.13,
    "var": 0.13,
    "if": 0.16,
    "function": 0.18,
    "this": 0.18,
}


def allWhitespace(str):
    for c in str:
        if not(c in string.whitespace):
            return False
    return True


def parseKeywords(keywordsText):

    if sys.platform == "cygwin":
        keywordsText = keywordsText.replace("\r\n", "\n")

    lines = keywordsText.split("\n")
    lines = [line.split("#")[0] for line in lines]
    lines = [line for line in lines if (not allWhitespace(line))]
    name = lines[0].split()
    terminator = lines[-1]

    if not name[0] == "@begin":
        raise Exception("expected description beginning with @begin")
    if not terminator == "@end":
        raise Exception("expected description ending with @end")

    lines = lines[1:-1]  # trim off the old heading
    return [line.split() for line in lines]


def makePadding(size):
    str = ""
    for i in range(size):
        str = str + " "
    return str


class Trie:
    def __init__(self, prefix):
        self.prefix = prefix
        self.keys = {}
        self.value = None

    def insert(self, key, value):
        if len(key) == 0:
            self.value = value
            return
        if not (key[0] in self.keys):
            self.keys[key[0]] = Trie(key[0])
        self.keys[key[0]].insert(key[1:], value)

    def coalesce(self):
        keys = {}
        for k, v in self.keys.items():
            t = v.coalesce()
            keys[t.prefix] = t
        self.keys = keys
        if self.value != None:
            return self
        if len(self.keys) != 1:
            return self
        # Python 3: for() loop for compatibility. Use next() when Python 2.6 is the baseline.
        for (prefix, suffix) in self.keys.items():
            res = Trie(self.prefix + prefix)
            res.value = suffix.value
            res.keys = suffix.keys
            return res

    def fillOut(self, prefix=""):
        self.fullPrefix = prefix + self.prefix
        weight = 0
        if self.fullPrefix in keyWordWeights:
            weight = weight + keyWordWeights[self.fullPrefix]
        self.selfWeight = weight
        for trie in self.keys.values():
            trie.fillOut(self.fullPrefix)
            weight = weight + trie.weight
        self.keys = [(trie.prefix, trie) for trie in sorted(self.keys.values(), key=operator.attrgetter('weight'), reverse=True)]
        self.weight = weight

    def printSubTreeAsC(self, indent):
        str = makePadding(indent)

        if self.value != None:
            print(str + "if (!isIdentPartIncludingEscape(code + %d, m_codeEnd)) {" % (len(self.fullPrefix)))
            print(str + "    internalShift<%d>();" % len(self.fullPrefix))
            print(str + "    if (shouldCreateIdentifier)")
            print(str + ("        data->ident = &m_vm.propertyNames->%sKeyword;" % self.fullPrefix))
            print(str + "    return " + self.value + ";")
            print(str + "}")
        rootIndex = len(self.fullPrefix)
        itemCount = 0
        for k, trie in self.keys:
            baseIndex = rootIndex
            if (baseIndex > 0) and (len(k) == 3):
                baseIndex = baseIndex - 1
                k = trie.fullPrefix[baseIndex] + k
            test = [("'%s'" % c) for c in k]
            base = "code + %d" % baseIndex
            length = __builtins__.str(len(test))
            needle = "(std::array<Char, " + length + ">{{" + ", ".join(test) + "}}).data()"
            comparison = ("!memcmp(%s, " % (base)) + needle + ", " + length + " * sizeof(Char))"
            if itemCount == 0:
                print(str + "if (" + comparison + ") {")
            else:
                print(str + "} else if (" + comparison + ") {")

            trie.printSubTreeAsC(indent + 4)
            itemCount = itemCount + 1

            if itemCount == len(self.keys):
                print(str + "}")

    def maxLength(self):
        max = len(self.fullPrefix)
        for (_, trie) in self.keys:
            l = trie.maxLength()
            if l > max:
                max = l
        return max

    def printAsC(self):
        print("namespace JSC {")
        print("")
        print("static ALWAYS_INLINE bool isIdentPartIncludingEscape(const LChar* code, const LChar* codeEnd);")
        print("static ALWAYS_INLINE bool isIdentPartIncludingEscape(const UChar* code, const UChar* codeEnd);")
        # max length + 1 so we don't need to do any bounds checking at all
        print("static const int maxTokenLength = %d;" % (self.maxLength() + 1))
        print("")
        print("template <typename Char>")
        print("template <bool shouldCreateIdentifier> ALWAYS_INLINE JSTokenType Lexer<Char>::parseKeyword(JSTokenData* data)")
        print("{")
        print("    ASSERT(m_codeEnd - m_code >= maxTokenLength);")
        print("")
        print("    const Char* code = m_code;")
        self.printSubTreeAsC(4)
        print("    return IDENT;")
        print("}")
        print("")
        print("} // namespace JSC")

keywords = parseKeywords(keywordsText)
trie = Trie("")
for k, v in keywords:
    trie.insert(k, v)
trie.coalesce()
trie.fillOut()
print("// This file was generated by KeywordLookupGenerator.py.  Do not edit.")

trie.printAsC()

# Close the redirected file if requested
if (redirect_to_file):
    file_output.close()
    sys.stdout = sys.__stdout__
