// This file was procedurally generated from the following sources:
// - src/class-elements/rs-static-generator-method-privatename-identifier-alt.case
// - src/class-elements/productions/cls-expr-after-same-line-static-gen.template
/*---
description: Valid Static GeneratorMethod PrivateName (field definitions after a static generator in the same line)
esid: prod-FieldDefinition
features: [class-static-methods-private, generators, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
info: |
    ClassElement :
      MethodDefinition
      static MethodDefinition
      FieldDefinition ;
      static FieldDefinition ;
      ;

    MethodDefinition :
      GeneratorMethod

    GeneratorMethod :
      * ClassElementName ( UniqueFormalParameters ){ GeneratorBody }

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
  static *m() { return 42; } static * #$(value) {
    yield * value;
  }
  static * #_(value) {
    yield * value;
  }
  static * #o(value) {
    yield * value;
  }
  static * #℘(value) {
    yield * value;
  }
  static * #ZW_‌_NJ(value) {
    yield * value;
  }
  static * #ZW_‍_J(value) {
    yield * value;
  };
  static get $() {
    return this.#$;
  }
  static get _() {
    return this.#_;
  }
  static get o() {
    return this.#o;
  }
  static get ℘() { // DO NOT CHANGE THE NAME OF THIS FIELD
    return this.#℘;
  }
  static get ZW_‌_NJ() { // DO NOT CHANGE THE NAME OF THIS FIELD
    return this.#ZW_‌_NJ;
  }
  static get ZW_‍_J() { // DO NOT CHANGE THE NAME OF THIS FIELD
    return this.#ZW_‍_J;
  }

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

assert.sameValue(C.$([1]).next().value, 1);
assert.sameValue(C._([1]).next().value, 1);
assert.sameValue(C.o([1]).next().value, 1);
assert.sameValue(C.℘([1]).next().value, 1);
assert.sameValue(C.ZW_‌_NJ([1]).next().value, 1);
assert.sameValue(C.ZW_‍_J([1]).next().value, 1);
