// This file was procedurally generated from the following sources:
// - src/class-elements/computed-symbol-names.case
// - src/class-elements/productions/cls-expr-after-same-line-async-method.template
/*---
description: Computed property symbol names (field definitions after an async method in the same line)
esid: prod-FieldDefinition
features: [class-fields-public, Symbol, computed-property-names, class, async-functions]
flags: [generated, async]
includes: [propertyHelper.js]
info: |
    ClassElement:
      ...
      FieldDefinition ;

    FieldDefinition:
      ClassElementName Initializer_opt

    ClassElementName:
      PropertyName

---*/
var x = Symbol();
var y = Symbol();



var C = class {
  async m() { return 42; } [x]; [y] = 42;
  
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
    assert.sameValue(Object.hasOwnProperty.call(C.prototype, x), false);
    assert.sameValue(Object.hasOwnProperty.call(C, x), false);

    verifyProperty(c, x, {
      value: undefined,
      enumerable: true,
      writable: true,
      configurable: true
    });

    assert.sameValue(Object.hasOwnProperty.call(C.prototype, y), false);
    assert.sameValue(Object.hasOwnProperty.call(C, y), false);

    verifyProperty(c, y, {
      value: 42,
      enumerable: true,
      writable: true,
      configurable: true
    });

    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "x"), false);
    assert.sameValue(Object.hasOwnProperty.call(C, "x"), false);
    assert.sameValue(Object.hasOwnProperty.call(c, "x"), false);

    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "y"), false);
    assert.sameValue(Object.hasOwnProperty.call(C, "y"), false);
    assert.sameValue(Object.hasOwnProperty.call(c, "y"), false);
  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
