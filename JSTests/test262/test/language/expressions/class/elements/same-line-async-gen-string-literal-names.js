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
    assert(
      !Object.prototype.hasOwnProperty.call(C.prototype, "a"),
      "a does not appear as an own property on C prototype"
    );
    assert(
      !Object.prototype.hasOwnProperty.call(C, "a"),
      "a does not appear as an own property on C constructor"
    );

    verifyProperty(c, "a", {
      value: undefined,
      enumerable: true,
      writable: true,
      configurable: true
    });

    assert(
      !Object.prototype.hasOwnProperty.call(C.prototype, "b"),
      "b does not appear as an own property on C prototype"
    );
    assert(
      !Object.prototype.hasOwnProperty.call(C, "b"),
      "b does not appear as an own property on C constructor"
    );

    verifyProperty(c, "b", {
      value: undefined,
      enumerable: true,
      writable: true,
      configurable: true
    });

    assert(
      !Object.prototype.hasOwnProperty.call(C.prototype, "c"),
      "c does not appear as an own property on C prototype"
    );
    assert(
      !Object.prototype.hasOwnProperty.call(C, "c"),
      "c does not appear as an own property on C constructor"
    );

    verifyProperty(c, "c", {
      value: 39,
      enumerable: true,
      writable: true,
      configurable: true
    });

    assert(
      !Object.prototype.hasOwnProperty.call(C.prototype, "d"),
      "d does not appear as an own property on C prototype"
    );
    assert(
      !Object.prototype.hasOwnProperty.call(C, "d"),
      "d does not appear as an own property on C constructor"
    );

    verifyProperty(c, "d", {
      value: 42,
      enumerable: true,
      writable: true,
      configurable: true
    });
  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
