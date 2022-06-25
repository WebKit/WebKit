// This file was procedurally generated from the following sources:
// - src/class-elements/grammar-static-private-async-gen-meth-prototype.case
// - src/class-elements/syntax/valid/cls-decl-elements-valid-syntax.template
/*---
description: Static Async Generator Private Methods can be named "#prototype" (class declaration)
esid: prod-ClassElement
features: [async-iteration, class-static-methods-private, class]
flags: [generated]
info: |
    Class Definitions / Static Semantics: Early Errors

    ClassElement : static MethodDefinition
        It is a Syntax Error if PropName of MethodDefinition is "prototype"

---*/


class C {
  static async * #prototype() {}
}
