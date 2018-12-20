/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 * Copyright (C) 2012 Igalia, S.L.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "config.h"
#include "BytecodeGenerator.h"

#include "ArithProfile.h"
#include "BuiltinExecutables.h"
#include "BuiltinNames.h"
#include "BytecodeGeneratorification.h"
#include "BytecodeLivenessAnalysis.h"
#include "BytecodeStructs.h"
#include "BytecodeUseDef.h"
#include "CatchScope.h"
#include "DefinePropertyAttributes.h"
#include "Interpreter.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "JSFixedArray.h"
#include "JSFunction.h"
#include "JSGeneratorFunction.h"
#include "JSImmutableButterfly.h"
#include "JSLexicalEnvironment.h"
#include "JSTemplateObjectDescriptor.h"
#include "LowLevelInterpreter.h"
#include "Options.h"
#include "PreciseJumpTargetsInlines.h"
#include "StackAlignment.h"
#include "StrongInlines.h"
#include "SuperSamplerBytecodeScope.h"
#include "UnlinkedCodeBlock.h"
#include "UnlinkedEvalCodeBlock.h"
#include "UnlinkedFunctionCodeBlock.h"
#include "UnlinkedMetadataTableInlines.h"
#include "UnlinkedModuleProgramCodeBlock.h"
#include "UnlinkedProgramCodeBlock.h"
#include <wtf/BitVector.h>
#include <wtf/CommaPrinter.h>
#include <wtf/SmallPtrSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace JSC {

template<typename CallOp, typename = std::true_type>
struct VarArgsOp;

template<typename CallOp>
struct VarArgsOp<CallOp, std::enable_if_t<std::is_same<CallOp, OpTailCall>::value, std::true_type>> {
    using type = OpTailCallVarargs;
};


template<typename CallOp>
struct VarArgsOp<CallOp, std::enable_if_t<!std::is_same<CallOp, OpTailCall>::value, std::true_type>> {
    using type = OpCallVarargs;
};


template<typename T>
static inline void shrinkToFit(T& segmentedVector)
{
    while (segmentedVector.size() && !segmentedVector.last().refCount())
        segmentedVector.removeLast();
}

void Label::setLocation(BytecodeGenerator& generator, unsigned location)
{
    m_location = location;
    
    for (auto offset : m_unresolvedJumps) {
        auto instruction = generator.m_writer.ref(offset);
        int target = m_location - offset;

#define CASE(__op) \
    case __op::opcodeID:  \
        instruction->cast<__op>()->setTarget(BoundLabel(target), [&]() { \
            generator.m_codeBlock->addOutOfLineJumpTarget(instruction.offset(), target); \
            return BoundLabel(); \
        }); \
        break;

        switch (instruction->opcodeID()) {
        CASE(OpJmp)
        CASE(OpJtrue)
        CASE(OpJfalse)
        CASE(OpJeqNull)
        CASE(OpJneqNull)
        CASE(OpJeq)
        CASE(OpJstricteq)
        CASE(OpJneq)
        CASE(OpJneqPtr)
        CASE(OpJnstricteq)
        CASE(OpJless)
        CASE(OpJlesseq)
        CASE(OpJgreater)
        CASE(OpJgreatereq)
        CASE(OpJnless)
        CASE(OpJnlesseq)
        CASE(OpJngreater)
        CASE(OpJngreatereq)
        CASE(OpJbelow)
        CASE(OpJbeloweq)
        default:
            ASSERT_NOT_REACHED();
        }
#undef CASE
    }
}

