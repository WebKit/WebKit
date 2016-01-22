/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2011, 2013 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef Parser_h
#define Parser_h

#include "Debugger.h"
#include "ExceptionHelpers.h"
#include "Executable.h"
#include "JSGlobalObject.h"
#include "Lexer.h"
#include "Nodes.h"
#include "ParserArena.h"
#include "ParserError.h"
#include "ParserFunctionInfo.h"
#include "ParserTokens.h"
#include "SourceProvider.h"
#include "SourceProviderCache.h"
#include "SourceProviderCacheItem.h"
#include "VariableEnvironment.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
namespace JSC {
struct Scope;
}

namespace WTF {
template <> struct VectorTraits<JSC::Scope> : SimpleClassVectorTraits {
    static const bool canInitializeWithMemset = false; // Not all Scope data members initialize to 0.
};
}

namespace JSC {

class ExecState;
class FunctionMetadataNode;
class FunctionParameters;
class Identifier;
class VM;
class ProgramNode;
class SourceCode;

// Macros to make the more common TreeBuilder types a little less verbose
#define TreeStatement typename TreeBuilder::Statement
#define TreeExpression typename TreeBuilder::Expression
#define TreeFormalParameterList typename TreeBuilder::FormalParameterList
#define TreeSourceElements typename TreeBuilder::SourceElements
#define TreeClause typename TreeBuilder::Clause
#define TreeClauseList typename TreeBuilder::ClauseList
#define TreeArguments typename TreeBuilder::Arguments
#define TreeArgumentsList typename TreeBuilder::ArgumentsList
#define TreeFunctionBody typename TreeBuilder::FunctionBody
#define TreeClassExpression typename TreeBuilder::ClassExpression
#define TreeProperty typename TreeBuilder::Property
#define TreePropertyList typename TreeBuilder::PropertyList
#define TreeDestructuringPattern typename TreeBuilder::DestructuringPattern

COMPILE_ASSERT(LastUntaggedToken < 64, LessThan64UntaggedTokens);

enum SourceElementsMode { CheckForStrictMode, DontCheckForStrictMode };
enum FunctionBodyType { ArrowFunctionBodyExpression, ArrowFunctionBodyBlock, StandardFunctionBodyBlock };
enum FunctionRequirements { FunctionNoRequirements, FunctionNeedsName };

enum class DestructuringKind {
    DestructureToVariables,
    DestructureToLet,
    DestructureToConst,
    DestructureToCatchParameters,
    DestructureToParameters,
    DestructureToExpressions
};

enum class DeclarationType { 
    VarDeclaration, 
    LetDeclaration,
    ConstDeclaration
};

enum class DeclarationImportType {
    Imported,
    ImportedNamespace,
    NotImported
};

enum DeclarationResult {
    Valid = 0,
    InvalidStrictMode = 1 << 0,
    InvalidDuplicateDeclaration = 1 << 1
};

typedef uint8_t DeclarationResultMask;


template <typename T> inline bool isEvalNode() { return false; }
template <> inline bool isEvalNode<EvalNode>() { return true; }

struct ScopeLabelInfo {
    UniquedStringImpl* uid;
    bool isLoop;
};

ALWAYS_INLINE static bool isArguments(const VM* vm, const Identifier* ident)
{
    return vm->propertyNames->arguments == *ident;
}
ALWAYS_INLINE static bool isEval(const VM* vm, const Identifier* ident)
{
    return vm->propertyNames->eval == *ident;
}
ALWAYS_INLINE static bool isEvalOrArgumentsIdentifier(const VM* vm, const Identifier* ident)
{
    return isEval(vm, ident) || isArguments(vm, ident);
}
ALWAYS_INLINE static bool isIdentifierOrKeyword(const JSToken& token)
{
    return token.m_type == IDENT || token.m_type & KeywordTokenFlag;
}

class ModuleScopeData : public RefCounted<ModuleScopeData> {
public:
    static Ref<ModuleScopeData> create() { return adoptRef(*new ModuleScopeData); }

    const IdentifierSet& exportedBindings() const { return m_exportedBindings; }

    bool exportName(const Identifier& exportedName)
    {
        return m_exportedNames.add(exportedName.impl()).isNewEntry;
    }

    void exportBinding(const Identifier& localName)
    {
        m_exportedBindings.add(localName.impl());
    }

private:
    IdentifierSet m_exportedNames { };
    IdentifierSet m_exportedBindings { };
};

struct Scope {
    Scope(const VM* vm, bool isFunction, bool isGenerator, bool strictMode)
        : m_vm(vm)
        , m_shadowsArguments(false)
        , m_usesEval(false)
        , m_needsFullActivation(false)
        , m_hasDirectSuper(false)
        , m_needsSuperBinding(false)
        , m_allowsVarDeclarations(true)
        , m_allowsLexicalDeclarations(true)
        , m_strictMode(strictMode)
        , m_isFunction(isFunction)
        , m_isGenerator(isGenerator)
        , m_isLexicalScope(false)
        , m_isFunctionBoundary(false)
        , m_isValidStrictMode(true)
        , m_hasArguments(false)
        , m_loopDepth(0)
        , m_switchDepth(0)
    {
    }

    Scope(const Scope& rhs)
        : m_vm(rhs.m_vm)
        , m_shadowsArguments(rhs.m_shadowsArguments)
        , m_usesEval(rhs.m_usesEval)
        , m_needsFullActivation(rhs.m_needsFullActivation)
        , m_hasDirectSuper(rhs.m_hasDirectSuper)
        , m_needsSuperBinding(rhs.m_needsSuperBinding)
        , m_allowsVarDeclarations(rhs.m_allowsVarDeclarations)
        , m_allowsLexicalDeclarations(rhs.m_allowsLexicalDeclarations)
        , m_strictMode(rhs.m_strictMode)
        , m_isFunction(rhs.m_isFunction)
        , m_isGenerator(rhs.m_isGenerator)
        , m_isLexicalScope(rhs.m_isLexicalScope)
        , m_isFunctionBoundary(rhs.m_isFunctionBoundary)
        , m_isValidStrictMode(rhs.m_isValidStrictMode)
        , m_hasArguments(rhs.m_hasArguments)
        , m_loopDepth(rhs.m_loopDepth)
        , m_switchDepth(rhs.m_switchDepth)
        , m_moduleScopeData(rhs.m_moduleScopeData)
    {
        if (rhs.m_labels) {
            m_labels = std::make_unique<LabelStack>();

            typedef LabelStack::const_iterator iterator;
            iterator end = rhs.m_labels->end();
            for (iterator it = rhs.m_labels->begin(); it != end; ++it)
                m_labels->append(ScopeLabelInfo { it->uid, it->isLoop });
        }
    }

    void startSwitch() { m_switchDepth++; }
    void endSwitch() { m_switchDepth--; }
    void startLoop() { m_loopDepth++; }
    void endLoop() { ASSERT(m_loopDepth); m_loopDepth--; }
    bool inLoop() { return !!m_loopDepth; }
    bool breakIsValid() { return m_loopDepth || m_switchDepth; }
    bool continueIsValid() { return m_loopDepth; }

    void pushLabel(const Identifier* label, bool isLoop)
    {
        if (!m_labels)
            m_labels = std::make_unique<LabelStack>();
        m_labels->append(ScopeLabelInfo { label->impl(), isLoop });
    }

