// This file was procedurally generated from the following sources:
// - src/class-elements/rs-field-identifier-initializer.case
// - src/class-elements/productions/cls-expr-multiple-definitions.template
/*---
description: Valid FieldDefinition (multiple fields definitions)
esid: prod-FieldDefinition
features: [class-fields-public, class]
flags: [generated]
includes: [propertyHelper.js]
info: |
    ClassElement :
      ...
      FieldDefinition ;
      ;

    FieldDefinition :
      ClassElementName Initializer _opt

    ClassElementName :
      PropertyName

    PropertyName :
      LiteralPropertyName
      ComputedPropertyName

    LiteralPropertyName :
      IdentifierName

    IdentifierName ::
      IdentifierStart
      IdentifierName IdentifierPart

    IdentifierStart ::
      UnicodeIDStart
      $
      _
      \ UnicodeEscapeSequence

    IdentifierPart::
      UnicodeIDContinue
      $
      \ UnicodeEscapeSequence
      <ZWNJ> <ZWJ>

    UnicodeIDStart::
      any Unicode code point with the Unicode property "ID_Start"

    UnicodeIDContinue::
      any Unicode code point with the Unicode property "ID_Continue"


    NOTE 3
    The sets of code points with Unicode properties "ID_Start" and
    "ID_Continue" include, respectively, the code points with Unicode
    properties "Other_ID_Start" and "Other_ID_Continue".

---*/


var C = class {
  foo = "foobar";
  m() { return 42 }
  $ = 1; _ = 1; \u{6F} = 1; \u2118 = 1; ZW_\u200C_NJ = 1; ZW_\u200D_J = 1
  m2() { return 39 }
  bar = "barbaz";
  
}

var c = new C();

assert.sameValue(c.m(), 42);
assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);
assert.sameValue(c.m, C.prototype.m);

verifyProperty(C.prototype, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
});

assert.sameValue(c.m2(), 39);
assert.sameValue(Object.hasOwnProperty.call(c, "m2"), false);
assert.sameValue(c.m2, C.prototype.m2);

verifyProperty(C.prototype, "m2", {
  enumerable: false,
  configurable: true,
  writable: true,
});

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

assert.sameValue(c.$, 1);
assert.sameValue(c._, 1);
assert.sameValue(c.\u{6F}, 1);
assert.sameValue(c.\u2118, 1);
assert.sameValue(c.ZW_\u200C_NJ, 1);
assert.sameValue(c.ZW_\u200D_J, 1);
