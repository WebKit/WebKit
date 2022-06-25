// This file was procedurally generated from the following sources:
// - src/class-elements/rs-static-generator-method-privatename-identifier.case
// - src/class-elements/productions/cls-expr-wrapped-in-sc.template
/*---
description: Valid Static GeneratorMethod PrivateName (fields definition wrapped in semicolons)
esid: prod-FieldDefinition
features: [class-static-methods-private, class, class-fields-public]
flags: [generated]
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
  ;;;;
  ;;;;;;static * #$(value) {
    yield * value;
  }
  static * #_(value) {
    yield * value;
  }
  static * #\u{6F}(value) {
    yield * value;
  }
  static * #\u2118(value) {
    yield * value;
  }
  static * #ZW_\u200C_NJ(value) {
    yield * value;
  }
  static * #ZW_\u200D_J(value) {
    yield * value;
  };;;;;;;
  ;;;;
  static get $() {
    return this.#$;
  }
  static get _() {
    return this.#_;
  }
  static get \u{6F}() {
    return this.#\u{6F};
  }
  static get \u2118() {
    return this.#\u2118;
  }
  static get ZW_\u200C_NJ() {
    return this.#ZW_\u200C_NJ;
  }
  static get ZW_\u200D_J() {
    return this.#ZW_\u200D_J;
  }

}

var c = new C();

assert.sameValue(C.$([1]).next().value, 1);
assert.sameValue(C._([1]).next().value, 1);
assert.sameValue(C.\u{6F}([1]).next().value, 1);
assert.sameValue(C.\u2118([1]).next().value, 1);
assert.sameValue(C.ZW_\u200C_NJ([1]).next().value, 1);
assert.sameValue(C.ZW_\u200D_J([1]).next().value, 1);

