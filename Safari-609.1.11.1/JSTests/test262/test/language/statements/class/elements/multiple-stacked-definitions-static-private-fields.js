// This file was procedurally generated from the following sources:
// - src/class-elements/static-private-fields.case
// - src/class-elements/productions/cls-decl-multiple-stacked-definitions.template
/*---
description: static private fields (multiple stacked fields definitions through ASI)
esid: prod-FieldDefinition
features: [class-static-fields-private, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
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


class C {
  static #x; static #y
  foo = "foobar"
  bar = "barbaz";
  static x() {
    this.#x = 42;
    return this.#x;
  }
  static y() {
    this.#y = 43;
    return this.#y;
  }
}

var c = new C();

assert.sameValue(c.foo, "foobar");
assert.sameValue(Object.hasOwnProperty.call(C, "foo"), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "foo"), false);

verifyProperty(c, "foo", {
  value: "foobar",
  enumerable: true,
  configurable: true,
  writable: true,
});

assert.sameValue(c.bar, "barbaz");
assert.sameValue(Object.hasOwnProperty.call(C, "bar"), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "bar"), false);

verifyProperty(c, "bar", {
  value: "barbaz",
  enumerable: true,
  configurable: true,
  writable: true,
});

// Test the private fields do not appear as properties before set to value
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#x"), false, "test 1");
assert.sameValue(Object.hasOwnProperty.call(C, "#x"), false, "test 2");
assert.sameValue(Object.hasOwnProperty.call(c, "#x"), false, "test 3");

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#y"), false, "test 4");
assert.sameValue(Object.hasOwnProperty.call(C, "#y"), false, "test 5");
assert.sameValue(Object.hasOwnProperty.call(c, "#y"), false, "test 6");

// Test if private fields can be sucessfully accessed and set to value
assert.sameValue(C.x(), 42, "test 7");
assert.sameValue(C.y(), 43, "test 8");

// Test the private fields do not appear as properties before after set to value
assert.sameValue(Object.hasOwnProperty.call(C, "#x"), false, "test 9");
assert.sameValue(Object.hasOwnProperty.call(C, "#y"), false, "test 10");
