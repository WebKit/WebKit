// This file was procedurally generated from the following sources:
// - src/class-elements/literal-names-asi.case
// - src/class-elements/productions/cls-expr-after-same-line-static-gen.template
/*---
description: Literal property names with ASI (field definitions after a static generator in the same line)
esid: prod-FieldDefinition
features: [class-fields-public, generators, class]
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


var C = class {
  static *m() { return 42; } a
  b = 42;;
  
}

var c = new C();

assert.sameValue(C.m().next().value, 42);
assert(
  !Object.prototype.hasOwnProperty.call(c, "m"),
  "m doesn't appear as an own property on the C instance"
);
assert(
  !Object.prototype.hasOwnProperty.call(C.prototype, "m"),
  "m doesn't appear as an own property on the C prototype"
);

verifyProperty(C, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
});

assert(
  !Object.prototype.hasOwnProperty.call(C.prototype, "a"),
  "a doesn't appear as an own property on C prototype"
);
assert(
  !Object.prototype.hasOwnProperty.call(C, "a"),
  "a doesn't appear as an own property on C constructor"
);

verifyProperty(c, "a", {
  value: undefined,
  enumerable: true,
  writable: true,
  configurable: true
});

assert(
  !Object.prototype.hasOwnProperty.call(C.prototype, "b"),
  "b doesn't appear as an own property on C prototype"
);
assert(
  !Object.prototype.hasOwnProperty.call(C, "b"),
  "b doesn't appear as an own property on C constructor"
);

verifyProperty(c, "b", {
  value: 42,
  enumerable: true,
  writable: true,
  configurable: true
});
