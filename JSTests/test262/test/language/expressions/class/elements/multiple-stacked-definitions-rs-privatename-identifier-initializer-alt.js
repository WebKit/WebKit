// This file was procedurally generated from the following sources:
// - src/class-elements/rs-privatename-identifier-initializer-alt.case
// - src/class-elements/productions/cls-expr-multiple-stacked-definitions.template
/*---
description: Valid PrivateName (multiple stacked fields definitions through ASI)
esid: prod-FieldDefinition
features: [class-fields-private, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
info: |
    ClassElement :
      MethodDefinition
      static MethodDefinition
      FieldDefinition ;
      ;

    FieldDefinition :
      ClassElementName Initializer _opt

    ClassElementName :
      PropertyName
      PrivateName

    PrivateName ::
      # IdentifierName

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
  #$ = 1; #_ = 1; #\u{6F} = 1; #℘ = 1; #ZW_‌_NJ = 1; #ZW_‍_J = 1
  foo = "foobar"
  bar = "barbaz";
  $() {
    return this.#$;
  }
  _() {
    return this.#_;
  }
  \u{6F}() {
    return this.#\u{6F};
  }
  ℘() {
    return this.#℘;
  }
  ZW_‌_NJ() {
    return this.#ZW_‌_NJ;
  }
  ZW_‍_J() {
    return this.#ZW_‍_J;
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

assert.sameValue(c.$(), 1);
assert.sameValue(c._(), 1);
assert.sameValue(c.\u{6F}(), 1);
assert.sameValue(c.℘(), 1);
assert.sameValue(c.ZW_‌_NJ(), 1);
assert.sameValue(c.ZW_‍_J(), 1);

