// This file was procedurally generated from the following sources:
// - src/class-elements/string-literal-names.case
// - src/class-elements/productions/cls-expr-after-same-line-async-gen.template
/*---
description: String literal names (field definitions after an async generator in the same line)
esid: prod-FieldDefinition
features: [class-fields-public, class, async-iteration]
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


var C = class {
  async *m() { return 42; } 'a'; "b"; 'c' = 39;
  "d" = 42;
  
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
    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "a"), false);
    assert.sameValue(Object.hasOwnProperty.call(C, "a"), false);

    verifyProperty(c, "a", {
      value: undefined,
      enumerable: true,
      writable: true,
      configurable: true
    });

    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "b"), false);
    assert.sameValue(Object.hasOwnProperty.call(C, "b"), false);

    verifyProperty(c, "b", {
      value: undefined,
      enumerable: true,
      writable: true,
      configurable: true
    });

    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "c"), false);
    assert.sameValue(Object.hasOwnProperty.call(C, "c"), false);

    verifyProperty(c, "c", {
      value: 39,
      enumerable: true,
      writable: true,
      configurable: true
    });

    assert.sameValue(Object.hasOwnProperty.call(C.prototype, "d"), false);
    assert.sameValue(Object.hasOwnProperty.call(C, "d"), false);

    verifyProperty(c, "d", {
      value: 42,
      enumerable: true,
      writable: true,
      configurable: true
    });
  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
