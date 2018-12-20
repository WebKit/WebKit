// This file was procedurally generated from the following sources:
// - src/class-elements/computed-symbol-names.case
// - src/class-elements/productions/cls-decl-after-same-line-static-async-gen.template
/*---
description: Computed property symbol names (field definitions after a static async generator in the same line)
esid: prod-FieldDefinition
features: [class-fields-public, Symbol, computed-property-names, class, async-iteration]
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



class C {
  static async *m() { return 42; } [x]; [y] = 42;
  
}

var c = new C();

assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "m"), false);

verifyProperty(C, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
}, {restore: true});

C.m().next().then(function(v) {
  assert.sameValue(v.value, 42);
  assert.sameValue(v.done, true);

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
}, $DONE).then($DONE, $DONE);
