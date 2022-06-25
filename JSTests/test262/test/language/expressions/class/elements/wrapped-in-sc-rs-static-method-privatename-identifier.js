// This file was procedurally generated from the following sources:
// - src/class-elements/rs-static-method-privatename-identifier.case
// - src/class-elements/productions/cls-expr-wrapped-in-sc.template
/*---
description: Valid Static Method PrivateName (fields definition wrapped in semicolons)
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
      ClassElementName ( UniqueFormalParameters ){ FunctionBody }

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
  ;;;;;;static #$(value) {
    return value;
  }
  static #_(value) {
    return value;
  }
  static #\u{6F}(value) {
    return value;
  }
  static #\u2118(value) {
    return value;
  }
  static #ZW_\u200C_NJ(value) {
    return value;
  }
  static #ZW_\u200D_J(value) {
    return value;
  };;;;;;;
  ;;;;
  static $(value) {
    return this.#$(value);
  }
  static _(value) {
    return this.#_(value);
  }
  static \u{6F}(value) {
    return this.#\u{6F}(value);
  }
  static \u2118(value) {
    return this.#\u2118(value);
  }
  static ZW_\u200C_NJ(value) {
    return this.#ZW_\u200C_NJ(value);
  }
  static ZW_\u200D_J(value) {
    return this.#ZW_\u200D_J(value);
  }

}

var c = new C();

assert.sameValue(C.$(1), 1);
assert.sameValue(C._(1), 1);
assert.sameValue(C.\u{6F}(1), 1);
assert.sameValue(C.\u2118(1), 1);
assert.sameValue(C.ZW_\u200C_NJ(1), 1);
assert.sameValue(C.ZW_\u200D_J(1), 1);

