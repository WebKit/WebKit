// This file was procedurally generated from the following sources:
// - src/class-elements/private-static-generator-method-name.case
// - src/class-elements/default/cls-decl.template
/*---
description: Private static generator methods have name property properly configured (field definitions in a class declaration)
esid: prod-FieldDefinition
features: [class-static-methods-private, class]
flags: [generated]
info: |
    Updated Productions

    ClassElement : MethodDefinition
      1. Return ClassElementEvaluation of MethodDefinition with arguments ! Get(homeObject, "prototype"),enumerable, and "prototype".

    ClassElement : static MethodDefinition
      1. Return ClassElementEvaluation of MethodDefinition with arguments homeObject, enumerable and "static".

    GeneratorMethod : * ClassElementName (UniqueFormalParameters) { GeneratorBody }
      ...
      12. Perform ? DefineOrdinaryMethod(key, homeObject, closure, _enumerable).

    ClassElement : MethodDefinition
    ClassElement : static MethodDefinition
      1. Perform ? PropertyDefinitionEvaluation with parameters object and enumerable.
      2. Return empty.

    ClassElementName : PrivateIdentifier
      1. Let bindingName be StringValue of PrivateIdentifier.
      ...
      5. If scopeEnvRec's binding for bindingName is uninitialized,
        a. Let field be NewPrivateName(bindingName).
        b. Perform ! scopeEnvRec.InitializeBinding(bindingName, field).
      6. Otherwise,
        a. Let field be scopeEnvRec.GetBindingValue(bindingName).
      7. Assert: field.[[Description]] is bindingName.
      8. Return field.

    DefineOrdinaryMethod(key, homeObject, closure, enumerable)
      1. Perform SetFunctionName(closure, key).
      2. If key is a Private Name,
        a. Assert: key does not have a [[Kind]] field.
        b. Set key.[[Kind]] to "method".
        c. Set key.[[Value]] to closure.
        d. Set key.[[Brand]] to homeObject.
      3. Else,
        a. Let desc be the PropertyDescriptor{[[Value]]: closure, [[Writable]]: true, [[Enumerable]]: enumerable, [[Configurable]]: true}.
        b. Perform ? DefinePropertyOrThrow(homeObject, key, desc).

---*/


class C {
  static * #method() {};

  static getPrivateMethod() {
    return this.#method;
  }
}

assert.sameValue(C.getPrivateMethod().name, "#method");
