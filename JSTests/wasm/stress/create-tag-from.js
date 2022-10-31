import * as assert from '../assert.js'

let parameters = ["i32", "f32", "externref"];

assert.throws(WebAssembly.Tag, TypeError, "calling WebAssembly.Tag constructor without new is invalid");
let passedParameters = { parameters };
let tag = new WebAssembly.Tag(passedParameters);

assert.truthy(tag.type() !== passedParameters, "Tag type should return a fresh object");
assert.eq(tag.type().parameters, parameters, "Tags type should be the same as the parameters passed in");

assert.throws(WebAssembly.Exception, TypeError, "calling WebAssembly.Exception constructor without new is invalid");

let exception = new WebAssembly.Exception(tag, [1, 2.5, parameters]);
assert.truthy(exception.is(tag));
assert.eq(exception.getArg(tag, 0), 1);
assert.eq(exception.getArg(tag, 1), 2.5);
assert.eq(exception.getArg(tag, 2), parameters);

assert.throws(() => exception.getArg(tag, 3), RangeError, "WebAssembly.Exception.getArg(): Index out of range");

assert.eq(WebAssembly.Exception.prototype.__proto__, Object.prototype)
