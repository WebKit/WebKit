// This file was procedurally generated from the following sources:
// - src/dstr-binding/ary-init-iter-get-err.case
// - src/dstr-binding/error/cls-expr-async-gen-meth-dflt.template
/*---
description: Abrupt completion returned by GetIterator (class expression async generator method (default parameter))
esid: sec-class-definitions-runtime-semantics-evaluation
features: [Symbol.iterator, async-iteration]
flags: [generated]
info: |
    ClassExpression : class BindingIdentifieropt ClassTail

    1. If BindingIdentifieropt is not present, let className be undefined.
    2. Else, let className be StringValue of BindingIdentifier.
    3. Let value be the result of ClassDefinitionEvaluation of ClassTail
       with argument className.
    [...]

    14.5.14 Runtime Semantics: ClassDefinitionEvaluation

    21. For each ClassElement m in order from methods
        a. If IsStatic of m is false, then
           i. Let status be the result of performing
              PropertyDefinitionEvaluation for m with arguments proto and
              false.
        [...]

    Runtime Semantics: PropertyDefinitionEvaluation

    AsyncGeneratorMethod :
        async [no LineTerminator here] * PropertyName ( UniqueFormalParameters )
            { AsyncGeneratorBody }

    1. Let propKey be the result of evaluating PropertyName.
    2. ReturnIfAbrupt(propKey).
    3. If the function code for this AsyncGeneratorMethod is strict mode code, let strict be true.
       Otherwise let strict be false.
    4. Let scope be the running execution context's LexicalEnvironment.
    5. Let closure be ! AsyncGeneratorFunctionCreate(Method, UniqueFormalParameters,
       AsyncGeneratorBody, scope, strict).
    [...]

    13.3.3.5 Runtime Semantics: BindingInitialization

    BindingPattern : ArrayBindingPattern

    1. Let iterator be GetIterator(value).
    2. ReturnIfAbrupt(iterator).

---*/
var iter = {};
iter[Symbol.iterator] = function() {
  throw new Test262Error();
};


var C = class {
  async *method([x] = iter) {
    
  }
};

var method = C.prototype.method;

assert.throws(Test262Error, function() {
  method();
});
