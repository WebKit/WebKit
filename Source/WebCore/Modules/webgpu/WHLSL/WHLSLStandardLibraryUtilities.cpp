/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WHLSLStandardLibraryUtilities.h"

#if ENABLE(WEBGPU)

#include "WHLSLCallExpression.h"
#include "WHLSLParser.h"
#include "WHLSLStandardLibrary.h"
#include "WHLSLStandardLibraryFunctionMap.h"
#include "WHLSLVisitor.h"
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <pal/Gunzip.h>

namespace WebCore {

namespace WHLSL {

static String decompressAndDecodeStandardLibrary()
{
    auto decompressedStandardLibrary = gunzip(WHLSLStandardLibrary, sizeof(WHLSLStandardLibrary));
    return String::fromUTF8(decompressedStandardLibrary.data(), decompressedStandardLibrary.size());
}

class NameFinder : public Visitor {
public:
    HashSet<String> takeFunctionNames()
    {
        HashSet<String> result;
        std::swap(result, m_functionNames);
        return result;
    }

private:
    void visit(AST::CallExpression& callExpression) override
    {
        m_functionNames.add(callExpression.name());
        Visitor::visit(callExpression);
    }

    void visit(AST::PropertyAccessExpression& propertyAccessExpression) override
    {
        m_functionNames.add(propertyAccessExpression.getterFunctionName());
        m_functionNames.add(propertyAccessExpression.setterFunctionName());
        m_functionNames.add(propertyAccessExpression.anderFunctionName());
        Visitor::visit(propertyAccessExpression);
    }

    HashSet<String> m_functionNames;
};

void includeStandardLibrary(Program& program, Parser& parser)
{
    static NeverDestroyed<String> standardLibrary(decompressAndDecodeStandardLibrary());
    static NeverDestroyed<HashMap<String, SubstringLocation>> standardLibraryFunctionMap(computeStandardLibraryFunctionMap());

    auto stringView = StringView(standardLibrary.get()).substring(0, firstFunctionOffsetInStandardLibrary());
    auto parseFailure = parser.parse(program, stringView, Parser::Mode::StandardLibrary);
    ASSERT_UNUSED(parseFailure, !parseFailure);

    NameFinder nameFinder;
    nameFinder.Visitor::visit(program);
    HashSet<String> functionNames = nameFinder.takeFunctionNames();
    HashSet<String> allFunctionNames;
    // The name of a call to a cast operator is the name of the type, so we can't match them up correctly.
    // Instead, just include all casting operators.
    functionNames.add("operator cast"_str);
    while (!functionNames.isEmpty()) {
        auto nativeFunctionDeclarationsCount = program.nativeFunctionDeclarations().size();
        auto functionDefinitionsCount = program.functionDefinitions().size();
        for (const auto& name : functionNames) {
            if (allFunctionNames.contains(name))
                continue;
            auto iterator = standardLibraryFunctionMap.get().find(name);
            if (iterator == standardLibraryFunctionMap.get().end())
                continue;
            auto stringView = StringView(standardLibrary.get()).substring(iterator->value.start, iterator->value.end - iterator->value.start);
            auto parseFailure = parser.parse(program, stringView, Parser::Mode::StandardLibrary);
            ASSERT_UNUSED(parseFailure, !parseFailure);
            allFunctionNames.add(name);
        }
        for ( ; nativeFunctionDeclarationsCount < program.nativeFunctionDeclarations().size(); ++nativeFunctionDeclarationsCount)
            nameFinder.Visitor::visit(program.nativeFunctionDeclarations()[nativeFunctionDeclarationsCount]);
        for ( ; functionDefinitionsCount < program.functionDefinitions().size(); ++functionDefinitionsCount)
            nameFinder.Visitor::visit(program.functionDefinitions()[functionDefinitionsCount]);
        functionNames = nameFinder.takeFunctionNames();
    }
}

} // namespace WHLSL

} // namespace WebCore

#endif
