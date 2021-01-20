// This file was procedurally generated from the following sources:
// - src/class-elements/static-private-methods.case
// - src/class-elements/productions/cls-expr-after-same-line-async-gen.template
/*---
description: static private methods (field definitions after an async generator in the same line)
esid: prod-FieldDefinition
features: [class-static-methods-private, class, class-fields-public, async-iteration]
flags: [generated, async]
includes: [propertyHelper.js]
info: |
    ClassElement :
      ...
      static FieldDefinition ;

    FieldDefinition :
      ClassElementName Initializer_opt

    ClassElementName :
      PrivateName

    PrivateName :
      # IdentifierName

---*/


var C = class {
  async *m() { return 42; } ;
  static #x(value) {
    return value / 2;
  }
  static #y(value) {
    return value * 2;
  }
  static x() {
    return this.#x(84);
  }
  static y() {
    return this.#y(43);
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
    // Test the private methods do not appear as properties before set to value
    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#x"), false, "test 1");
    assert.sameValue(Object.hasOwnProperty.call(C, "#x"), false, "test 2");
    assert.sameValue(Object.hasOwnProperty.call(c, "#x"), false, "test 3");

    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#y"), false, "test 4");
    assert.sameValue(Object.hasOwnProperty.call(C, "#y"), false, "test 5");
    assert.sameValue(Object.hasOwnProperty.call(c, "#y"), false, "test 6");

    // Test if private fields can be sucessfully accessed and set to value
    assert.sameValue(C.x(), 42, "test 7");
    assert.sameValue(C.y(), 86, "test 8");

    // Test the private fields do not appear as properties before after set to value
    assert.sameValue(Object.hasOwnProperty.call(C, "#x"), false, "test 9");
    assert.sameValue(Object.hasOwnProperty.call(C, "#y"), false, "test 10");
  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
