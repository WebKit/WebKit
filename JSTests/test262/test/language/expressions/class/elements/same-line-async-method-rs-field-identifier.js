// This file was procedurally generated from the following sources:
// - src/class-elements/rs-field-identifier.case
// - src/class-elements/productions/cls-expr-after-same-line-async-method.template
/*---
description: Valid FieldDefinition (field definitions after an async method in the same line)
esid: prod-FieldDefinition
features: [class-fields-public, class, async-functions]
flags: [generated, async]
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
  async m() { return 42; } $; _; \u{6F}; \u2118; ZW_\u200C_NJ; ZW_\u200D_J;
  
}

var c = new C();

assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);
assert.sameValue(c.m, C.prototype.m);

verifyProperty(C.prototype, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
}, {restore: true});

c.m().then(function(v) {
  assert.sameValue(v, 42);

  function assertions() {
    // Cover $DONE handler for async cases.
    function $DONE(error) {
      if (error) {
        throw new Test262Error('Test262:AsyncTestFailure')
      }
    }
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
  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
