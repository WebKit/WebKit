// This file was procedurally generated from the following sources:
// - src/class-elements/rs-private-setter.case
// - src/class-elements/productions/cls-expr-multiple-definitions.template
/*---
description: Valid PrivateName as private setter (multiple fields definitions)
esid: prod-FieldDefinition
features: [class-methods-private, class-fields-private, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
info: |
    ClassElement :
      MethodDefinition
      ...
      ;

    MethodDefinition :
      ...
      set ClassElementName ( PropertySetParameterList ) { FunctionBody }

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
  foo = "foobar";
  m() { return 42 }
  #$_; #__; #\u{6F}_; #\u2118_; #ZW_\u200C_NJ_; #ZW_\u200D_J_;
  set #$(value) {
    this.#$_ = value;
  }
  set #_(value) {
    this.#__ = value;
  }
  set #\u{6F}(value) {
    this.#\u{6F}_ = value;
  }
  set #\u2118(value) {
    this.#\u2118_ = value;
  }
  set #ZW_\u200C_NJ(value) {
    this.#ZW_\u200C_NJ_ = value;
  }
  set #ZW_\u200D_J(value) {
    this.#ZW_\u200D_J_ = value;
  }

  m2() { return 39 }
  bar = "barbaz";
  $(value) {
    this.#$ = value;
    return this.#$_;
  }
  _(value) {
    this.#_ = value;
    return this.#__;
  }
  \u{6F}(value) {
    this.#\u{6F} = value;
    return this.#\u{6F}_;
  }
  \u2118(value) {
    this.#\u2118 = value;
    return this.#\u2118_;
  }
  ZW_\u200C_NJ(value) {
    this.#ZW_\u200C_NJ = value;
    return this.#ZW_\u200C_NJ_;
  }
  ZW_\u200D_J(value) {
    this.#ZW_\u200D_J = value;
    return this.#ZW_\u200D_J_;
  }

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

assert.sameValue(c.$(1), 1);
assert.sameValue(c._(1), 1);
assert.sameValue(c.\u{6F}(1), 1);
assert.sameValue(c.\u2118(1), 1);
assert.sameValue(c.ZW_\u200C_NJ(1), 1);
assert.sameValue(c.ZW_\u200D_J(1), 1);