    void popLabel()
    {
        ASSERT(m_labels);
        ASSERT(m_labels->size());
        m_labels->removeLast();
    }

    ScopeLabelInfo* getLabel(const Identifier* label)
    {
        if (!m_labels)
            return 0;
        for (int i = m_labels->size(); i > 0; i--) {
            if (m_labels->at(i - 1).uid == label->impl())
                return &m_labels->at(i - 1);
        }
        return 0;
    }

    void setSourceParseMode(SourceParseMode mode)
    {
        switch (mode) {
        case SourceParseMode::GeneratorBodyMode:
            setIsGenerator();
            break;

        case SourceParseMode::GeneratorWrapperFunctionMode:
            setIsGeneratorFunction();
            break;

        case SourceParseMode::NormalFunctionMode:
        case SourceParseMode::GetterMode:
        case SourceParseMode::SetterMode:
        case SourceParseMode::MethodMode:
        case SourceParseMode::ArrowFunctionMode:
            setIsFunction();
            break;

        case SourceParseMode::ProgramMode:
            break;

        case SourceParseMode::ModuleAnalyzeMode:
        case SourceParseMode::ModuleEvaluateMode:
            setIsModule();
            break;
        }
    }

    bool isFunction() const { return m_isFunction; }
    bool isFunctionBoundary() const { return m_isFunctionBoundary; }
    bool isGenerator() const { return m_isGenerator; }

    bool hasArguments() const { return m_hasArguments; }

    void setIsLexicalScope() 
    { 
        m_isLexicalScope = true;
        m_allowsLexicalDeclarations = true;
    }
    bool isLexicalScope() { return m_isLexicalScope; }

    VariableEnvironment& declaredVariables() { return m_declaredVariables; }
    VariableEnvironment& lexicalVariables() { return m_lexicalVariables; }
    VariableEnvironment& finalizeLexicalEnvironment() 
    { 
        if (m_usesEval || m_needsFullActivation)
            m_lexicalVariables.markAllVariablesAsCaptured();
        else
            computeLexicallyCapturedVariablesAndPurgeCandidates();

        return m_lexicalVariables;
    }

    ModuleScopeData& moduleScopeData() const
    {
        ASSERT(m_moduleScopeData);
        return *m_moduleScopeData;
    }

    void computeLexicallyCapturedVariablesAndPurgeCandidates()
    {
        // Because variables may be defined at any time in the range of a lexical scope, we must
        // track lexical variables that might be captured. Then, when we're preparing to pop the top
        // lexical scope off the stack, we should find which variables are truly captured, and which
        // variable still may be captured in a parent scope.
        if (m_lexicalVariables.size() && m_closedVariableCandidates.size()) {
            auto end = m_closedVariableCandidates.end();
            for (auto iter = m_closedVariableCandidates.begin(); iter != end; ++iter)
                m_lexicalVariables.markVariableAsCapturedIfDefined(iter->get());
        }

        // We can now purge values from the captured candidates because they're captured in this scope.
        {
            for (auto entry : m_lexicalVariables) {
                if (entry.value.isCaptured())
                    m_closedVariableCandidates.remove(entry.key);
            }
        }
    }

    void declareCallee(const Identifier* ident)
    {
        auto addResult = m_declaredVariables.add(ident->impl());
        // We want to track if callee is captured, but we don't want to act like it's a 'var'
        // because that would cause the BytecodeGenerator to emit bad code.
        addResult.iterator->value.clearIsVar();
    }

    DeclarationResultMask declareVariable(const Identifier* ident)
    {
        ASSERT(m_allowsVarDeclarations);
        DeclarationResultMask result = DeclarationResult::Valid;
        bool isValidStrictMode = !isEvalOrArgumentsIdentifier(m_vm, ident);
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        auto addResult = m_declaredVariables.add(ident->impl());
        addResult.iterator->value.setIsVar();
        if (!isValidStrictMode)
            result |= DeclarationResult::InvalidStrictMode;
        if (m_lexicalVariables.contains(ident->impl()))
            result |= DeclarationResult::InvalidDuplicateDeclaration;
        return result;
    }

    DeclarationResultMask declareLexicalVariable(const Identifier* ident, bool isConstant, DeclarationImportType importType = DeclarationImportType::NotImported)
    {
        ASSERT(m_allowsLexicalDeclarations);
        DeclarationResultMask result = DeclarationResult::Valid;
        bool isValidStrictMode = !isEvalOrArgumentsIdentifier(m_vm, ident);
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        auto addResult = m_lexicalVariables.add(ident->impl());
        if (isConstant)
            addResult.iterator->value.setIsConst();
        else
            addResult.iterator->value.setIsLet();

        if (importType == DeclarationImportType::Imported)
            addResult.iterator->value.setIsImported();
        else if (importType == DeclarationImportType::ImportedNamespace) {
            addResult.iterator->value.setIsImported();
            addResult.iterator->value.setIsImportedNamespace();
        }

        if (!addResult.isNewEntry)
            result |= DeclarationResult::InvalidDuplicateDeclaration;
        if (!isValidStrictMode)
            result |= DeclarationResult::InvalidStrictMode;

        return result;
    }

    bool hasDeclaredVariable(const Identifier& ident)
    {
        return hasDeclaredVariable(ident.impl());
    }

    bool hasDeclaredVariable(const RefPtr<UniquedStringImpl>& ident)
    {
        return m_declaredVariables.contains(ident.get());
    }

    bool hasLexicallyDeclaredVariable(const RefPtr<UniquedStringImpl>& ident) const
    {
        return m_lexicalVariables.contains(ident.get());
    }
    
    ALWAYS_INLINE bool hasDeclaredParameter(const Identifier& ident)
    {
        return hasDeclaredParameter(ident.impl());
    }

    bool hasDeclaredParameter(const RefPtr<UniquedStringImpl>& ident)
    {
        return m_declaredParameters.contains(ident) || m_declaredVariables.contains(ident.get());
    }
    
    void declareWrite(const Identifier* ident)
    {
        ASSERT(m_strictMode);
        m_writtenVariables.add(ident->impl());
    }

    void preventAllVariableDeclarations()
    {
        m_allowsVarDeclarations = false; 
        m_allowsLexicalDeclarations = false;
    }
    void preventVarDeclarations() { m_allowsVarDeclarations = false; }
    bool allowsVarDeclarations() const { return m_allowsVarDeclarations; }
    bool allowsLexicalDeclarations() const { return m_allowsLexicalDeclarations; }

    DeclarationResultMask declareParameter(const Identifier* ident)
    {
        ASSERT(m_allowsVarDeclarations);
        DeclarationResultMask result = DeclarationResult::Valid;
        bool isArgumentsIdent = isArguments(m_vm, ident);
        auto addResult = m_declaredVariables.add(ident->impl());
        addResult.iterator->value.clearIsVar();
        bool isValidStrictMode = addResult.isNewEntry && m_vm->propertyNames->eval != *ident && !isArgumentsIdent;
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        m_declaredParameters.add(ident->impl());
        if (!isValidStrictMode)
            result |= DeclarationResult::InvalidStrictMode;
        if (isArgumentsIdent)
            m_shadowsArguments = true;
        if (!addResult.isNewEntry)
            result |= DeclarationResult::InvalidDuplicateDeclaration;

        return result;
    }
    