int BoundLabel::target()
{
    switch (m_type) {
    case Offset:
        return m_target;
    case GeneratorBackward:
        return m_target - m_generator->m_writer.position();
    case GeneratorForward:
        return 0;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

int BoundLabel::saveTarget()
{
    if (m_type == GeneratorForward) {
        m_savedTarget = m_generator->m_writer.position();
        return 0;
    }

    m_savedTarget = target();
    return m_savedTarget;
}

int BoundLabel::commitTarget()
{
    if (m_type == GeneratorForward) {
        m_label->m_unresolvedJumps.append(m_savedTarget);
        return 0;
    }

    return m_savedTarget;
}

void Variable::dump(PrintStream& out) const
{
    out.print(
        "{ident = ", m_ident,
        ", offset = ", m_offset,
        ", local = ", RawPointer(m_local),
        ", attributes = ", m_attributes,
        ", kind = ", m_kind,
        ", symbolTableConstantIndex = ", m_symbolTableConstantIndex,
        ", isLexicallyScoped = ", m_isLexicallyScoped, "}");
}

ParserError BytecodeGenerator::generate()
{
    m_codeBlock->setThisRegister(m_thisRegister.virtualRegister());

    emitLogShadowChickenPrologueIfNecessary();
    
    // If we have declared a variable named "arguments" and we are using arguments then we should
    // perform that assignment now.
    if (m_needToInitializeArguments)
        initializeVariable(variable(propertyNames().arguments), m_argumentsRegister);

    if (m_restParameter)
        m_restParameter->emit(*this);

    {
        RefPtr<RegisterID> temp = newTemporary();
        RefPtr<RegisterID> tolLevelScope;
        for (auto functionPair : m_functionsToInitialize) {
            FunctionMetadataNode* metadata = functionPair.first;
            FunctionVariableType functionType = functionPair.second;
            emitNewFunction(temp.get(), metadata);
            if (functionType == NormalFunctionVariable)
                initializeVariable(variable(metadata->ident()), temp.get());
            else if (functionType == TopLevelFunctionVariable) {
                if (!tolLevelScope) {
                    // We know this will resolve to the top level scope or global object because our parser/global initialization code 
                    // doesn't allow let/const/class variables to have the same names as functions.
                    // This is a top level function, and it's an error to ever create a top level function
                    // name that would resolve to a lexical variable. E.g:
                    // ```
                    //     function f() {
                    //         {
                    //             let x;
                    //             {
                    //             //// error thrown here
                    //                  eval("function x(){}");
                    //             }
                    //         }
                    //     }
                    // ```
                    // Therefore, we're guaranteed to have this resolve to a top level variable.
                    RefPtr<RegisterID> tolLevelObjectScope = emitResolveScope(nullptr, Variable(metadata->ident()));
                    tolLevelScope = newBlockScopeVariable();
                    move(tolLevelScope.get(), tolLevelObjectScope.get());
                }
                emitPutToScope(tolLevelScope.get(), Variable(metadata->ident()), temp.get(), ThrowIfNotFound, InitializationMode::NotInitialization);
            } else
                RELEASE_ASSERT_NOT_REACHED();
        }
    }
    
    bool callingClassConstructor = constructorKind() != ConstructorKind::None && !isConstructor();
    if (!callingClassConstructor)
        m_scopeNode->emitBytecode(*this);
    else {
        // At this point we would have emitted an unconditional throw followed by some nonsense that's
        // just an artifact of how this generator is structured. That code never runs, but it confuses
        // bytecode analyses because it constitutes an unterminated basic block. So, we terminate the
        // basic block the strongest way possible.
        emitUnreachable();
    }

    for (auto& tuple : m_catchesToEmit) {
        Ref<Label> realCatchTarget = newLabel();
        OpCatch::emit(this, std::get<1>(tuple), std::get<2>(tuple));
        realCatchTarget->setLocation(*this, m_lastInstruction.offset());
        m_codeBlock->addJumpTarget(m_lastInstruction.offset());


        TryData* tryData = std::get<0>(tuple);
        emitJump(tryData->target.get());
        tryData->target = WTFMove(realCatchTarget);
    }

    m_staticPropertyAnalyzer.kill();

    for (auto& range : m_tryRanges) {
        int start = range.start->bind();
        int end = range.end->bind();
        
        // This will happen for empty try blocks and for some cases of finally blocks:
        //
        // try {
        //    try {
        //    } finally {
        //        return 42;
        //        // *HERE*
        //    }
        // } finally {
        //    print("things");
        // }
        //
        // The return will pop scopes to execute the outer finally block. But this includes
        // popping the try context for the inner try. The try context is live in the fall-through
        // part of the finally block not because we will emit a handler that overlaps the finally,
        // but because we haven't yet had a chance to plant the catch target. Then when we finish
        // emitting code for the outer finally block, we repush the try contex, this time with a
        // new start index. But that means that the start index for the try range corresponding
        // to the inner-finally-following-the-return (marked as "*HERE*" above) will be greater
        // than the end index of the try block. This is harmless since end < start handlers will
        // never get matched in our logic, but we do the runtime a favor and choose to not emit
        // such handlers at all.
        if (end <= start)
            continue;
        
        UnlinkedHandlerInfo info(static_cast<uint32_t>(start), static_cast<uint32_t>(end),
            static_cast<uint32_t>(range.tryData->target->bind()), range.tryData->handlerType);
        m_codeBlock->addExceptionHandler(info);
    }
    

    if (isGeneratorOrAsyncFunctionBodyParseMode(m_codeBlock->parseMode()))
        performGeneratorification(*this, m_codeBlock.get(), m_writer, m_generatorFrameSymbolTable.get(), m_generatorFrameSymbolTableIndex);

    RELEASE_ASSERT(static_cast<unsigned>(m_codeBlock->numCalleeLocals()) < static_cast<unsigned>(FirstConstantRegisterIndex));
    m_codeBlock->setInstructions(m_writer.finalize());

    m_codeBlock->shrinkToFit();

    if (m_expressionTooDeep)
        return ParserError(ParserError::OutOfMemory);
    return ParserError(ParserError::ErrorNone);
}

BytecodeGenerator::BytecodeGenerator(VM& vm, ProgramNode* programNode, UnlinkedProgramCodeBlock* codeBlock, DebuggerMode debuggerMode, const VariableEnvironment* parentScopeTDZVariables)
    : m_shouldEmitDebugHooks(Options::forceDebuggerBytecodeGeneration() || debuggerMode == DebuggerOn)
    , m_scopeNode(programNode)
    , m_codeBlock(vm, codeBlock)
    , m_thisRegister(CallFrame::thisArgumentOffset())
    , m_codeType(GlobalCode)
    , m_vm(&vm)
    , m_needsToUpdateArrowFunctionContext(programNode->usesArrowFunction() || programNode->usesEval())
{
    ASSERT_UNUSED(parentScopeTDZVariables, !parentScopeTDZVariables->size());

    for (auto& constantRegister : m_linkTimeConstantRegisters)
        constantRegister = nullptr;

    allocateCalleeSaveSpace();

    m_codeBlock->setNumParameters(1); // Allocate space for "this"

    emitEnter();

    allocateAndEmitScope();

    emitCheckTraps();

    const FunctionStack& functionStack = programNode->functionStack();

    for (auto* function : functionStack)
        m_functionsToInitialize.append(std::make_pair(function, TopLevelFunctionVariable));

    if (Options::validateBytecode()) {
        for (auto& entry : programNode->varDeclarations())
            RELEASE_ASSERT(entry.value.isVar());
    }
    codeBlock->setVariableDeclarations(programNode->varDeclarations());
    codeBlock->setLexicalDeclarations(programNode->lexicalVariables());
    // Even though this program may have lexical variables that go under TDZ, when linking the get_from_scope/put_to_scope
    // operations we emit we will have ResolveTypes that implictly do TDZ checks. Therefore, we don't need
    // additional TDZ checks on top of those. This is why we can omit pushing programNode->lexicalVariables()
    // to the TDZ stack.
    
    if (needsToUpdateArrowFunctionContext()) {
        initializeArrowFunctionContextScopeIfNeeded();
        emitPutThisToArrowFunctionContextScope();
    }
}

BytecodeGenerator::BytecodeGenerator(VM& vm, FunctionNode* functionNode, UnlinkedFunctionCodeBlock* codeBlock, DebuggerMode debuggerMode, const VariableEnvironment* parentScopeTDZVariables)
    : m_shouldEmitDebugHooks(Options::forceDebuggerBytecodeGeneration() || debuggerMode == DebuggerOn)
    , m_scopeNode(functionNode)
    , m_codeBlock(vm, codeBlock)
    , m_codeType(FunctionCode)
    , m_vm(&vm)
    , m_isBuiltinFunction(codeBlock->isBuiltinFunction())
    , m_usesNonStrictEval(codeBlock->usesEval() && !codeBlock->isStrictMode())
    // FIXME: We should be able to have tail call elimination with the profiler
    // enabled. This is currently not possible because the profiler expects
    // op_will_call / op_did_call pairs before and after a call, which are not
    // compatible with tail calls (we have no way of emitting op_did_call).
    // https://bugs.webkit.org/show_bug.cgi?id=148819
    , m_inTailPosition(Options::useTailCalls() && !isConstructor() && constructorKind() == ConstructorKind::None && isStrictMode())
    , m_needsToUpdateArrowFunctionContext(functionNode->usesArrowFunction() || functionNode->usesEval())
    , m_derivedContextType(codeBlock->derivedContextType())
{
    for (auto& constantRegister : m_linkTimeConstantRegisters)
        constantRegister = nullptr;

    if (m_isBuiltinFunction)
        m_shouldEmitDebugHooks = false;

    allocateCalleeSaveSpace();
    
    SymbolTable* functionSymbolTable = SymbolTable::create(*m_vm);
    functionSymbolTable->setUsesNonStrictEval(m_usesNonStrictEval);
    int symbolTableConstantIndex = 0;

    FunctionParameters& parameters = *functionNode->parameters(); 
    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-functiondeclarationinstantiation
    // This implements IsSimpleParameterList in the Ecma 2015 spec.
    // If IsSimpleParameterList is false, we will create a strict-mode like arguments object.
    // IsSimpleParameterList is false if the argument list contains any default parameter values,
    // a rest parameter, or any destructuring patterns.
    // If we do have default parameters, destructuring parameters, or a rest parameter, our parameters will be allocated in a different scope.
    bool isSimpleParameterList = parameters.isSimpleParameterList();

    SourceParseMode parseMode = codeBlock->parseMode();

    bool containsArrowOrEvalButNotInArrowBlock = ((functionNode->usesArrowFunction() && functionNode->doAnyInnerArrowFunctionsUseAnyFeature()) || functionNode->usesEval()) && !m_codeBlock->isArrowFunction();
    bool shouldCaptureSomeOfTheThings = m_shouldEmitDebugHooks || functionNode->needsActivation() || containsArrowOrEvalButNotInArrowBlock;

    bool shouldCaptureAllOfTheThings = m_shouldEmitDebugHooks || codeBlock->usesEval();
    bool needsArguments = ((functionNode->usesArguments() && !codeBlock->isArrowFunction()) || codeBlock->usesEval() || (functionNode->usesArrowFunction() && !codeBlock->isArrowFunction() && isArgumentsUsedInInnerArrowFunction()));

    if (isGeneratorOrAsyncFunctionBodyParseMode(parseMode)) {
        // Generator and AsyncFunction never provides "arguments". "arguments" reference will be resolved in an upper generator function scope.
        needsArguments = false;

        // Generator and AsyncFunction uses the var scope to save and resume its variables. So the lexical scope is always instantiated.
        shouldCaptureSomeOfTheThings = true;
    }

    if (isGeneratorOrAsyncFunctionWrapperParseMode(parseMode) && needsArguments) {
        // Generator does not provide "arguments". Instead, wrapping GeneratorFunction provides "arguments".
        // This is because arguments of a generator should be evaluated before starting it.
        // To workaround it, we evaluate these arguments as arguments of a wrapping generator function, and reference it from a generator.
        //
        //    function *gen(a, b = hello())
        //    {
        //        return {
        //            @generatorNext: function (@generator, @generatorState, @generatorValue, @generatorResumeMode, @generatorFrame)
        //            {
        //                arguments;  // This `arguments` should reference to the gen's arguments.
        //                ...
        //            }
        //        }
        //    }
        shouldCaptureSomeOfTheThings = true;
    }

    if (shouldCaptureAllOfTheThings)
        functionNode->varDeclarations().markAllVariablesAsCaptured();
    
    auto captures = scopedLambda<bool (UniquedStringImpl*)>([&] (UniquedStringImpl* uid) -> bool {
        if (!shouldCaptureSomeOfTheThings)
            return false;
        if (needsArguments && uid == propertyNames().arguments.impl()) {
            // Actually, we only need to capture the arguments object when we "need full activation"
            // because of name scopes. But historically we did it this way, so for now we just preserve
            // the old behavior.
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=143072
            return true;
        }
        return functionNode->captures(uid);
    });
    auto varKind = [&] (UniquedStringImpl* uid) -> VarKind {
        return captures(uid) ? VarKind::Scope : VarKind::Stack;
    };

    m_calleeRegister.setIndex(CallFrameSlot::callee);

    initializeParameters(parameters);
    ASSERT(!(isSimpleParameterList && m_restParameter));

    emitEnter();

    if (isGeneratorOrAsyncFunctionBodyParseMode(parseMode))
        m_generatorRegister = &m_parameters[1];

    allocateAndEmitScope();

    emitCheckTraps();
    
    if (functionNameIsInScope(functionNode->ident(), functionNode->functionMode())) {
        ASSERT(parseMode != SourceParseMode::GeneratorBodyMode);
        ASSERT(!isAsyncFunctionBodyParseMode(parseMode));
        bool isDynamicScope = functionNameScopeIsDynamic(codeBlock->usesEval(), codeBlock->isStrictMode());
        bool isFunctionNameCaptured = captures(functionNode->ident().impl());
        bool markAsCaptured = isDynamicScope || isFunctionNameCaptured;
        emitPushFunctionNameScope(functionNode->ident(), &m_calleeRegister, markAsCaptured);
    }

    if (shouldCaptureSomeOfTheThings)
        m_lexicalEnvironmentRegister = addVar();

    if (shouldCaptureSomeOfTheThings || vm.typeProfiler())
        symbolTableConstantIndex = addConstantValue(functionSymbolTable)->index();

    // We can allocate the "var" environment if we don't have default parameter expressions. If we have
    // default parameter expressions, we have to hold off on allocating the "var" environment because
    // the parent scope of the "var" environment is the parameter environment.
    if (isSimpleParameterList)
        initializeVarLexicalEnvironment(symbolTableConstantIndex, functionSymbolTable, shouldCaptureSomeOfTheThings);

    // Figure out some interesting facts about our arguments.
    bool capturesAnyArgumentByName = false;
    if (functionNode->hasCapturedVariables()) {
        FunctionParameters& parameters = *functionNode->parameters();
        for (size_t i = 0; i < parameters.size(); ++i) {
            auto pattern = parameters.at(i).first;
            if (!pattern->isBindingNode())
                continue;
            const Identifier& ident = static_cast<const BindingNode*>(pattern)->boundProperty();
            capturesAnyArgumentByName |= captures(ident.impl());
        }
    }
    
    if (capturesAnyArgumentByName)
        ASSERT(m_lexicalEnvironmentRegister);

    // Need to know what our functions are called. Parameters have some goofy behaviors when it
    // comes to functions of the same name.
    for (FunctionMetadataNode* function : functionNode->functionStack())
        m_functions.add(function->ident().impl());
    
    if (needsArguments) {
        // Create the arguments object now. We may put the arguments object into the activation if
        // it is captured. Either way, we create two arguments object variables: one is our
        // private variable that is immutable, and another that is the user-visible variable. The
        // immutable one is only used here, or during formal parameter resolutions if we opt for
        // DirectArguments.
        
        m_argumentsRegister = addVar();
        m_argumentsRegister->ref();
    }
    
    if (needsArguments && !codeBlock->isStrictMode() && isSimpleParameterList) {
        // If we captured any formal parameter by name, then we use ScopedArguments. Otherwise we
        // use DirectArguments. With ScopedArguments, we lift all of our arguments into the
        // activation.
        
        if (capturesAnyArgumentByName) {
            functionSymbolTable->setArgumentsLength(vm, parameters.size());
            
            // For each parameter, we have two possibilities:
            // Either it's a binding node with no function overlap, in which case it gets a name
            // in the symbol table - or it just gets space reserved in the symbol table. Either
            // way we lift the value into the scope.
            for (unsigned i = 0; i < parameters.size(); ++i) {
                ScopeOffset offset = functionSymbolTable->takeNextScopeOffset(NoLockingNecessary);
                functionSymbolTable->setArgumentOffset(vm, i, offset);
                if (UniquedStringImpl* name = visibleNameForParameter(parameters.at(i).first)) {
                    VarOffset varOffset(offset);
                    SymbolTableEntry entry(varOffset);
                    // Stores to these variables via the ScopedArguments object will not do
                    // notifyWrite(), since that would be cumbersome. Also, watching formal
                    // parameters when "arguments" is in play is unlikely to be super profitable.
                    // So, we just disable it.
                    entry.disableWatching(*m_vm);
                    functionSymbolTable->set(NoLockingNecessary, name, entry);
                }
                OpPutToScope::emit(this, m_lexicalEnvironmentRegister, UINT_MAX, virtualRegisterForArgument(1 + i), GetPutInfo(ThrowIfNotFound, LocalClosureVar, InitializationMode::NotInitialization), symbolTableConstantIndex, offset.offset());
            }
            
            // This creates a scoped arguments object and copies the overflow arguments into the
            // scope. It's the equivalent of calling ScopedArguments::createByCopying().
            OpCreateScopedArguments::emit(this, m_argumentsRegister, m_lexicalEnvironmentRegister);
        } else {
            // We're going to put all parameters into the DirectArguments object. First ensure
            // that the symbol table knows that this is happening.
            for (unsigned i = 0; i < parameters.size(); ++i) {
                if (UniquedStringImpl* name = visibleNameForParameter(parameters.at(i).first))
                    functionSymbolTable->set(NoLockingNecessary, name, SymbolTableEntry(VarOffset(DirectArgumentsOffset(i))));
            }
            
            OpCreateDirectArguments::emit(this, m_argumentsRegister);
        }
    } else if (isSimpleParameterList) {
        // Create the formal parameters the normal way. Any of them could be captured, or not. If
        // captured, lift them into the scope. We cannot do this if we have default parameter expressions
        // because when default parameter expressions exist, they belong in their own lexical environment
        // separate from the "var" lexical environment.
        for (unsigned i = 0; i < parameters.size(); ++i) {
            UniquedStringImpl* name = visibleNameForParameter(parameters.at(i).first);
            if (!name)
                continue;
            
            if (!captures(name)) {
                // This is the easy case - just tell the symbol table about the argument. It will
                // be accessed directly.
                functionSymbolTable->set(NoLockingNecessary, name, SymbolTableEntry(VarOffset(virtualRegisterForArgument(1 + i))));
                continue;
            }
            
            ScopeOffset offset = functionSymbolTable->takeNextScopeOffset(NoLockingNecessary);
            const Identifier& ident =
                static_cast<const BindingNode*>(parameters.at(i).first)->boundProperty();
            functionSymbolTable->set(NoLockingNecessary, name, SymbolTableEntry(VarOffset(offset)));
            
            OpPutToScope::emit(this, m_lexicalEnvironmentRegister, addConstant(ident), virtualRegisterForArgument(1 + i), GetPutInfo(ThrowIfNotFound, LocalClosureVar, InitializationMode::NotInitialization), symbolTableConstantIndex, offset.offset());
        }
    }
    
    if (needsArguments && (codeBlock->isStrictMode() || !isSimpleParameterList)) {
        // Allocate a cloned arguments object.
        OpCreateClonedArguments::emit(this, m_argumentsRegister);
    }
    
    // There are some variables that need to be preinitialized to something other than Undefined:
    //
    // - "arguments": unless it's used as a function or parameter, this should refer to the
    //   arguments object.
    //
    // - functions: these always override everything else.
    //
    // The most logical way to do all of this is to initialize none of the variables until now,
    // and then initialize them in BytecodeGenerator::generate() in such an order that the rules
    // for how these things override each other end up holding. We would initialize "arguments" first, 
    // then all arguments, then the functions.
    //
    // But some arguments are already initialized by default, since if they aren't captured and we
    // don't have "arguments" then we just point the symbol table at the stack slot of those
    // arguments. We end up initializing the rest of the arguments that have an uncomplicated
    // binding (i.e. don't involve destructuring) above when figuring out how to lay them out,
    // because that's just the simplest thing. This means that when we initialize them, we have to
    // watch out for the things that override arguments (namely, functions).
    
    // This is our final act of weirdness. "arguments" is overridden by everything except the
    // callee. We add it to the symbol table if it's not already there and it's not an argument.
    bool shouldCreateArgumentsVariableInParameterScope = false;
    if (needsArguments) {
        // If "arguments" is overridden by a function or destructuring parameter name, then it's
        // OK for us to call createVariable() because it won't change anything. It's also OK for
        // us to them tell BytecodeGenerator::generate() to write to it because it will do so
        // before it initializes functions and destructuring parameters. But if "arguments" is
        // overridden by a "simple" function parameter, then we have to bail: createVariable()
        // would assert and BytecodeGenerator::generate() would write the "arguments" after the
        // argument value had already been properly initialized.
        
        bool haveParameterNamedArguments = false;
        for (unsigned i = 0; i < parameters.size(); ++i) {
            UniquedStringImpl* name = visibleNameForParameter(parameters.at(i).first);
            if (name == propertyNames().arguments.impl()) {
                haveParameterNamedArguments = true;
                break;
            }
        }

        bool shouldCreateArgumensVariable = !haveParameterNamedArguments
            && !SourceParseModeSet(SourceParseMode::ArrowFunctionMode, SourceParseMode::AsyncArrowFunctionMode).contains(m_codeBlock->parseMode());
        shouldCreateArgumentsVariableInParameterScope = shouldCreateArgumensVariable && !isSimpleParameterList;
        // Do not create arguments variable in case of Arrow function. Value will be loaded from parent scope
        if (shouldCreateArgumensVariable && !shouldCreateArgumentsVariableInParameterScope) {
            createVariable(
                propertyNames().arguments, varKind(propertyNames().arguments.impl()), functionSymbolTable);

            m_needToInitializeArguments = true;
        }
    }

    for (FunctionMetadataNode* function : functionNode->functionStack()) {
        const Identifier& ident = function->ident();
        createVariable(ident, varKind(ident.impl()), functionSymbolTable);
        m_functionsToInitialize.append(std::make_pair(function, NormalFunctionVariable));
    }
    for (auto& entry : functionNode->varDeclarations()) {
        ASSERT(!entry.value.isLet() && !entry.value.isConst());
        if (!entry.value.isVar()) // This is either a parameter or callee.
            continue;
        if (shouldCreateArgumentsVariableInParameterScope && entry.key.get() == propertyNames().arguments.impl())
            continue;
        createVariable(Identifier::fromUid(m_vm, entry.key.get()), varKind(entry.key.get()), functionSymbolTable, IgnoreExisting);
    }


    m_newTargetRegister = addVar();
    switch (parseMode) {
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GeneratorWrapperMethodMode:
    case SourceParseMode::AsyncGeneratorWrapperMethodMode:
    case SourceParseMode::AsyncGeneratorWrapperFunctionMode: {
        m_generatorRegister = addVar();

        // FIXME: Emit to_this only when Generator uses it.
        // https://bugs.webkit.org/show_bug.cgi?id=151586
        emitToThis();

        move(m_generatorRegister, &m_calleeRegister);
        emitCreateThis(m_generatorRegister);
        break;
    }

    case SourceParseMode::AsyncArrowFunctionMode:
    case SourceParseMode::AsyncMethodMode:
    case SourceParseMode::AsyncFunctionMode: {
        ASSERT(!isConstructor());
        ASSERT(constructorKind() == ConstructorKind::None);
        m_generatorRegister = addVar();
        m_promiseCapabilityRegister = addVar();

        if (parseMode != SourceParseMode::AsyncArrowFunctionMode) {
            // FIXME: Emit to_this only when AsyncFunctionBody uses it.
            // https://bugs.webkit.org/show_bug.cgi?id=151586
            emitToThis();
        }

        emitNewObject(m_generatorRegister);

        // let promiseCapability be @newPromiseCapability(@Promise)
        auto varNewPromiseCapability = variable(propertyNames().builtinNames().newPromiseCapabilityPrivateName());
        RefPtr<RegisterID> scope = newTemporary();
        move(scope.get(), emitResolveScope(scope.get(), varNewPromiseCapability));
        RefPtr<RegisterID> newPromiseCapability = emitGetFromScope(newTemporary(), scope.get(), varNewPromiseCapability, ThrowIfNotFound);

        CallArguments args(*this, nullptr, 1);
        emitLoad(args.thisRegister(), jsUndefined());

        auto varPromiseConstructor = variable(propertyNames().builtinNames().PromisePrivateName());
        move(scope.get(), emitResolveScope(scope.get(), varPromiseConstructor));
        emitGetFromScope(args.argumentRegister(0), scope.get(), varPromiseConstructor, ThrowIfNotFound);

        // JSTextPosition(int _line, int _offset, int _lineStartOffset)
        JSTextPosition divot(m_scopeNode->firstLine(), m_scopeNode->startOffset(), m_scopeNode->lineStartOffset());
        emitCall(promiseCapabilityRegister(), newPromiseCapability.get(), NoExpectedFunction, args, divot, divot, divot, DebuggableCall::No);
        break;
    }

    case SourceParseMode::AsyncGeneratorBodyMode:
    case SourceParseMode::AsyncFunctionBodyMode:
    case SourceParseMode::AsyncArrowFunctionBodyMode:
    case SourceParseMode::GeneratorBodyMode: {
        // |this| is already filled correctly before here.
        emitLoad(m_newTargetRegister, jsUndefined());
        break;
    }

    default: {
        if (SourceParseMode::ArrowFunctionMode != parseMode) {
            if (isConstructor()) {
                move(m_newTargetRegister, &m_thisRegister);
                if (constructorKind() == ConstructorKind::Extends) {
                    moveEmptyValue(&m_thisRegister);
                } else
                    emitCreateThis(&m_thisRegister);
            } else if (constructorKind() != ConstructorKind::None)
                emitThrowTypeError("Cannot call a class constructor without |new|");
            else {
                bool shouldEmitToThis = false;
                if (functionNode->usesThis() || codeBlock->usesEval() || m_scopeNode->doAnyInnerArrowFunctionsUseThis() || m_scopeNode->doAnyInnerArrowFunctionsUseEval())
                    shouldEmitToThis = true;
                else if ((functionNode->usesSuperProperty() || m_scopeNode->doAnyInnerArrowFunctionsUseSuperProperty()) && !codeBlock->isStrictMode()) {
                    // We must emit to_this when we're not in strict mode because we
                    // will convert |this| to an object, and that object may be passed
                    // to a strict function as |this|. This is observable because that
                    // strict function's to_this will just return the object.
                    //
                    // We don't need to emit this for strict-mode code because
                    // strict-mode code may call another strict function, which will
                    // to_this if it directly uses this; this is OK, because we defer
                    // to_this until |this| is used directly. Strict-mode code might
                    // also call a sloppy mode function, and that will to_this, which
                    // will defer the conversion, again, until necessary.
                    shouldEmitToThis = true;
                }

                if (shouldEmitToThis)
                    emitToThis();
            }
        }
        break;
    }
    }

    // We need load |super| & |this| for arrow function before initializeDefaultParameterValuesAndSetupFunctionScopeStack
    // if we have default parameter expression. Because |super| & |this| values can be used there
    if ((SourceParseModeSet(SourceParseMode::ArrowFunctionMode, SourceParseMode::AsyncArrowFunctionMode).contains(parseMode) && !isSimpleParameterList) || parseMode == SourceParseMode::AsyncArrowFunctionBodyMode) {
        if (functionNode->usesThis() || functionNode->usesSuperProperty())
            emitLoadThisFromArrowFunctionLexicalEnvironment();

        if (m_scopeNode->usesNewTarget() || m_scopeNode->usesSuperCall())
            emitLoadNewTargetFromArrowFunctionLexicalEnvironment();
    }

    if (needsToUpdateArrowFunctionContext() && !codeBlock->isArrowFunction()) {
        bool canReuseLexicalEnvironment = isSimpleParameterList;
        initializeArrowFunctionContextScopeIfNeeded(functionSymbolTable, canReuseLexicalEnvironment);
        emitPutThisToArrowFunctionContextScope();
        emitPutNewTargetToArrowFunctionContextScope();
        emitPutDerivedConstructorToArrowFunctionContextScope();
    }

    // All "addVar()"s needs to happen before "initializeDefaultParameterValuesAndSetupFunctionScopeStack()" is called
    // because a function's default parameter ExpressionNodes will use temporary registers.
    pushTDZVariables(*parentScopeTDZVariables, TDZCheckOptimization::DoNotOptimize, TDZRequirement::UnderTDZ);

    Ref<Label> catchLabel = newLabel();
    TryData* tryFormalParametersData = nullptr;
    bool needTryCatch = isAsyncFunctionWrapperParseMode(parseMode) && !isSimpleParameterList;
    if (needTryCatch) {
        Ref<Label> tryFormalParametersStart = newEmittedLabel();
        tryFormalParametersData = pushTry(tryFormalParametersStart.get(), catchLabel.get(), HandlerType::SynthesizedCatch);
    }

    initializeDefaultParameterValuesAndSetupFunctionScopeStack(parameters, isSimpleParameterList, functionNode, functionSymbolTable, symbolTableConstantIndex, captures, shouldCreateArgumentsVariableInParameterScope);

    if (needTryCatch) {
        Ref<Label> didNotThrow = newLabel();
        emitJump(didNotThrow.get());
        emitLabel(catchLabel.get());
        popTry(tryFormalParametersData, catchLabel.get());

        RefPtr<RegisterID> thrownValue = newTemporary();
        RegisterID* unused = newTemporary();
        emitCatch(unused, thrownValue.get(), tryFormalParametersData);

        // return promiseCapability.@reject(thrownValue)
        RefPtr<RegisterID> reject = emitGetById(newTemporary(), promiseCapabilityRegister(), m_vm->propertyNames->builtinNames().rejectPrivateName());

        CallArguments args(*this, nullptr, 1);
        emitLoad(args.thisRegister(), jsUndefined());
        move(args.argumentRegister(0), thrownValue.get());

        JSTextPosition divot(functionNode->firstLine(), functionNode->startOffset(), functionNode->lineStartOffset());

        RefPtr<RegisterID> result = emitCall(newTemporary(), reject.get(), NoExpectedFunction, args, divot, divot, divot, DebuggableCall::No);
        emitReturn(emitGetById(newTemporary(), promiseCapabilityRegister(), m_vm->propertyNames->builtinNames().promisePrivateName()));

        emitLabel(didNotThrow.get());
    }

    // If we don't have  default parameter expression, then loading |this| inside an arrow function must be done
    // after initializeDefaultParameterValuesAndSetupFunctionScopeStack() because that function sets up the
    // SymbolTable stack and emitLoadThisFromArrowFunctionLexicalEnvironment() consults the SymbolTable stack
    if (SourceParseModeSet(SourceParseMode::ArrowFunctionMode, SourceParseMode::AsyncArrowFunctionMode).contains(parseMode) && isSimpleParameterList) {
        if (functionNode->usesThis() || functionNode->usesSuperProperty())
            emitLoadThisFromArrowFunctionLexicalEnvironment();
    
        if (m_scopeNode->usesNewTarget() || m_scopeNode->usesSuperCall())
            emitLoadNewTargetFromArrowFunctionLexicalEnvironment();
    }
    
    // Set up the lexical environment scope as the generator frame. We store the saved and resumed generator registers into this scope with the symbol keys.
    // Since they are symbol keyed, these variables cannot be reached from the usual code.
    if (isGeneratorOrAsyncFunctionBodyParseMode(parseMode)) {
        ASSERT(m_lexicalEnvironmentRegister);
        m_generatorFrameSymbolTable.set(*m_vm, functionSymbolTable);
        m_generatorFrameSymbolTableIndex = symbolTableConstantIndex;
        move(generatorFrameRegister(), m_lexicalEnvironmentRegister);
        emitPutById(generatorRegister(), propertyNames().builtinNames().generatorFramePrivateName(), generatorFrameRegister());
    }

    bool shouldInitializeBlockScopedFunctions = false; // We generate top-level function declarations in ::generate().
    pushLexicalScope(m_scopeNode, TDZCheckOptimization::Optimize, NestedScopeType::IsNotNested, nullptr, shouldInitializeBlockScopedFunctions);
}

BytecodeGenerator::BytecodeGenerator(VM& vm, EvalNode* evalNode, UnlinkedEvalCodeBlock* codeBlock, DebuggerMode debuggerMode, const VariableEnvironment* parentScopeTDZVariables)
    : m_shouldEmitDebugHooks(Options::forceDebuggerBytecodeGeneration() || debuggerMode == DebuggerOn)
    , m_scopeNode(evalNode)
    , m_codeBlock(vm, codeBlock)
    , m_thisRegister(CallFrame::thisArgumentOffset())
    , m_codeType(EvalCode)
    , m_vm(&vm)
    , m_usesNonStrictEval(codeBlock->usesEval() && !codeBlock->isStrictMode())
    , m_needsToUpdateArrowFunctionContext(evalNode->usesArrowFunction() || evalNode->usesEval())
    , m_derivedContextType(codeBlock->derivedContextType())
{
    for (auto& constantRegister : m_linkTimeConstantRegisters)
        constantRegister = nullptr;

    allocateCalleeSaveSpace();

    m_codeBlock->setNumParameters(1);

    pushTDZVariables(*parentScopeTDZVariables, TDZCheckOptimization::DoNotOptimize, TDZRequirement::UnderTDZ);

    emitEnter();

    allocateAndEmitScope();

    emitCheckTraps();
    
    for (FunctionMetadataNode* function : evalNode->functionStack()) {
        m_codeBlock->addFunctionDecl(makeFunction(function));
        m_functionsToInitialize.append(std::make_pair(function, TopLevelFunctionVariable));
    }

    const VariableEnvironment& varDeclarations = evalNode->varDeclarations();
    Vector<Identifier, 0, UnsafeVectorOverflow> variables;
    Vector<Identifier, 0, UnsafeVectorOverflow> hoistedFunctions;
    for (auto& entry : varDeclarations) {
        ASSERT(entry.value.isVar());
        ASSERT(entry.key->isAtomic() || entry.key->isSymbol());
        if (entry.value.isSloppyModeHoistingCandidate())
            hoistedFunctions.append(Identifier::fromUid(m_vm, entry.key.get()));
        else
            variables.append(Identifier::fromUid(m_vm, entry.key.get()));
    }
    codeBlock->adoptVariables(variables);
    codeBlock->adoptFunctionHoistingCandidates(WTFMove(hoistedFunctions));
    
    if (evalNode->usesSuperCall() || evalNode->usesNewTarget())
        m_newTargetRegister = addVar();

    if (codeBlock->isArrowFunctionContext() && (evalNode->usesThis() || evalNode->usesSuperProperty()))
        emitLoadThisFromArrowFunctionLexicalEnvironment();

    if (evalNode->usesSuperCall() || evalNode->usesNewTarget())
        emitLoadNewTargetFromArrowFunctionLexicalEnvironment();

    if (needsToUpdateArrowFunctionContext() && !codeBlock->isArrowFunctionContext() && !isDerivedConstructorContext()) {
        initializeArrowFunctionContextScopeIfNeeded();
        emitPutThisToArrowFunctionContextScope();
    }
    
    bool shouldInitializeBlockScopedFunctions = false; // We generate top-level function declarations in ::generate().
    pushLexicalScope(m_scopeNode, TDZCheckOptimization::Optimize, NestedScopeType::IsNotNested, nullptr, shouldInitializeBlockScopedFunctions);
}

BytecodeGenerator::BytecodeGenerator(VM& vm, ModuleProgramNode* moduleProgramNode, UnlinkedModuleProgramCodeBlock* codeBlock, DebuggerMode debuggerMode, const VariableEnvironment* parentScopeTDZVariables)
    : m_shouldEmitDebugHooks(Options::forceDebuggerBytecodeGeneration() || debuggerMode == DebuggerOn)
    , m_scopeNode(moduleProgramNode)
    , m_codeBlock(vm, codeBlock)
    , m_thisRegister(CallFrame::thisArgumentOffset())
    , m_codeType(ModuleCode)
    , m_vm(&vm)
    , m_usesNonStrictEval(false)
    , m_needsToUpdateArrowFunctionContext(moduleProgramNode->usesArrowFunction() || moduleProgramNode->usesEval())
{
    ASSERT_UNUSED(parentScopeTDZVariables, !parentScopeTDZVariables->size());

    for (auto& constantRegister : m_linkTimeConstantRegisters)
        constantRegister = nullptr;

    if (m_isBuiltinFunction)
        m_shouldEmitDebugHooks = false;

    allocateCalleeSaveSpace();

    SymbolTable* moduleEnvironmentSymbolTable = SymbolTable::create(*m_vm);
    moduleEnvironmentSymbolTable->setUsesNonStrictEval(m_usesNonStrictEval);
    moduleEnvironmentSymbolTable->setScopeType(SymbolTable::ScopeType::LexicalScope);

    bool shouldCaptureAllOfTheThings = m_shouldEmitDebugHooks || codeBlock->usesEval();
    if (shouldCaptureAllOfTheThings)
        moduleProgramNode->varDeclarations().markAllVariablesAsCaptured();

    auto captures = [&] (UniquedStringImpl* uid) -> bool {
        return moduleProgramNode->captures(uid);
    };
    auto lookUpVarKind = [&] (UniquedStringImpl* uid, const VariableEnvironmentEntry& entry) -> VarKind {
        // Allocate the exported variables in the module environment.
        if (entry.isExported())
            return VarKind::Scope;

        // Allocate the namespace variables in the module environment to instantiate
        // it from the outside of the module code.
        if (entry.isImportedNamespace())
            return VarKind::Scope;

        if (entry.isCaptured())
            return VarKind::Scope;
        return captures(uid) ? VarKind::Scope : VarKind::Stack;
    };

    emitEnter();

    allocateAndEmitScope();

    emitCheckTraps();
    
    m_calleeRegister.setIndex(CallFrameSlot::callee);

    m_codeBlock->setNumParameters(1); // Allocate space for "this"

    // Now declare all variables.

    createVariable(m_vm->propertyNames->builtinNames().metaPrivateName(), VarKind::Scope, moduleEnvironmentSymbolTable, VerifyExisting);

    for (auto& entry : moduleProgramNode->varDeclarations()) {
        ASSERT(!entry.value.isLet() && !entry.value.isConst());
        if (!entry.value.isVar()) // This is either a parameter or callee.
            continue;
        // Imported bindings are not allocated in the module environment as usual variables' way.
        // These references remain the "Dynamic" in the unlinked code block. Later, when linking
        // the code block, we resolve the reference to the "ModuleVar".
        if (entry.value.isImported() && !entry.value.isImportedNamespace())
            continue;
        createVariable(Identifier::fromUid(m_vm, entry.key.get()), lookUpVarKind(entry.key.get(), entry.value), moduleEnvironmentSymbolTable, IgnoreExisting);
    }

    VariableEnvironment& lexicalVariables = moduleProgramNode->lexicalVariables();
    instantiateLexicalVariables(lexicalVariables, moduleEnvironmentSymbolTable, ScopeRegisterType::Block, lookUpVarKind);

    // We keep the symbol table in the constant pool.
    RegisterID* constantSymbolTable = nullptr;
    if (vm.typeProfiler())
        constantSymbolTable = addConstantValue(moduleEnvironmentSymbolTable);
    else
        constantSymbolTable = addConstantValue(moduleEnvironmentSymbolTable->cloneScopePart(*m_vm));

    pushTDZVariables(lexicalVariables, TDZCheckOptimization::Optimize, TDZRequirement::UnderTDZ);
    bool isWithScope = false;
    m_lexicalScopeStack.append({ moduleEnvironmentSymbolTable, m_topMostScope, isWithScope, constantSymbolTable->index() });
    emitPrefillStackTDZVariables(lexicalVariables, moduleEnvironmentSymbolTable);

    // makeFunction assumes that there's correct TDZ stack entries.
    // So it should be called after putting our lexical environment to the TDZ stack correctly.

    for (FunctionMetadataNode* function : moduleProgramNode->functionStack()) {
        const auto& iterator = moduleProgramNode->varDeclarations().find(function->ident().impl());
        RELEASE_ASSERT(iterator != moduleProgramNode->varDeclarations().end());
        RELEASE_ASSERT(!iterator->value.isImported());

        VarKind varKind = lookUpVarKind(iterator->key.get(), iterator->value);
        if (varKind == VarKind::Scope) {
            // http://www.ecma-international.org/ecma-262/6.0/#sec-moduledeclarationinstantiation
            // Section 15.2.1.16.4, step 16-a-iv-1.
            // All heap allocated function declarations should be instantiated when the module environment
            // is created. They include the exported function declarations and not-exported-but-heap-allocated
            // function declarations. This is required because exported function should be instantiated before
            // executing the any module in the dependency graph. This enables the modules to link the imported
            // bindings before executing the any module code.
            //
            // And since function declarations are instantiated before executing the module body code, the spec
            // allows the functions inside the module to be executed before its module body is executed under
            // the circular dependencies. The following is the example.
            //
            // Module A (executed first):
            //    import { b } from "B";
            //    // Here, the module "B" is not executed yet, but the function declaration is already instantiated.
            //    // So we can call the function exported from "B".
            //    b();
            //
            //    export function a() {
            //    }
            //
            // Module B (executed second):
            //    import { a } from "A";
            //
            //    export function b() {
            //        c();
            //    }
            //
            //    // c is not exported, but since it is referenced from the b, we should instantiate it before
            //    // executing the "B" module code.
            //    function c() {
            //        a();
            //    }
            //
            // Module EntryPoint (executed last):
            //    import "B";
            //    import "A";
            //
            m_codeBlock->addFunctionDecl(makeFunction(function));
        } else {
            // Stack allocated functions can be allocated when executing the module's body.
            m_functionsToInitialize.append(std::make_pair(function, NormalFunctionVariable));
        }
    }

    // Remember the constant register offset to the top-most symbol table. This symbol table will be
    // cloned in the code block linking. After that, to create the module environment, we retrieve
    // the cloned symbol table from the linked code block by using this offset.
    codeBlock->setModuleEnvironmentSymbolTableConstantRegisterOffset(constantSymbolTable->index());
}

BytecodeGenerator::~BytecodeGenerator()
{
}

void BytecodeGenerator::initializeDefaultParameterValuesAndSetupFunctionScopeStack(
    FunctionParameters& parameters, bool isSimpleParameterList, FunctionNode* functionNode, SymbolTable* functionSymbolTable, 
    int symbolTableConstantIndex, const ScopedLambda<bool (UniquedStringImpl*)>& captures, bool shouldCreateArgumentsVariableInParameterScope)
{
    Vector<std::pair<Identifier, RefPtr<RegisterID>>> valuesToMoveIntoVars;
    ASSERT(!(isSimpleParameterList && shouldCreateArgumentsVariableInParameterScope));
    if (!isSimpleParameterList) {
        // Refer to the ES6 spec section 9.2.12: http://www.ecma-international.org/ecma-262/6.0/index.html#sec-functiondeclarationinstantiation
        // This implements step 21.
        VariableEnvironment environment;
        Vector<Identifier> allParameterNames; 
        for (unsigned i = 0; i < parameters.size(); i++)
            parameters.at(i).first->collectBoundIdentifiers(allParameterNames);
        if (shouldCreateArgumentsVariableInParameterScope)
            allParameterNames.append(propertyNames().arguments);
        IdentifierSet parameterSet;
        for (auto& ident : allParameterNames) {
            parameterSet.add(ident.impl());
            auto addResult = environment.add(ident);
            addResult.iterator->value.setIsLet(); // When we have default parameter expressions, parameters act like "let" variables.
            if (captures(ident.impl()))
                addResult.iterator->value.setIsCaptured();
        }
        // This implements step 25 of section 9.2.12.
        pushLexicalScopeInternal(environment, TDZCheckOptimization::Optimize, NestedScopeType::IsNotNested, nullptr, TDZRequirement::UnderTDZ, ScopeType::LetConstScope, ScopeRegisterType::Block);

        if (shouldCreateArgumentsVariableInParameterScope) {
            Variable argumentsVariable = variable(propertyNames().arguments); 
            initializeVariable(argumentsVariable, m_argumentsRegister);
            liftTDZCheckIfPossible(argumentsVariable);
        }

        RefPtr<RegisterID> temp = newTemporary();
        for (unsigned i = 0; i < parameters.size(); i++) {
            std::pair<DestructuringPatternNode*, ExpressionNode*> parameter = parameters.at(i);
            if (parameter.first->isRestParameter())
                continue;
            if ((i + 1) < m_parameters.size())
                move(temp.get(), &m_parameters[i + 1]);
            else
                emitGetArgument(temp.get(), i);
            if (parameter.second) {
                RefPtr<RegisterID> condition = emitIsUndefined(newTemporary(), temp.get());
                Ref<Label> skipDefaultParameterBecauseNotUndefined = newLabel();
                emitJumpIfFalse(condition.get(), skipDefaultParameterBecauseNotUndefined.get());
                emitNode(temp.get(), parameter.second);
                emitLabel(skipDefaultParameterBecauseNotUndefined.get());
            }

            parameter.first->bindValue(*this, temp.get());
        }

        // Final act of weirdness for default parameters. If a "var" also
        // has the same name as a parameter, it should start out as the
        // value of that parameter. Note, though, that they will be distinct
        // bindings.
        // This is step 28 of section 9.2.12. 
        for (auto& entry : functionNode->varDeclarations()) {
            if (!entry.value.isVar()) // This is either a parameter or callee.
                continue;

            if (parameterSet.contains(entry.key)) {
                Identifier ident = Identifier::fromUid(m_vm, entry.key.get());
                Variable var = variable(ident);
                RegisterID* scope = emitResolveScope(nullptr, var);
                RefPtr<RegisterID> value = emitGetFromScope(newTemporary(), scope, var, DoNotThrowIfNotFound);
                valuesToMoveIntoVars.append(std::make_pair(ident, value));
            }
        }

        // Functions with default parameter expressions must have a separate environment
        // record for parameters and "var"s. The "var" environment record must have the
        // parameter environment record as its parent.
        // See step 28 of section 9.2.12.
        bool hasCapturedVariables = !!m_lexicalEnvironmentRegister; 
        initializeVarLexicalEnvironment(symbolTableConstantIndex, functionSymbolTable, hasCapturedVariables);
    }

    // This completes step 28 of section 9.2.12.
    for (unsigned i = 0; i < valuesToMoveIntoVars.size(); i++) {
        ASSERT(!isSimpleParameterList);
        Variable var = variable(valuesToMoveIntoVars[i].first);
        RegisterID* scope = emitResolveScope(nullptr, var);
        emitPutToScope(scope, var, valuesToMoveIntoVars[i].second.get(), DoNotThrowIfNotFound, InitializationMode::NotInitialization);
    }
}

bool BytecodeGenerator::needsDerivedConstructorInArrowFunctionLexicalEnvironment()
{
    ASSERT(m_codeBlock->isClassContext() || !(isConstructor() && constructorKind() == ConstructorKind::Extends));
    return m_codeBlock->isClassContext() && isSuperUsedInInnerArrowFunction();
}

void BytecodeGenerator::initializeArrowFunctionContextScopeIfNeeded(SymbolTable* functionSymbolTable, bool canReuseLexicalEnvironment)
{
    ASSERT(!m_arrowFunctionContextLexicalEnvironmentRegister);

    if (canReuseLexicalEnvironment && m_lexicalEnvironmentRegister) {
        RELEASE_ASSERT(!m_codeBlock->isArrowFunction());
        RELEASE_ASSERT(functionSymbolTable);

        m_arrowFunctionContextLexicalEnvironmentRegister = m_lexicalEnvironmentRegister;
        
        ScopeOffset offset;
        
        if (isThisUsedInInnerArrowFunction()) {
            offset = functionSymbolTable->takeNextScopeOffset(NoLockingNecessary);
            functionSymbolTable->set(NoLockingNecessary, propertyNames().thisIdentifier.impl(), SymbolTableEntry(VarOffset(offset)));
        }

        if (m_codeType == FunctionCode && isNewTargetUsedInInnerArrowFunction()) {
            offset = functionSymbolTable->takeNextScopeOffset();
            functionSymbolTable->set(NoLockingNecessary, propertyNames().builtinNames().newTargetLocalPrivateName().impl(), SymbolTableEntry(VarOffset(offset)));
        }
        
        if (needsDerivedConstructorInArrowFunctionLexicalEnvironment()) {
            offset = functionSymbolTable->takeNextScopeOffset(NoLockingNecessary);
            functionSymbolTable->set(NoLockingNecessary, propertyNames().builtinNames().derivedConstructorPrivateName().impl(), SymbolTableEntry(VarOffset(offset)));
        }

        return;
    }

    VariableEnvironment environment;

    if (isThisUsedInInnerArrowFunction()) {
        auto addResult = environment.add(propertyNames().thisIdentifier);
        addResult.iterator->value.setIsCaptured();
        addResult.iterator->value.setIsLet();
    }
    
    if (m_codeType == FunctionCode && isNewTargetUsedInInnerArrowFunction()) {
        auto addTarget = environment.add(propertyNames().builtinNames().newTargetLocalPrivateName());
        addTarget.iterator->value.setIsCaptured();
        addTarget.iterator->value.setIsLet();
    }

    if (needsDerivedConstructorInArrowFunctionLexicalEnvironment()) {
        auto derivedConstructor = environment.add(propertyNames().builtinNames().derivedConstructorPrivateName());
        derivedConstructor.iterator->value.setIsCaptured();
        derivedConstructor.iterator->value.setIsLet();
    }

    if (environment.size() > 0) {
        size_t size = m_lexicalScopeStack.size();
        pushLexicalScopeInternal(environment, TDZCheckOptimization::Optimize, NestedScopeType::IsNotNested, nullptr, TDZRequirement::UnderTDZ, ScopeType::LetConstScope, ScopeRegisterType::Block);

        ASSERT_UNUSED(size, m_lexicalScopeStack.size() == size + 1);

        m_arrowFunctionContextLexicalEnvironmentRegister = m_lexicalScopeStack.last().m_scope;
    }
}

RegisterID* BytecodeGenerator::initializeNextParameter()
{
    VirtualRegister reg = virtualRegisterForArgument(m_codeBlock->numParameters());
    m_parameters.grow(m_parameters.size() + 1);
    auto& parameter = registerFor(reg);
    parameter.setIndex(reg.offset());
    m_codeBlock->addParameter();
    return &parameter;
}

void BytecodeGenerator::initializeParameters(FunctionParameters& parameters)
{
    // Make sure the code block knows about all of our parameters, and make sure that parameters
    // needing destructuring are noted.
    m_thisRegister.setIndex(initializeNextParameter()->index()); // this

    bool nonSimpleArguments = false;
    for (unsigned i = 0; i < parameters.size(); ++i) {
        auto parameter = parameters.at(i);
        auto pattern = parameter.first;
        if (pattern->isRestParameter()) {
            RELEASE_ASSERT(!m_restParameter);
            m_restParameter = static_cast<RestParameterNode*>(pattern);
            nonSimpleArguments = true;
            continue;
        }
        if (parameter.second) {
            nonSimpleArguments = true;
            continue;
        }
        if (!nonSimpleArguments)
            initializeNextParameter();
    }
}

void BytecodeGenerator::initializeVarLexicalEnvironment(int symbolTableConstantIndex, SymbolTable* functionSymbolTable, bool hasCapturedVariables)
{
    if (hasCapturedVariables) {
        RELEASE_ASSERT(m_lexicalEnvironmentRegister);
        OpCreateLexicalEnvironment::emit(this, m_lexicalEnvironmentRegister, scopeRegister(), VirtualRegister { symbolTableConstantIndex }, addConstantValue(jsUndefined()));

        OpMov::emit(this, scopeRegister(), m_lexicalEnvironmentRegister);

        pushLocalControlFlowScope();
    }
    bool isWithScope = false;
    m_lexicalScopeStack.append({ functionSymbolTable, m_lexicalEnvironmentRegister, isWithScope, symbolTableConstantIndex });
    m_varScopeLexicalScopeStackIndex = m_lexicalScopeStack.size() - 1;
}

UniquedStringImpl* BytecodeGenerator::visibleNameForParameter(DestructuringPatternNode* pattern)
{
    if (pattern->isBindingNode()) {
        const Identifier& ident = static_cast<const BindingNode*>(pattern)->boundProperty();
        if (!m_functions.contains(ident.impl()))
            return ident.impl();
    }
    return nullptr;
}

RegisterID* BytecodeGenerator::newRegister()
{
    m_calleeLocals.append(virtualRegisterForLocal(m_calleeLocals.size()));
    int numCalleeLocals = std::max<int>(m_codeBlock->m_numCalleeLocals, m_calleeLocals.size());
    numCalleeLocals = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), numCalleeLocals);
    m_codeBlock->m_numCalleeLocals = numCalleeLocals;
    return &m_calleeLocals.last();
}

