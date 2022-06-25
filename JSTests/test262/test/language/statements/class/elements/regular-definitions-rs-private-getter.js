// This file was procedurally generated from the following sources:
// - src/class-elements/rs-private-getter.case
// - src/class-elements/productions/cls-decl-regular-definitions.template
/*---
description: Valid PrivateName as private getter (regular fields defintion)
esid: prod-FieldDefinition
features: [class-methods-private, class-fields-private, class, class-fields-public]
flags: [generated]
info: |
    ClassElement :
      MethodDefinition
      ...
      ;

    MethodDefinition :
      ...
      get ClassElementName ( ){ FunctionBody }
      ...

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
  #$_; #__; #\u{6F}_; #\u2118_; #ZW_\u200C_NJ_; #ZW_\u200D_J_;
  get #$() {
    return this.#$_;
  }
  get #_() {
    return this.#__;
  }
  get #\u{6F}() {
    return this.#\u{6F}_;
  }
  get #\u2118() {
    return this.#\u2118_;
  }
  get #ZW_\u200C_NJ() {
    return this.#ZW_\u200C_NJ_;
  }
  get #ZW_\u200D_J() {
    return this.#ZW_\u200D_J_;
  }

  $(value) {
    this.#$_ = value;
    return this.#$;
  }
  _(value) {
    this.#__ = value;
    return this.#_;
  }
  \u{6F}(value) {
    this.#\u{6F}_ = value;
    return this.#\u{6F};
  }
  \u2118(value) {
    this.#\u2118_ = value;
    return this.#\u2118;
  }
  ZW_\u200C_NJ(value) {
    this.#ZW_\u200C_NJ_ = value;
    return this.#ZW_\u200C_NJ;
  }
  ZW_\u200D_J(value) {
    this.#ZW_\u200D_J_ = value;
    return this.#ZW_\u200D_J;
  }

}

var c = new C();

assert.sameValue(c.$(1), 1);
assert.sameValue(c._(1), 1);
assert.sameValue(c.\u{6F}(1), 1);
assert.sameValue(c.\u2118(1), 1);
assert.sameValue(c.ZW_\u200C_NJ(1), 1);
assert.sameValue(c.ZW_\u200D_J(1), 1);
