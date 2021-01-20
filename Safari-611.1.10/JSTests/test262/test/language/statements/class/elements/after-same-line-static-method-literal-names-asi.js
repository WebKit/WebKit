// This file was procedurally generated from the following sources:
// - src/class-elements/literal-names-asi.case
// - src/class-elements/productions/cls-decl-after-same-line-static-method.template
/*---
description: Literal property names with ASI (field definitions after a static method in the same line)
esid: prod-FieldDefinition
features: [class-fields-public, class]
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


class C {
  static m() { return 42; } a
  b = 42;;
  
}

var c = new C();

assert.sameValue(C.m(), 42);
assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "m"), false);

verifyProperty(C, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
});

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "a"), false);
assert.sameValue(Object.hasOwnProperty.call(C, "a"), false);

verifyProperty(c, "a", {
  value: undefined,
  enumerable: true,
  writable: true,
  configurable: true
});

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "b"), false);
assert.sameValue(Object.hasOwnProperty.call(C, "b"), false);

verifyProperty(c, "b", {
  value: 42,
  enumerable: true,
  writable: true,
  configurable: true
});
