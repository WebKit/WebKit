// This file was procedurally generated from the following sources:
// - src/class-elements/literal-names-asi.case
// - src/class-elements/productions/cls-expr-after-same-line-static-async-method.template
/*---
description: Literal property names with ASI (field definitions after a static async method in the same line)
esid: prod-FieldDefinition
features: [class-fields-public, class, async-functions]
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
  static async m() { return 42; } a
  b = 42;;
  
}

var c = new C();

assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "m"), false);

verifyProperty(C, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
}, {restore: true});

C.m().then(function(v) {
  assert.sameValue(v, 42);

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
      value: 42,
      enumerable: true,
      writable: true,
      configurable: true
    });
  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