void BytecodeGenerator::reclaimFreeRegisters()
{
    shrinkToFit(m_calleeLocals);
}

RegisterID* BytecodeGenerator::newBlockScopeVariable()
{
    reclaimFreeRegisters();

    return newRegister();
}

RegisterID* BytecodeGenerator::newTemporary()
{
    reclaimFreeRegisters();

    RegisterID* result = newRegister();
    result->setTemporary();
    return result;
}

Ref<LabelScope> BytecodeGenerator::newLabelScope(LabelScope::Type type, const Identifier* name)
{
    shrinkToFit(m_labelScopes);

    // Allocate new label scope.
    m_labelScopes.append(type, name, labelScopeDepth(), newLabel(), type == LabelScope::Loop ? RefPtr<Label>(newLabel()) : RefPtr<Label>()); // Only loops have continue targets.
    return m_labelScopes.last();
}

Ref<Label> BytecodeGenerator::newLabel()
{
    shrinkToFit(m_labels);

    // Allocate new label ID.
    m_labels.append();
    return m_labels.last();
}

Ref<Label> BytecodeGenerator::newEmittedLabel()
{
    Ref<Label> label = newLabel();
    emitLabel(label.get());
    return label;
}

void BytecodeGenerator::recordOpcode(OpcodeID opcodeID)
{
    ASSERT(m_lastOpcodeID == op_end || (m_lastOpcodeID == m_lastInstruction->opcodeID() && m_writer.position() == m_lastInstruction.offset() + m_lastInstruction->size()));
    m_lastInstruction = m_writer.ref();
    m_lastOpcodeID = opcodeID;
}

void BytecodeGenerator::alignWideOpcode()
{
#if CPU(NEEDS_ALIGNED_ACCESS)
    while ((m_writer.position() + 1) % OpcodeSize::Wide)
        OpNop::emit<OpcodeSize::Narrow>(this);
#endif
}

void BytecodeGenerator::emitLabel(Label& l0)
{
    unsigned newLabelIndex = instructions().size();
    l0.setLocation(*this, newLabelIndex);

    if (m_codeBlock->numberOfJumpTargets()) {
        unsigned lastLabelIndex = m_codeBlock->lastJumpTarget();
        ASSERT(lastLabelIndex <= newLabelIndex);
        if (newLabelIndex == lastLabelIndex) {
            // Peephole optimizations have already been disabled by emitting the last label
            return;
        }
    }

    m_codeBlock->addJumpTarget(newLabelIndex);

    // This disables peephole optimizations when an instruction is a jump target
    m_lastOpcodeID = op_end;
}

void BytecodeGenerator::emitEnter()
{
    OpEnter::emit(this);

    if (LIKELY(Options::optimizeRecursiveTailCalls())) {
        // We must add the end of op_enter as a potential jump target, because the bytecode parser may decide to split its basic block
        // to have somewhere to jump to if there is a recursive tail-call that points to this function.
        m_codeBlock->addJumpTarget(instructions().size());
        // This disables peephole optimizations when an instruction is a jump target
        m_lastOpcodeID = op_end;
    }
}

void BytecodeGenerator::emitLoopHint()
{
    OpLoopHint::emit(this);
    emitCheckTraps();
}

void BytecodeGenerator::emitJump(Label& target)
{
    OpJmp::emit(this, target.bind(this));
}

void BytecodeGenerator::emitCheckTraps()
{
    OpCheckTraps::emit(this);
}

void ALWAYS_INLINE BytecodeGenerator::rewind()
{
    ASSERT(m_lastInstruction.isValid());
    m_lastOpcodeID = op_end;
    m_writer.rewind(m_lastInstruction);
}

template<typename BinOp, typename JmpOp>
bool BytecodeGenerator::fuseCompareAndJump(RegisterID* cond, Label& target, bool swapOperands)
{
    auto binop = m_lastInstruction->as<BinOp>();
    if (cond->index() == binop.dst.offset() && cond->isTemporary() && !cond->refCount()) {
        rewind();

        if (swapOperands)
            std::swap(binop.lhs, binop.rhs);

        JmpOp::emit(this, binop.lhs, binop.rhs, target.bind(this));
        return true;
    }
    return false;
}

template<typename UnaryOp, typename JmpOp>
bool BytecodeGenerator::fuseTestAndJmp(RegisterID* cond, Label& target)
{
    auto unop = m_lastInstruction->as<UnaryOp>();
    if (cond->index() == unop.dst.offset() && cond->isTemporary() && !cond->refCount()) {
        rewind();

        JmpOp::emit(this, unop.operand, target.bind(this));
        return true;
    }
    return false;
}

