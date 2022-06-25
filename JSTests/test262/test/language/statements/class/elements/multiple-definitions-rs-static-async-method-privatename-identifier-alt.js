// This file was procedurally generated from the following sources:
// - src/class-elements/rs-static-async-method-privatename-identifier-alt.case
// - src/class-elements/productions/cls-decl-multiple-definitions.template
/*---
description: Valid Static AsyncMethod PrivateName (multiple fields definitions)
esid: prod-FieldDefinition
features: [class-static-methods-private, class, class-fields-public]
flags: [generated, async]
includes: [propertyHelper.js]
info: |
    ClassElement :
      MethodDefinition
      static MethodDefinition
      FieldDefinition ;
      static FieldDefinition ;
      ;

    MethodDefinition :
      AsyncMethod

    AsyncMethod :
      async [no LineTerminator here] ClassElementName ( UniqueFormalParameters ){ AsyncFunctionBody }

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
  foo = "foobar";
  m() { return 42 }
  static async #$(value) {
    return await value;
  }
  static async #_(value) {
    return await value;
  }
  static async #o(value) {
    return await value;
  }
  static async #℘(value) {
    return await value;
  }
  static async #ZW_‌_NJ(value) {
    return await value;
  }
  static async #ZW_‍_J(value) {
    return await value;
  }
  m2() { return 39 }
  bar = "barbaz";
  static async $(value) {
    return await this.#$(value);
  }
  static async _(value) {
    return await this.#_(value);
  }
  static async o(value) {
    return await this.#o(value);
  }
  static async ℘(value) { // DO NOT CHANGE THE NAME OF THIS FIELD
    return await this.#℘(value);
  }
  static async ZW_‌_NJ(value) { // DO NOT CHANGE THE NAME OF THIS FIELD
    return await this.#ZW_‌_NJ(value);
  }
  static async ZW_‍_J(value) { // DO NOT CHANGE THE NAME OF THIS FIELD
    return await this.#ZW_‍_J(value);
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

Promise.all([
  C.$(1),
  C._(1),
  C.o(1),
  C.℘(1), // DO NOT CHANGE THE NAME OF THIS FIELD
  C.ZW_‌_NJ(1), // DO NOT CHANGE THE NAME OF THIS FIELD
  C.ZW_‍_J(1), // DO NOT CHANGE THE NAME OF THIS FIELD
]).then(results => {

  assert.sameValue(results[0], 1);
  assert.sameValue(results[1], 1);
  assert.sameValue(results[2], 1);
  assert.sameValue(results[3], 1);
  assert.sameValue(results[4], 1);
  assert.sameValue(results[5], 1);

}).then($DONE, $DONE);
