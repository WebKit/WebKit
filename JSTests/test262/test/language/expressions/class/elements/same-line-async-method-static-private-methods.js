// This file was procedurally generated from the following sources:
// - src/class-elements/static-private-methods.case
// - src/class-elements/productions/cls-expr-after-same-line-async-method.template
/*---
description: static private methods (field definitions after an async method in the same line)
esid: prod-FieldDefinition
features: [class-static-methods-private, class, class-fields-public, async-functions]
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
  async m() { return 42; } ;
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

assert(
  !Object.prototype.hasOwnProperty.call(c, "m"),
  "m doesn't appear as an own property on the C instance"
);
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
    // Test the private methods do not appear as properties before set to value
    assert(!Object.prototype.hasOwnProperty.call(C.prototype, "#x"), "test 1");
    assert(!Object.prototype.hasOwnProperty.call(C, "#x"), "test 2");
    assert(!Object.prototype.hasOwnProperty.call(c, "#x"), "test 3");

    assert(!Object.prototype.hasOwnProperty.call(C.prototype, "#y"), "test 4");
    assert(!Object.prototype.hasOwnProperty.call(C, "#y"), "test 5");
    assert(!Object.prototype.hasOwnProperty.call(c, "#y"), "test 6");

    // Test if private fields can be sucessfully accessed and set to value
    assert.sameValue(C.x(), 42, "test 7");
    assert.sameValue(C.y(), 86, "test 8");

    // Test the private fields do not appear as properties before after set to value
    assert(!Object.prototype.hasOwnProperty.call(C, "#x"), "test 9");
    assert(!Object.prototype.hasOwnProperty.call(C, "#y"), "test 10");
  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
