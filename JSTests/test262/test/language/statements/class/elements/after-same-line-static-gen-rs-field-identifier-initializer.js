// This file was procedurally generated from the following sources:
// - src/class-elements/rs-field-identifier-initializer.case
// - src/class-elements/productions/cls-decl-after-same-line-static-gen.template
/*---
description: Valid FieldDefinition (field definitions after a static generator in the same line)
esid: prod-FieldDefinition
features: [class-fields-public, generators, class]
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


class C {
  static *m() { return 42; } $ = 1; _ = 1; \u{6F} = 1; \u2118 = 1; ZW_\u200C_NJ = 1; ZW_\u200D_J = 1;
  
}

var c = new C();

assert.sameValue(C.m().next().value, 42);
assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "m"), false);

verifyProperty(C, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
});

assert.sameValue(c.$, 1);
assert.sameValue(c._, 1);
assert.sameValue(c.\u{6F}, 1);
assert.sameValue(c.\u2118, 1);
assert.sameValue(c.ZW_\u200C_NJ, 1);
assert.sameValue(c.ZW_\u200D_J, 1);
