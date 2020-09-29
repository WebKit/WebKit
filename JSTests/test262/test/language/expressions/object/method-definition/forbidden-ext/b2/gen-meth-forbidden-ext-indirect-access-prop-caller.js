// This file was procedurally generated from the following sources:
// - src/function-forms/forbidden-ext-indirect-access-prop-caller.case
// - src/function-forms/forbidden-extensions/bullet-two/gen-meth.template
/*---
description: Forbidden extension, o.caller (generator method)
esid: sec-generator-definitions-runtime-semantics-propertydefinitionevaluation
features: [generators]
flags: [generated, noStrict]
info: |
    GeneratorMethod :
        * PropertyName ( StrictFormalParameters ) { GeneratorBody }


    If an implementation extends any function object with an own property named "caller" the value of that property, as observed using [[Get]] or [[GetOwnProperty]], must not be a strict function object. If it is an accessor property, the function that is the value of the property's [[Get]] attribute must never return a strict function when called.

---*/
function inner() {
  // This property is forbidden from having a value that is strict function object
  return inner.caller;
}

var callCount = 0;
var obj = {
  *method() {
    "use strict";
    inner().toString();
    callCount++;
  }
};

assert.throws(TypeError, function() {
  obj.method().next();
});
assert.sameValue(callCount, 0, 'generator method body not evaluated');
