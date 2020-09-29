// This file was procedurally generated from the following sources:
// - src/function-forms/forbidden-ext-indirect-access-prop-caller.case
// - src/function-forms/forbidden-extensions/bullet-two/meth.template
/*---
description: Forbidden extension, o.caller (method)
esid: sec-runtime-semantics-definemethod
flags: [generated, noStrict]
info: |
    MethodDefinition : PropertyName ( StrictFormalParameters ) { FunctionBody }

    If an implementation extends any function object with an own property named "caller" the value of that property, as observed using [[Get]] or [[GetOwnProperty]], must not be a strict function object. If it is an accessor property, the function that is the value of the property's [[Get]] attribute must never return a strict function when called.

---*/
function inner() {
  // This property is forbidden from having a value that is strict function object
  return inner.caller;
}

var callCount = 0;
var obj = {
  method() {
    "use strict";
    inner().toString();
    callCount++;
  }
};

assert.throws(TypeError, function() {
  obj.method();
});
assert.sameValue(callCount, 0, 'method body not evaluated');