    void getUsedVariables(IdentifierSet& usedVariables)
    {
        usedVariables.swap(m_usedVariables);
    }

    void useVariable(const Identifier* ident, bool isEval)
    {
        m_usesEval |= isEval;
        m_usedVariables.add(ident->impl());
    }

    void setNeedsFullActivation() { m_needsFullActivation = true; }
    bool needsFullActivation() const { return m_needsFullActivation; }

    bool hasDirectSuper() { return m_hasDirectSuper; }
    void setHasDirectSuper() { m_hasDirectSuper = true; }

    bool needsSuperBinding() { return m_needsSuperBinding; }
    void setNeedsSuperBinding() { m_needsSuperBinding = true; }

    void collectFreeVariables(Scope* nestedScope, bool shouldTrackClosedVariables)
    {
        if (nestedScope->m_usesEval)
            m_usesEval = true;

        {
            for (const RefPtr<UniquedStringImpl>& impl : nestedScope->m_usedVariables) {
                if (nestedScope->m_declaredVariables.contains(impl) || nestedScope->m_lexicalVariables.contains(impl))
                    continue;

                // "arguments" reference should be resolved at function boudary.
                if (nestedScope->isFunctionBoundary() && nestedScope->hasArguments() && impl == m_vm->propertyNames->arguments.impl())
                    continue;

                m_usedVariables.add(impl);
                // We don't want a declared variable that is used in an inner scope to be thought of as captured if
                // that inner scope is both a lexical scope and not a function. Only inner functions and "catch" 
                // statements can cause variables to be captured.
                if (shouldTrackClosedVariables && (nestedScope->m_isFunctionBoundary || !nestedScope->m_isLexicalScope))
                    m_closedVariableCandidates.add(impl);
            }
        }
        // Propagate closed variable candidates downwards within the same function.
        // Cross function captures will be realized via m_usedVariables propagation.
        if (shouldTrackClosedVariables && !nestedScope->m_isFunctionBoundary && nestedScope->m_closedVariableCandidates.size()) {
            IdentifierSet::iterator end = nestedScope->m_closedVariableCandidates.end();
            IdentifierSet::iterator begin = nestedScope->m_closedVariableCandidates.begin();
            m_closedVariableCandidates.add(begin, end);
        }

        if (nestedScope->m_writtenVariables.size()) {
            IdentifierSet::iterator end = nestedScope->m_writtenVariables.end();
            for (IdentifierSet::iterator ptr = nestedScope->m_writtenVariables.begin(); ptr != end; ++ptr) {
                if (nestedScope->m_declaredVariables.contains(*ptr) || nestedScope->m_lexicalVariables.contains(*ptr))
                    continue;
                m_writtenVariables.add(*ptr);
            }
        }
    }
    
    void getCapturedVars(IdentifierSet& capturedVariables, bool& modifiedParameter, bool& modifiedArguments)
    {
        if (m_needsFullActivation || m_usesEval) {
            modifiedParameter = true;
            for (auto& entry : m_declaredVariables)
                capturedVariables.add(entry.key);
            return;
        }
        for (IdentifierSet::iterator ptr = m_closedVariableCandidates.begin(); ptr != m_closedVariableCandidates.end(); ++ptr) {
            if (!m_declaredVariables.contains(*ptr))
                continue;
            capturedVariables.add(*ptr);
        }
        modifiedParameter = false;
        if (shadowsArguments())
            modifiedArguments = true;
        if (m_declaredParameters.size()) {
            IdentifierSet::iterator end = m_writtenVariables.end();
            for (IdentifierSet::iterator ptr = m_writtenVariables.begin(); ptr != end; ++ptr) {
                if (*ptr == m_vm->propertyNames->arguments.impl())
                    modifiedArguments = true;
                if (!m_declaredParameters.contains(*ptr))
                    continue;
                modifiedParameter = true;
                break;
            }
        }
    }
    void setStrictMode() { m_strictMode = true; }
    bool strictMode() const { return m_strictMode; }
    bool isValidStrictMode() const { return m_isValidStrictMode; }
    bool shadowsArguments() const { return m_shadowsArguments; }

    void copyCapturedVariablesToVector(const IdentifierSet& capturedVariables, Vector<RefPtr<UniquedStringImpl>>& vector)
    {
        IdentifierSet::iterator end = capturedVariables.end();
        for (IdentifierSet::iterator it = capturedVariables.begin(); it != end; ++it) {
            if (m_declaredVariables.contains(*it) || m_lexicalVariables.contains(*it))
                continue;
            vector.append(*it);
        }
    }

    void fillParametersForSourceProviderCache(SourceProviderCacheItemCreationParameters& parameters)
    {
        ASSERT(m_isFunction);
        parameters.usesEval = m_usesEval;
        parameters.strictMode = m_strictMode;
        parameters.needsFullActivation = m_needsFullActivation;
        copyCapturedVariablesToVector(m_writtenVariables, parameters.writtenVariables);
        copyCapturedVariablesToVector(m_usedVariables, parameters.usedVariables);
    }

    void restoreFromSourceProviderCache(const SourceProviderCacheItem* info)
    {
        ASSERT(m_isFunction);
        m_usesEval = info->usesEval;
        m_strictMode = info->strictMode;
        m_needsFullActivation = info->needsFullActivation;
        for (unsigned i = 0; i < info->usedVariablesCount; ++i)
            m_usedVariables.add(info->usedVariables()[i]);
        for (unsigned i = 0; i < info->writtenVariablesCount; ++i)
            m_writtenVariables.add(info->writtenVariables()[i]);
    }

private:
    void setIsFunction()
    {
        m_isFunction = true;
        m_isFunctionBoundary = true;
        m_hasArguments = true;
        setIsLexicalScope();
        m_isGenerator = false;
    }

    void setIsGeneratorFunction()
    {
        setIsFunction();
        m_isGenerator = true;
    }

    void setIsGenerator()
    {
        setIsFunction();
        m_isGenerator = true;
        m_hasArguments = false;
    }

    void setIsModule()
    {
        m_moduleScopeData = ModuleScopeData::create();
    }

    const VM* m_vm;
    bool m_shadowsArguments : 1;
    bool m_usesEval : 1;
    bool m_needsFullActivation : 1;
    bool m_hasDirectSuper : 1;
    bool m_needsSuperBinding : 1;
    bool m_allowsVarDeclarations : 1;
    bool m_allowsLexicalDeclarations : 1;
    bool m_strictMode : 1;
    bool m_isFunction : 1;
    bool m_isGenerator : 1;
    bool m_isLexicalScope : 1;
    bool m_isFunctionBoundary : 1;
    bool m_isValidStrictMode : 1;
    bool m_hasArguments : 1;
    int m_loopDepth;
    int m_switchDepth;

    typedef Vector<ScopeLabelInfo, 2> LabelStack;
    std::unique_ptr<LabelStack> m_labels;
    IdentifierSet m_declaredParameters;
    VariableEnvironment m_declaredVariables;
    VariableEnvironment m_lexicalVariables;
    IdentifierSet m_usedVariables;
    IdentifierSet m_closedVariableCandidates;
    IdentifierSet m_writtenVariables;
    RefPtr<ModuleScopeData> m_moduleScopeData { };
};

typedef Vector<Scope, 10> ScopeStack;

struct ScopeRef {
    ScopeRef(ScopeStack* scopeStack, unsigned index)
        : m_scopeStack(scopeStack)
        , m_index(index)
    {
    }
    Scope* operator->() { return &m_scopeStack->at(m_index); }
    unsigned index() const { return m_index; }

