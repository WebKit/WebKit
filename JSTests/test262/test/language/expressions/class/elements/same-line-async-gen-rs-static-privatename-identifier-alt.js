// This file was procedurally generated from the following sources:
// - src/class-elements/rs-static-privatename-identifier-alt.case
// - src/class-elements/productions/cls-expr-after-same-line-async-gen.template
/*---
description: Valid Static PrivateName (field definitions after an async generator in the same line)
esid: prod-FieldDefinition
features: [class-static-fields-private, class, class-fields-public, async-iteration]
flags: [generated, async]
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


var C = class {
  async *m() { return 42; } static #$; static #_; static #\u{6F}; static #℘; static #ZW_‌_NJ; static #ZW_‍_J;
  static $(value) {
    this.#$ = value;
    return this.#$;
  }
  static _(value) {
    this.#_ = value;
    return this.#_;
  }
  static o(value) {
    this.#\u{6F} = value;
    return this.#\u{6F};
  }
  static ℘(value) { // DO NOT CHANGE THE NAME OF THIS FIELD
    this.#℘ = value;
    return this.#℘;
  }
  static ZW_‌_NJ(value) { // DO NOT CHANGE THE NAME OF THIS FIELD
    this.#ZW_‌_NJ = value;
    return this.#ZW_‌_NJ;
  }
  static ZW_‍_J(value) { // DO NOT CHANGE THE NAME OF THIS FIELD
    this.#ZW_‍_J = value;
    return this.#ZW_‍_J;
  }
}

var c = new C();

assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);
assert.sameValue(c.m, C.prototype.m);

verifyProperty(C.prototype, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
}, {restore: true});

c.m().next().then(function(v) {
  assert.sameValue(v.value, 42);
  assert.sameValue(v.done, true);

  function assertions() {
    // Cover $DONE handler for async cases.
    function $DONE(error) {
      if (error) {
        throw new Test262Error('Test262:AsyncTestFailure')
      }
    }
    assert.sameValue(C.$(1), 1);
    assert.sameValue(C._(1), 1);
    assert.sameValue(C.o(1), 1);
    assert.sameValue(C.℘(1), 1);      // DO NOT CHANGE THE NAME OF THIS FIELD
    assert.sameValue(C.ZW_‌_NJ(1), 1); // DO NOT CHANGE THE NAME OF THIS FIELD
    assert.sameValue(C.ZW_‍_J(1), 1);  // DO NOT CHANGE THE NAME OF THIS FIELD

  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