void BytecodeGenerator::emitJumpIfTrue(RegisterID* cond, Label& target)
{

    if (m_lastOpcodeID == op_less) {
        if (fuseCompareAndJump<OpLess, OpJless>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_lesseq) {
        if (fuseCompareAndJump<OpLesseq, OpJlesseq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_greater) {
        if (fuseCompareAndJump<OpGreater, OpJgreater>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_greatereq) {
        if (fuseCompareAndJump<OpGreatereq, OpJgreatereq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_eq) {
        if (fuseCompareAndJump<OpEq, OpJeq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_stricteq) {
        if (fuseCompareAndJump<OpStricteq, OpJstricteq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_neq) {
        if (fuseCompareAndJump<OpNeq, OpJneq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_nstricteq) {
        if (fuseCompareAndJump<OpNstricteq, OpJnstricteq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_below) {
        if (fuseCompareAndJump<OpBelow, OpJbelow>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_beloweq) {
        if (fuseCompareAndJump<OpBeloweq, OpJbeloweq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_eq_null && target.isForward()) {
        if (fuseTestAndJmp<OpEqNull, OpJeqNull>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_neq_null && target.isForward()) {
        if (fuseTestAndJmp<OpNeqNull, OpJneqNull>(cond, target))
            return;
    }

    OpJtrue::emit(this, cond, target.bind(this));
}

void BytecodeGenerator::emitJumpIfFalse(RegisterID* cond, Label& target)
{
    if (m_lastOpcodeID == op_less && target.isForward()) {
        if (fuseCompareAndJump<OpLess, OpJnless>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_lesseq && target.isForward()) {
        if (fuseCompareAndJump<OpLesseq, OpJnlesseq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_greater && target.isForward()) {
        if (fuseCompareAndJump<OpGreater, OpJngreater>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_greatereq && target.isForward()) {
        if (fuseCompareAndJump<OpGreatereq, OpJngreatereq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_eq && target.isForward()) {
        if (fuseCompareAndJump<OpEq, OpJneq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_stricteq && target.isForward()) {
        if (fuseCompareAndJump<OpStricteq, OpJnstricteq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_neq && target.isForward()) {
        if (fuseCompareAndJump<OpNeq, OpJeq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_nstricteq && target.isForward()) {
        if (fuseCompareAndJump<OpNstricteq, OpJstricteq>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_below && target.isForward()) {
        if (fuseCompareAndJump<OpBelow, OpJbeloweq>(cond, target, true))
            return;
    } else if (m_lastOpcodeID == op_beloweq && target.isForward()) {
        if (fuseCompareAndJump<OpBeloweq, OpJbelow>(cond, target, true))
            return;
    } else if (m_lastOpcodeID == op_not) {
        if (fuseTestAndJmp<OpNot, OpJtrue>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_eq_null && target.isForward()) {
        if (fuseTestAndJmp<OpEqNull, OpJneqNull>(cond, target))
            return;
    } else if (m_lastOpcodeID == op_neq_null && target.isForward()) {
        if (fuseTestAndJmp<OpNeqNull, OpJeqNull>(cond, target))
            return;
    }

    OpJfalse::emit(this, cond, target.bind(this));
}

void BytecodeGenerator::emitJumpIfNotFunctionCall(RegisterID* cond, Label& target)
{
    OpJneqPtr::emit(this, cond, Special::CallFunction, target.bind(this));
}

void BytecodeGenerator::emitJumpIfNotFunctionApply(RegisterID* cond, Label& target)
{
    OpJneqPtr::emit(this, cond, Special::ApplyFunction, target.bind(this));
}

bool BytecodeGenerator::hasConstant(const Identifier& ident) const
{
    UniquedStringImpl* rep = ident.impl();
    return m_identifierMap.contains(rep);
}

unsigned BytecodeGenerator::addConstant(const Identifier& ident)
{
    UniquedStringImpl* rep = ident.impl();
    IdentifierMap::AddResult result = m_identifierMap.add(rep, m_codeBlock->numberOfIdentifiers());
    if (result.isNewEntry)
        m_codeBlock->addIdentifier(ident);

    return result.iterator->value;
}

// We can't hash JSValue(), so we use a dedicated data member to cache it.
RegisterID* BytecodeGenerator::addConstantEmptyValue()
{
    if (!m_emptyValueRegister) {
        int index = addConstantIndex();
        m_codeBlock->addConstant(JSValue());
        m_emptyValueRegister = &m_constantPoolRegisters[index];
    }

    return m_emptyValueRegister;
}

RegisterID* BytecodeGenerator::addConstantValue(JSValue v, SourceCodeRepresentation sourceCodeRepresentation)
{
    if (!v)
        return addConstantEmptyValue();

    int index = m_nextConstantOffset;

    if (sourceCodeRepresentation == SourceCodeRepresentation::Double && v.isInt32())
        v = jsDoubleNumber(v.asNumber());
    EncodedJSValueWithRepresentation valueMapKey { JSValue::encode(v), sourceCodeRepresentation };
    JSValueMap::AddResult result = m_jsValueMap.add(valueMapKey, m_nextConstantOffset);
    if (result.isNewEntry) {
        addConstantIndex();
        m_codeBlock->addConstant(v, sourceCodeRepresentation);
    } else
        index = result.iterator->value;
    return &m_constantPoolRegisters[index];
}

RegisterID* BytecodeGenerator::moveLinkTimeConstant(RegisterID* dst, LinkTimeConstant type)
{
    unsigned constantIndex = static_cast<unsigned>(type);
    if (!m_linkTimeConstantRegisters[constantIndex]) {
        int index = addConstantIndex();
        m_codeBlock->addConstant(type);
        m_linkTimeConstantRegisters[constantIndex] = &m_constantPoolRegisters[index];
    }

    if (!dst)
        return m_linkTimeConstantRegisters[constantIndex];

    OpMov::emit(this, dst, m_linkTimeConstantRegisters[constantIndex]);

    return dst;
}

RegisterID* BytecodeGenerator::moveEmptyValue(RegisterID* dst)
{
    RefPtr<RegisterID> emptyValue = addConstantEmptyValue();

    OpMov::emit(this, dst, emptyValue.get());

    return dst;
}

RegisterID* BytecodeGenerator::emitMove(RegisterID* dst, RegisterID* src)
{
    ASSERT(src != m_emptyValueRegister);

    m_staticPropertyAnalyzer.mov(dst, src);
    OpMov::emit(this, dst, src);

    return dst;
}

RegisterID* BytecodeGenerator::emitUnaryOp(OpcodeID opcodeID, RegisterID* dst, RegisterID* src, OperandTypes types)
{
    switch (opcodeID) {
    case op_not:
        emitUnaryOp<OpNot>(dst, src);
        break;
    case op_negate:
        OpNegate::emit(this, dst, src, types);
        break;
    case op_bitnot:
        emitUnaryOp<OpBitnot>(dst, src);
        break;
    case op_to_number:
        emitUnaryOp<OpToNumber>(dst, src);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return dst;
}

RegisterID* BytecodeGenerator::emitBinaryOp(OpcodeID opcodeID, RegisterID* dst, RegisterID* src1, RegisterID* src2, OperandTypes types)
{
    switch (opcodeID) {
    case op_eq:
        return emitBinaryOp<OpEq>(dst, src1, src2, types);
    case op_neq:
        return emitBinaryOp<OpNeq>(dst, src1, src2, types);
    case op_stricteq:
        return emitBinaryOp<OpStricteq>(dst, src1, src2, types);
    case op_nstricteq:
        return emitBinaryOp<OpNstricteq>(dst, src1, src2, types);
    case op_less:
        return emitBinaryOp<OpLess>(dst, src1, src2, types);
    case op_lesseq:
        return emitBinaryOp<OpLesseq>(dst, src1, src2, types);
    case op_greater:
        return emitBinaryOp<OpGreater>(dst, src1, src2, types);
    case op_greatereq:
        return emitBinaryOp<OpGreatereq>(dst, src1, src2, types);
    case op_below:
        return emitBinaryOp<OpBelow>(dst, src1, src2, types);
    case op_beloweq:
        return emitBinaryOp<OpBeloweq>(dst, src1, src2, types);
    case op_mod:
        return emitBinaryOp<OpMod>(dst, src1, src2, types);
    case op_pow:
        return emitBinaryOp<OpPow>(dst, src1, src2, types);
    case op_lshift:
        return emitBinaryOp<OpLshift>(dst, src1, src2, types);
    case op_rshift:
        return emitBinaryOp<OpRshift>(dst, src1, src2, types);
    case op_urshift:
        return emitBinaryOp<OpUrshift>(dst, src1, src2, types);
    case op_add:
        return emitBinaryOp<OpAdd>(dst, src1, src2, types);
    case op_mul:
        return emitBinaryOp<OpMul>(dst, src1, src2, types);
    case op_div:
        return emitBinaryOp<OpDiv>(dst, src1, src2, types);
    case op_sub:
        return emitBinaryOp<OpSub>(dst, src1, src2, types);
    case op_bitand:
        return emitBinaryOp<OpBitand>(dst, src1, src2, types);
    case op_bitxor:
        return emitBinaryOp<OpBitxor>(dst, src1, src2, types);
    case op_bitor:
        return emitBinaryOp<OpBitor>(dst, src1, src2, types);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

RegisterID* BytecodeGenerator::emitToObject(RegisterID* dst, RegisterID* src, const Identifier& message)
{
    OpToObject::emit(this, dst, src, addConstant(message));
    return dst;
}

RegisterID* BytecodeGenerator::emitToNumber(RegisterID* dst, RegisterID* src)
{
    return emitUnaryOp<OpToNumber>(dst, src);
}

RegisterID* BytecodeGenerator::emitToString(RegisterID* dst, RegisterID* src)
{
    return emitUnaryOp<OpToString>(dst, src);
}

RegisterID* BytecodeGenerator::emitTypeOf(RegisterID* dst, RegisterID* src)
{
    return emitUnaryOp<OpTypeof>(dst, src);
}

RegisterID* BytecodeGenerator::emitInc(RegisterID* srcDst)
{
    OpInc::emit(this, srcDst);
    return srcDst;
}

RegisterID* BytecodeGenerator::emitDec(RegisterID* srcDst)
{
    OpDec::emit(this, srcDst);
    return srcDst;
}

template<typename EqOp>
RegisterID* BytecodeGenerator::emitEqualityOp(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    if (m_lastInstruction->is<OpTypeof>()) {
        auto op = m_lastInstruction->as<OpTypeof>();
        if (src1->index() == op.dst.offset()
            && src1->isTemporary()
            && m_codeBlock->isConstantRegisterIndex(src2->index())
            && m_codeBlock->constantRegister(src2->index()).get().isString()) {
            const String& value = asString(m_codeBlock->constantRegister(src2->index()).get())->tryGetValue();
            if (value == "undefined") {
                rewind();
                OpIsUndefined::emit(this, dst, op.value);
                return dst;
            }
            if (value == "boolean") {
                rewind();
                OpIsBoolean::emit(this, dst, op.value);
                return dst;
            }
            if (value == "number") {
                rewind();
                OpIsNumber::emit(this, dst, op.value);
                return dst;
            }
            if (value == "string") {
                rewind();
                OpIsCellWithType::emit(this, dst, op.value, StringType);
                return dst;
            }
            if (value == "symbol") {
                rewind();
                OpIsCellWithType::emit(this, dst, op.value, SymbolType);
                return dst;
            }
            if (Options::useBigInt() && value == "bigint") {
                rewind();
                OpIsCellWithType::emit(this, dst, op.value, BigIntType);
                return dst;
            }
            if (value == "object") {
                rewind();
                OpIsObjectOrNull::emit(this, dst, op.value);
                return dst;
            }
            if (value == "function") {
                rewind();
                OpIsFunction::emit(this, dst, op.value);
                return dst;
            }
        }
    }

    EqOp::emit(this, dst, src1, src2);
    return dst;
}

void BytecodeGenerator::emitTypeProfilerExpressionInfo(const JSTextPosition& startDivot, const JSTextPosition& endDivot)
{
    ASSERT(vm()->typeProfiler());

    unsigned start = startDivot.offset; // Ranges are inclusive of their endpoints, AND 0 indexed.
    unsigned end = endDivot.offset - 1; // End Ranges already go one past the inclusive range, so subtract 1.
    unsigned instructionOffset = instructions().size() - 1;
    m_codeBlock->addTypeProfilerExpressionInfo(instructionOffset, start, end);
}

void BytecodeGenerator::emitProfileType(RegisterID* registerToProfile, ProfileTypeBytecodeFlag flag)
{
    if (!vm()->typeProfiler())
        return;

    if (!registerToProfile)
        return;

    OpProfileType::emit(this, registerToProfile, 0, flag, { }, resolveType());

    // Don't emit expression info for this version of profile type. This generally means
    // we're profiling information for something that isn't in the actual text of a JavaScript
    // program. For example, implicit return undefined from a function call.
}

void BytecodeGenerator::emitProfileType(RegisterID* registerToProfile, const JSTextPosition& startDivot, const JSTextPosition& endDivot)
{
    emitProfileType(registerToProfile, ProfileTypeBytecodeDoesNotHaveGlobalID, startDivot, endDivot);
}

void BytecodeGenerator::emitProfileType(RegisterID* registerToProfile, ProfileTypeBytecodeFlag flag, const JSTextPosition& startDivot, const JSTextPosition& endDivot)
{
    if (!vm()->typeProfiler())
        return;

    if (!registerToProfile)
        return;

    OpProfileType::emit(this, registerToProfile, 0,  flag, { }, resolveType());
    emitTypeProfilerExpressionInfo(startDivot, endDivot);
}

void BytecodeGenerator::emitProfileType(RegisterID* registerToProfile, const Variable& var, const JSTextPosition& startDivot, const JSTextPosition& endDivot)
{
    if (!vm()->typeProfiler())
        return;

    if (!registerToProfile)
        return;

    ProfileTypeBytecodeFlag flag;
    int symbolTableOrScopeDepth;
    if (var.local() || var.offset().isScope()) {
        flag = ProfileTypeBytecodeLocallyResolved;
        ASSERT(var.symbolTableConstantIndex());
        symbolTableOrScopeDepth = var.symbolTableConstantIndex();
    } else {
        flag = ProfileTypeBytecodeClosureVar;
        symbolTableOrScopeDepth = localScopeDepth();
    }

    OpProfileType::emit(this, registerToProfile, symbolTableOrScopeDepth, flag, addConstant(var.ident()), resolveType());
    emitTypeProfilerExpressionInfo(startDivot, endDivot);
}

void BytecodeGenerator::emitProfileControlFlow(int textOffset)
{
    if (vm()->controlFlowProfiler()) {
        RELEASE_ASSERT(textOffset >= 0);

        OpProfileControlFlow::emit(this, textOffset);
        m_codeBlock->addOpProfileControlFlowBytecodeOffset(m_lastInstruction.offset());
    }
}

unsigned BytecodeGenerator::addConstantIndex()
{
    unsigned index = m_nextConstantOffset;
    m_constantPoolRegisters.append(FirstConstantRegisterIndex + m_nextConstantOffset);
    ++m_nextConstantOffset;
    return index;
}

RegisterID* BytecodeGenerator::emitLoad(RegisterID* dst, bool b)
{
    return emitLoad(dst, jsBoolean(b));
}

RegisterID* BytecodeGenerator::emitLoad(RegisterID* dst, const Identifier& identifier)
{
    ASSERT(!identifier.isSymbol());
    JSString*& stringInMap = m_stringMap.add(identifier.impl(), nullptr).iterator->value;
    if (!stringInMap)
        stringInMap = jsOwnedString(vm(), identifier.string());

    return emitLoad(dst, JSValue(stringInMap));
}

RegisterID* BytecodeGenerator::emitLoad(RegisterID* dst, JSValue v, SourceCodeRepresentation sourceCodeRepresentation)
{
    RegisterID* constantID = addConstantValue(v, sourceCodeRepresentation);
    if (dst)
        return move(dst, constantID);
    return constantID;
}

RegisterID* BytecodeGenerator::emitLoad(RegisterID* dst, IdentifierSet& set)
{
    for (const auto& entry : m_codeBlock->constantIdentifierSets()) {
        if (entry.first != set)
            continue;
        
        return &m_constantPoolRegisters[entry.second];
    }
    
    unsigned index = addConstantIndex();
    m_codeBlock->addSetConstant(set);
    RegisterID* m_setRegister = &m_constantPoolRegisters[index];
    
    if (dst)
        return move(dst, m_setRegister);
    
    return m_setRegister;
}

RegisterID* BytecodeGenerator::emitLoadGlobalObject(RegisterID* dst)
{
    if (!m_globalObjectRegister) {
        int index = addConstantIndex();
        m_codeBlock->addConstant(JSValue());
        m_globalObjectRegister = &m_constantPoolRegisters[index];
        m_codeBlock->setGlobalObjectRegister(VirtualRegister(index));
    }
    if (dst)
        move(dst, m_globalObjectRegister);
    return m_globalObjectRegister;
}

template<typename LookUpVarKindFunctor>
bool BytecodeGenerator::instantiateLexicalVariables(const VariableEnvironment& lexicalVariables, SymbolTable* symbolTable, ScopeRegisterType scopeRegisterType, LookUpVarKindFunctor lookUpVarKind)
{
    bool hasCapturedVariables = false;
    {
        for (auto& entry : lexicalVariables) {
            ASSERT(entry.value.isLet() || entry.value.isConst() || entry.value.isFunction());
            ASSERT(!entry.value.isVar());
            SymbolTableEntry symbolTableEntry = symbolTable->get(NoLockingNecessary, entry.key.get());
            ASSERT(symbolTableEntry.isNull());

            // Imported bindings which are not the namespace bindings are not allocated
            // in the module environment as usual variables' way.
            // And since these types of the variables only seen in the module environment,
            // other lexical environment need not to take care this.
            if (entry.value.isImported() && !entry.value.isImportedNamespace())
                continue;

            VarKind varKind = lookUpVarKind(entry.key.get(), entry.value);
            VarOffset varOffset;
            if (varKind == VarKind::Scope) {
                varOffset = VarOffset(symbolTable->takeNextScopeOffset(NoLockingNecessary));
                hasCapturedVariables = true;
            } else {
                ASSERT(varKind == VarKind::Stack);
                RegisterID* local;
                if (scopeRegisterType == ScopeRegisterType::Block) {
                    local = newBlockScopeVariable();
                    local->ref();
                } else
                    local = addVar();
                varOffset = VarOffset(local->virtualRegister());
            }

            SymbolTableEntry newEntry(varOffset, static_cast<unsigned>(entry.value.isConst() ? PropertyAttribute::ReadOnly : PropertyAttribute::None));
            symbolTable->add(NoLockingNecessary, entry.key.get(), newEntry);
        }
    }
    return hasCapturedVariables;
}

void BytecodeGenerator::emitPrefillStackTDZVariables(const VariableEnvironment& lexicalVariables, SymbolTable* symbolTable)
{
    // Prefill stack variables with the TDZ empty value.
    // Scope variables will be initialized to the TDZ empty value when JSLexicalEnvironment is allocated.
    for (auto& entry : lexicalVariables) {
        // Imported bindings which are not the namespace bindings are not allocated
        // in the module environment as usual variables' way.
        // And since these types of the variables only seen in the module environment,
        // other lexical environment need not to take care this.
        if (entry.value.isImported() && !entry.value.isImportedNamespace())
            continue;

        if (entry.value.isFunction())
            continue;

        SymbolTableEntry symbolTableEntry = symbolTable->get(NoLockingNecessary, entry.key.get());
        ASSERT(!symbolTableEntry.isNull());
        VarOffset offset = symbolTableEntry.varOffset();
        if (offset.isScope())
            continue;

        ASSERT(offset.isStack());
        moveEmptyValue(&registerFor(offset.stackOffset()));
    }
}

void BytecodeGenerator::pushLexicalScope(VariableEnvironmentNode* node, TDZCheckOptimization tdzCheckOptimization, NestedScopeType nestedScopeType, RegisterID** constantSymbolTableResult, bool shouldInitializeBlockScopedFunctions)
{
    VariableEnvironment& environment = node->lexicalVariables();
    RegisterID* constantSymbolTableResultTemp = nullptr;
    pushLexicalScopeInternal(environment, tdzCheckOptimization, nestedScopeType, &constantSymbolTableResultTemp, TDZRequirement::UnderTDZ, ScopeType::LetConstScope, ScopeRegisterType::Block);

    if (shouldInitializeBlockScopedFunctions)
        initializeBlockScopedFunctions(environment, node->functionStack(), constantSymbolTableResultTemp);

    if (constantSymbolTableResult && constantSymbolTableResultTemp)
        *constantSymbolTableResult = constantSymbolTableResultTemp;
}

void BytecodeGenerator::pushLexicalScopeInternal(VariableEnvironment& environment, TDZCheckOptimization tdzCheckOptimization, NestedScopeType nestedScopeType,
    RegisterID** constantSymbolTableResult, TDZRequirement tdzRequirement, ScopeType scopeType, ScopeRegisterType scopeRegisterType)
{
    if (!environment.size())
        return;

    if (m_shouldEmitDebugHooks)
        environment.markAllVariablesAsCaptured();

    SymbolTable* symbolTable = SymbolTable::create(*m_vm);
    switch (scopeType) {
    case ScopeType::CatchScope:
        symbolTable->setScopeType(SymbolTable::ScopeType::CatchScope);
        break;
    case ScopeType::LetConstScope:
        symbolTable->setScopeType(SymbolTable::ScopeType::LexicalScope);
        break;
    case ScopeType::FunctionNameScope:
        symbolTable->setScopeType(SymbolTable::ScopeType::FunctionNameScope);
        break;
    }

    if (nestedScopeType == NestedScopeType::IsNested)
        symbolTable->markIsNestedLexicalScope();

    auto lookUpVarKind = [] (UniquedStringImpl*, const VariableEnvironmentEntry& entry) -> VarKind {
        return entry.isCaptured() ? VarKind::Scope : VarKind::Stack;
    };

    bool hasCapturedVariables = instantiateLexicalVariables(environment, symbolTable, scopeRegisterType, lookUpVarKind);

    RegisterID* newScope = nullptr;
    RegisterID* constantSymbolTable = nullptr;
    int symbolTableConstantIndex = 0;
    if (vm()->typeProfiler()) {
        constantSymbolTable = addConstantValue(symbolTable);
        symbolTableConstantIndex = constantSymbolTable->index();
    }
    if (hasCapturedVariables) {
        if (scopeRegisterType == ScopeRegisterType::Block) {
            newScope = newBlockScopeVariable();
            newScope->ref();
        } else
            newScope = addVar();
        if (!constantSymbolTable) {
            ASSERT(!vm()->typeProfiler());
            constantSymbolTable = addConstantValue(symbolTable->cloneScopePart(*m_vm));
            symbolTableConstantIndex = constantSymbolTable->index();
        }
        if (constantSymbolTableResult)
            *constantSymbolTableResult = constantSymbolTable;

        OpCreateLexicalEnvironment::emit(this, newScope, scopeRegister(), VirtualRegister { symbolTableConstantIndex }, addConstantValue(tdzRequirement == TDZRequirement::UnderTDZ ? jsTDZValue() : jsUndefined()));

        move(scopeRegister(), newScope);

        pushLocalControlFlowScope();
    }

    bool isWithScope = false;
    m_lexicalScopeStack.append({ symbolTable, newScope, isWithScope, symbolTableConstantIndex });
    pushTDZVariables(environment, tdzCheckOptimization, tdzRequirement);

    if (tdzRequirement == TDZRequirement::UnderTDZ)
        emitPrefillStackTDZVariables(environment, symbolTable);
}

void BytecodeGenerator::initializeBlockScopedFunctions(VariableEnvironment& environment, FunctionStack& functionStack, RegisterID* constantSymbolTable)
{
    /*
     * We must transform block scoped function declarations in strict mode like so:
     *
     * function foo() {
     *     if (c) {
     *           function foo() { ... }
     *           if (bar) { ... }
     *           else { ... }
     *           function baz() { ... }
     *     }
     * }
     *
     * to:
     *
     * function foo() {
     *     if (c) {
     *         let foo = function foo() { ... }
     *         let baz = function baz() { ... }
     *         if (bar) { ... }
     *         else { ... }
     *     }
     * }
     * 
     * But without the TDZ checks.
    */

    if (!environment.size()) {
        RELEASE_ASSERT(!functionStack.size());
        return;
    }

    if (!functionStack.size())
        return;

    SymbolTable* symbolTable = m_lexicalScopeStack.last().m_symbolTable;
    RegisterID* scope = m_lexicalScopeStack.last().m_scope;
    RefPtr<RegisterID> temp = newTemporary();
    int symbolTableIndex = constantSymbolTable ? constantSymbolTable->index() : 0;
    for (FunctionMetadataNode* function : functionStack) {
        const Identifier& name = function->ident();
        auto iter = environment.find(name.impl());
        RELEASE_ASSERT(iter != environment.end());
        RELEASE_ASSERT(iter->value.isFunction());
        // We purposefully don't hold the symbol table lock around this loop because emitNewFunctionExpressionCommon may GC.
        SymbolTableEntry entry = symbolTable->get(NoLockingNecessary, name.impl()); 
        RELEASE_ASSERT(!entry.isNull());
        emitNewFunctionExpressionCommon(temp.get(), function);
        bool isLexicallyScoped = true;
        emitPutToScope(scope, variableForLocalEntry(name, entry, symbolTableIndex, isLexicallyScoped), temp.get(), DoNotThrowIfNotFound, InitializationMode::Initialization);
    }
}

void BytecodeGenerator::hoistSloppyModeFunctionIfNecessary(const Identifier& functionName)
{
    if (m_scopeNode->hasSloppyModeHoistedFunction(functionName.impl())) {
        if (codeType() != EvalCode) {
            Variable currentFunctionVariable = variable(functionName);
            RefPtr<RegisterID> currentValue;
            if (RegisterID* local = currentFunctionVariable.local())
                currentValue = local;
            else {
                RefPtr<RegisterID> scope = emitResolveScope(nullptr, currentFunctionVariable);
                currentValue = emitGetFromScope(newTemporary(), scope.get(), currentFunctionVariable, DoNotThrowIfNotFound);
            }

            ASSERT(m_varScopeLexicalScopeStackIndex);
            ASSERT(*m_varScopeLexicalScopeStackIndex < m_lexicalScopeStack.size());
            LexicalScopeStackEntry varScope = m_lexicalScopeStack[*m_varScopeLexicalScopeStackIndex];
            SymbolTable* varSymbolTable = varScope.m_symbolTable;
            ASSERT(varSymbolTable->scopeType() == SymbolTable::ScopeType::VarScope);
            SymbolTableEntry entry = varSymbolTable->get(NoLockingNecessary, functionName.impl());
            if (functionName == propertyNames().arguments && entry.isNull()) {
                // "arguments" might be put in the parameter scope when we have a non-simple
                // parameter list since "arguments" is visible to expressions inside the
                // parameter evaluation list.
                // e.g:
                // function foo(x = arguments) { { function arguments() { } } }
                RELEASE_ASSERT(*m_varScopeLexicalScopeStackIndex > 0);
                varScope = m_lexicalScopeStack[*m_varScopeLexicalScopeStackIndex - 1];
                SymbolTable* parameterSymbolTable = varScope.m_symbolTable;
                entry = parameterSymbolTable->get(NoLockingNecessary, functionName.impl());
            }
            RELEASE_ASSERT(!entry.isNull());
            bool isLexicallyScoped = false;
            emitPutToScope(varScope.m_scope, variableForLocalEntry(functionName, entry, varScope.m_symbolTableConstantIndex, isLexicallyScoped), currentValue.get(), DoNotThrowIfNotFound, InitializationMode::NotInitialization);
        } else {
            Variable currentFunctionVariable = variable(functionName);
            RefPtr<RegisterID> currentValue;
            if (RegisterID* local = currentFunctionVariable.local())
                currentValue = local;
            else {
                RefPtr<RegisterID> scope = emitResolveScope(nullptr, currentFunctionVariable);
                currentValue = emitGetFromScope(newTemporary(), scope.get(), currentFunctionVariable, DoNotThrowIfNotFound);
            }

            RefPtr<RegisterID> scopeId = emitResolveScopeForHoistingFuncDeclInEval(nullptr, functionName);
            RefPtr<RegisterID> checkResult = emitIsUndefined(newTemporary(), scopeId.get());
            
            Ref<Label> isNotVarScopeLabel = newLabel();
            emitJumpIfTrue(checkResult.get(), isNotVarScopeLabel.get());

            // Put to outer scope
            emitPutToScope(scopeId.get(), functionName, currentValue.get(), DoNotThrowIfNotFound, InitializationMode::NotInitialization);
            emitLabel(isNotVarScopeLabel.get());

        }
    }
}

RegisterID* BytecodeGenerator::emitResolveScopeForHoistingFuncDeclInEval(RegisterID* dst, const Identifier& property)
{
    ASSERT(m_codeType == EvalCode);

    dst = finalDestination(dst);
    OpResolveScopeForHoistingFuncDeclInEval::emit(this, kill(dst), m_topMostScope, addConstant(property));
    return dst;
}

void BytecodeGenerator::popLexicalScope(VariableEnvironmentNode* node)
{
    VariableEnvironment& environment = node->lexicalVariables();
    popLexicalScopeInternal(environment);
}

void BytecodeGenerator::popLexicalScopeInternal(VariableEnvironment& environment)
{
    // NOTE: This function only makes sense for scopes that aren't ScopeRegisterType::Var (only function name scope right now is ScopeRegisterType::Var).
    // This doesn't make sense for ScopeRegisterType::Var because we deref RegisterIDs here.
    if (!environment.size())
        return;

    if (m_shouldEmitDebugHooks)
        environment.markAllVariablesAsCaptured();

    auto stackEntry = m_lexicalScopeStack.takeLast();
    SymbolTable* symbolTable = stackEntry.m_symbolTable;
    bool hasCapturedVariables = false;
    for (auto& entry : environment) {
        if (entry.value.isCaptured()) {
            hasCapturedVariables = true;
            continue;
        }
        SymbolTableEntry symbolTableEntry = symbolTable->get(NoLockingNecessary, entry.key.get());
        ASSERT(!symbolTableEntry.isNull());
        VarOffset offset = symbolTableEntry.varOffset();
        ASSERT(offset.isStack());
        RegisterID* local = &registerFor(offset.stackOffset());
        local->deref();
    }

    if (hasCapturedVariables) {
        RELEASE_ASSERT(stackEntry.m_scope);
        emitPopScope(scopeRegister(), stackEntry.m_scope);
        popLocalControlFlowScope();
        stackEntry.m_scope->deref();
    }

    m_TDZStack.removeLast();
}

void BytecodeGenerator::prepareLexicalScopeForNextForLoopIteration(VariableEnvironmentNode* node, RegisterID* loopSymbolTable)
{
    VariableEnvironment& environment = node->lexicalVariables();
    if (!environment.size())
        return;
    if (m_shouldEmitDebugHooks)
        environment.markAllVariablesAsCaptured();
    if (!environment.hasCapturedVariables())
        return;

    RELEASE_ASSERT(loopSymbolTable);

    // This function needs to do setup for a for loop's activation if any of
    // the for loop's lexically declared variables are captured (that is, variables
    // declared in the loop header, not the loop body). This function needs to
    // make a copy of the current activation and copy the values from the previous
    // activation into the new activation because each iteration of a for loop
    // gets a new activation.

    auto stackEntry = m_lexicalScopeStack.last();
    SymbolTable* symbolTable = stackEntry.m_symbolTable;
    RegisterID* loopScope = stackEntry.m_scope;
    ASSERT(symbolTable->scopeSize());
    ASSERT(loopScope);
    Vector<std::pair<RegisterID*, Identifier>> activationValuesToCopyOver;

    {
        activationValuesToCopyOver.reserveInitialCapacity(symbolTable->scopeSize());

        for (auto end = symbolTable->end(NoLockingNecessary), ptr = symbolTable->begin(NoLockingNecessary); ptr != end; ++ptr) {
            if (!ptr->value.varOffset().isScope())
                continue;

            RefPtr<UniquedStringImpl> ident = ptr->key;
            Identifier identifier = Identifier::fromUid(m_vm, ident.get());

            RegisterID* transitionValue = newBlockScopeVariable();
            transitionValue->ref();
            emitGetFromScope(transitionValue, loopScope, variableForLocalEntry(identifier, ptr->value, loopSymbolTable->index(), true), DoNotThrowIfNotFound);
            activationValuesToCopyOver.uncheckedAppend(std::make_pair(transitionValue, identifier));
        }
    }

    // We need this dynamic behavior of the executing code to ensure
    // each loop iteration has a new activation object. (It's pretty ugly).
    // Also, this new activation needs to be assigned to the same register
    // as the previous scope because the loop body is compiled under
    // the assumption that the scope's register index is constant even
    // though the value in that register will change on each loop iteration.
    RefPtr<RegisterID> parentScope = emitGetParentScope(newTemporary(), loopScope);
    move(scopeRegister(), parentScope.get());

    OpCreateLexicalEnvironment::emit(this, loopScope, scopeRegister(), loopSymbolTable, addConstantValue(jsTDZValue()));

    move(scopeRegister(), loopScope);

    {
        for (auto pair : activationValuesToCopyOver) {
            const Identifier& identifier = pair.second;
            SymbolTableEntry entry = symbolTable->get(NoLockingNecessary, identifier.impl());
            RELEASE_ASSERT(!entry.isNull());
            RegisterID* transitionValue = pair.first;
            emitPutToScope(loopScope, variableForLocalEntry(identifier, entry, loopSymbolTable->index(), true), transitionValue, DoNotThrowIfNotFound, InitializationMode::NotInitialization);
            transitionValue->deref();
        }
    }
}

Variable BytecodeGenerator::variable(const Identifier& property, ThisResolutionType thisResolutionType)
{
    if (property == propertyNames().thisIdentifier && thisResolutionType == ThisResolutionType::Local)
        return Variable(property, VarOffset(thisRegister()->virtualRegister()), thisRegister(), static_cast<unsigned>(PropertyAttribute::ReadOnly), Variable::SpecialVariable, 0, false);
    
    // We can optimize lookups if the lexical variable is found before a "with" or "catch"
    // scope because we're guaranteed static resolution. If we have to pass through
    // a "with" or "catch" scope we loose this guarantee.
    // We can't optimize cases like this:
    // {
    //     let x = ...;
    //     with (o) {
    //         doSomethingWith(x);
    //     }
    // }
    // Because we can't gaurantee static resolution on x.
    // But, in this case, we are guaranteed static resolution:
    // {
    //     let x = ...;
    //     with (o) {
    //         let x = ...;
    //         doSomethingWith(x);
    //     }
    // }
    for (unsigned i = m_lexicalScopeStack.size(); i--; ) {
        auto& stackEntry = m_lexicalScopeStack[i];
        if (stackEntry.m_isWithScope)
            return Variable(property);
        SymbolTable* symbolTable = stackEntry.m_symbolTable;
        SymbolTableEntry symbolTableEntry = symbolTable->get(NoLockingNecessary, property.impl());
        if (symbolTableEntry.isNull())
            continue;
        bool resultIsCallee = false;
        if (symbolTable->scopeType() == SymbolTable::ScopeType::FunctionNameScope) {
            if (m_usesNonStrictEval) {
                // We don't know if an eval has introduced a "var" named the same thing as the function name scope variable name.
                // We resort to dynamic lookup to answer this question.
                Variable result = Variable(property);
                return result;
            }
            resultIsCallee = true;
        }
        Variable result = variableForLocalEntry(property, symbolTableEntry, stackEntry.m_symbolTableConstantIndex, symbolTable->scopeType() == SymbolTable::ScopeType::LexicalScope);
        if (resultIsCallee)
            result.setIsReadOnly();
        return result;
    }
    
    return Variable(property);
}

Variable BytecodeGenerator::variableForLocalEntry(
    const Identifier& property, const SymbolTableEntry& entry, int symbolTableConstantIndex, bool isLexicallyScoped)
{
    VarOffset offset = entry.varOffset();
    
    RegisterID* local;
    if (offset.isStack())
        local = &registerFor(offset.stackOffset());
    else
        local = nullptr;
    
    return Variable(property, offset, local, entry.getAttributes(), Variable::NormalVariable, symbolTableConstantIndex, isLexicallyScoped);
}

void BytecodeGenerator::createVariable(
    const Identifier& property, VarKind varKind, SymbolTable* symbolTable, ExistingVariableMode existingVariableMode)
{
    ASSERT(property != propertyNames().thisIdentifier);
    SymbolTableEntry entry = symbolTable->get(NoLockingNecessary, property.impl());
    
    if (!entry.isNull()) {
        if (existingVariableMode == IgnoreExisting)
            return;
        
        // Do some checks to ensure that the variable we're being asked to create is sufficiently
        // compatible with the one we have already created.

        VarOffset offset = entry.varOffset();
        
        // We can't change our minds about whether it's captured.
        if (offset.kind() != varKind) {
            dataLog(
                "Trying to add variable called ", property, " as ", varKind,
                " but it was already added as ", offset, ".\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        return;
    }
    
    VarOffset varOffset;
    if (varKind == VarKind::Scope)
        varOffset = VarOffset(symbolTable->takeNextScopeOffset(NoLockingNecessary));
    else {
        ASSERT(varKind == VarKind::Stack);
        varOffset = VarOffset(virtualRegisterForLocal(m_calleeLocals.size()));
    }
    SymbolTableEntry newEntry(varOffset, 0);
    symbolTable->add(NoLockingNecessary, property.impl(), newEntry);
    
    if (varKind == VarKind::Stack) {
        RegisterID* local = addVar();
        RELEASE_ASSERT(local->index() == varOffset.stackOffset().offset());
    }
}

RegisterID* BytecodeGenerator::emitOverridesHasInstance(RegisterID* dst, RegisterID* constructor, RegisterID* hasInstanceValue)
{
    OpOverridesHasInstance::emit(this, dst, constructor, hasInstanceValue);
    return dst;
}

// Indicates the least upper bound of resolve type based on local scope. The bytecode linker
// will start with this ResolveType and compute the least upper bound including intercepting scopes.
ResolveType BytecodeGenerator::resolveType()
{
    for (unsigned i = m_lexicalScopeStack.size(); i--; ) {
        if (m_lexicalScopeStack[i].m_isWithScope)
            return Dynamic;
        if (m_usesNonStrictEval && m_lexicalScopeStack[i].m_symbolTable->scopeType() == SymbolTable::ScopeType::FunctionNameScope) {
            // We never want to assign to a FunctionNameScope. Returning Dynamic here achieves this goal.
            // If we aren't in non-strict eval mode, then NodesCodeGen needs to take care not to emit
            // a put_to_scope with the destination being the function name scope variable.
            return Dynamic;
        }
    }

    if (m_usesNonStrictEval)
        return GlobalPropertyWithVarInjectionChecks;
    return GlobalProperty;
}

RegisterID* BytecodeGenerator::emitResolveScope(RegisterID* dst, const Variable& variable)
{
    switch (variable.offset().kind()) {
    case VarKind::Stack:
        return nullptr;
        
    case VarKind::DirectArgument:
        return argumentsRegister();
        
    case VarKind::Scope: {
        // This always refers to the activation that *we* allocated, and not the current scope that code
        // lives in. Note that this will change once we have proper support for block scoping. Once that
        // changes, it will be correct for this code to return scopeRegister(). The only reason why we
        // don't do that already is that m_lexicalEnvironment is required by ConstDeclNode. ConstDeclNode
        // requires weird things because it is a shameful pile of nonsense, but block scoping would make
        // that code sensible and obviate the need for us to do bad things.
        for (unsigned i = m_lexicalScopeStack.size(); i--; ) {
            auto& stackEntry = m_lexicalScopeStack[i];
            // We should not resolve a variable to VarKind::Scope if a "with" scope lies in between the current
            // scope and the resolved scope.
            RELEASE_ASSERT(!stackEntry.m_isWithScope);

            if (stackEntry.m_symbolTable->get(NoLockingNecessary, variable.ident().impl()).isNull())
                continue;
            
            RegisterID* scope = stackEntry.m_scope;
            RELEASE_ASSERT(scope);
            return scope;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
        
    }
    case VarKind::Invalid:
        // Indicates non-local resolution.
        
        dst = tempDestination(dst);
        OpResolveScope::emit(this, kill(dst), scopeRegister(), addConstant(variable.ident()), resolveType(), localScopeDepth());
        m_codeBlock->addPropertyAccessInstruction(m_lastInstruction.offset());
        return dst;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RegisterID* BytecodeGenerator::emitGetFromScope(RegisterID* dst, RegisterID* scope, const Variable& variable, ResolveMode resolveMode)
{
    switch (variable.offset().kind()) {
    case VarKind::Stack:
        return move(dst, variable.local());
        
    case VarKind::DirectArgument: {
        OpGetFromArguments::emit(this, kill(dst), scope, variable.offset().capturedArgumentsOffset().offset());
        return dst;
    }
        
    case VarKind::Scope:
    case VarKind::Invalid: {
        OpGetFromScope::emit(
            this,
            kill(dst),
            scope,
            addConstant(variable.ident()),
            GetPutInfo(resolveMode, variable.offset().isScope() ? LocalClosureVar : resolveType(), InitializationMode::NotInitialization),
            localScopeDepth(),
            variable.offset().isScope() ? variable.offset().scopeOffset().offset() : 0);
        m_codeBlock->addPropertyAccessInstruction(m_lastInstruction.offset());
        return dst;
    } }
    
    RELEASE_ASSERT_NOT_REACHED();
}

RegisterID* BytecodeGenerator::emitPutToScope(RegisterID* scope, const Variable& variable, RegisterID* value, ResolveMode resolveMode, InitializationMode initializationMode)
{
    switch (variable.offset().kind()) {
    case VarKind::Stack:
        move(variable.local(), value);
        return value;
        
    case VarKind::DirectArgument:
        OpPutToArguments::emit(this, scope, variable.offset().capturedArgumentsOffset().offset(), value);
        return value;
        
    case VarKind::Scope:
    case VarKind::Invalid: {
        GetPutInfo getPutInfo(0);
        int scopeDepth;
        ScopeOffset offset;
        if (variable.offset().isScope()) {
            offset = variable.offset().scopeOffset();
            getPutInfo = GetPutInfo(resolveMode, LocalClosureVar, initializationMode);
            scopeDepth = variable.symbolTableConstantIndex();
        } else {
            ASSERT(resolveType() != LocalClosureVar);
            getPutInfo = GetPutInfo(resolveMode, resolveType(), initializationMode);
            scopeDepth = localScopeDepth();
        }
        OpPutToScope::emit(this, scope, addConstant(variable.ident()), value, getPutInfo, scopeDepth, !!offset ? offset.offset() : 0);
        m_codeBlock->addPropertyAccessInstruction(m_lastInstruction.offset());
        return value;
    } }
    
    RELEASE_ASSERT_NOT_REACHED();
}

RegisterID* BytecodeGenerator::initializeVariable(const Variable& variable, RegisterID* value)
{
    RELEASE_ASSERT(variable.offset().kind() != VarKind::Invalid);
    RegisterID* scope = emitResolveScope(nullptr, variable);
    return emitPutToScope(scope, variable, value, ThrowIfNotFound, InitializationMode::NotInitialization);
}

RegisterID* BytecodeGenerator::emitInstanceOf(RegisterID* dst, RegisterID* value, RegisterID* basePrototype)
{
    OpInstanceof::emit(this, dst, value, basePrototype);
    return dst;
}

RegisterID* BytecodeGenerator::emitInstanceOfCustom(RegisterID* dst, RegisterID* value, RegisterID* constructor, RegisterID* hasInstanceValue)
{
    OpInstanceofCustom::emit(this, dst, value, constructor, hasInstanceValue);
    return dst;
}

RegisterID* BytecodeGenerator::emitInByVal(RegisterID* dst, RegisterID* property, RegisterID* base)
{
    OpInByVal::emit(this, dst, base, property);
    return dst;
}

RegisterID* BytecodeGenerator::emitInById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    OpInById::emit(this, dst, base, addConstant(property));
    return dst;
}

RegisterID* BytecodeGenerator::emitTryGetById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    ASSERT_WITH_MESSAGE(!parseIndex(property), "Indexed properties are not supported with tryGetById.");

    OpTryGetById::emit(this, kill(dst), base, addConstant(property));
    return dst;
}

RegisterID* BytecodeGenerator::emitGetById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    ASSERT_WITH_MESSAGE(!parseIndex(property), "Indexed properties should be handled with get_by_val.");

    OpGetById::emit(this, kill(dst), base, addConstant(property));
    m_codeBlock->addPropertyAccessInstruction(m_lastInstruction.offset());
    return dst;
}

RegisterID* BytecodeGenerator::emitGetById(RegisterID* dst, RegisterID* base, RegisterID* thisVal, const Identifier& property)
{
    ASSERT_WITH_MESSAGE(!parseIndex(property), "Indexed properties should be handled with get_by_val.");

    OpGetByIdWithThis::emit(this, kill(dst), base, thisVal, addConstant(property));
    return dst;
}

RegisterID* BytecodeGenerator::emitDirectGetById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    ASSERT_WITH_MESSAGE(!parseIndex(property), "Indexed properties should be handled with get_by_val_direct.");

    OpGetByIdDirect::emit(this, kill(dst), base, addConstant(property));
    m_codeBlock->addPropertyAccessInstruction(m_lastInstruction.offset());
    return dst;
}

RegisterID* BytecodeGenerator::emitPutById(RegisterID* base, const Identifier& property, RegisterID* value)
{
    ASSERT_WITH_MESSAGE(!parseIndex(property), "Indexed properties should be handled with put_by_val.");

    unsigned propertyIndex = addConstant(property);

    m_staticPropertyAnalyzer.putById(base, propertyIndex);

    OpPutById::emit(this, base, propertyIndex, value, PutByIdNone); // is not direct
    m_codeBlock->addPropertyAccessInstruction(m_lastInstruction.offset());

    return value;
}

RegisterID* BytecodeGenerator::emitPutById(RegisterID* base, RegisterID* thisValue, const Identifier& property, RegisterID* value)
{
    ASSERT_WITH_MESSAGE(!parseIndex(property), "Indexed properties should be handled with put_by_val.");

    unsigned propertyIndex = addConstant(property);

    OpPutByIdWithThis::emit(this, base, thisValue, propertyIndex, value);

    return value;
}

RegisterID* BytecodeGenerator::emitDirectPutById(RegisterID* base, const Identifier& property, RegisterID* value, PropertyNode::PutType putType)
{
    ASSERT_WITH_MESSAGE(!parseIndex(property), "Indexed properties should be handled with put_by_val(direct).");

    unsigned propertyIndex = addConstant(property);

    m_staticPropertyAnalyzer.putById(base, propertyIndex);

    PutByIdFlags type = (putType == PropertyNode::KnownDirect || property != m_vm->propertyNames->underscoreProto) ? PutByIdIsDirect : PutByIdNone;
    OpPutById::emit(this, base, propertyIndex, value, type);
    m_codeBlock->addPropertyAccessInstruction(m_lastInstruction.offset());
    return value;
}

void BytecodeGenerator::emitPutGetterById(RegisterID* base, const Identifier& property, unsigned attributes, RegisterID* getter)
{
    unsigned propertyIndex = addConstant(property);
    m_staticPropertyAnalyzer.putById(base, propertyIndex);

    OpPutGetterById::emit(this, base, propertyIndex, attributes, getter);
}

void BytecodeGenerator::emitPutSetterById(RegisterID* base, const Identifier& property, unsigned attributes, RegisterID* setter)
{
    unsigned propertyIndex = addConstant(property);
    m_staticPropertyAnalyzer.putById(base, propertyIndex);

    OpPutSetterById::emit(this, base, propertyIndex, attributes, setter);
}

void BytecodeGenerator::emitPutGetterSetter(RegisterID* base, const Identifier& property, unsigned attributes, RegisterID* getter, RegisterID* setter)
{
    unsigned propertyIndex = addConstant(property);

    m_staticPropertyAnalyzer.putById(base, propertyIndex);

    OpPutGetterSetterById::emit(this, base, propertyIndex, attributes, getter, setter);
}

void BytecodeGenerator::emitPutGetterByVal(RegisterID* base, RegisterID* property, unsigned attributes, RegisterID* getter)
{
    OpPutGetterByVal::emit(this, base, property, attributes, getter);
}

void BytecodeGenerator::emitPutSetterByVal(RegisterID* base, RegisterID* property, unsigned attributes, RegisterID* setter)
{
    OpPutSetterByVal::emit(this, base, property, attributes, setter);
}

void BytecodeGenerator::emitPutGeneratorFields(RegisterID* nextFunction)
{
    // FIXME: Currently, we just create an object and store generator related fields as its properties for ease.
    // But to make it efficient, we will introduce JSGenerator class, add opcode new_generator and use its C++ fields instead of these private properties.
    // https://bugs.webkit.org/show_bug.cgi?id=151545

    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().generatorNextPrivateName(), nextFunction, PropertyNode::KnownDirect);

    // We do not store 'this' in arrow function within constructor,
    // because it might be not initialized, if super is called later.
    if (!(isDerivedConstructorContext() && m_codeBlock->parseMode() == SourceParseMode::AsyncArrowFunctionMode))
        emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().generatorThisPrivateName(), &m_thisRegister, PropertyNode::KnownDirect);

    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().generatorStatePrivateName(), emitLoad(nullptr, jsNumber(0)), PropertyNode::KnownDirect);

    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().generatorFramePrivateName(), emitLoad(nullptr, jsNull()), PropertyNode::KnownDirect);
}

void BytecodeGenerator::emitPutAsyncGeneratorFields(RegisterID* nextFunction)
{
    ASSERT(isAsyncGeneratorWrapperParseMode(parseMode()));

    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().generatorNextPrivateName(), nextFunction, PropertyNode::KnownDirect);
        
    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().generatorThisPrivateName(), &m_thisRegister, PropertyNode::KnownDirect);
        
    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().generatorStatePrivateName(), emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSAsyncGeneratorFunction::AsyncGeneratorState::SuspendedStart))), PropertyNode::KnownDirect);
        
    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().generatorFramePrivateName(), emitLoad(nullptr, jsNull()), PropertyNode::KnownDirect);

    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().asyncGeneratorSuspendReasonPrivateName(), emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSAsyncGeneratorFunction::AsyncGeneratorSuspendReason::None))), PropertyNode::KnownDirect);

    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().asyncGeneratorQueueFirstPrivateName(), emitLoad(nullptr, jsNull()), PropertyNode::KnownDirect);
    emitDirectPutById(m_generatorRegister, propertyNames().builtinNames().asyncGeneratorQueueLastPrivateName(), emitLoad(nullptr, jsNull()), PropertyNode::KnownDirect);
}

RegisterID* BytecodeGenerator::emitDeleteById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    OpDelById::emit(this, dst, base, addConstant(property));
    return dst;
}

RegisterID* BytecodeGenerator::emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    for (size_t i = m_forInContextStack.size(); i--; ) {
        ForInContext& context = m_forInContextStack[i].get();
        if (context.local() != property)
            continue;

        if (context.isIndexedForInContext()) {
            auto& indexedContext = context.asIndexedForInContext();
            OpGetByVal::emit<OpcodeSize::Wide>(this, kill(dst), base, indexedContext.index());
            indexedContext.addGetInst(m_lastInstruction.offset(), property->index());
            return dst;
        }

        StructureForInContext& structureContext = context.asStructureForInContext();
        OpGetDirectPname::emit<OpcodeSize::Wide>(this, kill(dst), base, property, structureContext.index(), structureContext.enumerator());

        structureContext.addGetInst(m_lastInstruction.offset(), property->index());
        return dst;
    }

    OpGetByVal::emit(this, kill(dst), base, property);
    return dst;
}

RegisterID* BytecodeGenerator::emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* thisValue, RegisterID* property)
{
    OpGetByValWithThis::emit(this, kill(dst), base, thisValue, property);
    return dst;
}

RegisterID* BytecodeGenerator::emitPutByVal(RegisterID* base, RegisterID* property, RegisterID* value)
{
    OpPutByVal::emit(this, base, property, value);
    return value;
}

RegisterID* BytecodeGenerator::emitPutByVal(RegisterID* base, RegisterID* thisValue, RegisterID* property, RegisterID* value)
{
    OpPutByValWithThis::emit(this, base, thisValue, property, value);
    return value;
}

RegisterID* BytecodeGenerator::emitDirectPutByVal(RegisterID* base, RegisterID* property, RegisterID* value)
{
    OpPutByValDirect::emit(this, base, property, value);
    return value;
}

RegisterID* BytecodeGenerator::emitDeleteByVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    OpDelByVal::emit(this, dst, base, property);
    return dst;
}

void BytecodeGenerator::emitSuperSamplerBegin()
{
    OpSuperSamplerBegin::emit(this);
}

void BytecodeGenerator::emitSuperSamplerEnd()
{
    OpSuperSamplerEnd::emit(this);
}

RegisterID* BytecodeGenerator::emitIdWithProfile(RegisterID* src, SpeculatedType profile)
{
    OpIdentityWithProfile::emit(this, src, static_cast<uint32_t>(profile >> 32), static_cast<uint32_t>(profile));
    return src;
}

void BytecodeGenerator::emitUnreachable()
{
    OpUnreachable::emit(this);
}

RegisterID* BytecodeGenerator::emitGetArgument(RegisterID* dst, int32_t index)
{
    OpGetArgument::emit(this, dst, index + 1 /* Including |this| */);
    return dst;
}

RegisterID* BytecodeGenerator::emitCreateThis(RegisterID* dst)
{
    OpCreateThis::emit(this, dst, dst, 0);
    m_staticPropertyAnalyzer.createThis(dst, m_lastInstruction);

    m_codeBlock->addPropertyAccessInstruction(m_lastInstruction.offset());
    return dst;
}

void BytecodeGenerator::emitTDZCheck(RegisterID* target)
{
    OpCheckTdz::emit(this, target);
}

bool BytecodeGenerator::needsTDZCheck(const Variable& variable)
{
    for (unsigned i = m_TDZStack.size(); i--;) {
        auto iter = m_TDZStack[i].find(variable.ident().impl());
        if (iter == m_TDZStack[i].end())
            continue;
        return iter->value != TDZNecessityLevel::NotNeeded;
    }

    return false;
}

void BytecodeGenerator::emitTDZCheckIfNecessary(const Variable& variable, RegisterID* target, RegisterID* scope)
{
    if (needsTDZCheck(variable)) {
        if (target)
            emitTDZCheck(target);
        else {
            RELEASE_ASSERT(!variable.isLocal() && scope);
            RefPtr<RegisterID> result = emitGetFromScope(newTemporary(), scope, variable, DoNotThrowIfNotFound);
            emitTDZCheck(result.get());
        }
    }
}

void BytecodeGenerator::liftTDZCheckIfPossible(const Variable& variable)
{
    RefPtr<UniquedStringImpl> identifier(variable.ident().impl());
    for (unsigned i = m_TDZStack.size(); i--;) {
        auto iter = m_TDZStack[i].find(identifier);
        if (iter != m_TDZStack[i].end()) {
            if (iter->value == TDZNecessityLevel::Optimize)
                iter->value = TDZNecessityLevel::NotNeeded;
            break;
        }
    }
}

void BytecodeGenerator::pushTDZVariables(const VariableEnvironment& environment, TDZCheckOptimization optimization, TDZRequirement requirement)
{
    if (!environment.size())
        return;
    
    TDZNecessityLevel level;
    if (requirement == TDZRequirement::UnderTDZ) {
        if (optimization == TDZCheckOptimization::Optimize)
            level = TDZNecessityLevel::Optimize;
        else
            level = TDZNecessityLevel::DoNotOptimize;
    } else
        level = TDZNecessityLevel::NotNeeded;
    
    TDZMap map;
    for (const auto& entry : environment)
        map.add(entry.key, entry.value.isFunction() ? TDZNecessityLevel::NotNeeded : level);

    m_TDZStack.append(WTFMove(map));
}

void BytecodeGenerator::getVariablesUnderTDZ(VariableEnvironment& result)
{
    // We keep track of variablesThatDontNeedTDZ in this algorithm to prevent
    // reporting that "x" is under TDZ if this function is called at "...".
    //
    //     {
    //         {
    //             let x;
    //             ...
    //         }
    //         let x;
    //     }
    //
    SmallPtrSet<UniquedStringImpl*, 16> variablesThatDontNeedTDZ;
    for (unsigned i = m_TDZStack.size(); i--; ) {
        auto& map = m_TDZStack[i];
        for (auto& entry : map)  {
            if (entry.value != TDZNecessityLevel::NotNeeded) {
                if (!variablesThatDontNeedTDZ.contains(entry.key.get()))
                    result.add(entry.key.get());
            } else
                variablesThatDontNeedTDZ.add(entry.key.get());
        }
    }
}

void BytecodeGenerator::preserveTDZStack(BytecodeGenerator::PreservedTDZStack& preservedStack)
{
    preservedStack.m_preservedTDZStack = m_TDZStack;
}

void BytecodeGenerator::restoreTDZStack(const BytecodeGenerator::PreservedTDZStack& preservedStack)
{
    m_TDZStack = preservedStack.m_preservedTDZStack;
}

RegisterID* BytecodeGenerator::emitNewObject(RegisterID* dst)
{
    OpNewObject::emit(this, dst, 0);
    m_staticPropertyAnalyzer.newObject(dst, m_lastInstruction);

    return dst;
}

JSValue BytecodeGenerator::addBigIntConstant(const Identifier& identifier, uint8_t radix, bool sign)
{
    return m_bigIntMap.ensure(BigIntMapEntry(identifier.impl(), radix, sign), [&] {
        auto scope = DECLARE_CATCH_SCOPE(*vm());
        auto parseIntSign = sign ? JSBigInt::ParseIntSign::Signed : JSBigInt::ParseIntSign::Unsigned;
        JSBigInt* bigIntInMap = JSBigInt::parseInt(nullptr, *vm(), identifier.string(), radix, JSBigInt::ErrorParseMode::ThrowExceptions, parseIntSign);
        // FIXME: [ESNext] Enables a way to throw an error on ByteCodeGenerator step
        // https://bugs.webkit.org/show_bug.cgi?id=180139
        scope.assertNoException();
        RELEASE_ASSERT(bigIntInMap);
        addConstantValue(bigIntInMap);

        return bigIntInMap;
    }).iterator->value;
}

JSString* BytecodeGenerator::addStringConstant(const Identifier& identifier)
{
    JSString*& stringInMap = m_stringMap.add(identifier.impl(), nullptr).iterator->value;
    if (!stringInMap) {
        stringInMap = jsString(vm(), identifier.string());
        addConstantValue(stringInMap);
    }
    return stringInMap;
}

RegisterID* BytecodeGenerator::addTemplateObjectConstant(Ref<TemplateObjectDescriptor>&& descriptor)
{
    JSTemplateObjectDescriptor* descriptorValue = m_templateObjectDescriptorMap.ensure(descriptor.copyRef(), [&] {
        return JSTemplateObjectDescriptor::create(*vm(), WTFMove(descriptor));
    }).iterator->value;

    int index = addConstantIndex();
    m_codeBlock->addConstant(descriptorValue);
    return &m_constantPoolRegisters[index];
}

RegisterID* BytecodeGenerator::emitNewArrayBuffer(RegisterID* dst, JSImmutableButterfly* array, IndexingType recommendedIndexingType)
{
    OpNewArrayBuffer::emit(this, dst, addConstantValue(array), recommendedIndexingType);
    return dst;
}

RegisterID* BytecodeGenerator::emitNewArray(RegisterID* dst, ElementNode* elements, unsigned length, IndexingType recommendedIndexingType)
{
    Vector<RefPtr<RegisterID>, 16, UnsafeVectorOverflow> argv;
    for (ElementNode* n = elements; n; n = n->next()) {
        if (!length)
            break;
        length--;
        ASSERT(!n->value()->isSpreadExpression());
        argv.append(newTemporary());
        // op_new_array requires the initial values to be a sequential range of registers
        ASSERT(argv.size() == 1 || argv[argv.size() - 1]->index() == argv[argv.size() - 2]->index() - 1);
        emitNode(argv.last().get(), n->value());
    }
    ASSERT(!length);
    OpNewArray::emit(this, dst, argv.size() ? argv[0].get() : VirtualRegister { 0 }, argv.size(), recommendedIndexingType);
    return dst;
}

RegisterID* BytecodeGenerator::emitNewArrayWithSpread(RegisterID* dst, ElementNode* elements)
{
    BitVector bitVector;
    Vector<RefPtr<RegisterID>, 16> argv;
    for (ElementNode* node = elements; node; node = node->next()) {
        bitVector.set(argv.size(), node->value()->isSpreadExpression());

        argv.append(newTemporary());
        // op_new_array_with_spread requires the initial values to be a sequential range of registers.
        RELEASE_ASSERT(argv.size() == 1 || argv[argv.size() - 1]->index() == argv[argv.size() - 2]->index() - 1);
    }

    RELEASE_ASSERT(argv.size());

    {
        unsigned i = 0;
        for (ElementNode* node = elements; node; node = node->next()) {
            if (node->value()->isSpreadExpression()) {
                ExpressionNode* expression = static_cast<SpreadExpressionNode*>(node->value())->expression();
                RefPtr<RegisterID> tmp = newTemporary();
                emitNode(tmp.get(), expression);

                OpSpread::emit(this, argv[i].get(), tmp.get());
            } else {
                ExpressionNode* expression = node->value();
                emitNode(argv[i].get(), expression);
            }
            i++;
        }
    }

    unsigned bitVectorIndex = m_codeBlock->addBitVector(WTFMove(bitVector));
    OpNewArrayWithSpread::emit(this, dst, argv[0].get(), argv.size(), bitVectorIndex);
    return dst;
}

RegisterID* BytecodeGenerator::emitNewArrayWithSize(RegisterID* dst, RegisterID* length)
{
    OpNewArrayWithSize::emit(this, dst, length);
    return dst;
}

RegisterID* BytecodeGenerator::emitNewRegExp(RegisterID* dst, RegExp* regExp)
{
    OpNewRegexp::emit(this, dst, addConstantValue(regExp));
    return dst;
}

void BytecodeGenerator::emitNewFunctionExpressionCommon(RegisterID* dst, FunctionMetadataNode* function)
{
    unsigned index = m_codeBlock->addFunctionExpr(makeFunction(function));

    switch (function->parseMode()) {
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GeneratorWrapperMethodMode:
        OpNewGeneratorFuncExp::emit(this, dst, scopeRegister(), index);
        break;
    case SourceParseMode::AsyncFunctionMode:
    case SourceParseMode::AsyncMethodMode:
    case SourceParseMode::AsyncArrowFunctionMode:
        OpNewAsyncFuncExp::emit(this, dst, scopeRegister(), index);
        break;
    case SourceParseMode::AsyncGeneratorWrapperFunctionMode:
    case SourceParseMode::AsyncGeneratorWrapperMethodMode:
        OpNewAsyncGeneratorFuncExp::emit(this, dst, scopeRegister(), index);
        break;
    default:
        OpNewFuncExp::emit(this, dst, scopeRegister(), index);
        break;
    }
}

RegisterID* BytecodeGenerator::emitNewFunctionExpression(RegisterID* dst, FuncExprNode* func)
{
    emitNewFunctionExpressionCommon(dst, func->metadata());
    return dst;
}

RegisterID* BytecodeGenerator::emitNewArrowFunctionExpression(RegisterID* dst, ArrowFuncExprNode* func)
{
    ASSERT(SourceParseModeSet(SourceParseMode::ArrowFunctionMode, SourceParseMode::AsyncArrowFunctionMode).contains(func->metadata()->parseMode()));
    emitNewFunctionExpressionCommon(dst, func->metadata());
    return dst;
}

RegisterID* BytecodeGenerator::emitNewMethodDefinition(RegisterID* dst, MethodDefinitionNode* func)
{
    ASSERT(isMethodParseMode(func->metadata()->parseMode()));
    emitNewFunctionExpressionCommon(dst, func->metadata());
    return dst;
}

RegisterID* BytecodeGenerator::emitNewDefaultConstructor(RegisterID* dst, ConstructorKind constructorKind, const Identifier& name,
    const Identifier& ecmaName, const SourceCode& classSource)
{
    UnlinkedFunctionExecutable* executable = m_vm->builtinExecutables()->createDefaultConstructor(constructorKind, name);
    executable->setInvalidTypeProfilingOffsets();
    executable->setEcmaName(ecmaName);
    executable->setClassSource(classSource);

    unsigned index = m_codeBlock->addFunctionExpr(executable);

    OpNewFuncExp::emit(this, dst, scopeRegister(), index);
    return dst;
}

RegisterID* BytecodeGenerator::emitNewFunction(RegisterID* dst, FunctionMetadataNode* function)
{
    unsigned index = m_codeBlock->addFunctionDecl(makeFunction(function));
    if (isGeneratorWrapperParseMode(function->parseMode()))
        OpNewGeneratorFunc::emit(this, dst, scopeRegister(), index);
    else if (function->parseMode() == SourceParseMode::AsyncFunctionMode)
        OpNewAsyncFunc::emit(this, dst, scopeRegister(), index);
    else if (isAsyncGeneratorWrapperParseMode(function->parseMode()))
        OpNewAsyncGeneratorFunc::emit(this, dst, scopeRegister(), index);
    else
        OpNewFunc::emit(this, dst, scopeRegister(), index);
    return dst;
}

void BytecodeGenerator::emitSetFunctionNameIfNeeded(ExpressionNode* valueNode, RegisterID* value, RegisterID* name)
{
    if (valueNode->isBaseFuncExprNode()) {
        FunctionMetadataNode* metadata = static_cast<BaseFuncExprNode*>(valueNode)->metadata();
        if (!metadata->ecmaName().isNull())
            return;
    } else if (valueNode->isClassExprNode()) {
        ClassExprNode* classExprNode = static_cast<ClassExprNode*>(valueNode);
        if (!classExprNode->ecmaName().isNull())
            return;
        if (classExprNode->hasStaticProperty(m_vm->propertyNames->name))
            return;
    } else
        return;

    // FIXME: We should use an op_call to an internal function here instead.
    // https://bugs.webkit.org/show_bug.cgi?id=155547
    OpSetFunctionName::emit(this, value, name);
}

RegisterID* BytecodeGenerator::emitCall(RegisterID* dst, RegisterID* func, ExpectedFunction expectedFunction, CallArguments& callArguments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall debuggableCall)
{
    return emitCall<OpCall>(dst, func, expectedFunction, callArguments, divot, divotStart, divotEnd, debuggableCall);
}

RegisterID* BytecodeGenerator::emitCallInTailPosition(RegisterID* dst, RegisterID* func, ExpectedFunction expectedFunction, CallArguments& callArguments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall debuggableCall)
{
    if (m_inTailPosition) {
        m_codeBlock->setHasTailCalls();
        return emitCall<OpTailCall>(dst, func, expectedFunction, callArguments, divot, divotStart, divotEnd, debuggableCall);
    }
    return emitCall<OpCall>(dst, func, expectedFunction, callArguments, divot, divotStart, divotEnd, debuggableCall);
}

RegisterID* BytecodeGenerator::emitCallEval(RegisterID* dst, RegisterID* func, CallArguments& callArguments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall debuggableCall)
{
    return emitCall<OpCallEval>(dst, func, NoExpectedFunction, callArguments, divot, divotStart, divotEnd, debuggableCall);
}

ExpectedFunction BytecodeGenerator::expectedFunctionForIdentifier(const Identifier& identifier)
{
    if (identifier == propertyNames().Object || identifier == propertyNames().builtinNames().ObjectPrivateName())
        return ExpectObjectConstructor;
    if (identifier == propertyNames().Array || identifier == propertyNames().builtinNames().ArrayPrivateName())
        return ExpectArrayConstructor;
    return NoExpectedFunction;
}

ExpectedFunction BytecodeGenerator::emitExpectedFunctionSnippet(RegisterID* dst, RegisterID* func, ExpectedFunction expectedFunction, CallArguments& callArguments, Label& done)
{
    Ref<Label> realCall = newLabel();
    switch (expectedFunction) {
    case ExpectObjectConstructor: {
        // If the number of arguments is non-zero, then we can't do anything interesting.
        if (callArguments.argumentCountIncludingThis() >= 2)
            return NoExpectedFunction;
        
        OpJneqPtr::emit(this, func, Special::ObjectConstructor, realCall->bind(this));
        
        if (dst != ignoredResult())
            emitNewObject(dst);
        break;
    }
        
    case ExpectArrayConstructor: {
        // If you're doing anything other than "new Array()" or "new Array(foo)" then we
        // don't do inline it, for now. The only reason is that call arguments are in
        // the opposite order of what op_new_array expects, so we'd either need to change
        // how op_new_array works or we'd need an op_new_array_reverse. Neither of these
        // things sounds like it's worth it.
        if (callArguments.argumentCountIncludingThis() > 2)
            return NoExpectedFunction;
        
        OpJneqPtr::emit(this, func, Special::ArrayConstructor, realCall->bind(this));
        
        if (dst != ignoredResult()) {
            if (callArguments.argumentCountIncludingThis() == 2)
                emitNewArrayWithSize(dst, callArguments.argumentRegister(0));
            else {
                ASSERT(callArguments.argumentCountIncludingThis() == 1);
                OpNewArray::emit(this, dst, VirtualRegister { 0 }, 0, ArrayWithUndecided);
            }
        }
        break;
    }
        
    default:
        ASSERT(expectedFunction == NoExpectedFunction);
        return NoExpectedFunction;
    }
    
    OpJmp::emit(this, done.bind(this));
    emitLabel(realCall.get());
    
    return expectedFunction;
}

template<typename CallOp>
RegisterID* BytecodeGenerator::emitCall(RegisterID* dst, RegisterID* func, ExpectedFunction expectedFunction, CallArguments& callArguments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall debuggableCall)
{
    constexpr auto opcodeID = CallOp::opcodeID;
    ASSERT(opcodeID == op_call || opcodeID == op_call_eval || opcodeID == op_tail_call);
    ASSERT(func->refCount());
    
    // Generate code for arguments.
    unsigned argument = 0;
    if (callArguments.argumentsNode()) {
        ArgumentListNode* n = callArguments.argumentsNode()->m_listNode;
        if (n && n->m_expr->isSpreadExpression()) {
            RELEASE_ASSERT(!n->m_next);
            auto expression = static_cast<SpreadExpressionNode*>(n->m_expr)->expression();
            if (expression->isArrayLiteral()) {
                auto* elements = static_cast<ArrayNode*>(expression)->elements();
                if (elements && !elements->next() && elements->value()->isSpreadExpression()) {
                    ExpressionNode* expression = static_cast<SpreadExpressionNode*>(elements->value())->expression();
                    RefPtr<RegisterID> argumentRegister = emitNode(callArguments.argumentRegister(0), expression);
                    OpSpread::emit(this, argumentRegister.get(), argumentRegister.get());

                    return emitCallVarargs<typename VarArgsOp<CallOp>::type>(dst, func, callArguments.thisRegister(), argumentRegister.get(), newTemporary(), 0, divot, divotStart, divotEnd, debuggableCall);
                }
            }
            RefPtr<RegisterID> argumentRegister;
            argumentRegister = expression->emitBytecode(*this, callArguments.argumentRegister(0));
            RefPtr<RegisterID> thisRegister = move(newTemporary(), callArguments.thisRegister());
            return emitCallVarargs<typename VarArgsOp<CallOp>::type>(dst, func, callArguments.thisRegister(), argumentRegister.get(), newTemporary(), 0, divot, divotStart, divotEnd, debuggableCall);
        }
        for (; n; n = n->m_next)
            emitNode(callArguments.argumentRegister(argument++), n);
    }
    
    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, CallFrame::headerSizeInRegisters, UnsafeVectorOverflow> callFrame;
    for (int i = 0; i < CallFrame::headerSizeInRegisters; ++i)
        callFrame.append(newTemporary());

    if (m_shouldEmitDebugHooks && debuggableCall == DebuggableCall::Yes)
        emitDebugHook(WillExecuteExpression, divotStart);

    emitExpressionInfo(divot, divotStart, divotEnd);

    Ref<Label> done = newLabel();
    expectedFunction = emitExpectedFunctionSnippet(dst, func, expectedFunction, callArguments, done.get());
    
    if (opcodeID == op_tail_call)
        emitLogShadowChickenTailIfNecessary();
    
    // Emit call.
    ASSERT(dst);
    ASSERT(dst != ignoredResult());
    CallOp::emit(this, dst, func, callArguments.argumentCountIncludingThis(), callArguments.stackOffset());
    
    if (expectedFunction != NoExpectedFunction)
        emitLabel(done.get());

    return dst;
}

RegisterID* BytecodeGenerator::emitCallVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall debuggableCall)
{
    return emitCallVarargs<OpCallVarargs>(dst, func, thisRegister, arguments, firstFreeRegister, firstVarArgOffset, divot, divotStart, divotEnd, debuggableCall);
}

RegisterID* BytecodeGenerator::emitCallVarargsInTailPosition(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall debuggableCall)
{
    if (m_inTailPosition)
        return emitCallVarargs<OpTailCallVarargs>(dst, func, thisRegister, arguments, firstFreeRegister, firstVarArgOffset, divot, divotStart, divotEnd, debuggableCall);
    return emitCallVarargs<OpCallVarargs>(dst, func, thisRegister, arguments, firstFreeRegister, firstVarArgOffset, divot, divotStart, divotEnd, debuggableCall);
}

RegisterID* BytecodeGenerator::emitConstructVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall debuggableCall)
{
    return emitCallVarargs<OpConstructVarargs>(dst, func, thisRegister, arguments, firstFreeRegister, firstVarArgOffset, divot, divotStart, divotEnd, debuggableCall);
}

RegisterID* BytecodeGenerator::emitCallForwardArgumentsInTailPosition(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall debuggableCall)
{
    ASSERT(m_inTailPosition);
    return emitCallVarargs<OpTailCallForwardArguments>(dst, func, thisRegister, nullptr, firstFreeRegister, firstVarArgOffset, divot, divotStart, divotEnd, debuggableCall);
}
    
template<typename VarargsOp>
RegisterID* BytecodeGenerator::emitCallVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall debuggableCall)
{
    if (m_shouldEmitDebugHooks && debuggableCall == DebuggableCall::Yes)
        emitDebugHook(WillExecuteExpression, divotStart);

    emitExpressionInfo(divot, divotStart, divotEnd);

    if (VarargsOp::opcodeID == op_tail_call_varargs)
        emitLogShadowChickenTailIfNecessary();
    
    // Emit call.
    ASSERT(dst != ignoredResult());
    VarargsOp::emit(this, dst, func, thisRegister, arguments ? arguments : VirtualRegister(0), firstFreeRegister, firstVarArgOffset);
    return dst;
}

void BytecodeGenerator::emitLogShadowChickenPrologueIfNecessary()
{
    if (!m_shouldEmitDebugHooks && !Options::alwaysUseShadowChicken())
        return;
    OpLogShadowChickenPrologue::emit(this, scopeRegister());
}

void BytecodeGenerator::emitLogShadowChickenTailIfNecessary()
{
    if (!m_shouldEmitDebugHooks && !Options::alwaysUseShadowChicken())
        return;
    OpLogShadowChickenTail::emit(this, thisRegister(), scopeRegister());
}

void BytecodeGenerator::emitCallDefineProperty(RegisterID* newObj, RegisterID* propertyNameRegister,
    RegisterID* valueRegister, RegisterID* getterRegister, RegisterID* setterRegister, unsigned options, const JSTextPosition& position)
{
    DefinePropertyAttributes attributes;
    if (options & PropertyConfigurable)
        attributes.setConfigurable(true);

    if (options & PropertyWritable)
        attributes.setWritable(true);
    else if (valueRegister)
        attributes.setWritable(false);

    if (options & PropertyEnumerable)
        attributes.setEnumerable(true);

    if (valueRegister)
        attributes.setValue();
    if (getterRegister)
        attributes.setGet();
    if (setterRegister)
        attributes.setSet();

    ASSERT(!valueRegister || (!getterRegister && !setterRegister));

    emitExpressionInfo(position, position, position);

    if (attributes.hasGet() || attributes.hasSet()) {
        RefPtr<RegisterID> throwTypeErrorFunction;
        if (!attributes.hasGet() || !attributes.hasSet())
            throwTypeErrorFunction = moveLinkTimeConstant(nullptr, LinkTimeConstant::ThrowTypeErrorFunction);

        RefPtr<RegisterID> getter;
        if (attributes.hasGet())
            getter = getterRegister;
        else
            getter = throwTypeErrorFunction;

        RefPtr<RegisterID> setter;
        if (attributes.hasSet())
            setter = setterRegister;
        else
            setter = throwTypeErrorFunction;

        OpDefineAccessorProperty::emit(this, newObj, propertyNameRegister, getter.get(), setter.get(), emitLoad(nullptr, jsNumber(attributes.rawRepresentation())));
    } else {
        OpDefineDataProperty::emit(this, newObj, propertyNameRegister, valueRegister, emitLoad(nullptr, jsNumber(attributes.rawRepresentation())));
    }
}

RegisterID* BytecodeGenerator::emitReturn(RegisterID* src, ReturnFrom from)
{
    if (isConstructor()) {
        bool isDerived = constructorKind() == ConstructorKind::Extends;
        bool srcIsThis = src->index() == m_thisRegister.index();

        if (isDerived && (srcIsThis || from == ReturnFrom::Finally))
            emitTDZCheck(src);

        if (!srcIsThis || from == ReturnFrom::Finally) {
            Ref<Label> isObjectLabel = newLabel();
            emitJumpIfTrue(emitIsObject(newTemporary(), src), isObjectLabel.get());

            if (isDerived) {
                Ref<Label> isUndefinedLabel = newLabel();
                emitJumpIfTrue(emitIsUndefined(newTemporary(), src), isUndefinedLabel.get());
                emitThrowTypeError("Cannot return a non-object type in the constructor of a derived class.");
                emitLabel(isUndefinedLabel.get());
                emitTDZCheck(&m_thisRegister);
            }
            OpRet::emit(this, &m_thisRegister);
            emitLabel(isObjectLabel.get());
        }
    }

    OpRet::emit(this, src);
    return src;
}

RegisterID* BytecodeGenerator::emitEnd(RegisterID* src)
{
    OpEnd::emit(this, src);
    return src;
}


RegisterID* BytecodeGenerator::emitConstruct(RegisterID* dst, RegisterID* func, RegisterID* lazyThis, ExpectedFunction expectedFunction, CallArguments& callArguments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
{
    ASSERT(func->refCount());

    // Generate code for arguments.
    unsigned argument = 0;
    if (ArgumentsNode* argumentsNode = callArguments.argumentsNode()) {
        
        ArgumentListNode* n = callArguments.argumentsNode()->m_listNode;
        if (n && n->m_expr->isSpreadExpression()) {
            RELEASE_ASSERT(!n->m_next);
            auto expression = static_cast<SpreadExpressionNode*>(n->m_expr)->expression();
            if (expression->isArrayLiteral()) {
                auto* elements = static_cast<ArrayNode*>(expression)->elements();
                if (elements && !elements->next() && elements->value()->isSpreadExpression()) {
                    ExpressionNode* expression = static_cast<SpreadExpressionNode*>(elements->value())->expression();
                    RefPtr<RegisterID> argumentRegister = emitNode(callArguments.argumentRegister(0), expression);
                    OpSpread::emit(this, argumentRegister.get(), argumentRegister.get());

                    move(callArguments.thisRegister(), lazyThis);
                    RefPtr<RegisterID> thisRegister = move(newTemporary(), callArguments.thisRegister());
                    return emitConstructVarargs(dst, func, callArguments.thisRegister(), argumentRegister.get(), newTemporary(), 0, divot, divotStart, divotEnd, DebuggableCall::No);
                }
            }
            RefPtr<RegisterID> argumentRegister;
            argumentRegister = expression->emitBytecode(*this, callArguments.argumentRegister(0));
            move(callArguments.thisRegister(), lazyThis);
            return emitConstructVarargs(dst, func, callArguments.thisRegister(), argumentRegister.get(), newTemporary(), 0, divot, divotStart, divotEnd, DebuggableCall::No);
        }
        
        for (ArgumentListNode* n = argumentsNode->m_listNode; n; n = n->m_next)
            emitNode(callArguments.argumentRegister(argument++), n);
    }
    
    move(callArguments.thisRegister(), lazyThis);
    
    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, CallFrame::headerSizeInRegisters, UnsafeVectorOverflow> callFrame;
    for (int i = 0; i < CallFrame::headerSizeInRegisters; ++i)
        callFrame.append(newTemporary());

    emitExpressionInfo(divot, divotStart, divotEnd);
    
    Ref<Label> done = newLabel();
    expectedFunction = emitExpectedFunctionSnippet(dst, func, expectedFunction, callArguments, done.get());

    OpConstruct::emit(this, dst, func, callArguments.argumentCountIncludingThis(), callArguments.stackOffset());

    if (expectedFunction != NoExpectedFunction)
        emitLabel(done.get());

    return dst;
}

RegisterID* BytecodeGenerator::emitStrcat(RegisterID* dst, RegisterID* src, int count)
{
    OpStrcat::emit(this, dst, src, count);
    return dst;
}

void BytecodeGenerator::emitToPrimitive(RegisterID* dst, RegisterID* src)
{
    OpToPrimitive::emit(this, dst, src);
}

void BytecodeGenerator::emitGetScope()
{
    OpGetScope::emit(this, scopeRegister());
}

RegisterID* BytecodeGenerator::emitPushWithScope(RegisterID* objectScope)
{
    pushLocalControlFlowScope();
    RegisterID* newScope = newBlockScopeVariable();
    newScope->ref();

    OpPushWithScope::emit(this, newScope, scopeRegister(), objectScope);

    move(scopeRegister(), newScope);
    m_lexicalScopeStack.append({ nullptr, newScope, true, 0 });

    return newScope;
}

RegisterID* BytecodeGenerator::emitGetParentScope(RegisterID* dst, RegisterID* scope)
{
    OpGetParentScope::emit(this, dst, scope);
    return dst;
}

void BytecodeGenerator::emitPopScope(RegisterID* dst, RegisterID* scope)
{
    RefPtr<RegisterID> parentScope = emitGetParentScope(newTemporary(), scope);
    move(dst, parentScope.get());
}

void BytecodeGenerator::emitPopWithScope()
{
    emitPopScope(scopeRegister(), scopeRegister());
    popLocalControlFlowScope();
    auto stackEntry = m_lexicalScopeStack.takeLast();
    stackEntry.m_scope->deref();
    RELEASE_ASSERT(stackEntry.m_isWithScope);
}

void BytecodeGenerator::emitDebugHook(DebugHookType debugHookType, const JSTextPosition& divot)
{
    if (!m_shouldEmitDebugHooks)
        return;

    emitExpressionInfo(divot, divot, divot);
    OpDebug::emit(this, debugHookType, false);
}

void BytecodeGenerator::emitDebugHook(DebugHookType debugHookType, unsigned line, unsigned charOffset, unsigned lineStart)
{
    emitDebugHook(debugHookType, JSTextPosition(line, charOffset, lineStart));
}

void BytecodeGenerator::emitDebugHook(StatementNode* statement)
{
    // DebuggerStatementNode will output its own special debug hook.
    if (statement->isDebuggerStatement())
        return;

    emitDebugHook(WillExecuteStatement, statement->position());
}

void BytecodeGenerator::emitDebugHook(ExpressionNode* expr)
{
    emitDebugHook(WillExecuteStatement, expr->position());
}

void BytecodeGenerator::emitWillLeaveCallFrameDebugHook()
{
    RELEASE_ASSERT(m_scopeNode->isFunctionNode());
    emitDebugHook(WillLeaveCallFrame, m_scopeNode->lastLine(), m_scopeNode->startOffset(), m_scopeNode->lineStartOffset());
}

FinallyContext* BytecodeGenerator::pushFinallyControlFlowScope(Label& finallyLabel)
{
    ControlFlowScope scope(ControlFlowScope::Finally, currentLexicalScopeIndex(), FinallyContext(m_currentFinallyContext, finallyLabel));
    m_controlFlowScopeStack.append(WTFMove(scope));

    m_finallyDepth++;
    m_currentFinallyContext = &m_controlFlowScopeStack.last().finallyContext;
    return m_currentFinallyContext;
}

FinallyContext BytecodeGenerator::popFinallyControlFlowScope()
{
    ASSERT(m_controlFlowScopeStack.size());
    ASSERT(m_controlFlowScopeStack.last().isFinallyScope());
    ASSERT(m_finallyDepth > 0);
    ASSERT(m_currentFinallyContext);
    m_currentFinallyContext = m_currentFinallyContext->outerContext();
    m_finallyDepth--;
    return m_controlFlowScopeStack.takeLast().finallyContext;
}

LabelScope* BytecodeGenerator::breakTarget(const Identifier& name)
{
    shrinkToFit(m_labelScopes);

    if (!m_labelScopes.size())
        return nullptr;

    // We special-case the following, which is a syntax error in Firefox:
    // label:
    //     break;
    if (name.isEmpty()) {
        for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
            LabelScope& scope = m_labelScopes[i];
            if (scope.type() != LabelScope::NamedLabel)
                return &scope;
        }
        return nullptr;
    }

    for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
        LabelScope& scope = m_labelScopes[i];
        if (scope.name() && *scope.name() == name)
            return &scope;
    }
    return nullptr;
}

