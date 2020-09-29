// This file was procedurally generated from the following sources:
// - src/function-forms/forbidden-ext-direct-access-prop-arguments.case
// - src/function-forms/forbidden-extensions/bullet-one/arrow-function.template
/*---
description: Forbidden extension, o.arguments (arrow function expression)
esid: sec-arrow-function-definitions-runtime-semantics-evaluation
flags: [generated, noStrict]
info: |
    ArrowFunction : ArrowParameters => ConciseBody

    ECMAScript function objects defined using syntactic constructors in strict mode code must not be created with own properties named "caller" or "arguments". Such own properties also must not be created for function objects defined using an ArrowFunction, MethodDefinition, GeneratorDeclaration, GeneratorExpression, AsyncGeneratorDeclaration, AsyncGeneratorExpression, ClassDeclaration, ClassExpression, AsyncFunctionDeclaration, AsyncFunctionExpression, or AsyncArrowFunction regardless of whether the definition is contained in strict mode code. Built-in functions, strict functions created using the Function constructor, generator functions created using the Generator constructor, async functions created using the AsyncFunction constructor, and functions created using the bind method also must not be created with such own properties.

---*/

var callCount = 0;
var f;
f = () => {
  f.arguments;
  callCount++;
};

assert.throws(TypeError, function() {
  f();
});
assert.sameValue(callCount, 0, 'arrow function body not evaluated');
