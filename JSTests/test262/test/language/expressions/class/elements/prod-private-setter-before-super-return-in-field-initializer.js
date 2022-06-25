// This file was procedurally generated from the following sources:
// - src/class-elements/prod-private-setter-before-super-return-in-field-initializer.case
// - src/class-elements/default/cls-expr.template
/*---
description: Private settters are installed "when super returns" and no earlier (call in field initializer) (field definitions in a class expression)
esid: prod-FieldDefinition
features: [class-methods-private, class-fields-public, class]
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
  f = this.g();

}

class D extends C {
    g() { this.#m = 42; }
    set #m(val) {}
}

assert(D.prototype.hasOwnProperty('g'));
assert.throws(TypeError, function() {
    var d = new D();
}, 'private setters are not installed before super returns');