LabelScope* BytecodeGenerator::continueTarget(const Identifier& name)
{
    shrinkToFit(m_labelScopes);

    if (!m_labelScopes.size())
        return nullptr;

    if (name.isEmpty()) {
        for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
            LabelScope& scope = m_labelScopes[i];
            if (scope.type() == LabelScope::Loop) {
                ASSERT(scope.continueTarget());
                return &scope;
            }
        }
        return nullptr;
    }

    // Continue to the loop nested nearest to the label scope that matches
    // 'name'.
    LabelScope* result = nullptr;
    for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
        LabelScope& scope = m_labelScopes[i];
        if (scope.type() == LabelScope::Loop) {
            ASSERT(scope.continueTarget());
            result = &scope;
        }
        if (scope.name() && *scope.name() == name)
            return result; // may be null.
    }
    return nullptr;
}

void BytecodeGenerator::allocateCalleeSaveSpace()
{
    size_t virtualRegisterCountForCalleeSaves = CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters();

    for (size_t i = 0; i < virtualRegisterCountForCalleeSaves; i++) {
        RegisterID* localRegister = addVar();
        localRegister->ref();
        m_localRegistersForCalleeSaveRegisters.append(localRegister);
    }
}

void BytecodeGenerator::allocateAndEmitScope()
{
    m_scopeRegister = addVar();
    m_scopeRegister->ref();
    m_codeBlock->setScopeRegister(scopeRegister()->virtualRegister());
    emitGetScope();
    m_topMostScope = addVar();
    move(m_topMostScope, scopeRegister());
}

