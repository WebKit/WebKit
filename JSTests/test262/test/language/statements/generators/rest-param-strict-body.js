// This file was procedurally generated from the following sources:
// - src/function-forms/rest-param-strict-body.case
// - src/function-forms/syntax/gen-func-decl.template
/*---
description: RestParameter and Use Strict Directive are not allowed to coexist for the same function. (generator function declaration)
esid: sec-generator-function-definitions-runtime-semantics-instantiatefunctionobject
features: [rest-parameters, generators]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    GeneratorDeclaration : function * ( FormalParameters ) { GeneratorBody }

        [...]
        2. Let F be GeneratorFunctionCreate(Normal, FormalParameters,
           GeneratorBody, scope, strict).
        [...]

    9.2.1 [[Call]] ( thisArgument, argumentsList)

    [...]
    7. Let result be OrdinaryCallEvaluateBody(F, argumentsList).
    [...]

    9.2.1.3 OrdinaryCallEvaluateBody ( F, argumentsList )

    1. Let status be FunctionDeclarationInstantiation(F, argumentsList).
    [...]

    9.2.12 FunctionDeclarationInstantiation(func, argumentsList)

    [...]
    23. Let iteratorRecord be Record {[[iterator]]:
        CreateListIterator(argumentsList), [[done]]: false}.
    24. If hasDuplicates is true, then
        [...]
    25. Else,
        b. Let formalStatus be IteratorBindingInitialization for formals with
           iteratorRecord and env as arguments.
    [...]


    14.1.13 Static Semantics: IsSimpleParameterList

    FormalParameters : FormalParameterList , FunctionRestParameter

    1. Return false.

    14.1.2 Static Semantics: Early Errors

    FunctionDeclaration : function BindingIdentifier ( FormalParameters ) { FunctionBody }
    FunctionDeclaration : function ( FormalParameters ) { FunctionBody }
    FunctionExpression : function BindingIdentifier ( FormalParameters ) { FunctionBody }

    - It is a Syntax Error if ContainsUseStrict of FunctionBody is true and
      IsSimpleParameterList of FormalParameters is false.

    14.2.1 Static Semantics: Early Errors

    ArrowFunction : ArrowParameters => ConciseBody

    - It is a Syntax Error if ContainsUseStrict of ConciseBody is true and
      IsSimpleParameterList of ArrowParameters is false.

    14.3.1 Static Semantics: Early Errors

    MethodDefinition : PropertyName ( UniqueFormalParameters ) { FunctionBody }

    - It is a Syntax Error if ContainsUseStrict of FunctionBody is true and
      IsSimpleParameterList of UniqueFormalParameters is false.

    MethodDefinition : set PropertyName ( PropertySetParameterList ) { FunctionBody }

    - It is a Syntax Error if ContainsUseStrict of FunctionBody is true and
      IsSimpleParameterList of PropertySetParameterList is false.

    14.4.1 Static Semantics: Early Errors

    GeneratorMethod : * PropertyName ( UniqueFormalParameters ) { GeneratorBody }

    - It is a Syntax Error if ContainsUseStrict of GeneratorBody is true and
      IsSimpleParameterList of UniqueFormalParameters is false.

    GeneratorDeclaration : function * BindingIdentifier ( FormalParameters ) { GeneratorBody }
    GeneratorDeclaration : function * ( FormalParameters ) { GeneratorBody }
    GeneratorExpression : function * BindingIdentifier ( FormalParameters ) { GeneratorBody }

    - It is a Syntax Error if ContainsUseStrict of GeneratorBody is true and
      IsSimpleParameterList of UniqueFormalParameters is false.

    14.5.1 Static Semantics: Early Errors

    AsyncGeneratorMethod : async * PropertyName ( UniqueFormalParameters ) { AsyncGeneratorBody }

    - It is a Syntax Error if ContainsUseStrict of AsyncGeneratorBody is true and
      IsSimpleParameterList of UniqueFormalParameters is false.

    AsyncGeneratorDeclaration : async function * BindingIdentifier ( FormalParameters ) { AsyncGeneratorBody }
    AsyncGeneratorDeclaration : async function * ( FormalParameters ) { AsyncGeneratorBody }
    AsyncGeneratorExpression : async function * BindingIdentifier ( FormalParameters ) { AsyncGeneratorBody }

    - It is a Syntax Error if ContainsUseStrict of AsyncGeneratorBody is true and
      IsSimpleParameterList of FormalParameters is false.

    14.7.1 Static Semantics: Early Errors

    AsyncMethod : async PropertyName ( UniqueFormalParameters ) { AsyncFunctionBody }

    - It is a Syntax Error if ContainsUseStrict of AsyncFunctionBody is true and
      IsSimpleParameterList of UniqueFormalParameters is false.

    AsyncFunctionDeclaration : async function BindingIdentifier ( FormalParameters ) { AsyncFunctionBody }
    AsyncFunctionDeclaration : async function ( FormalParameters ) { AsyncFunctionBody }
    AsyncFunctionExpression : async function ( FormalParameters ) { AsyncFunctionBody }
    AsyncFunctionExpression : async function BindingIdentifier ( FormalParameters ) { AsyncFunctionBody }

    - It is a Syntax Error if ContainsUseStrict of AsyncFunctionBody is true and
      IsSimpleParameterList of FormalParameters is false.

    14.8.1 Static Semantics: Early Errors

    AsyncArrowFunction : CoverCallExpressionAndAsyncArrowHead => AsyncConciseBody

    - It is a Syntax Error if ContainsUseStrict of AsyncConciseBody is true and
      IsSimpleParameterList of CoverCallExpressionAndAsyncArrowHead is false.

---*/
$DONOTEVALUATE();

function* f(a,...rest) {
  "use strict";
}
