// This file was procedurally generated from the following sources:
// - src/class-elements/grammar-static-ctor-async-gen-meth-valid.case
// - src/class-elements/syntax/valid/cls-expr-elements-valid-syntax.template
/*---
description: Static Async Generator Methods can be named constructor (class expression)
esid: prod-ClassElement
features: [async-iteration, class]
flags: [generated]
info: |
    Class Definitions / Static Semantics: Early Errors

    ClassElement : MethodDefinition
        It is a Syntax Error if PropName of MethodDefinition is not "constructor" and HasDirectSuper of MethodDefinition is true.
        It is a Syntax Error if PropName of MethodDefinition is "constructor" and SpecialMethod of MethodDefinition is true.
    ClassElement : static MethodDefinition
        It is a Syntax Error if HasDirectSuper of MethodDefinition is true.
        It is a Syntax Error if PropName of MethodDefinition is "prototype".

---*/


var C = class {
  static async * constructor() {}
  constructor() {} // stacks with a valid constructor
};

assert(C.hasOwnProperty('constructor'));
assert(C.prototype.hasOwnProperty('constructor'));
assert.notSameValue(C.prototype.constructor, C.constructor);