TryData* BytecodeGenerator::pushTry(Label& start, Label& handlerLabel, HandlerType handlerType)
{
    m_tryData.append(TryData { handlerLabel, handlerType });
    TryData* result = &m_tryData.last();
    
    m_tryContextStack.append(TryContext {
        start,
        result
    });
    
    return result;
}

void BytecodeGenerator::popTry(TryData* tryData, Label& end)
{
    m_usesExceptions = true;
    
    ASSERT_UNUSED(tryData, m_tryContextStack.last().tryData == tryData);
    
    m_tryRanges.append(TryRange {
        m_tryContextStack.last().start.copyRef(),
        end,
        m_tryContextStack.last().tryData
    });
    m_tryContextStack.removeLast();
}

void BytecodeGenerator::emitCatch(RegisterID* exceptionRegister, RegisterID* thrownValueRegister, TryData* data)
{
    m_catchesToEmit.append(CatchEntry { data, exceptionRegister, thrownValueRegister });
}

void BytecodeGenerator::restoreScopeRegister(int lexicalScopeIndex)
{
    if (lexicalScopeIndex == CurrentLexicalScopeIndex)
        return; // No change needed.

    if (lexicalScopeIndex != OutermostLexicalScopeIndex) {
        ASSERT(lexicalScopeIndex < static_cast<int>(m_lexicalScopeStack.size()));
        int endIndex = lexicalScopeIndex + 1;
        for (size_t i = endIndex; i--; ) {
            if (m_lexicalScopeStack[i].m_scope) {
                move(scopeRegister(), m_lexicalScopeStack[i].m_scope);
                return;
            }
        }
    }
    // Note that if we don't find a local scope in the current function/program,
    // we must grab the outer-most scope of this bytecode generation.
    move(scopeRegister(), m_topMostScope);
}

void BytecodeGenerator::restoreScopeRegister()
{
    restoreScopeRegister(currentLexicalScopeIndex());
}

int BytecodeGenerator::labelScopeDepthToLexicalScopeIndex(int targetLabelScopeDepth)
{
    ASSERT(labelScopeDepth() - targetLabelScopeDepth >= 0);
    size_t scopeDelta = labelScopeDepth() - targetLabelScopeDepth;
    ASSERT(scopeDelta <= m_controlFlowScopeStack.size());
    if (!scopeDelta)
        return CurrentLexicalScopeIndex;

    ControlFlowScope& targetScope = m_controlFlowScopeStack[targetLabelScopeDepth];
    return targetScope.lexicalScopeIndex;
}

void BytecodeGenerator::emitThrow(RegisterID* exc)
{
    m_usesExceptions = true;
    OpThrow::emit(this, exc);
}

RegisterID* BytecodeGenerator::emitArgumentCount(RegisterID* dst)
{
    OpArgumentCount::emit(this, dst);
    return dst;
}

int BytecodeGenerator::localScopeDepth() const
{
    return m_localScopeDepth;
}

int BytecodeGenerator::labelScopeDepth() const
{
    unsigned depth = localScopeDepth() + m_finallyDepth;
    ASSERT(depth == m_controlFlowScopeStack.size());
    return depth;
}

void BytecodeGenerator::emitThrowStaticError(ErrorType errorType, RegisterID* raw)
{
    RefPtr<RegisterID> message = newTemporary();
    emitToString(message.get(), raw);
    OpThrowStaticError::emit(this, message.get(), errorType);
}

void BytecodeGenerator::emitThrowStaticError(ErrorType errorType, const Identifier& message)
{
    OpThrowStaticError::emit(this, addConstantValue(addStringConstant(message)), errorType);
}

void BytecodeGenerator::emitThrowReferenceError(const String& message)
{
    emitThrowStaticError(ErrorType::ReferenceError, Identifier::fromString(m_vm, message));
}

void BytecodeGenerator::emitThrowTypeError(const String& message)
{
    emitThrowStaticError(ErrorType::TypeError, Identifier::fromString(m_vm, message));
}

void BytecodeGenerator::emitThrowTypeError(const Identifier& message)
{
    emitThrowStaticError(ErrorType::TypeError, message);
}

void BytecodeGenerator::emitThrowRangeError(const Identifier& message)
{
    emitThrowStaticError(ErrorType::RangeError, message);
}

void BytecodeGenerator::emitThrowOutOfMemoryError()
{
    emitThrowStaticError(ErrorType::Error, Identifier::fromString(m_vm, "Out of memory"));
}

void BytecodeGenerator::emitPushFunctionNameScope(const Identifier& property, RegisterID* callee, bool isCaptured)
{
    // There is some nuance here:
    // If we're in strict mode code, the function name scope variable acts exactly like a "const" variable.
    // If we're not in strict mode code, we want to allow bogus assignments to the name scoped variable.
    // This means any assignment to the variable won't throw, but it won't actually assign a new value to it.
    // To accomplish this, we don't report that this scope is a lexical scope. This will prevent
    // any throws when trying to assign to the variable (while still ensuring it keeps its original
    // value). There is some ugliness and exploitation of a leaky abstraction here, but it's better than 
    // having a completely new op code and a class to handle name scopes which are so close in functionality
    // to lexical environments.
    VariableEnvironment nameScopeEnvironment;
    auto addResult = nameScopeEnvironment.add(property);
    if (isCaptured)
        addResult.iterator->value.setIsCaptured();
    addResult.iterator->value.setIsConst(); // The function name scope name acts like a const variable.
    unsigned numVars = m_codeBlock->m_numVars;
    pushLexicalScopeInternal(nameScopeEnvironment, TDZCheckOptimization::Optimize, NestedScopeType::IsNotNested, nullptr, TDZRequirement::NotUnderTDZ, ScopeType::FunctionNameScope, ScopeRegisterType::Var);
    ASSERT_UNUSED(numVars, m_codeBlock->m_numVars == static_cast<int>(numVars + 1)); // Should have only created one new "var" for the function name scope.
    bool shouldTreatAsLexicalVariable = isStrictMode();
    Variable functionVar = variableForLocalEntry(property, m_lexicalScopeStack.last().m_symbolTable->get(NoLockingNecessary, property.impl()), m_lexicalScopeStack.last().m_symbolTableConstantIndex, shouldTreatAsLexicalVariable);
    emitPutToScope(m_lexicalScopeStack.last().m_scope, functionVar, callee, ThrowIfNotFound, InitializationMode::NotInitialization);
}

