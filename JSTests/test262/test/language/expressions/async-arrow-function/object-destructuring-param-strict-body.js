// This file was procedurally generated from the following sources:
// - src/function-forms/object-destructuring-param-strict-body.case
// - src/function-forms/syntax/async-arrow-function.template
/*---
description: ObjectBindingPattern and Use Strict Directive are not allowed to coexist for the same function. (async arrow function expression)
esid: sec-async-arrow-function-definitions
features: [rest-parameters]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    14.7 Async Arrow Function Definitions

    AsyncArrowFunction :
      ...
      CoverCallExpressionAndAsyncArrowHead => AsyncConciseBody

    AsyncConciseBody :
      { AsyncFunctionBody }

    ...

    Supplemental Syntax

    When processing an instance of the production AsyncArrowFunction :
    CoverCallExpressionAndAsyncArrowHead => AsyncConciseBody the interpretation of
    CoverCallExpressionAndAsyncArrowHead is refined using the following grammar:

    AsyncArrowHead :
      async ArrowFormalParameters

    13.3.3.4 Static Semantics: IsSimpleParameterList

    BindingElement : BindingPattern

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


(async ({property}) => {
  "use strict";
});
