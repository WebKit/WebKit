// This file was procedurally generated from the following sources:
// - src/class-fields/computed-symbol-names.case
// - src/class-fields/productions/cls-expr-regular-definitions.template
/*---
description: Computed property symbol names (regular fields defintion)
esid: prod-FieldDefinition
features: [Symbol, computed-property-names, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
info: |
    ClassElement:
      ...
      FieldDefinition ;

    FieldDefinition:
      ClassElementName Initializer_opt

    ClassElementName:
      PropertyName

---*/
var x = Symbol();
var y = Symbol();



var C = class {
  [x]; [y] = 42

}

var c = new C();

assert.sameValue(Object.hasOwnProperty.call(C.prototype, x), false);
assert.sameValue(Object.hasOwnProperty.call(C, x), false);

verifyProperty(c, x, {
  value: undefined,
  enumerable: true,
  writable: true,
  configurable: true
});

assert.sameValue(Object.hasOwnProperty.call(C.prototype, y), false);
assert.sameValue(Object.hasOwnProperty.call(C, y), false);

verifyProperty(c, y, {
  value: 42,
  enumerable: true,
  writable: true,
  configurable: true
});

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "x"), false);
assert.sameValue(Object.hasOwnProperty.call(C, "x"), false);
assert.sameValue(Object.hasOwnProperty.call(c, "x"), false);

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "y"), false);
assert.sameValue(Object.hasOwnProperty.call(C, "y"), false);
assert.sameValue(Object.hasOwnProperty.call(c, "y"), false);