    bool hasContainingScope()
    {
        return m_index && !m_scopeStack->at(m_index).isFunctionBoundary();
    }

    ScopeRef containingScope()
    {
        ASSERT(hasContainingScope());
        return ScopeRef(m_scopeStack, m_index - 1);
    }

private:
    ScopeStack* m_scopeStack;
    unsigned m_index;
};

template <typename LexerType>
class Parser {
    WTF_MAKE_NONCOPYABLE(Parser);
    WTF_MAKE_FAST_ALLOCATED;

public:
    Parser(
        VM*, const SourceCode&, JSParserBuiltinMode, JSParserStrictMode, SourceParseMode, SuperBinding,
        ConstructorKind defaultConstructorKind = ConstructorKind::None, ThisTDZMode = ThisTDZMode::CheckIfNeeded);
    ~Parser();

    template <class ParsedNode>
    std::unique_ptr<ParsedNode> parse(ParserError&, const Identifier&, SourceParseMode);

    JSTextPosition positionBeforeLastNewline() const { return m_lexer->positionBeforeLastNewline(); }
    JSTokenLocation locationBeforeLastToken() const { return m_lexer->lastTokenLocation(); }
    Vector<RefPtr<UniquedStringImpl>>&& closedVariables() { return WTFMove(m_closedVariables); }

private:
    struct AllowInOverride {
        AllowInOverride(Parser* parser)
            : m_parser(parser)
            , m_oldAllowsIn(parser->m_allowsIn)
        {
            parser->m_allowsIn = true;
        }
        ~AllowInOverride()
        {
            m_parser->m_allowsIn = m_oldAllowsIn;
        }
        Parser* m_parser;
        bool m_oldAllowsIn;
    };

    struct AutoPopScopeRef : public ScopeRef {
        AutoPopScopeRef(Parser* parser, ScopeRef scope)
        : ScopeRef(scope)
        , m_parser(parser)
        {
        }
        
        ~AutoPopScopeRef()
        {
            if (m_parser)
                m_parser->popScope(*this, false);
        }
        
        void setPopped()
        {
            m_parser = 0;
        }
        
    private:
        Parser* m_parser;
    };

    struct AutoCleanupLexicalScope {
        // We can allocate this object on the stack without actually knowing beforehand if we're 
        // going to create a new lexical scope. If we decide to create a new lexical scope, we
        // can pass the scope into this obejct and it will take care of the cleanup for us if the parse fails.
        // This is helpful if we may fail from syntax errors after creating a lexical scope conditionally.
        AutoCleanupLexicalScope()
            : m_scope(nullptr, UINT_MAX)
            , m_parser(nullptr)
        {
        }

        ~AutoCleanupLexicalScope()
        {
            // This should only ever be called if we fail from a syntax error. Otherwise
            // it's the intention that a user of this class pops this scope manually on a 
            // successful parse. 
            if (isValid())
                m_parser->popScope(*this, false);
        }

        void setIsValid(ScopeRef& scope, Parser* parser)
        {
            RELEASE_ASSERT(scope->isLexicalScope());
            m_scope = scope;
            m_parser = parser;
        }

        bool isValid() const { return !!m_parser; }

        void setPopped()
        {
            m_parser = nullptr;
        }

        ScopeRef& scope() { return m_scope; }

    private:
        ScopeRef m_scope;
        Parser* m_parser;
    };

    enum ExpressionErrorClass {
        ErrorIndicatesNothing,
        ErrorIndicatesPattern
    };

    struct ExpressionErrorClassifier {
        ExpressionErrorClassifier(Parser* parser)
            : m_class(ErrorIndicatesNothing)
            , m_previous(parser->m_expressionErrorClassifier)
            , m_parser(parser)
        {
            m_parser->m_expressionErrorClassifier = this;
        }

        ~ExpressionErrorClassifier()
        {
            m_parser->m_expressionErrorClassifier = m_previous;
        }

        void classifyExpressionError(ExpressionErrorClass classification)
        {
            if (m_class != ErrorIndicatesNothing)
                return;
            m_class = classification;
        }

        void reclassifyExpressionError(ExpressionErrorClass oldClassification, ExpressionErrorClass classification)
        {
            if (m_class != oldClassification)
                return;
            m_class = classification;
        }

        void propagateExpressionErrorClass()
        {
            if (m_previous && m_class != ErrorIndicatesNothing)
                m_previous->m_class = m_class;
        }

        bool indicatesPossiblePattern() const { return m_class == ErrorIndicatesPattern; }

    private:
        ExpressionErrorClass m_class;
        ExpressionErrorClassifier* m_previous;
        Parser* m_parser;
    };

    ALWAYS_INLINE void classifyExpressionError(ExpressionErrorClass classification)
    {
        if (m_expressionErrorClassifier)
            m_expressionErrorClassifier->classifyExpressionError(classification);
    }

    ALWAYS_INLINE void reclassifyExpressionError(ExpressionErrorClass oldClassification, ExpressionErrorClass classification)
    {
        if (m_expressionErrorClassifier)
            m_expressionErrorClassifier->reclassifyExpressionError(oldClassification, classification);
    }

    ALWAYS_INLINE DestructuringKind destructuringKindFromDeclarationType(DeclarationType type)
    {
        switch (type) {
        case DeclarationType::VarDeclaration:
            return DestructuringKind::DestructureToVariables;
        case DeclarationType::LetDeclaration:
            return DestructuringKind::DestructureToLet;
        case DeclarationType::ConstDeclaration:
            return DestructuringKind::DestructureToConst;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return DestructuringKind::DestructureToVariables;
    }

    ALWAYS_INLINE AssignmentContext assignmentContextFromDeclarationType(DeclarationType type)
    {
        switch (type) {
        case DeclarationType::ConstDeclaration:
            return AssignmentContext::ConstDeclarationStatement;
        default:
            return AssignmentContext::DeclarationStatement;
        }
    }

    ALWAYS_INLINE bool isEvalOrArguments(const Identifier* ident) { return isEvalOrArgumentsIdentifier(m_vm, ident); }

    ScopeRef currentScope()
    {
        return ScopeRef(&m_scopeStack, m_scopeStack.size() - 1);
    }

    ScopeRef currentVariableScope()
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsVarDeclarations()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        return ScopeRef(&m_scopeStack, i);
    }

    ScopeRef currentFunctionScope()
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (i && !m_scopeStack[i].isFunctionBoundary()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        // When reaching the top level scope (it can be non function scope), we return it.
        return ScopeRef(&m_scopeStack, i);
    }

    
    ScopeRef pushScope()
    {
        bool isFunction = false;
        bool isStrict = false;
        bool isGenerator = false;
        if (!m_scopeStack.isEmpty()) {
            isStrict = m_scopeStack.last().strictMode();
            isFunction = m_scopeStack.last().isFunction();
            isGenerator = m_scopeStack.last().isGenerator();
        }
        m_scopeStack.append(Scope(m_vm, isFunction, isGenerator, isStrict));
        return currentScope();
    }
    