void BytecodeGenerator::pushLocalControlFlowScope()
{
    ControlFlowScope scope(ControlFlowScope::Label, currentLexicalScopeIndex());
    m_controlFlowScopeStack.append(WTFMove(scope));
    m_localScopeDepth++;
}

void BytecodeGenerator::popLocalControlFlowScope()
{
    ASSERT(m_controlFlowScopeStack.size());
    ASSERT(!m_controlFlowScopeStack.last().isFinallyScope());
    m_controlFlowScopeStack.removeLast();
    m_localScopeDepth--;
}

void BytecodeGenerator::emitPushCatchScope(VariableEnvironment& environment)
{
    pushLexicalScopeInternal(environment, TDZCheckOptimization::Optimize, NestedScopeType::IsNotNested, nullptr, TDZRequirement::UnderTDZ, ScopeType::CatchScope, ScopeRegisterType::Block);
}

void BytecodeGenerator::emitPopCatchScope(VariableEnvironment& environment) 
{
    popLexicalScopeInternal(environment);
}

void BytecodeGenerator::beginSwitch(RegisterID* scrutineeRegister, SwitchInfo::SwitchType type)
{
    switch (type) {
    case SwitchInfo::SwitchImmediate: {
        size_t tableIndex = m_codeBlock->numberOfSwitchJumpTables();
        m_codeBlock->addSwitchJumpTable();
        OpSwitchImm::emit(this, tableIndex, BoundLabel(), scrutineeRegister);
        break;
    }
    case SwitchInfo::SwitchCharacter: {
        size_t tableIndex = m_codeBlock->numberOfSwitchJumpTables();
        m_codeBlock->addSwitchJumpTable();
        OpSwitchChar::emit(this, tableIndex, BoundLabel(), scrutineeRegister);
        break;
    }
    case SwitchInfo::SwitchString: {
        size_t tableIndex = m_codeBlock->numberOfStringSwitchJumpTables();
        m_codeBlock->addStringSwitchJumpTable();
        OpSwitchString::emit(this, tableIndex, BoundLabel(), scrutineeRegister);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    SwitchInfo info = { m_lastInstruction.offset(), type };
    m_switchContextStack.append(info);
}

static int32_t keyForImmediateSwitch(ExpressionNode* node, int32_t min, int32_t max)
{
    UNUSED_PARAM(max);
    ASSERT(node->isNumber());
    double value = static_cast<NumberNode*>(node)->value();
    int32_t key = static_cast<int32_t>(value);
    ASSERT(key == value);
    ASSERT(key >= min);
    ASSERT(key <= max);
    return key - min;
}

static int32_t keyForCharacterSwitch(ExpressionNode* node, int32_t min, int32_t max)
{
    UNUSED_PARAM(max);
    ASSERT(node->isString());
    StringImpl* clause = static_cast<StringNode*>(node)->value().impl();
    ASSERT(clause->length() == 1);
    
    int32_t key = (*clause)[0];
    ASSERT(key >= min);
    ASSERT(key <= max);
    return key - min;
}

static void prepareJumpTableForSwitch(
    UnlinkedSimpleJumpTable& jumpTable, int32_t switchAddress, uint32_t clauseCount,
    const Vector<Ref<Label>, 8>& labels, ExpressionNode** nodes, int32_t min, int32_t max,
    int32_t (*keyGetter)(ExpressionNode*, int32_t min, int32_t max))
{
    jumpTable.min = min;
    jumpTable.branchOffsets.resize(max - min + 1);
    jumpTable.branchOffsets.fill(0);
    for (uint32_t i = 0; i < clauseCount; ++i) {
        // We're emitting this after the clause labels should have been fixed, so 
        // the labels should not be "forward" references
        ASSERT(!labels[i]->isForward());
        jumpTable.add(keyGetter(nodes[i], min, max), labels[i]->bind(switchAddress)); 
    }
}

static void prepareJumpTableForStringSwitch(UnlinkedStringJumpTable& jumpTable, int32_t switchAddress, uint32_t clauseCount, const Vector<Ref<Label>, 8>& labels, ExpressionNode** nodes)
{
    for (uint32_t i = 0; i < clauseCount; ++i) {
        // We're emitting this after the clause labels should have been fixed, so 
        // the labels should not be "forward" references
        ASSERT(!labels[i]->isForward());
        
        ASSERT(nodes[i]->isString());
        StringImpl* clause = static_cast<StringNode*>(nodes[i])->value().impl();
        jumpTable.offsetTable.add(clause, UnlinkedStringJumpTable::OffsetLocation { labels[i]->bind(switchAddress) });
    }
}

void BytecodeGenerator::endSwitch(uint32_t clauseCount, const Vector<Ref<Label>, 8>& labels, ExpressionNode** nodes, Label& defaultLabel, int32_t min, int32_t max)
{
    SwitchInfo switchInfo = m_switchContextStack.last();
    m_switchContextStack.removeLast();

    BoundLabel defaultTarget = defaultLabel.bind(switchInfo.bytecodeOffset);
    auto handleSwitch = [&](auto* op, auto bytecode) {
        op->setDefaultOffset(defaultTarget, [&]() {
            m_codeBlock->addOutOfLineJumpTarget(switchInfo.bytecodeOffset, defaultTarget);
            return BoundLabel();
        });

        UnlinkedSimpleJumpTable& jumpTable = m_codeBlock->switchJumpTable(bytecode.tableIndex);
        prepareJumpTableForSwitch(
            jumpTable, switchInfo.bytecodeOffset, clauseCount, labels, nodes, min, max,
            switchInfo.switchType == SwitchInfo::SwitchImmediate
                ? keyForImmediateSwitch
                : keyForCharacterSwitch); 
    };
    
    auto ref = m_writer.ref(switchInfo.bytecodeOffset);
    switch (switchInfo.switchType) {
    case SwitchInfo::SwitchImmediate: {
        handleSwitch(ref->cast<OpSwitchImm>(), ref->as<OpSwitchImm>());
        break;
    }
    case SwitchInfo::SwitchCharacter: {
        handleSwitch(ref->cast<OpSwitchChar>(), ref->as<OpSwitchChar>());
        break;
    }
        
    case SwitchInfo::SwitchString: {
        ref->cast<OpSwitchString>()->setDefaultOffset(defaultTarget, [&]() {
            m_codeBlock->addOutOfLineJumpTarget(switchInfo.bytecodeOffset, defaultTarget);
            return BoundLabel();
        });

        UnlinkedStringJumpTable& jumpTable = m_codeBlock->stringSwitchJumpTable(ref->as<OpSwitchString>().tableIndex);
        prepareJumpTableForStringSwitch(jumpTable, switchInfo.bytecodeOffset, clauseCount, labels, nodes);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

RegisterID* BytecodeGenerator::emitThrowExpressionTooDeepException()
{
    // It would be nice to do an even better job of identifying exactly where the expression is.
    // And we could make the caller pass the node pointer in, if there was some way of getting
    // that from an arbitrary node. However, calling emitExpressionInfo without any useful data
    // is still good enough to get us an accurate line number.
    m_expressionTooDeep = true;
    return newTemporary();
}

bool BytecodeGenerator::isArgumentNumber(const Identifier& ident, int argumentNumber)
{
    RegisterID* registerID = variable(ident).local();
    if (!registerID)
        return false;
    return registerID->index() == CallFrame::argumentOffset(argumentNumber);
}

bool BytecodeGenerator::emitReadOnlyExceptionIfNeeded(const Variable& variable)
{
    // If we're in strict mode, we always throw.
    // If we're not in strict mode, we throw for "const" variables but not the function callee.
    if (isStrictMode() || variable.isConst()) {
        emitThrowTypeError(Identifier::fromString(m_vm, ReadonlyPropertyWriteError));
        return true;
    }
    return false;
}

void BytecodeGenerator::emitEnumeration(ThrowableExpressionData* node, ExpressionNode* subjectNode, const ScopedLambda<void(BytecodeGenerator&, RegisterID*)>& callBack, ForOfNode* forLoopNode, RegisterID* forLoopSymbolTable)
{
    bool isForAwait = forLoopNode ? forLoopNode->isForAwait() : false;
    ASSERT(!isForAwait || (isForAwait && isAsyncFunctionParseMode(parseMode())));

    CompletionRecordScope completionRecordScope(*this);

    RefPtr<RegisterID> subject = newTemporary();
    emitNode(subject.get(), subjectNode);
    RefPtr<RegisterID> iterator = isForAwait ? emitGetAsyncIterator(subject.get(), node) : emitGetIterator(subject.get(), node);
    RefPtr<RegisterID> nextMethod = emitGetById(newTemporary(), iterator.get(), propertyNames().next);

    Ref<Label> loopDone = newLabel();
    Ref<Label> tryStartLabel = newLabel();
    Ref<Label> finallyViaThrowLabel = newLabel();
    Ref<Label> finallyLabel = newLabel();
    Ref<Label> catchLabel = newLabel();
    Ref<Label> endCatchLabel = newLabel();

    // RefPtr<Register> iterator's lifetime must be longer than IteratorCloseContext.
    FinallyContext* finallyContext = pushFinallyControlFlowScope(finallyLabel.get());

    {
        Ref<LabelScope> scope = newLabelScope(LabelScope::Loop);
        RefPtr<RegisterID> value = newTemporary();
        emitLoad(value.get(), jsUndefined());

        emitJump(*scope->continueTarget());

        Ref<Label> loopStart = newLabel();
        emitLabel(loopStart.get());
        emitLoopHint();

        emitLabel(tryStartLabel.get());
        TryData* tryData = pushTry(tryStartLabel.get(), finallyViaThrowLabel.get(), HandlerType::SynthesizedFinally);
        callBack(*this, value.get());
        emitJump(*scope->continueTarget());

        // IteratorClose sequence for abrupt completions.
        {
            // Finally block for the enumeration.
            emitLabel(finallyViaThrowLabel.get());
            popTry(tryData, finallyViaThrowLabel.get());

            Ref<Label> finallyBodyLabel = newLabel();
            RefPtr<RegisterID> finallyExceptionRegister = newTemporary();
            RegisterID* unused = newTemporary();

            emitCatch(completionValueRegister(), unused, tryData);
            emitSetCompletionType(CompletionType::Throw);
            move(finallyExceptionRegister.get(), completionValueRegister());
            emitJump(finallyBodyLabel.get());

            emitLabel(finallyLabel.get());
            moveEmptyValue(finallyExceptionRegister.get());

            emitLabel(finallyBodyLabel.get());
            restoreScopeRegister();

            Ref<Label> finallyDone = newLabel();

            RefPtr<RegisterID> returnMethod = emitGetById(newTemporary(), iterator.get(), propertyNames().returnKeyword);
            emitJumpIfTrue(emitIsUndefined(newTemporary(), returnMethod.get()), finallyDone.get());

            Ref<Label> returnCallTryStart = newLabel();
            emitLabel(returnCallTryStart.get());
            TryData* returnCallTryData = pushTry(returnCallTryStart.get(), catchLabel.get(), HandlerType::SynthesizedCatch);

            CallArguments returnArguments(*this, nullptr);
            move(returnArguments.thisRegister(), iterator.get());
            emitCall(value.get(), returnMethod.get(), NoExpectedFunction, returnArguments, node->divot(), node->divotStart(), node->divotEnd(), DebuggableCall::No);

            if (isForAwait)
                emitAwait(value.get());

            emitJumpIfTrue(emitIsObject(newTemporary(), value.get()), finallyDone.get());
            emitThrowTypeError("Iterator result interface is not an object."_s);

            emitLabel(finallyDone.get());
            emitFinallyCompletion(*finallyContext, completionTypeRegister(), endCatchLabel.get());

            popTry(returnCallTryData, finallyDone.get());

            // Catch block for exceptions that may be thrown while calling the return
            // handler in the enumeration finally block. The only reason we need this
            // catch block is because if entered the above finally block due to a thrown
            // exception, then we want to re-throw the original exception on exiting
            // the finally block. Otherwise, we'll let any new exception pass through.
            {
                emitLabel(catchLabel.get());
                RefPtr<RegisterID> exceptionRegister = newTemporary();
                RegisterID* unused = newTemporary();
                emitCatch(exceptionRegister.get(), unused, returnCallTryData);
                // Since this is a synthesized catch block and we're guaranteed to never need
                // to resolve any symbols from the scope, we can skip restoring the scope
                // register here.

                Ref<Label> throwLabel = newLabel();
                emitJumpIfTrue(emitIsEmpty(newTemporary(), finallyExceptionRegister.get()), throwLabel.get());
                move(exceptionRegister.get(), finallyExceptionRegister.get());

                emitLabel(throwLabel.get());
                emitThrow(exceptionRegister.get());

                emitLabel(endCatchLabel.get());
            }
        }

        emitLabel(*scope->continueTarget());
        if (forLoopNode) {
            RELEASE_ASSERT(forLoopNode->isForOfNode());
            prepareLexicalScopeForNextForLoopIteration(forLoopNode, forLoopSymbolTable);
            emitDebugHook(forLoopNode->lexpr());
        }

        {
            emitIteratorNext(value.get(), nextMethod.get(), iterator.get(), node, isForAwait ? EmitAwait::Yes : EmitAwait::No);

            emitJumpIfTrue(emitGetById(newTemporary(), value.get(), propertyNames().done), loopDone.get());
            emitGetById(value.get(), value.get(), propertyNames().value);
            emitJump(loopStart.get());
        }

        bool breakLabelIsBound = scope->breakTargetMayBeBound();
        if (breakLabelIsBound)
            emitLabel(scope->breakTarget());
        popFinallyControlFlowScope();
        if (breakLabelIsBound) {
            // IteratorClose sequence for break-ed control flow.
            emitIteratorClose(iterator.get(), node, isForAwait ? EmitAwait::Yes : EmitAwait::No);
        }
    }
    emitLabel(loopDone.get());
}

RegisterID* BytecodeGenerator::emitGetTemplateObject(RegisterID* dst, TaggedTemplateNode* taggedTemplate)
{
    TemplateObjectDescriptor::StringVector rawStrings;
    TemplateObjectDescriptor::OptionalStringVector cookedStrings;

    TemplateStringListNode* templateString = taggedTemplate->templateLiteral()->templateStrings();
    for (; templateString; templateString = templateString->next()) {
        auto* string = templateString->value();
        ASSERT(string->raw());
        rawStrings.append(string->raw()->impl());
        if (!string->cooked())
            cookedStrings.append(WTF::nullopt);
        else
            cookedStrings.append(string->cooked()->impl());
    }
    RefPtr<RegisterID> constant = addTemplateObjectConstant(TemplateObjectDescriptor::create(WTFMove(rawStrings), WTFMove(cookedStrings)));
    if (!dst)
        return constant.get();
    return move(dst, constant.get());
}

RegisterID* BytecodeGenerator::emitGetGlobalPrivate(RegisterID* dst, const Identifier& property)
{
    dst = tempDestination(dst);
    Variable var = variable(property);
    if (RegisterID* local = var.local())
        return move(dst, local);

    RefPtr<RegisterID> scope = newTemporary();
    move(scope.get(), emitResolveScope(scope.get(), var));
    return emitGetFromScope(dst, scope.get(), var, ThrowIfNotFound);
}

RegisterID* BytecodeGenerator::emitGetEnumerableLength(RegisterID* dst, RegisterID* base)
{
    OpGetEnumerableLength::emit(this, dst, base);
    return dst;
}

RegisterID* BytecodeGenerator::emitHasGenericProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName)
{
    OpHasGenericProperty::emit(this, dst, base, propertyName);
    return dst;
}

RegisterID* BytecodeGenerator::emitHasIndexedProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName)
{
    OpHasIndexedProperty::emit(this, dst, base, propertyName);
    return dst;
}

RegisterID* BytecodeGenerator::emitHasStructureProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName, RegisterID* enumerator)
{
    OpHasStructureProperty::emit(this, dst, base, propertyName, enumerator);
    return dst;
}

RegisterID* BytecodeGenerator::emitGetPropertyEnumerator(RegisterID* dst, RegisterID* base)
{
    OpGetPropertyEnumerator::emit(this, dst, base);
    return dst;
}

RegisterID* BytecodeGenerator::emitEnumeratorStructurePropertyName(RegisterID* dst, RegisterID* enumerator, RegisterID* index)
{
    OpEnumeratorStructurePname::emit(this, dst, enumerator, index);
    return dst;
}

RegisterID* BytecodeGenerator::emitEnumeratorGenericPropertyName(RegisterID* dst, RegisterID* enumerator, RegisterID* index)
{
    OpEnumeratorGenericPname::emit(this, dst, enumerator, index);
    return dst;
}

RegisterID* BytecodeGenerator::emitToIndexString(RegisterID* dst, RegisterID* index)
{
    OpToIndexString::emit(this, dst, index);
    return dst;
}

RegisterID* BytecodeGenerator::emitIsCellWithType(RegisterID* dst, RegisterID* src, JSType type)
{
    OpIsCellWithType::emit(this, dst, src, type);
    return dst;
}

RegisterID* BytecodeGenerator::emitIsObject(RegisterID* dst, RegisterID* src)
{
    OpIsObject::emit(this, dst, src);
    return dst;
}

RegisterID* BytecodeGenerator::emitIsNumber(RegisterID* dst, RegisterID* src)
{
    OpIsNumber::emit(this, dst, src);
    return dst;
}

RegisterID* BytecodeGenerator::emitIsUndefined(RegisterID* dst, RegisterID* src)
{
    OpIsUndefined::emit(this, dst, src);
    return dst;
}

RegisterID* BytecodeGenerator::emitIsEmpty(RegisterID* dst, RegisterID* src)
{
    OpIsEmpty::emit(this, dst, src);
    return dst;
}
    
RegisterID* BytecodeGenerator::emitIteratorNext(RegisterID* dst, RegisterID* nextMethod, RegisterID* iterator, const ThrowableExpressionData* node, EmitAwait doEmitAwait)
{
    {
        CallArguments nextArguments(*this, nullptr);
        move(nextArguments.thisRegister(), iterator);
        emitCall(dst, nextMethod, NoExpectedFunction, nextArguments, node->divot(), node->divotStart(), node->divotEnd(), DebuggableCall::No);

        if (doEmitAwait == EmitAwait::Yes)
            emitAwait(dst);
    }
    {
        Ref<Label> typeIsObject = newLabel();
        emitJumpIfTrue(emitIsObject(newTemporary(), dst), typeIsObject.get());
        emitThrowTypeError("Iterator result interface is not an object."_s);
        emitLabel(typeIsObject.get());
    }
    return dst;
}

RegisterID* BytecodeGenerator::emitIteratorNextWithValue(RegisterID* dst, RegisterID* nextMethod, RegisterID* iterator, RegisterID* value, const ThrowableExpressionData* node)
{
    {
        CallArguments nextArguments(*this, nullptr, 1);
        move(nextArguments.thisRegister(), iterator);
        move(nextArguments.argumentRegister(0), value);
        emitCall(dst, nextMethod, NoExpectedFunction, nextArguments, node->divot(), node->divotStart(), node->divotEnd(), DebuggableCall::No);
    }

    return dst;
}

void BytecodeGenerator::emitIteratorClose(RegisterID* iterator, const ThrowableExpressionData* node, EmitAwait doEmitAwait)
{
    Ref<Label> done = newLabel();
    RefPtr<RegisterID> returnMethod = emitGetById(newTemporary(), iterator, propertyNames().returnKeyword);
    emitJumpIfTrue(emitIsUndefined(newTemporary(), returnMethod.get()), done.get());

    RefPtr<RegisterID> value = newTemporary();
    CallArguments returnArguments(*this, nullptr);
    move(returnArguments.thisRegister(), iterator);
    emitCall(value.get(), returnMethod.get(), NoExpectedFunction, returnArguments, node->divot(), node->divotStart(), node->divotEnd(), DebuggableCall::No);

    if (doEmitAwait == EmitAwait::Yes)
        emitAwait(value.get());

    emitJumpIfTrue(emitIsObject(newTemporary(), value.get()), done.get());
    emitThrowTypeError("Iterator result interface is not an object."_s);
    emitLabel(done.get());
}

void BytecodeGenerator::pushIndexedForInScope(RegisterID* localRegister, RegisterID* indexRegister)
{
    if (!localRegister)
        return;
    unsigned bodyBytecodeStartOffset = instructions().size();
    m_forInContextStack.append(adoptRef(*new IndexedForInContext(localRegister, indexRegister, bodyBytecodeStartOffset)));
}

void BytecodeGenerator::popIndexedForInScope(RegisterID* localRegister)
{
    if (!localRegister)
        return;
    unsigned bodyBytecodeEndOffset = instructions().size();
    m_forInContextStack.last()->asIndexedForInContext().finalize(*this, m_codeBlock.get(), bodyBytecodeEndOffset);
    m_forInContextStack.removeLast();
}

RegisterID* BytecodeGenerator::emitLoadArrowFunctionLexicalEnvironment(const Identifier& identifier)
{
    ASSERT(m_codeBlock->isArrowFunction() || m_codeBlock->isArrowFunctionContext() || constructorKind() == ConstructorKind::Extends || m_codeType == EvalCode);

    return emitResolveScope(nullptr, variable(identifier, ThisResolutionType::Scoped));
}

void BytecodeGenerator::emitLoadThisFromArrowFunctionLexicalEnvironment()
{
    emitGetFromScope(thisRegister(), emitLoadArrowFunctionLexicalEnvironment(propertyNames().thisIdentifier), variable(propertyNames().thisIdentifier, ThisResolutionType::Scoped), DoNotThrowIfNotFound);
}
    
RegisterID* BytecodeGenerator::emitLoadNewTargetFromArrowFunctionLexicalEnvironment()
{
    Variable newTargetVar = variable(propertyNames().builtinNames().newTargetLocalPrivateName());

    return emitGetFromScope(m_newTargetRegister, emitLoadArrowFunctionLexicalEnvironment(propertyNames().builtinNames().newTargetLocalPrivateName()), newTargetVar, ThrowIfNotFound);
    
}

RegisterID* BytecodeGenerator::emitLoadDerivedConstructorFromArrowFunctionLexicalEnvironment()
{
    Variable protoScopeVar = variable(propertyNames().builtinNames().derivedConstructorPrivateName());
    return emitGetFromScope(newTemporary(), emitLoadArrowFunctionLexicalEnvironment(propertyNames().builtinNames().derivedConstructorPrivateName()), protoScopeVar, ThrowIfNotFound);
}

RegisterID* BytecodeGenerator::ensureThis()
{
    if (constructorKind() == ConstructorKind::Extends || isDerivedConstructorContext()) {
        if ((needsToUpdateArrowFunctionContext() && isSuperCallUsedInInnerArrowFunction()) || m_codeBlock->parseMode() == SourceParseMode::AsyncArrowFunctionBodyMode)
            emitLoadThisFromArrowFunctionLexicalEnvironment();

        emitTDZCheck(thisRegister());
    }

    return thisRegister();
}
    
bool BytecodeGenerator::isThisUsedInInnerArrowFunction()
{
    return m_scopeNode->doAnyInnerArrowFunctionsUseThis() || m_scopeNode->doAnyInnerArrowFunctionsUseSuperProperty() || m_scopeNode->doAnyInnerArrowFunctionsUseSuperCall() || m_scopeNode->doAnyInnerArrowFunctionsUseEval() || m_codeBlock->usesEval();
}
    
bool BytecodeGenerator::isArgumentsUsedInInnerArrowFunction()
{
    return m_scopeNode->doAnyInnerArrowFunctionsUseArguments() || m_scopeNode->doAnyInnerArrowFunctionsUseEval();
}

bool BytecodeGenerator::isNewTargetUsedInInnerArrowFunction()
{
    return m_scopeNode->doAnyInnerArrowFunctionsUseNewTarget() || m_scopeNode->doAnyInnerArrowFunctionsUseSuperCall() || m_scopeNode->doAnyInnerArrowFunctionsUseEval() || m_codeBlock->usesEval();
}

bool BytecodeGenerator::isSuperUsedInInnerArrowFunction()
{
    return m_scopeNode->doAnyInnerArrowFunctionsUseSuperCall() || m_scopeNode->doAnyInnerArrowFunctionsUseSuperProperty() || m_scopeNode->doAnyInnerArrowFunctionsUseEval() || m_codeBlock->usesEval();
}

bool BytecodeGenerator::isSuperCallUsedInInnerArrowFunction()
{
    return m_scopeNode->doAnyInnerArrowFunctionsUseSuperCall() || m_scopeNode->doAnyInnerArrowFunctionsUseEval() || m_codeBlock->usesEval();
}
    
void BytecodeGenerator::emitPutNewTargetToArrowFunctionContextScope()
{
    if (isNewTargetUsedInInnerArrowFunction()) {
        ASSERT(m_arrowFunctionContextLexicalEnvironmentRegister);
        
        Variable newTargetVar = variable(propertyNames().builtinNames().newTargetLocalPrivateName());
        emitPutToScope(m_arrowFunctionContextLexicalEnvironmentRegister, newTargetVar, newTarget(), DoNotThrowIfNotFound, InitializationMode::Initialization);
    }
}
    
void BytecodeGenerator::emitPutDerivedConstructorToArrowFunctionContextScope()
{
    if (needsDerivedConstructorInArrowFunctionLexicalEnvironment()) {
        ASSERT(m_arrowFunctionContextLexicalEnvironmentRegister);

        Variable protoScope = variable(propertyNames().builtinNames().derivedConstructorPrivateName());
        emitPutToScope(m_arrowFunctionContextLexicalEnvironmentRegister, protoScope, &m_calleeRegister, DoNotThrowIfNotFound, InitializationMode::Initialization);
    }
}

void BytecodeGenerator::emitPutThisToArrowFunctionContextScope()
{
    if (isThisUsedInInnerArrowFunction() || (m_scopeNode->usesSuperCall() && m_codeType == EvalCode)) {
        ASSERT(isDerivedConstructorContext() || m_arrowFunctionContextLexicalEnvironmentRegister != nullptr);

        Variable thisVar = variable(propertyNames().thisIdentifier, ThisResolutionType::Scoped);
        RegisterID* scope = isDerivedConstructorContext() ? emitLoadArrowFunctionLexicalEnvironment(propertyNames().thisIdentifier) : m_arrowFunctionContextLexicalEnvironmentRegister;
    
        emitPutToScope(scope, thisVar, thisRegister(), ThrowIfNotFound, InitializationMode::NotInitialization);
    }
}

void BytecodeGenerator::pushStructureForInScope(RegisterID* localRegister, RegisterID* indexRegister, RegisterID* propertyRegister, RegisterID* enumeratorRegister)
{
    if (!localRegister)
        return;
    unsigned bodyBytecodeStartOffset = instructions().size();
    m_forInContextStack.append(adoptRef(*new StructureForInContext(localRegister, indexRegister, propertyRegister, enumeratorRegister, bodyBytecodeStartOffset)));
}

void BytecodeGenerator::popStructureForInScope(RegisterID* localRegister)
{
    if (!localRegister)
        return;
    unsigned bodyBytecodeEndOffset = instructions().size();
    m_forInContextStack.last()->asStructureForInContext().finalize(*this, m_codeBlock.get(), bodyBytecodeEndOffset);
    m_forInContextStack.removeLast();
}

RegisterID* BytecodeGenerator::emitRestParameter(RegisterID* result, unsigned numParametersToSkip)
{
    RefPtr<RegisterID> restArrayLength = newTemporary();
    OpGetRestLength::emit(this, restArrayLength.get(), numParametersToSkip);

    OpCreateRest::emit(this, result, restArrayLength.get(), numParametersToSkip);

    return result;
}

void BytecodeGenerator::emitRequireObjectCoercible(RegisterID* value, const String& error)
{
    // FIXME: op_jneq_null treats "undetectable" objects as null/undefined. RequireObjectCoercible
    // thus incorrectly throws a TypeError for interfaces like HTMLAllCollection.
    Ref<Label> target = newLabel();
    OpJneqNull::emit(this, value, target->bind(this));
    emitThrowTypeError(error);
    emitLabel(target.get());
}

void BytecodeGenerator::emitYieldPoint(RegisterID* argument, JSAsyncGeneratorFunction::AsyncGeneratorSuspendReason result)
{
    Ref<Label> mergePoint = newLabel();
    unsigned yieldPointIndex = m_yieldPoints++;
    emitGeneratorStateChange(yieldPointIndex + 1);

    if (parseMode() == SourceParseMode::AsyncGeneratorBodyMode) {
        int suspendReason = static_cast<int32_t>(result);
        emitPutById(generatorRegister(), propertyNames().builtinNames().asyncGeneratorSuspendReasonPrivateName(), emitLoad(nullptr, jsNumber(suspendReason)));
    }

    // Split the try range here.
    Ref<Label> savePoint = newEmittedLabel();
    for (unsigned i = m_tryContextStack.size(); i--;) {
        TryContext& context = m_tryContextStack[i];
        m_tryRanges.append(TryRange {
            context.start.copyRef(),
            savePoint.copyRef(),
            context.tryData
        });
        // Try range will be restared at the merge point.
        context.start = mergePoint.get();
    }
    Vector<TryContext> savedTryContextStack;
    m_tryContextStack.swap(savedTryContextStack);


#if CPU(NEEDS_ALIGNED_ACCESS)
    // conservatively align for the bytecode rewriter: it will delete this yield and
    // append a fragment, so we make sure that the start of the fragments is aligned
    while (m_writer.position() % OpcodeSize::Wide)
        OpNop::emit<OpcodeSize::Narrow>(this);
#endif
    OpYield::emit(this, generatorFrameRegister(), yieldPointIndex, argument);

    // Restore the try contexts, which start offset is updated to the merge point.
    m_tryContextStack.swap(savedTryContextStack);
    emitLabel(mergePoint.get());
}

RegisterID* BytecodeGenerator::emitYield(RegisterID* argument, JSAsyncGeneratorFunction::AsyncGeneratorSuspendReason result)
{
    emitYieldPoint(argument, result);

    Ref<Label> normalLabel = newLabel();
    RefPtr<RegisterID> condition = newTemporary();
    emitEqualityOp<OpStricteq>(condition.get(), generatorResumeModeRegister(), emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSGeneratorFunction::GeneratorResumeMode::NormalMode))));
    emitJumpIfTrue(condition.get(), normalLabel.get());

    Ref<Label> throwLabel = newLabel();
    emitEqualityOp<OpStricteq>(condition.get(), generatorResumeModeRegister(), emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSGeneratorFunction::GeneratorResumeMode::ThrowMode))));
    emitJumpIfTrue(condition.get(), throwLabel.get());
    // Return.
    {
        RefPtr<RegisterID> returnRegister = generatorValueRegister();
        bool hasFinally = emitReturnViaFinallyIfNeeded(returnRegister.get());
        if (!hasFinally)
            emitReturn(returnRegister.get());
    }

    // Throw.
    emitLabel(throwLabel.get());
    emitThrow(generatorValueRegister());

    // Normal.
    emitLabel(normalLabel.get());
    return generatorValueRegister();
}

