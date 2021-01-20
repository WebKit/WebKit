/*
 *  Copyright (C) 1999-2001, 2004 Harri Porten (porten@kde.org)
 *  Copyright (c) 2007-2020 Apple Inc. All rights reserved.
 *  Copyright (C) 2009 Torch Mobile, Inc.
 *  Copyright (C) 2010 Peter Varga (pvarga@inf.u-szeged.hu), University of Szeged
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "RegExp.h"

#include "RegExpCache.h"
#include "RegExpInlines.h"
#include "YarrJIT.h"
#include <wtf/Assertions.h>

namespace JSC {

const ClassInfo RegExp::s_info = { "RegExp", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(RegExp) };

#if REGEXP_FUNC_TEST_DATA_GEN
const char* const RegExpFunctionalTestCollector::s_fileName = "/tmp/RegExpTestsData";
RegExpFunctionalTestCollector* RegExpFunctionalTestCollector::s_instance = 0;

RegExpFunctionalTestCollector* RegExpFunctionalTestCollector::get()
{
    if (!s_instance)
        s_instance = new RegExpFunctionalTestCollector();

    return s_instance;
}

void RegExpFunctionalTestCollector::outputOneTest(RegExp* regExp, const String& s, int startOffset, int* ovector, int result)
{
    if ((!m_lastRegExp) || (m_lastRegExp != regExp)) {
        m_lastRegExp = regExp;
        fputc('/', m_file);
        outputEscapedString(regExp->pattern(), true);
        fputc('/', m_file);
        if (regExp->global())
            fputc('g', m_file);
        if (regExp->ignoreCase())
            fputc('i', m_file);
        if (regExp->multiline())
            fputc('m', m_file);
        if (regExp->dotAll())
            fputc('s', m_file);
        if (regExp->unicode())
            fputc('u', m_file);
        if (regExp->sticky())
            fputc('y', m_file);
        fprintf(m_file, "\n");
    }

    fprintf(m_file, " \"");
    outputEscapedString(s);
    fprintf(m_file, "\", %d, %d, (", startOffset, result);
    for (unsigned i = 0; i <= regExp->numSubpatterns(); i++) {
        int subpatternBegin = ovector[i * 2];
        int subpatternEnd = ovector[i * 2 + 1];
        if (subpatternBegin == -1)
            subpatternEnd = -1;
        fprintf(m_file, "%d, %d", subpatternBegin, subpatternEnd);
        if (i < regExp->numSubpatterns())
            fputs(", ", m_file);
    }

    fprintf(m_file, ")\n");
    fflush(m_file);
}

RegExpFunctionalTestCollector::RegExpFunctionalTestCollector()
{
    m_file = fopen(s_fileName, "r+");
    if  (!m_file)
        m_file = fopen(s_fileName, "w+");

    fseek(m_file, 0L, SEEK_END);
}

RegExpFunctionalTestCollector::~RegExpFunctionalTestCollector()
{
    fclose(m_file);
    s_instance = 0;
}

void RegExpFunctionalTestCollector::outputEscapedString(const String& s, bool escapeSlash)
{
    int len = s.length();
    
    for (int i = 0; i < len; ++i) {
        UChar c = s[i];

        switch (c) {
        case '\0':
            fputs("\\0", m_file);
            break;
        case '\a':
            fputs("\\a", m_file);
            break;
        case '\b':
            fputs("\\b", m_file);
            break;
        case '\f':
            fputs("\\f", m_file);
            break;
        case '\n':
            fputs("\\n", m_file);
            break;
        case '\r':
            fputs("\\r", m_file);
            break;
        case '\t':
            fputs("\\t", m_file);
            break;
        case '\v':
            fputs("\\v", m_file);
            break;
        case '/':
            if (escapeSlash)
                fputs("\\/", m_file);
            else
                fputs("/", m_file);
            break;
        case '\"':
            fputs("\\\"", m_file);
            break;
        case '\\':
            fputs("\\\\", m_file);
            break;
        case '\?':
            fputs("\?", m_file);
            break;
        default:
            if (c > 0x7f)
                fprintf(m_file, "\\u%04x", c);
            else 
                fputc(c, m_file);
            break;
        }
    }
}
#endif

RegExp::RegExp(VM& vm, const String& patternString, OptionSet<Yarr::Flags> flags)
    : JSCell(vm, vm.regExpStructure.get())
    , m_patternString(patternString)
    , m_flags(flags)
{
    ASSERT(m_flags != Yarr::Flags::DeletedValue);
}

void RegExp::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    Yarr::YarrPattern pattern(m_patternString, m_flags, m_constructionErrorCode);
    if (!isValid()) {
        m_state = ParseError;
        return;
    }

    m_numSubpatterns = pattern.m_numSubpatterns;
    if (!pattern.m_captureGroupNames.isEmpty() || !pattern.m_namedGroupToParenIndex.isEmpty()) {
        m_rareData = makeUnique<RareData>();
        m_rareData->m_captureGroupNames.swap(pattern.m_captureGroupNames);
        m_rareData->m_namedGroupToParenIndex.swap(pattern.m_namedGroupToParenIndex);
    }
}

void RegExp::destroy(JSCell* cell)
{
    RegExp* thisObject = static_cast<RegExp*>(cell);
#if REGEXP_FUNC_TEST_DATA_GEN
    RegExpFunctionalTestCollector::get()->clearRegExp(this);
#endif
    thisObject->RegExp::~RegExp();
}

size_t RegExp::estimatedSize(JSCell* cell, VM& vm)
{
    RegExp* thisObject = static_cast<RegExp*>(cell);
    size_t regexDataSize = thisObject->m_regExpBytecode ? thisObject->m_regExpBytecode->estimatedSizeInBytes() : 0;
#if ENABLE(YARR_JIT)
    if (auto* jitCode = thisObject->m_regExpJITCode.get())
        regexDataSize += jitCode->size();
#endif
    return Base::estimatedSize(cell, vm) + regexDataSize;
}

RegExp* RegExp::createWithoutCaching(VM& vm, const String& patternString, OptionSet<Yarr::Flags> flags)
{
    RegExp* regExp = new (NotNull, allocateCell<RegExp>(vm.heap)) RegExp(vm, patternString, flags);
    regExp->finishCreation(vm);
    return regExp;
}

RegExp* RegExp::create(VM& vm, const String& patternString, OptionSet<Yarr::Flags> flags)
{
    return vm.regExpCache()->lookupOrCreate(patternString, flags);
}


static std::unique_ptr<Yarr::BytecodePattern> byteCodeCompilePattern(VM* vm, Yarr::YarrPattern& pattern, Yarr::ErrorCode& errorCode)
{
    return Yarr::byteCompile(pattern, &vm->m_regExpAllocator, errorCode, &vm->m_regExpAllocatorLock);
}

void RegExp::byteCodeCompileIfNecessary(VM* vm)
{
    if (m_regExpBytecode)
        return;

    Yarr::YarrPattern pattern(m_patternString, m_flags, m_constructionErrorCode);
    if (hasError(m_constructionErrorCode)) {
        m_state = ParseError;
        return;
    }
    ASSERT(m_numSubpatterns == pattern.m_numSubpatterns);

    m_regExpBytecode = byteCodeCompilePattern(vm, pattern, m_constructionErrorCode);
    if (!m_regExpBytecode) {
        m_state = ParseError;
        return;
    }
}

void RegExp::compile(VM* vm, Yarr::YarrCharSize charSize)
{
    auto locker = holdLock(cellLock());
    
    Yarr::YarrPattern pattern(m_patternString, m_flags, m_constructionErrorCode);
    if (hasError(m_constructionErrorCode)) {
        m_state = ParseError;
        return;
    }
    ASSERT(m_numSubpatterns == pattern.m_numSubpatterns);

    if (!hasCode()) {
        ASSERT(m_state == NotCompiled);
        vm->regExpCache()->addToStrongCache(this);
        m_state = ByteCode;
    }

#if ENABLE(YARR_JIT)
    if (!pattern.containsUnsignedLengthPattern() && Options::useRegExpJIT()
#if !ENABLE(YARR_JIT_BACKREFERENCES)
        && !pattern.m_containsBackreferences
#endif
        ) {
        auto& jitCode = ensureRegExpJITCode();
        Yarr::jitCompile(pattern, m_patternString, charSize, vm, jitCode);
        if (!jitCode.failureReason()) {
            m_state = JITCode;
            return;
        }
    }
#else
    UNUSED_PARAM(charSize);
#endif

    if (Options::dumpCompiledRegExpPatterns())
        dataLog("Can't JIT this regular expression: \"", m_patternString, "\"\n");

    m_state = ByteCode;
    m_regExpBytecode = byteCodeCompilePattern(vm, pattern, m_constructionErrorCode);
    if (!m_regExpBytecode) {
        m_state = ParseError;
        return;
    }
}

int RegExp::match(JSGlobalObject* globalObject, const String& s, unsigned startOffset, Vector<int>& ovector)
{
    return matchInline(globalObject, globalObject->vm(), s, startOffset, ovector);
}

bool RegExp::matchConcurrently(
    VM& vm, const String& s, unsigned startOffset, int& position, Vector<int>& ovector)
{
    auto locker = holdLock(cellLock());

    if (!hasCodeFor(s.is8Bit() ? Yarr::Char8 : Yarr::Char16))
        return false;

    position = matchInline<Vector<int>&, Yarr::MatchFrom::CompilerThread>(nullptr, vm, s, startOffset, ovector);
    if (m_state == ParseError)
        return false;
    return true;
}

void RegExp::compileMatchOnly(VM* vm, Yarr::YarrCharSize charSize)
{
    auto locker = holdLock(cellLock());
    
    Yarr::YarrPattern pattern(m_patternString, m_flags, m_constructionErrorCode);
    if (hasError(m_constructionErrorCode)) {
        m_state = ParseError;
        return;
    }
    ASSERT(m_numSubpatterns == pattern.m_numSubpatterns);

    if (!hasCode()) {
        ASSERT(m_state == NotCompiled);
        vm->regExpCache()->addToStrongCache(this);
        m_state = ByteCode;
    }

#if ENABLE(YARR_JIT)
    if (!pattern.containsUnsignedLengthPattern() && Options::useRegExpJIT()
#if !ENABLE(YARR_JIT_BACKREFERENCES)
        && !pattern.m_containsBackreferences
#endif
        ) {
        auto& jitCode = ensureRegExpJITCode();
        Yarr::jitCompile(pattern, m_patternString, charSize, vm, jitCode, Yarr::MatchOnly);
        if (!jitCode.failureReason()) {
            m_state = JITCode;
            return;
        }
    }
#else
    UNUSED_PARAM(charSize);
#endif

    if (Options::dumpCompiledRegExpPatterns())
        dataLog("Can't JIT this regular expression: \"", m_patternString, "\"\n");

    m_state = ByteCode;
    m_regExpBytecode = byteCodeCompilePattern(vm, pattern, m_constructionErrorCode);
    if (!m_regExpBytecode) {
        m_state = ParseError;
        return;
    }
}

MatchResult RegExp::match(JSGlobalObject* globalObject, const String& s, unsigned startOffset)
{
    return matchInline(globalObject, globalObject->vm(), s, startOffset);
}

bool RegExp::matchConcurrently(VM& vm, const String& s, unsigned startOffset, MatchResult& result)
{
    auto locker = holdLock(cellLock());

    if (!hasMatchOnlyCodeFor(s.is8Bit() ? Yarr::Char8 : Yarr::Char16))
        return false;

    result = matchInline<Yarr::MatchFrom::CompilerThread>(nullptr, vm, s, startOffset);
    return true;
}

void RegExp::deleteCode()
{
    auto locker = holdLock(cellLock());
    
    if (!hasCode())
        return;
    m_state = NotCompiled;
#if ENABLE(YARR_JIT)
    if (m_regExpJITCode)
        m_regExpJITCode->clear();
#endif
    m_regExpBytecode = nullptr;
}

#if ENABLE(YARR_JIT_DEBUG)
void RegExp::matchCompareWithInterpreter(const String& s, int startOffset, int* offsetVector, int jitResult)
{
    int offsetVectorSize = (m_numSubpatterns + 1) * 2;
    Vector<int> interpreterOvector;
    interpreterOvector.resize(offsetVectorSize);
    int* interpreterOffsetVector = interpreterOvector.data();
    int interpreterResult = 0;
    int differences = 0;

    // Initialize interpreterOffsetVector with the return value (index 0) and the 
    // first subpattern start indicies (even index values) set to -1.
    // No need to init the subpattern end indicies.
    for (unsigned j = 0, i = 0; i < m_numSubpatterns + 1; j += 2, i++)
        interpreterOffsetVector[j] = -1;

    interpreterResult = Yarr::interpret(m_regExpBytecode.get(), s, startOffset, reinterpret_cast<unsigned*>(interpreterOffsetVector));

    if (jitResult != interpreterResult)
        differences++;

    for (unsigned j = 2, i = 0; i < m_numSubpatterns; j +=2, i++)
        if ((offsetVector[j] != interpreterOffsetVector[j])
            || ((offsetVector[j] >= 0) && (offsetVector[j+1] != interpreterOffsetVector[j+1])))
            differences++;

    if (differences) {
        dataLogF("RegExp Discrepency for /%s/\n    string input ", pattern().utf8().data());
        unsigned segmentLen = s.length() - static_cast<unsigned>(startOffset);

        dataLogF((segmentLen < 150) ? "\"%s\"\n" : "\"%148s...\"\n", s.utf8().data() + startOffset);

        if (jitResult != interpreterResult) {
            dataLogF("    JIT result = %d, interpreted result = %d\n", jitResult, interpreterResult);
            differences--;
        } else {
            dataLogF("    Correct result = %d\n", jitResult);
        }

        if (differences) {
            for (unsigned j = 2, i = 0; i < m_numSubpatterns; j +=2, i++) {
                if (offsetVector[j] != interpreterOffsetVector[j])
                    dataLogF("    JIT offset[%d] = %d, interpreted offset[%d] = %d\n", j, offsetVector[j], j, interpreterOffsetVector[j]);
                if ((offsetVector[j] >= 0) && (offsetVector[j+1] != interpreterOffsetVector[j+1]))
                    dataLogF("    JIT offset[%d] = %d, interpreted offset[%d] = %d\n", j+1, offsetVector[j+1], j+1, interpreterOffsetVector[j+1]);
            }
        }
    }
}
#endif

#if ENABLE(REGEXP_TRACING)
    void RegExp::printTraceData()
    {
        char formattedPattern[41];
        char rawPattern[41];

        strncpy(rawPattern, pattern().utf8().data(), 40);
        rawPattern[40]= '\0';

        int pattLen = strlen(rawPattern);

        snprintf(formattedPattern, 41, (pattLen <= 38) ? "/%.38s/" : "/%.36s...", rawPattern);

#if ENABLE(YARR_JIT)
        const size_t jitAddrSize = 20;
        char jit8BitMatchOnlyAddr[jitAddrSize] { };
        char jit16BitMatchOnlyAddr[jitAddrSize] { };
        char jit8BitMatchAddr[jitAddrSize] { };
        char jit16BitMatchAddr[jitAddrSize] { };
        switch (m_state) {
        case ParseError:
        case NotCompiled:
            break;
        case ByteCode:
            snprintf(jit8BitMatchOnlyAddr, jitAddrSize, "fallback    ");
            snprintf(jit16BitMatchOnlyAddr, jitAddrSize, "----      ");
            snprintf(jit8BitMatchAddr, jitAddrSize, "fallback    ");
            snprintf(jit16BitMatchAddr, jitAddrSize, "----      ");
            break;
        case JITCode: {
            Yarr::YarrCodeBlock& codeBlock = *m_regExpJITCode.get();
            snprintf(jit8BitMatchOnlyAddr, jitAddrSize, "0x%014lx", reinterpret_cast<uintptr_t>(codeBlock.get8BitMatchOnlyAddr()));
            snprintf(jit16BitMatchOnlyAddr, jitAddrSize, "0x%014lx", reinterpret_cast<uintptr_t>(codeBlock.get16BitMatchOnlyAddr()));
            snprintf(jit8BitMatchAddr, jitAddrSize, "0x%014lx", reinterpret_cast<uintptr_t>(codeBlock.get8BitMatchAddr()));
            snprintf(jit16BitMatchAddr, jitAddrSize, "0x%014lx", reinterpret_cast<uintptr_t>(codeBlock.get16BitMatchAddr()));
            break;
        }
        }
#else
        const char* jit8BitMatchOnlyAddr = "JIT Off";
        const char* jit16BitMatchOnlyAddr = "";
        const char* jit8BitMatchAddr = "JIT Off";
        const char* jit16BitMatchAddr = "";
#endif
        unsigned averageMatchOnlyStringLen = (unsigned)(m_rtMatchOnlyTotalSubjectStringLen / m_rtMatchOnlyCallCount);
        unsigned averageMatchStringLen = (unsigned)(m_rtMatchTotalSubjectStringLen / m_rtMatchCallCount);

        printf("%-40.40s %16.16s %16.16s %10d %10d %10u\n", formattedPattern, jit8BitMatchOnlyAddr, jit16BitMatchOnlyAddr, m_rtMatchOnlyCallCount, m_rtMatchOnlyFoundCount, averageMatchOnlyStringLen);
        printf("                                         %16.16s %16.16s %10d %10d %10u\n", jit8BitMatchAddr, jit16BitMatchAddr, m_rtMatchCallCount, m_rtMatchFoundCount, averageMatchStringLen);
    }
#endif

static CString regexpToSourceString(const RegExp* regExp)
{
    char postfix[7] = { '/', 0, 0, 0, 0, 0, 0 };
    int index = 1;
    if (regExp->global())
        postfix[index++] = 'g';
    if (regExp->ignoreCase())
        postfix[index++] = 'i';
    if (regExp->multiline())
        postfix[index] = 'm';
    if (regExp->dotAll())
        postfix[index++] = 's';
    if (regExp->unicode())
        postfix[index++] = 'u';
    if (regExp->sticky())
        postfix[index++] = 'y';

    return toCString("/", regExp->pattern().impl(), postfix);
}

void RegExp::dumpToStream(const JSCell* cell, PrintStream& out)
{
    out.print(regexpToSourceString(jsCast<const RegExp*>(cell)));
}

} // namespace JSC
