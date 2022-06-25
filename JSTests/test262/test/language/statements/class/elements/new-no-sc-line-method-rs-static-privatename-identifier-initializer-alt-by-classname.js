// This file was procedurally generated from the following sources:
// - src/class-elements/rs-static-privatename-identifier-initializer-alt-by-classname.case
// - src/class-elements/productions/cls-decl-new-no-sc-line-method.template
/*---
description: Valid Static PrivateName (field definitions followed by a method in a new line without a semicolon)
esid: prod-FieldDefinition
features: [class-static-fields-private, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
info: |
    ClassElement :
      MethodDefinition
      static MethodDefinition
      FieldDefinition ;
      static FieldDefinition ;
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


class C {
  static #$ = 1; static #_ = 1; static #\u{6F} = 1; static #℘ = 1; static #ZW_‌_NJ = 1; static #ZW_‍_J = 1
  m() { return 42; }
  static $() {
    return C.#$;
  }
  static _() {
    return C.#_;
  }
  static \u{6F}() {
    return C.#\u{6F};
  }
  static ℘() {
    return C.#℘;
  }
  static ZW_‌_NJ() {
    return C.#ZW_‌_NJ;
  }
  static ZW_‍_J() {
    return C.#ZW_‍_J;
  }
}

var c = new C();

assert.sameValue(c.m(), 42);
assert.sameValue(c.m, C.prototype.m);
assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);

verifyProperty(C.prototype, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
});

assert.sameValue(C.$(), 1);
assert.sameValue(C._(), 1);
assert.sameValue(C.\u{6F}(), 1);
assert.sameValue(C.℘(), 1);
assert.sameValue(C.ZW_‌_NJ(), 1);
assert.sameValue(C.ZW_‍_J(), 1);