RegisterID* BytecodeGenerator::emitCallIterator(RegisterID* iterator, RegisterID* argument, ThrowableExpressionData* node)
{
    CallArguments args(*this, nullptr);
    move(args.thisRegister(), argument);
    emitCall(iterator, iterator, NoExpectedFunction, args, node->divot(), node->divotStart(), node->divotEnd(), DebuggableCall::No);

    return iterator;
}

void BytecodeGenerator::emitAwait(RegisterID* value)
{
    emitYield(value, JSAsyncGeneratorFunction::AsyncGeneratorSuspendReason::Await);
    move(value, generatorValueRegister());
}

RegisterID* BytecodeGenerator::emitGetIterator(RegisterID* argument, ThrowableExpressionData* node)
{
    RefPtr<RegisterID> iterator = emitGetById(newTemporary(), argument, propertyNames().iteratorSymbol);
    emitCallIterator(iterator.get(), argument, node);

    return iterator.get();
}

RegisterID* BytecodeGenerator::emitGetAsyncIterator(RegisterID* argument, ThrowableExpressionData* node)
{
    RefPtr<RegisterID> iterator = emitGetById(newTemporary(), argument, propertyNames().asyncIteratorSymbol);
    Ref<Label> asyncIteratorNotFound = newLabel();
    Ref<Label> asyncIteratorFound = newLabel();
    Ref<Label> iteratorReceived = newLabel();

    emitJumpIfTrue(emitUnaryOp<OpEqNull>(newTemporary(), iterator.get()), asyncIteratorNotFound.get());

    emitJump(asyncIteratorFound.get());
    emitLabel(asyncIteratorNotFound.get());

    RefPtr<RegisterID> commonIterator = emitGetIterator(argument, node);
    move(iterator.get(), commonIterator.get());

    RefPtr<RegisterID> nextMethod = emitGetById(newTemporary(), iterator.get(), propertyNames().next);

    auto varCreateAsyncFromSyncIterator = variable(propertyNames().builtinNames().createAsyncFromSyncIteratorPrivateName());
    RefPtr<RegisterID> scope = newTemporary();
    move(scope.get(), emitResolveScope(scope.get(), varCreateAsyncFromSyncIterator));
    RefPtr<RegisterID> createAsyncFromSyncIterator = emitGetFromScope(newTemporary(), scope.get(), varCreateAsyncFromSyncIterator, ThrowIfNotFound);

    CallArguments args(*this, nullptr, 2);
    emitLoad(args.thisRegister(), jsUndefined());

    move(args.argumentRegister(0), iterator.get());
    move(args.argumentRegister(1), nextMethod.get());

    JSTextPosition divot(m_scopeNode->firstLine(), m_scopeNode->startOffset(), m_scopeNode->lineStartOffset());
    emitCall(iterator.get(), createAsyncFromSyncIterator.get(), NoExpectedFunction, args, divot, divot, divot, DebuggableCall::No);

    emitJump(iteratorReceived.get());

    emitLabel(asyncIteratorFound.get());
    emitCallIterator(iterator.get(), argument, node);
    emitLabel(iteratorReceived.get());

    return iterator.get();
}

RegisterID* BytecodeGenerator::emitDelegateYield(RegisterID* argument, ThrowableExpressionData* node)
{
    RefPtr<RegisterID> value = newTemporary();
    {
        RefPtr<RegisterID> iterator = parseMode() == SourceParseMode::AsyncGeneratorBodyMode ? emitGetAsyncIterator(argument, node) : emitGetIterator(argument, node);
        RefPtr<RegisterID> nextMethod = emitGetById(newTemporary(), iterator.get(), propertyNames().next);

        Ref<Label> loopDone = newLabel();
        {
            Ref<Label> nextElement = newLabel();
            emitLoad(value.get(), jsUndefined());

            emitJump(nextElement.get());

            Ref<Label> loopStart = newLabel();
            emitLabel(loopStart.get());
            emitLoopHint();

            Ref<Label> branchOnResult = newLabel();
            {
                emitYieldPoint(value.get(), JSAsyncGeneratorFunction::AsyncGeneratorSuspendReason::Yield);

                Ref<Label> normalLabel = newLabel();
                Ref<Label> returnLabel = newLabel();
                {
                    RefPtr<RegisterID> condition = newTemporary();
                    emitEqualityOp<OpStricteq>(condition.get(), generatorResumeModeRegister(), emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSGeneratorFunction::GeneratorResumeMode::NormalMode))));
                    emitJumpIfTrue(condition.get(), normalLabel.get());

                    emitEqualityOp<OpStricteq>(condition.get(), generatorResumeModeRegister(), emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSGeneratorFunction::GeneratorResumeMode::ReturnMode))));
                    emitJumpIfTrue(condition.get(), returnLabel.get());

                    // Fallthrough to ThrowMode.
                }

                // Throw.
                {
                    Ref<Label> throwMethodFound = newLabel();
                    RefPtr<RegisterID> throwMethod = emitGetById(newTemporary(), iterator.get(), propertyNames().throwKeyword);
                    emitJumpIfFalse(emitIsUndefined(newTemporary(), throwMethod.get()), throwMethodFound.get());

                    EmitAwait emitAwaitInIteratorClose = parseMode() == SourceParseMode::AsyncGeneratorBodyMode ? EmitAwait::Yes : EmitAwait::No;
                    emitIteratorClose(iterator.get(), node, emitAwaitInIteratorClose);

                    emitThrowTypeError("Delegated generator does not have a 'throw' method."_s);

                    emitLabel(throwMethodFound.get());
                    CallArguments throwArguments(*this, nullptr, 1);
                    move(throwArguments.thisRegister(), iterator.get());
                    move(throwArguments.argumentRegister(0), generatorValueRegister());
                    emitCall(value.get(), throwMethod.get(), NoExpectedFunction, throwArguments, node->divot(), node->divotStart(), node->divotEnd(), DebuggableCall::No);

                    emitJump(branchOnResult.get());
                }

                // Return.
                emitLabel(returnLabel.get());
                {
                    Ref<Label> returnMethodFound = newLabel();
                    RefPtr<RegisterID> returnMethod = emitGetById(newTemporary(), iterator.get(), propertyNames().returnKeyword);
                    emitJumpIfFalse(emitIsUndefined(newTemporary(), returnMethod.get()), returnMethodFound.get());

                    move(value.get(), generatorValueRegister());

                    Ref<Label> returnSequence = newLabel();
                    emitJump(returnSequence.get());

                    emitLabel(returnMethodFound.get());
                    CallArguments returnArguments(*this, nullptr, 1);
                    move(returnArguments.thisRegister(), iterator.get());
                    move(returnArguments.argumentRegister(0), generatorValueRegister());
                    emitCall(value.get(), returnMethod.get(), NoExpectedFunction, returnArguments, node->divot(), node->divotStart(), node->divotEnd(), DebuggableCall::No);

                    if (parseMode() == SourceParseMode::AsyncGeneratorBodyMode)
                        emitAwait(value.get());

                    Ref<Label> returnIteratorResultIsObject = newLabel();
                    emitJumpIfTrue(emitIsObject(newTemporary(), value.get()), returnIteratorResultIsObject.get());
                    emitThrowTypeError("Iterator result interface is not an object."_s);

                    emitLabel(returnIteratorResultIsObject.get());

                    Ref<Label> returnFromGenerator = newLabel();
                    emitJumpIfTrue(emitGetById(newTemporary(), value.get(), propertyNames().done), returnFromGenerator.get());

                    emitGetById(value.get(), value.get(), propertyNames().value);
                    emitJump(loopStart.get());

                    emitLabel(returnFromGenerator.get());
                    emitGetById(value.get(), value.get(), propertyNames().value);

                    emitLabel(returnSequence.get());
                    bool hasFinally = emitReturnViaFinallyIfNeeded(value.get());
                    if (!hasFinally)
                        emitReturn(value.get());
                }

                // Normal.
                emitLabel(normalLabel.get());
                move(value.get(), generatorValueRegister());
            }

            emitLabel(nextElement.get());
            emitIteratorNextWithValue(value.get(), nextMethod.get(), iterator.get(), value.get(), node);

            emitLabel(branchOnResult.get());

            if (parseMode() == SourceParseMode::AsyncGeneratorBodyMode)
                emitAwait(value.get());

            Ref<Label> iteratorValueIsObject = newLabel();
            emitJumpIfTrue(emitIsObject(newTemporary(), value.get()), iteratorValueIsObject.get());
            emitThrowTypeError("Iterator result interface is not an object."_s);
            emitLabel(iteratorValueIsObject.get());

            emitJumpIfTrue(emitGetById(newTemporary(), value.get(), propertyNames().done), loopDone.get());
            emitGetById(value.get(), value.get(), propertyNames().value);

            emitJump(loopStart.get());
        }
        emitLabel(loopDone.get());
    }

    emitGetById(value.get(), value.get(), propertyNames().value);
    return value.get();
}


void BytecodeGenerator::emitGeneratorStateChange(int32_t state)
{
    RegisterID* completedState = emitLoad(nullptr, jsNumber(state));
    emitPutById(generatorRegister(), propertyNames().builtinNames().generatorStatePrivateName(), completedState);
}

bool BytecodeGenerator::emitJumpViaFinallyIfNeeded(int targetLabelScopeDepth, Label& jumpTarget)
{
    ASSERT(labelScopeDepth() - targetLabelScopeDepth >= 0);
    size_t numberOfScopesToCheckForFinally = labelScopeDepth() - targetLabelScopeDepth;
    ASSERT(numberOfScopesToCheckForFinally <= m_controlFlowScopeStack.size());
    if (!numberOfScopesToCheckForFinally)
        return false;

    FinallyContext* innermostFinallyContext = nullptr;
    FinallyContext* outermostFinallyContext = nullptr;
    size_t scopeIndex = m_controlFlowScopeStack.size() - 1;
    while (numberOfScopesToCheckForFinally--) {
        ControlFlowScope* scope = &m_controlFlowScopeStack[scopeIndex--];
        if (scope->isFinallyScope()) {
            FinallyContext* finallyContext = &scope->finallyContext;
            if (!innermostFinallyContext)
                innermostFinallyContext = finallyContext;
            outermostFinallyContext = finallyContext;
            finallyContext->incNumberOfBreaksOrContinues();
        }
    }
    if (!outermostFinallyContext)
        return false; // No finallys to thread through.

    auto jumpID = bytecodeOffsetToJumpID(instructions().size());
    int lexicalScopeIndex = labelScopeDepthToLexicalScopeIndex(targetLabelScopeDepth);
    outermostFinallyContext->registerJump(jumpID, lexicalScopeIndex, jumpTarget);

    emitSetCompletionType(jumpID);
    emitJump(*innermostFinallyContext->finallyLabel());
    return true; // We'll be jumping to a finally block.
}

bool BytecodeGenerator::emitReturnViaFinallyIfNeeded(RegisterID* returnRegister)
{
    size_t numberOfScopesToCheckForFinally = m_controlFlowScopeStack.size();
    if (!numberOfScopesToCheckForFinally)
        return false;

    FinallyContext* innermostFinallyContext = nullptr;
    while (numberOfScopesToCheckForFinally) {
        size_t scopeIndex = --numberOfScopesToCheckForFinally;
        ControlFlowScope* scope = &m_controlFlowScopeStack[scopeIndex];
        if (scope->isFinallyScope()) {
            FinallyContext* finallyContext = &scope->finallyContext;
            if (!innermostFinallyContext)
                innermostFinallyContext = finallyContext;
            finallyContext->setHandlesReturns();
        }
    }
    if (!innermostFinallyContext)
        return false; // No finallys to thread through.

    emitSetCompletionType(CompletionType::Return);
    emitSetCompletionValue(returnRegister);
    emitJump(*innermostFinallyContext->finallyLabel());
    return true; // We'll be jumping to a finally block.
}

void BytecodeGenerator::emitFinallyCompletion(FinallyContext& context, RegisterID* completionTypeRegister, Label& normalCompletionLabel)
{
    if (context.numberOfBreaksOrContinues() || context.handlesReturns()) {
        emitJumpIf<OpStricteq>(completionTypeRegister, CompletionType::Normal, normalCompletionLabel);

        FinallyContext* outerContext = context.outerContext();

        size_t numberOfJumps = context.numberOfJumps();
        ASSERT(outerContext || numberOfJumps == context.numberOfBreaksOrContinues());

        for (size_t i = 0; i < numberOfJumps; i++) {
            Ref<Label> nextLabel = newLabel();
            auto& jump = context.jumps(i);
            emitJumpIf<OpNstricteq>(completionTypeRegister, jump.jumpID, nextLabel.get());

            restoreScopeRegister(jump.targetLexicalScopeIndex);
            emitSetCompletionType(CompletionType::Normal);
            emitJump(jump.targetLabel.get());

            emitLabel(nextLabel.get());
        }

        if (outerContext) {
            // We are not the outermost finally.
            bool hasBreaksOrContinuesNotCoveredByJumps = context.numberOfBreaksOrContinues() > numberOfJumps;
            if (hasBreaksOrContinuesNotCoveredByJumps || context.handlesReturns())
                emitJumpIf<OpNstricteq>(completionTypeRegister, CompletionType::Throw, *outerContext->finallyLabel());

        } else {
            // We are the outermost finally.
            if (context.handlesReturns()) {
                Ref<Label> notReturnLabel = newLabel();
                emitJumpIf<OpNstricteq>(completionTypeRegister, CompletionType::Return, notReturnLabel.get());

                emitWillLeaveCallFrameDebugHook();
                emitReturn(completionValueRegister(), ReturnFrom::Finally);
                
                emitLabel(notReturnLabel.get());
            }
        }
    }
    emitJumpIf<OpNstricteq>(completionTypeRegister, CompletionType::Throw, normalCompletionLabel);
    emitThrow(completionValueRegister());
}

bool BytecodeGenerator::allocateCompletionRecordRegisters()
{
    if (m_completionTypeRegister)
        return false;

    ASSERT(!m_completionValueRegister);
    m_completionTypeRegister = newTemporary();
    m_completionValueRegister = newTemporary();

    emitSetCompletionType(CompletionType::Normal);
    moveEmptyValue(m_completionValueRegister.get());
    return true;
}

void BytecodeGenerator::releaseCompletionRecordRegisters()
{
    ASSERT(m_completionTypeRegister && m_completionValueRegister);
    m_completionTypeRegister = nullptr;
    m_completionValueRegister = nullptr;
}

template<typename CompareOp>
void BytecodeGenerator::emitJumpIf(RegisterID* completionTypeRegister, CompletionType type, Label& jumpTarget)
{
    RefPtr<RegisterID> tempRegister = newTemporary();
    RegisterID* valueConstant = addConstantValue(jsNumber(static_cast<int>(type)));
    OperandTypes operandTypes = OperandTypes(ResultType::numberTypeIsInt32(), ResultType::unknownType());

    auto equivalenceResult = emitBinaryOp<CompareOp>(tempRegister.get(), valueConstant, completionTypeRegister, operandTypes);
    emitJumpIfTrue(equivalenceResult, jumpTarget);
}

void ForInContext::finalize(BytecodeGenerator& generator, UnlinkedCodeBlock* codeBlock, unsigned bodyBytecodeEndOffset)
{
    // Lexically invalidating ForInContexts is kind of weak sauce, but it only occurs if
    // either of the following conditions is true:
    //
    // (1) The loop iteration variable is re-assigned within the body of the loop.
    // (2) The loop iteration variable is captured in the lexical scope of the function.
    //
    // These two situations occur sufficiently rarely that it's okay to use this style of
    // "analysis" to make iteration faster. If we didn't want to do this, we would either have
    // to perform some flow-sensitive analysis to see if/when the loop iteration variable was
    // reassigned, or we'd have to resort to runtime checks to see if the variable had been
    // reassigned from its original value.

    for (unsigned offset = bodyBytecodeStartOffset(); isValid() && offset < bodyBytecodeEndOffset;) {
        auto instruction = generator.instructions().at(offset);
        OpcodeID opcodeID = instruction->opcodeID();

        ASSERT(opcodeID != op_enter);
        computeDefsForBytecodeOffset(codeBlock, opcodeID, instruction.ptr(), [&] (VirtualRegister operand) {
            if (local()->virtualRegister() == operand)
                invalidate();
        });
        offset += instruction->size();
    }
}

void StructureForInContext::finalize(BytecodeGenerator& generator, UnlinkedCodeBlock* codeBlock, unsigned bodyBytecodeEndOffset)
{
    Base::finalize(generator, codeBlock, bodyBytecodeEndOffset);
    if (isValid())
        return;

    OpcodeID lastOpcodeID = generator.m_lastOpcodeID;
    InstructionStream::MutableRef lastInstruction = generator.m_lastInstruction;
    for (const auto& instTuple : m_getInsts) {
        unsigned instIndex = std::get<0>(instTuple);
        int propertyRegIndex = std::get<1>(instTuple);
        auto instruction = generator.m_writer.ref(instIndex);
        auto end = instIndex + instruction->size();
        ASSERT(instruction->isWide());

        generator.m_writer.seek(instIndex);

        auto bytecode = instruction->as<OpGetDirectPname>();

        // disable peephole optimizations
        generator.m_lastOpcodeID = op_end;

        // Change the opcode to get_by_val.
        // 1. dst stays the same.
        // 2. base stays the same.
        // 3. property gets switched to the original property.
        OpGetByVal::emit<OpcodeSize::Wide>(&generator, bytecode.dst, bytecode.base, VirtualRegister(propertyRegIndex));

        // 4. nop out the remaining bytes
        while (generator.m_writer.position() < end)
            OpNop::emit<OpcodeSize::Narrow>(&generator);
    }
    generator.m_writer.seek(generator.m_writer.size());
    if (generator.m_lastInstruction.offset() + generator.m_lastInstruction->size() != generator.m_writer.size()) {
        generator.m_lastOpcodeID = lastOpcodeID;
        generator.m_lastInstruction = lastInstruction;
    }
}

void IndexedForInContext::finalize(BytecodeGenerator& generator, UnlinkedCodeBlock* codeBlock, unsigned bodyBytecodeEndOffset)
{
    Base::finalize(generator, codeBlock, bodyBytecodeEndOffset);
    if (isValid())
        return;

    for (const auto& instPair : m_getInsts) {
        unsigned instIndex = instPair.first;
        int propertyRegIndex = instPair.second;
        // FIXME: we should not have to force this get_by_val to be wide, just guarantee that propertyRegIndex fits
        // https://bugs.webkit.org/show_bug.cgi?id=190929
        generator.m_writer.ref(instIndex)->cast<OpGetByVal>()->setProperty(VirtualRegister(propertyRegIndex), []() {
            ASSERT_NOT_REACHED();
            return VirtualRegister();
        });
    }
}

void StaticPropertyAnalysis::record()
{
    auto* instruction = m_instructionRef.ptr();
    auto size = m_propertyIndexes.size();
    switch (instruction->opcodeID()) {
    case OpNewObject::opcodeID:
        instruction->cast<OpNewObject>()->setInlineCapacity(size, []() {
            return 255;
        });
        return;
    case OpCreateThis::opcodeID:
        instruction->cast<OpCreateThis>()->setInlineCapacity(size, []() {
            return 255;
        });
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

void BytecodeGenerator::emitToThis()
{
    OpToThis::emit(this, kill(&m_thisRegister));
    m_codeBlock->addPropertyAccessInstruction(m_lastInstruction.offset());
}

} // namespace JSC

namespace WTF {

void printInternal(PrintStream& out, JSC::Variable::VariableKind kind)
{
    switch (kind) {
    case JSC::Variable::NormalVariable:
        out.print("Normal");
        return;
    case JSC::Variable::SpecialVariable:
        out.print("Special");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

