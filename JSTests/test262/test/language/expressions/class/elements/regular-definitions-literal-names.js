// This file was procedurally generated from the following sources:
// - src/class-elements/literal-names.case
// - src/class-elements/productions/cls-expr-regular-definitions.template
/*---
description: Literal property names (regular fields defintion)
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
const fn = function() {}



var C = class {
  a; b = 42;
  c = fn
  
}

var c = new C();

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

assert(
  !Object.prototype.hasOwnProperty.call(C.prototype, "c"),
  "c doesn't appear as an own property on C prototype"
);
assert(
  !Object.prototype.hasOwnProperty.call(C, "c"),
  "c doesn't appear as an own property on C constructor"
);

verifyProperty(c, "c", {
  value: fn,
  enumerable: true,
  writable: true,
  configurable: true
});