    void popScopeInternal(ScopeRef& scope, bool shouldTrackClosedVariables)
    {
        ASSERT_UNUSED(scope, scope.index() == m_scopeStack.size() - 1);
        ASSERT(m_scopeStack.size() > 1);
        m_scopeStack[m_scopeStack.size() - 2].collectFreeVariables(&m_scopeStack.last(), shouldTrackClosedVariables);
        if (!m_scopeStack.last().isFunctionBoundary() && m_scopeStack.last().needsFullActivation())
            m_scopeStack[m_scopeStack.size() - 2].setNeedsFullActivation();
        m_scopeStack.removeLast();
    }
    
    ALWAYS_INLINE void popScope(ScopeRef& scope, bool shouldTrackClosedVariables)
    {
        popScopeInternal(scope, shouldTrackClosedVariables);
    }
    
    ALWAYS_INLINE void popScope(AutoPopScopeRef& scope, bool shouldTrackClosedVariables)
    {
        scope.setPopped();
        popScopeInternal(scope, shouldTrackClosedVariables);
    }

    ALWAYS_INLINE void popScope(AutoCleanupLexicalScope& cleanupScope, bool shouldTrackClosedVariables)
    {
        RELEASE_ASSERT(cleanupScope.isValid());
        ScopeRef& scope = cleanupScope.scope();
        cleanupScope.setPopped();
        popScopeInternal(scope, shouldTrackClosedVariables);
    }
    
    DeclarationResultMask declareVariable(const Identifier* ident, DeclarationType type = DeclarationType::VarDeclaration, DeclarationImportType importType = DeclarationImportType::NotImported)
    {
        if (type == DeclarationType::VarDeclaration)
            return currentVariableScope()->declareVariable(ident);

        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        ASSERT(type == DeclarationType::LetDeclaration || type == DeclarationType::ConstDeclaration);

        // Lexical variables declared at a top level scope that shadow arguments or vars are not allowed.
        if (m_statementDepth == 1 && (hasDeclaredParameter(*ident) || hasDeclaredVariable(*ident)))
            return DeclarationResult::InvalidDuplicateDeclaration;

        while (!m_scopeStack[i].allowsLexicalDeclarations()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }

        return m_scopeStack[i].declareLexicalVariable(ident, type == DeclarationType::ConstDeclaration, importType);
    }
    
    NEVER_INLINE bool hasDeclaredVariable(const Identifier& ident)
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsVarDeclarations()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        return m_scopeStack[i].hasDeclaredVariable(ident);
    }

