// This file was procedurally generated from the following sources:
// - src/class-elements/rs-field-identifier.case
// - src/class-elements/productions/cls-expr-same-line-generator.template
/*---
description: Valid FieldDefinition (field definitions followed by a generator method in the same line)
esid: prod-FieldDefinition
features: [class-fields-public, class, generators]
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
  $; _; \u{6F}; \u2118; ZW_\u200C_NJ; ZW_\u200D_J; *m() { return 42; }
  
}

var c = new C();

assert.sameValue(c.m().next().value, 42);
assert.sameValue(c.m, C.prototype.m);
assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);

verifyProperty(C.prototype, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
});

c.$ = 1;
c._ = 1;
c.\u{6F} = 1;
c.\u2118 = 1;
c.ZW_\u200C_NJ = 1;
c.ZW_\u200D_J = 1;

assert.sameValue(c.$, 1);
assert.sameValue(c._, 1);
assert.sameValue(c.\u{6F}, 1);
assert.sameValue(c.\u2118, 1);
assert.sameValue(c.ZW_\u200C_NJ, 1);
assert.sameValue(c.ZW_\u200D_J, 1);
