// This file was procedurally generated from the following sources:
// - src/function-forms/forbidden-ext-direct-access-prop-arguments.case
// - src/function-forms/forbidden-extensions/bullet-one/cls-expr-async-meth-static.template
/*---
description: Forbidden extension, f.arguments (static class expression async method)
esid: sec-class-definitions-runtime-semantics-evaluation
features: [arrow-function, async-functions, class]
flags: [generated, noStrict, async]
info: |
    ClassExpression : class BindingIdentifieropt ClassTail


    ECMAScript function objects defined using syntactic constructors in strict mode code must
    not be created with own properties named "caller" or "arguments". Such own properties also
    must not be created for function objects defined using an ArrowFunction, MethodDefinition,
    GeneratorDeclaration, GeneratorExpression, AsyncGeneratorDeclaration, AsyncGeneratorExpression,
    ClassDeclaration, ClassExpression, AsyncFunctionDeclaration, AsyncFunctionExpression, or
    AsyncArrowFunction regardless of whether the definition is contained in strict mode code.
    Built-in functions, strict functions created using the Function constructor, generator functions
    created using the Generator constructor, async functions created using the AsyncFunction
    constructor, and functions created using the bind method also must not be created with such own
    properties.

---*/


var callCount = 0;

var C = class {
  static async method() {
    assert.sameValue(this.method.hasOwnProperty("arguments"), false);
    callCount++;
  }
};

C.method()
  .then(() => {
    assert.sameValue(callCount, 1, 'function body evaluated');
  }, $DONE).then($DONE, $DONE);
