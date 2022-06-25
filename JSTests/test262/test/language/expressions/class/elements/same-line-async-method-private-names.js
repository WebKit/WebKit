// This file was procedurally generated from the following sources:
// - src/class-elements/private-names.case
// - src/class-elements/productions/cls-expr-after-same-line-async-method.template
/*---
description: private names (field definitions after an async method in the same line)
esid: prod-FieldDefinition
features: [class-fields-private, class, class-fields-public, async-functions]
flags: [generated, async]
includes: [propertyHelper.js]
info: |
    ClassElement :
      ...
      FieldDefinition ;

    FieldDefinition :
      ClassElementName Initializer_opt

    ClassElementName :
      PrivateName

    PrivateName :
      # IdentifierName

---*/


var C = class {
  async m() { return 42; } #x; #y;
  x() {
    this.#x = 42;
    return this.#x;
  }
  y() {
    this.#y = 43;
    return this.#y;
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

c.m().then(function(v) {
  assert.sameValue(v, 42);

  function assertions() {
    // Cover $DONE handler for async cases.
    function $DONE(error) {
      if (error) {
        throw new Test262Error('Test262:AsyncTestFailure')
      }
    }
    // Test the private fields do not appear as properties before set to value
    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#x"), false, "test 1");
    assert.sameValue(Object.hasOwnProperty.call(C, "#x"), false, "test 2");
    assert.sameValue(Object.hasOwnProperty.call(c, "#x"), false, "test 3");

    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "#y"), false, "test 4");
    assert.sameValue(Object.hasOwnProperty.call(C, "#y"), false, "test 5");
    assert.sameValue(Object.hasOwnProperty.call(c, "#y"), false, "test 6");

    // Test if private fields can be sucessfully accessed and set to value
    assert.sameValue(c.x(), 42, "test 7");
    assert.sameValue(c.y(), 43, "test 8");

    // Test the private fields do not appear as properties before after set to value
    assert.sameValue(Object.hasOwnProperty.call(c, "#x"), false, "test 9");
    assert.sameValue(Object.hasOwnProperty.call(c, "#y"), false, "test 10");
  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
