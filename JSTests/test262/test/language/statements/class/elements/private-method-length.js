// This file was procedurally generated from the following sources:
// - src/class-elements/private-method-length.case
// - src/class-elements/default/cls-decl.template
/*---
description: Private methods have length property properly configured (field definitions in a class declaration)
esid: prod-FieldDefinition
features: [class-methods-private, class]
flags: [generated]
info: |
    Updated Productions

    ClassElement : MethodDefinition
      1. Return ClassElementEvaluation of MethodDefinition with arguments ! Get(homeObject, "prototype"),enumerable, and "prototype".

    ClassElement : static MethodDefinition
      1. Return ClassElementEvaluation of MethodDefinition with arguments homeObject, enumerable and "static".

    MethodDefinition : ClassElementName( UniqueFormalParameters ) { FunctionBody }
      1. Let methodDef be DefineMethod of MethodDefinition with argument homeObject.
      2. ReturnIfAbrupt(methodDef).
      3. Perform ? DefineOrdinaryMethod(methodDef.[[Key]], homeObject, methodDef.[[Closure]], _enumerable).

    ClassElement : MethodDefinition
    ClassElement : static MethodDefinition
      1. Perform ? PropertyDefinitionEvaluation with parameters object and enumerable.
      2. Return empty.

    MethodDefinition : ClassElementName (UniqueFormalParameters) { FunctionBody }
      ...
      8. Let closure be FunctionCreate(kind, UniqueFormalParameters, FunctionBody, scope, privateScope, strict, prototype).
      9. Perform MakeMethod(closure, object).
      10. Return the Record{[[Key]]: propKey, [[Closure]]: closure}.

---*/


class C {
  #method(a) {};

  getPrivateMethod() {
    return this.#method;
  }

}

let c = new C();
assert.sameValue(c.getPrivateMethod().length, 1);
