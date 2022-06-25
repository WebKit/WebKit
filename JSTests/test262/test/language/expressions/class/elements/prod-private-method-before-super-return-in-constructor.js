// This file was procedurally generated from the following sources:
// - src/class-elements/prod-private-method-before-super-return-in-constructor.case
// - src/class-elements/default/cls-expr.template
/*---
description: Private methods are installed "when super returns" and no earlier (call in constructor) (field definitions in a class expression)
esid: prod-FieldDefinition
features: [class-methods-private, class]
flags: [generated]
info: |
    SuperCall: super Arguments
      1. Let newTarget be GetNewTarget().
      2. If newTarget is undefined, throw a ReferenceError exception.
      3. Let func be ? GetSuperConstructor().
      4. Let argList be ArgumentListEvaluation of Arguments.
      5. ReturnIfAbrupt(argList).
      6. Let result be ? Construct(func, argList, newTarget).
      7. Let thisER be GetThisEnvironment( ).
      8. Let F be thisER.[[FunctionObject]].
      9. Assert: F is an ECMAScript function object.
      10. Perform ? InitializeInstanceElements(result, F).

    EDITOR'S NOTE:
      Private fields are added to the object one by one, interspersed with
      evaluation of the initializers, following the construction of the
      receiver. These semantics allow for a later initializer to refer to
      a previous private field.

---*/


var C = class {
  constructor() {
      this.f();
  }

}

class D extends C {
    f() { this.#m(); }
    #m() { return 42; }
}

assert(D.prototype.hasOwnProperty('f'));
assert.throws(TypeError, function() {
    var d = new D();
}, 'private methods are not installed before super returns');
