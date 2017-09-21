// This file was procedurally generated from the following sources:
// - src/class-fields/static-literal-names.case
// - src/class-fields/default/cls-decl-wrapped-in-sc.template
/*---
description: Static literal property names (fields definition wrapped in semicolons)
features: [class-fields]
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
const fn = function() {}



class C {
  ;;;;
  ;;;;;;static a; b = 42;
  static c = fn;;;;;;;
  ;;;;
}

var c = new C();

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "a"), false);
assert.sameValue(Object.hasOwnProperty.call(c, "a"), false);

verifyProperty(C, "a", {
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

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "c"), false);
assert.sameValue(Object.hasOwnProperty.call(c, "c"), false);

verifyProperty(C, "c", {
  value: fn,
  enumerable: true,
  writable: true,
  configurable: true
});

