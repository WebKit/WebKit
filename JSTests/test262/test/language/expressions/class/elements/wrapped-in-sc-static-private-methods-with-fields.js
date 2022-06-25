// This file was procedurally generated from the following sources:
// - src/class-elements/static-private-methods-with-fields.case
// - src/class-elements/productions/cls-expr-wrapped-in-sc.template
/*---
description: static private methods with fields (fields definition wrapped in semicolons)
esid: prod-FieldDefinition
features: [class-static-methods-private, class-static-fields-private, class, class-fields-public]
flags: [generated]
info: |
    ClassElement :
      ...
      static FieldDefinition ;

    FieldDefinition :
      ClassElementName Initializer_opt

    ClassElementName :
      PrivateName

    PrivateName :
      # IdentifierName

---*/


var C = class {
  ;;;;
  ;;;;;;static #xVal; static #yVal;;;;;;;
  ;;;;
  static #x(value) {
    this.#xVal = value;
    return this.#xVal;
  }
  static #y(value) {
    this.#yVal = value;
    return this.#yVal;
  }
  static x() {
    return this.#x(42);
  }
  static y() {
    return this.#y(43);
  }
}

var c = new C();

// Test the private methods do not appear as properties before set to value
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#x"), false, "test 1");
assert.sameValue(Object.hasOwnProperty.call(C, "#x"), false, "test 2");
assert.sameValue(Object.hasOwnProperty.call(c, "#x"), false, "test 3");

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#y"), false, "test 4");
assert.sameValue(Object.hasOwnProperty.call(C, "#y"), false, "test 5");
assert.sameValue(Object.hasOwnProperty.call(c, "#y"), false, "test 6");

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#xVal"), false, "test 7");
assert.sameValue(Object.hasOwnProperty.call(C, "#xVal"), false, "test 8");
assert.sameValue(Object.hasOwnProperty.call(c, "#xVal"), false, "test 9");

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#yVal"), false, "test 10");
assert.sameValue(Object.hasOwnProperty.call(C, "#yVal"), false, "test 11");
assert.sameValue(Object.hasOwnProperty.call(c, "#yVal"), false, "test 12");

// Test if private fields can be sucessfully accessed and set to value
assert.sameValue(C.x(), 42, "test 13");
assert.sameValue(C.y(), 43, "test 14");

// Test the private fields do not appear as properties before after set to value
assert.sameValue(Object.hasOwnProperty.call(C, "#x"), false, "test 15");
assert.sameValue(Object.hasOwnProperty.call(C, "#y"), false, "test 16");

assert.sameValue(Object.hasOwnProperty.call(C, "#xVal"), false, "test 17");
assert.sameValue(Object.hasOwnProperty.call(C, "#yVal"), false, "test 18");