    NEVER_INLINE bool hasDeclaredParameter(const Identifier& ident)
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsVarDeclarations()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        return m_scopeStack[i].hasDeclaredParameter(ident);
    }
    
    void declareWrite(const Identifier* ident)
    {
        if (!m_syntaxAlreadyValidated || strictMode())
            m_scopeStack.last().declareWrite(ident);
    }

    bool exportName(const Identifier& ident)
    {
        ASSERT(currentScope().index() == 0);
        return currentScope()->moduleScopeData().exportName(ident);
    }

    ScopeStack m_scopeStack;
    
    const SourceProviderCacheItem* findCachedFunctionInfo(int openBracePos) 
    {
        return m_functionCache ? m_functionCache->get(openBracePos) : 0;
    }

    Parser();
    String parseInner(const Identifier&, SourceParseMode);

    void didFinishParsing(SourceElements*, DeclarationStacks::FunctionStack&, VariableEnvironment&, CodeFeatures, int, const Vector<RefPtr<UniquedStringImpl>>&&);

    // Used to determine type of error to report.
    bool isFunctionMetadataNode(ScopeNode*) { return false; }
    bool isFunctionMetadataNode(FunctionMetadataNode*) { return true; }

    ALWAYS_INLINE void next(unsigned lexerFlags = 0)
    {
        int lastLine = m_token.m_location.line;
        int lastTokenEnd = m_token.m_location.endOffset;
        int lastTokenLineStart = m_token.m_location.lineStartOffset;
        m_lastTokenEndPosition = JSTextPosition(lastLine, lastTokenEnd, lastTokenLineStart);
        m_lexer->setLastLineNumber(lastLine);
        m_token.m_type = m_lexer->lex(&m_token, lexerFlags, strictMode());
        if (UNLIKELY(m_token.m_type == CONSTTOKEN && m_vm->shouldRewriteConstAsVar()))
            m_token.m_type = VAR;
    }

    ALWAYS_INLINE void nextExpectIdentifier(unsigned lexerFlags = 0)
    {
        int lastLine = m_token.m_location.line;
        int lastTokenEnd = m_token.m_location.endOffset;
        int lastTokenLineStart = m_token.m_location.lineStartOffset;
        m_lastTokenEndPosition = JSTextPosition(lastLine, lastTokenEnd, lastTokenLineStart);
        m_lexer->setLastLineNumber(lastLine);
        m_token.m_type = m_lexer->lexExpectIdentifier(&m_token, lexerFlags, strictMode());
    }

    ALWAYS_INLINE bool nextTokenIsColon()
    {
        return m_lexer->nextTokenIsColon();
    }

    ALWAYS_INLINE bool consume(JSTokenType expected, unsigned flags = 0)
    {
        bool result = m_token.m_type == expected;
        if (result)
            next(flags);
        return result;
    }

    void printUnexpectedTokenText(WTF::PrintStream&);
    ALWAYS_INLINE StringView getToken() {
        SourceProvider* sourceProvider = m_source->provider();
        return sourceProvider->getRange(tokenStart(), tokenEndPosition().offset);
    }
    
    ALWAYS_INLINE bool match(JSTokenType expected)
    {
        return m_token.m_type == expected;
    }
    
    ALWAYS_INLINE bool matchContextualKeyword(const Identifier& identifier)
    {
        return m_token.m_type == IDENT && *m_token.m_data.ident == identifier;
    }

    ALWAYS_INLINE bool matchIdentifierOrKeyword()
    {
        return isIdentifierOrKeyword(m_token);
    }
    
    ALWAYS_INLINE bool isEndOfArrowFunction()
    {
        return match(SEMICOLON) || match(COMMA) || match(CLOSEPAREN) || match(CLOSEBRACE) || match(CLOSEBRACKET) || match(EOFTOK) || m_lexer->prevTerminator();
    }

    ALWAYS_INLINE unsigned tokenStart()
    {
        return m_token.m_location.startOffset;
    }
    
    ALWAYS_INLINE const JSTextPosition& tokenStartPosition()
    {
        return m_token.m_startPosition;
    }

    ALWAYS_INLINE int tokenLine()
    {
        return m_token.m_location.line;
    }
    
    ALWAYS_INLINE int tokenColumn()
    {
        return tokenStart() - tokenLineStart();
    }

    ALWAYS_INLINE const JSTextPosition& tokenEndPosition()
    {
        return m_token.m_endPosition;
    }
    
    ALWAYS_INLINE unsigned tokenLineStart()
    {
        return m_token.m_location.lineStartOffset;
    }
    
    ALWAYS_INLINE const JSTokenLocation& tokenLocation()
    {
        return m_token.m_location;
    }

    void setErrorMessage(const String& message)
    {
        m_errorMessage = message;
    }
    
    NEVER_INLINE void logError(bool);
    template <typename A> NEVER_INLINE void logError(bool, const A&);
    template <typename A, typename B> NEVER_INLINE void logError(bool, const A&, const B&);
    template <typename A, typename B, typename C> NEVER_INLINE void logError(bool, const A&, const B&, const C&);
    template <typename A, typename B, typename C, typename D> NEVER_INLINE void logError(bool, const A&, const B&, const C&, const D&);
    template <typename A, typename B, typename C, typename D, typename E> NEVER_INLINE void logError(bool, const A&, const B&, const C&, const D&, const E&);
    template <typename A, typename B, typename C, typename D, typename E, typename F> NEVER_INLINE void logError(bool, const A&, const B&, const C&, const D&, const E&, const F&);
    template <typename A, typename B, typename C, typename D, typename E, typename F, typename G> NEVER_INLINE void logError(bool, const A&, const B&, const C&, const D&, const E&, const F&, const G&);
    
    NEVER_INLINE void updateErrorWithNameAndMessage(const char* beforeMessage, const String& name, const char* afterMessage)
    {
        m_errorMessage = makeString(beforeMessage, " '", name, "' ", afterMessage);
    }
    
    NEVER_INLINE void updateErrorMessage(const char* msg)
    {
        ASSERT(msg);
        m_errorMessage = String(msg);
        ASSERT(!m_errorMessage.isNull());
    }
    
    void startLoop() { currentScope()->startLoop(); }
    void endLoop() { currentScope()->endLoop(); }
    void startSwitch() { currentScope()->startSwitch(); }
    void endSwitch() { currentScope()->endSwitch(); }
    void setStrictMode() { currentScope()->setStrictMode(); }
    bool strictMode() { return currentScope()->strictMode(); }
    bool isValidStrictMode() { return currentScope()->isValidStrictMode(); }
    DeclarationResultMask declareParameter(const Identifier* ident) { return currentScope()->declareParameter(ident); }
    bool declareRestOrNormalParameter(const Identifier&, const Identifier**);

    bool breakIsValid()
    {
        ScopeRef current = currentScope();
        while (!current->breakIsValid()) {
            if (!current.hasContainingScope())
                return false;
            current = current.containingScope();
        }
        return true;
    }
    bool continueIsValid()
    {
        ScopeRef current = currentScope();
        while (!current->continueIsValid()) {
            if (!current.hasContainingScope())
                return false;
            current = current.containingScope();
        }
        return true;
    }
    void pushLabel(const Identifier* label, bool isLoop) { currentScope()->pushLabel(label, isLoop); }
    void popLabel(ScopeRef scope) { scope->popLabel(); }
    ScopeLabelInfo* getLabel(const Identifier* label)
    {
        ScopeRef current = currentScope();
        ScopeLabelInfo* result = 0;
        while (!(result = current->getLabel(label))) {
            if (!current.hasContainingScope())
                return 0;
            current = current.containingScope();
        }
        return result;
    }

    // http://ecma-international.org/ecma-262/6.0/#sec-identifiers-static-semantics-early-errors
    ALWAYS_INLINE bool isLETMaskedAsIDENT()
    {
        return match(LET) && !strictMode();
    }

    // http://ecma-international.org/ecma-262/6.0/#sec-identifiers-static-semantics-early-errors
    ALWAYS_INLINE bool isYIELDMaskedAsIDENT(bool inGenerator)
    {
        return match(YIELD) && !strictMode() && !inGenerator;
    }

    // http://ecma-international.org/ecma-262/6.0/#sec-generator-function-definitions-static-semantics-early-errors
    ALWAYS_INLINE bool matchSpecIdentifier(bool inGenerator)
    {
        return match(IDENT) || isLETMaskedAsIDENT() || isYIELDMaskedAsIDENT(inGenerator);
    }

    ALWAYS_INLINE bool matchSpecIdentifier()
    {
        return matchSpecIdentifier(currentScope()->isGenerator());
    }

    template <class TreeBuilder> TreeSourceElements parseSourceElements(TreeBuilder&, SourceElementsMode);
    template <class TreeBuilder> TreeSourceElements parseGeneratorFunctionSourceElements(TreeBuilder&, SourceElementsMode);
    template <class TreeBuilder> TreeStatement parseStatementListItem(TreeBuilder&, const Identifier*& directive, unsigned* directiveLiteralLength);
    template <class TreeBuilder> TreeStatement parseStatement(TreeBuilder&, const Identifier*& directive, unsigned* directiveLiteralLength = 0);
    enum class ExportType { Exported, NotExported };
    template <class TreeBuilder> TreeStatement parseClassDeclaration(TreeBuilder&, ExportType = ExportType::NotExported);
    template <class TreeBuilder> TreeStatement parseFunctionDeclaration(TreeBuilder&, ExportType = ExportType::NotExported);
    template <class TreeBuilder> TreeStatement parseVariableDeclaration(TreeBuilder&, DeclarationType, ExportType = ExportType::NotExported);
    template <class TreeBuilder> TreeStatement parseDoWhileStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseWhileStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseForStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseBreakStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseContinueStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseReturnStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseThrowStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseWithStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseSwitchStatement(TreeBuilder&);
    template <class TreeBuilder> TreeClauseList parseSwitchClauses(TreeBuilder&);
    template <class TreeBuilder> TreeClause parseSwitchDefaultClause(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseTryStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseDebuggerStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseExpressionStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseExpressionOrLabelStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseIfStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseBlockStatement(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseAssignmentExpression(TreeBuilder&, ExpressionErrorClassifier&);
    template <class TreeBuilder> TreeExpression parseAssignmentExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseAssignmentExpressionOrPropagateErrorClass(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseYieldExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseConditionalExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseBinaryExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseUnaryExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseMemberExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parsePrimaryExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseArrayLiteral(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseObjectLiteral(TreeBuilder&);
    template <class TreeBuilder> NEVER_INLINE TreeExpression parseStrictObjectLiteral(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseFunctionExpression(TreeBuilder&);
    enum SpreadMode { AllowSpread, DontAllowSpread };
    template <class TreeBuilder> ALWAYS_INLINE TreeArguments parseArguments(TreeBuilder&, SpreadMode);
    template <class TreeBuilder> TreeProperty parseProperty(TreeBuilder&, bool strict);
    template <class TreeBuilder> TreeExpression parsePropertyMethod(TreeBuilder& context, const Identifier* methodName, bool isGenerator);
    template <class TreeBuilder> TreeProperty parseGetterSetter(TreeBuilder&, bool strict, PropertyNode::Type, unsigned getterOrSetterStartOffset, ConstructorKind = ConstructorKind::None, SuperBinding = SuperBinding::NotNeeded);
    template <class TreeBuilder> ALWAYS_INLINE TreeFunctionBody parseFunctionBody(TreeBuilder&, const JSTokenLocation&, int, int functionKeywordStart, int functionNameStart, int parametersStart, ConstructorKind, SuperBinding, FunctionBodyType, unsigned, SourceParseMode);
    template <class TreeBuilder> ALWAYS_INLINE bool parseFormalParameters(TreeBuilder&, TreeFormalParameterList, unsigned&);
    enum VarDeclarationListContext { ForLoopContext, VarDeclarationContext };
    template <class TreeBuilder> TreeExpression parseVariableDeclarationList(TreeBuilder&, int& declarations, TreeDestructuringPattern& lastPattern, TreeExpression& lastInitializer, JSTextPosition& identStart, JSTextPosition& initStart, JSTextPosition& initEnd, VarDeclarationListContext, DeclarationType, ExportType, bool& forLoopConstDoesNotHaveInitializer);
    template <class TreeBuilder> TreeSourceElements parseArrowFunctionSingleExpressionBodySourceElements(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseArrowFunctionExpression(TreeBuilder&);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern createBindingPattern(TreeBuilder&, DestructuringKind, ExportType, const Identifier&, JSToken, AssignmentContext, const Identifier** duplicateIdentifier);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern createAssignmentElement(TreeBuilder&, TreeExpression&, const JSTextPosition&, const JSTextPosition&);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern parseBindingOrAssignmentElement(TreeBuilder& context, DestructuringKind, ExportType, const Identifier** duplicateIdentifier, bool* hasDestructuringPattern, AssignmentContext bindingContext, int depth);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern parseAssignmentElement(TreeBuilder& context, DestructuringKind, ExportType, const Identifier** duplicateIdentifier, bool* hasDestructuringPattern, AssignmentContext bindingContext, int depth);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern parseDestructuringPattern(TreeBuilder&, DestructuringKind, ExportType, const Identifier** duplicateIdentifier = nullptr, bool* hasDestructuringPattern = nullptr, AssignmentContext = AssignmentContext::DeclarationStatement, int depth = 0);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern tryParseDestructuringPatternExpression(TreeBuilder&, AssignmentContext);
    template <class TreeBuilder> NEVER_INLINE TreeExpression parseDefaultValueForDestructuringPattern(TreeBuilder&);
    template <class TreeBuilder> TreeSourceElements parseModuleSourceElements(TreeBuilder&, SourceParseMode);
    enum class ImportSpecifierType { NamespaceImport, NamedImport, DefaultImport };
    template <class TreeBuilder> typename TreeBuilder::ImportSpecifier parseImportClauseItem(TreeBuilder&, ImportSpecifierType);
    template <class TreeBuilder> typename TreeBuilder::ModuleName parseModuleName(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseImportDeclaration(TreeBuilder&);
    template <class TreeBuilder> typename TreeBuilder::ExportSpecifier parseExportSpecifier(TreeBuilder& context, Vector<const Identifier*>& maybeLocalNames, bool& hasKeywordForLocalBindings);
    template <class TreeBuilder> TreeStatement parseExportDeclaration(TreeBuilder&);

    enum class FunctionDefinitionType { Expression, Declaration, Method };
    template <class TreeBuilder> NEVER_INLINE bool parseFunctionInfo(TreeBuilder&, FunctionRequirements, SourceParseMode, bool nameIsInContainingScope, ConstructorKind, SuperBinding, int functionKeywordStart, ParserFunctionInfo<TreeBuilder>&, FunctionDefinitionType);
    
    ALWAYS_INLINE bool isArrowFunctionParameters();
    
    template <class TreeBuilder> NEVER_INLINE int parseFunctionParameters(TreeBuilder&, SourceParseMode, ParserFunctionInfo<TreeBuilder>&);
    template <class TreeBuilder> NEVER_INLINE typename TreeBuilder::FormalParameterList createGeneratorParameters(TreeBuilder&);

    template <class TreeBuilder> NEVER_INLINE TreeClassExpression parseClass(TreeBuilder&, FunctionRequirements, ParserClassInfo<TreeBuilder>&);

    template <class TreeBuilder> NEVER_INLINE typename TreeBuilder::TemplateString parseTemplateString(TreeBuilder& context, bool isTemplateHead, typename LexerType::RawStringsBuildMode, bool& elementIsTail);
    template <class TreeBuilder> NEVER_INLINE typename TreeBuilder::TemplateLiteral parseTemplateLiteral(TreeBuilder&, typename LexerType::RawStringsBuildMode);

    template <class TreeBuilder> ALWAYS_INLINE bool shouldCheckPropertyForUnderscoreProtoDuplicate(TreeBuilder&, const TreeProperty&);

    ALWAYS_INLINE int isBinaryOperator(JSTokenType);
    bool allowAutomaticSemicolon();
    
    bool autoSemiColon()
    {
        if (m_token.m_type == SEMICOLON) {
            next();
            return true;
        }
        return allowAutomaticSemicolon();
    }
    
    void setEndOfStatement()
    {
        m_lexer->setTokenPosition(&m_token);
    }

    bool canRecurse()
    {
        return m_vm->isSafeToRecurse();
    }
    
    const JSTextPosition& lastTokenEndPosition() const
    {
        return m_lastTokenEndPosition;
    }

    bool hasError() const
    {
        return !m_errorMessage.isNull();
    }

    struct SavePoint {
        int startOffset;
        unsigned oldLineStartOffset;
        unsigned oldLastLineNumber;
        unsigned oldLineNumber;
    };
    
    ALWAYS_INLINE SavePoint createSavePointForError()
    {
        SavePoint result;
        result.startOffset = m_token.m_location.startOffset;
        result.oldLineStartOffset = m_token.m_location.lineStartOffset;
        result.oldLastLineNumber = m_lexer->lastLineNumber();
        result.oldLineNumber = m_lexer->lineNumber();
        return result;
    }
    
    ALWAYS_INLINE SavePoint createSavePoint()
    {
        ASSERT(!hasError());
        return createSavePointForError();
    }

    ALWAYS_INLINE void restoreSavePointWithError(const SavePoint& savePoint, const String& message)
    {
        m_errorMessage = message;
        m_lexer->setOffset(savePoint.startOffset, savePoint.oldLineStartOffset);
        next();
        m_lexer->setLastLineNumber(savePoint.oldLastLineNumber);
        m_lexer->setLineNumber(savePoint.oldLineNumber);
    }

    ALWAYS_INLINE void restoreSavePoint(const SavePoint& savePoint)
    {
        restoreSavePointWithError(savePoint, String());
    }

    enum class FunctionParsePhase { Parameters, Body };
    struct ParserState {
        int assignmentCount;
        int nonLHSCount;
        int nonTrivialExpressionCount;
        FunctionParsePhase functionParsePhase;
    };

    ALWAYS_INLINE ParserState saveState()
    {
        ParserState result;
        result.assignmentCount = m_assignmentCount;
        result.nonLHSCount = m_nonLHSCount;
        result.nonTrivialExpressionCount = m_nonTrivialExpressionCount;
        result.functionParsePhase = m_functionParsePhase;
        return result;
    }

    ALWAYS_INLINE void restoreState(const ParserState& state)
    {
        m_assignmentCount = state.assignmentCount;
        m_nonLHSCount = state.nonLHSCount;
        m_nonTrivialExpressionCount = state.nonTrivialExpressionCount;
        m_functionParsePhase = state.functionParsePhase;
    }

    VM* m_vm;
    const SourceCode* m_source;
    ParserArena m_parserArena;
    std::unique_ptr<LexerType> m_lexer;
    FunctionParameters* m_parameters { nullptr };
    
    bool m_hasStackOverflow;
    String m_errorMessage;
    JSToken m_token;
    bool m_allowsIn;
    JSTextPosition m_lastTokenEndPosition;
    int m_assignmentCount;
    int m_nonLHSCount;
    bool m_syntaxAlreadyValidated;
    int m_statementDepth;
    int m_nonTrivialExpressionCount;
    FunctionParsePhase m_functionParsePhase;
    const Identifier* m_lastIdentifier;
    const Identifier* m_lastFunctionName;
    RefPtr<SourceProviderCache> m_functionCache;
    SourceElements* m_sourceElements;
    bool m_parsingBuiltin;
    SuperBinding m_superBinding;
    ConstructorKind m_defaultConstructorKind;
    ThisTDZMode m_thisTDZMode;
    VariableEnvironment m_varDeclarations;
    DeclarationStacks::FunctionStack m_funcDeclarations;
    Vector<RefPtr<UniquedStringImpl>> m_closedVariables;
    CodeFeatures m_features;
    int m_numConstants;
    ExpressionErrorClassifier* m_expressionErrorClassifier;
    
    struct DepthManager {
        DepthManager(int* depth)
        : m_originalDepth(*depth)
        , m_depth(depth)
        {
        }
        
        ~DepthManager()
        {
            *m_depth = m_originalDepth;
        }
        
    private:
        int m_originalDepth;
        int* m_depth;
    };
};


template <typename LexerType>
template <class ParsedNode>
std::unique_ptr<ParsedNode> Parser<LexerType>::parse(ParserError& error, const Identifier& calleeName, SourceParseMode parseMode)
{
    int errLine;
    String errMsg;

    if (ParsedNode::scopeIsFunction)
        m_lexer->setIsReparsingFunction();

    m_sourceElements = 0;

    errLine = -1;
    errMsg = String();

    JSTokenLocation startLocation(tokenLocation());
    ASSERT(m_source->startColumn() > 0);
    unsigned startColumn = m_source->startColumn() - 1;

    String parseError = parseInner(calleeName, parseMode);

    int lineNumber = m_lexer->lineNumber();
    bool lexError = m_lexer->sawError();
    String lexErrorMessage = lexError ? m_lexer->getErrorMessage() : String();
    ASSERT(lexErrorMessage.isNull() != lexError);
    m_lexer->clear();

    if (!parseError.isNull() || lexError) {
        errLine = lineNumber;
        errMsg = !lexErrorMessage.isNull() ? lexErrorMessage : parseError;
        m_sourceElements = 0;
    }

    std::unique_ptr<ParsedNode> result;
    if (m_sourceElements) {
        JSTokenLocation endLocation;
        endLocation.line = m_lexer->lineNumber();
        endLocation.lineStartOffset = m_lexer->currentLineStartOffset();
        endLocation.startOffset = m_lexer->currentOffset();
        unsigned endColumn = endLocation.startOffset - endLocation.lineStartOffset;
        result = std::make_unique<ParsedNode>(m_parserArena,
                                    startLocation,
                                    endLocation,
                                    startColumn,
                                    endColumn,
                                    m_sourceElements,
                                    m_varDeclarations,
                                    m_funcDeclarations,
                                    currentScope()->finalizeLexicalEnvironment(),
                                    m_parameters,
                                    *m_source,
                                    m_features,
                                    m_numConstants);
        result->setLoc(m_source->firstLine(), m_lexer->lineNumber(), m_lexer->currentOffset(), m_lexer->currentLineStartOffset());
        result->setEndOffset(m_lexer->currentOffset());

        if (!isFunctionParseMode(parseMode)) {
            m_source->provider()->setSourceURLDirective(m_lexer->sourceURL());
            m_source->provider()->setSourceMappingURLDirective(m_lexer->sourceMappingURL());
        }
    } else {
        // We can never see a syntax error when reparsing a function, since we should have
        // reported the error when parsing the containing program or eval code. So if we're
        // parsing a function body node, we assume that what actually happened here is that
        // we ran out of stack while parsing. If we see an error while parsing eval or program
        // code we assume that it was a syntax error since running out of stack is much less
        // likely, and we are currently unable to distinguish between the two cases.
        if (isFunctionMetadataNode(static_cast<ParsedNode*>(0)) || m_hasStackOverflow)
            error = ParserError(ParserError::StackOverflow, ParserError::SyntaxErrorNone, m_token);
        else {
            ParserError::SyntaxErrorType errorType = ParserError::SyntaxErrorIrrecoverable;
            if (m_token.m_type == EOFTOK)
                errorType = ParserError::SyntaxErrorRecoverable;
            else if (m_token.m_type & UnterminatedErrorTokenFlag) {
                // Treat multiline capable unterminated literals as recoverable.
                if (m_token.m_type == UNTERMINATED_MULTILINE_COMMENT_ERRORTOK || m_token.m_type == UNTERMINATED_TEMPLATE_LITERAL_ERRORTOK)
                    errorType = ParserError::SyntaxErrorRecoverable;
                else
                    errorType = ParserError::SyntaxErrorUnterminatedLiteral;
            }
            
            if (isEvalNode<ParsedNode>())
                error = ParserError(ParserError::EvalError, errorType, m_token, errMsg, errLine);
            else
                error = ParserError(ParserError::SyntaxError, errorType, m_token, errMsg, errLine);
        }
    }

    return result;
}

template <class ParsedNode>
std::unique_ptr<ParsedNode> parse(
    VM* vm, const SourceCode& source,
    const Identifier& name, JSParserBuiltinMode builtinMode,
    JSParserStrictMode strictMode, SourceParseMode parseMode, SuperBinding superBinding,
    ParserError& error, JSTextPosition* positionBeforeLastNewline = nullptr,
    ConstructorKind defaultConstructorKind = ConstructorKind::None,
    ThisTDZMode thisTDZMode = ThisTDZMode::CheckIfNeeded)
{
    SamplingRegion samplingRegion("Parsing");

    ASSERT(!source.provider()->source().isNull());
    if (source.provider()->source().is8Bit()) {
        Parser<Lexer<LChar>> parser(vm, source, builtinMode, strictMode, parseMode, superBinding, defaultConstructorKind, thisTDZMode);
        std::unique_ptr<ParsedNode> result = parser.parse<ParsedNode>(error, name, parseMode);
        if (positionBeforeLastNewline)
            *positionBeforeLastNewline = parser.positionBeforeLastNewline();
        if (builtinMode == JSParserBuiltinMode::Builtin) {
            if (!result)
                WTF::dataLog("Error compiling builtin: ", error.message(), "\n");
            RELEASE_ASSERT(result);
            result->setClosedVariables(parser.closedVariables());
        }
        return result;
    }
    ASSERT_WITH_MESSAGE(defaultConstructorKind == ConstructorKind::None, "BuiltinExecutables::createDefaultConstructor should always use a 8-bit string");
    Parser<Lexer<UChar>> parser(vm, source, builtinMode, strictMode, parseMode, superBinding, defaultConstructorKind, thisTDZMode);
    std::unique_ptr<ParsedNode> result = parser.parse<ParsedNode>(error, name, parseMode);
    if (positionBeforeLastNewline)
        *positionBeforeLastNewline = parser.positionBeforeLastNewline();
    return result;
}

} // namespace
#endif
