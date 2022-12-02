// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-eh --experimental-wasm-reftypes

load("wasm-module-builder.js");

(function TestImport() {

  assertThrows(() => new WebAssembly.Tag(), TypeError,
      /expects the tag type as the first argument/);
  assertThrows(() => new WebAssembly.Tag({}), TypeError,
      /expects a tag type with the 'parameters' property/);
  assertThrows(() => new WebAssembly.Tag({parameters: ['foo']}), TypeError,
      /expects the 'parameters' field of the first argument to be a sequence of WebAssembly value types/);
  assertThrows(() => new WebAssembly.Tag({parameters: {}}), TypeError,
      /Type error/);

  let js_except_i32 = new WebAssembly.Tag({parameters: ['i32']});
  let js_except_v = new WebAssembly.Tag({parameters: []});
  let builder = new WasmModuleBuilder();
  builder.addImportedTag("m", "ex", kSig_v_i);

  assertDoesNotThrow(() => builder.instantiate({ m: { ex: js_except_i32 }}));
  assertThrows(
      () => builder.instantiate({ m: { ex: js_except_v }}), WebAssembly.LinkError,
      /imported Tag m:ex signature doesn't match the imported WebAssembly Tag's signature/);
  assertThrows(
      () => builder.instantiate({ m: { ex: js_except_v }}), WebAssembly.LinkError,
      /imported Tag m:ex signature doesn't match the imported WebAssembly Tag's signature/);
  assertTrue(js_except_i32.toString() == "[object WebAssembly.Tag]");
})();

(function TestExport() {
  let builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);
  builder.addExportOfKind("ex", kExternalTag, except);
  let instance = builder.instantiate();

  assertTrue(Object.prototype.hasOwnProperty.call(instance.exports, 'ex'));
  assertEquals("object", typeof instance.exports.ex);
  assertInstanceof(instance.exports.ex, WebAssembly.Tag);
  assertSame(instance.exports.ex.constructor, WebAssembly.Tag);
})();

(function TestImportExport() {

  let js_ex_i32 = new WebAssembly.Tag({parameters: ['i32']});
  let builder = new WasmModuleBuilder();
  let index = builder.addImportedTag("m", "ex", kSig_v_i);
  builder.addExportOfKind("ex", kExternalTag, index);

  let instance = builder.instantiate({ m: { ex: js_ex_i32 }});
  let res = instance.exports.ex;
  assertEquals(res, js_ex_i32);
})();


(function TestExceptionConstructor() {
  // Check errors.
  let js_tag = new WebAssembly.Tag({parameters: []});
  assertThrows(() => new WebAssembly.Exception(0), TypeError,
      /WebAssembly.Exception constructor expects the first argument to be a WebAssembly.Tag/);
  assertThrows(() => new WebAssembly.Exception({}), TypeError,
      /WebAssembly.Exception constructor expects the first argument to be a WebAssembly.Tag/);
  assertThrows(() => WebAssembly.Exception(js_tag), TypeError,
      /calling WebAssembly.Exception constructor without new is invalid/);
  let js_exception = new WebAssembly.Exception(js_tag, []);

  // Check prototype.
  assertSame(WebAssembly.Exception.prototype, js_exception.__proto__);
  assertTrue(js_exception instanceof WebAssembly.Exception);

  // Check prototype of a thrown exception.
  let builder = new WasmModuleBuilder();
  let wasm_tag = builder.addTag(kSig_v_v);
  builder.addFunction("throw", kSig_v_v)
      .addBody([kExprThrow, wasm_tag]).exportFunc();
  let instance = builder.instantiate();
  try {
    instance.exports.throw();
  } catch (e) {
    assertTrue(e instanceof WebAssembly.Exception);
  }
})();

(function TestExceptionConstructorWithPayload() {
  let tag = new WebAssembly.Tag(
      {parameters: ['i32', 'f32', 'i64', 'f64', 'externref']});
  assertThrows(() => new WebAssembly.Exception(
      tag, [1n, 2, 3n, 4, {}]), TypeError);
  assertDoesNotThrow(() => new WebAssembly.Exception(tag, [3, 4, 5n, 6, {}]));
})();

(function TestCatchJSException() {
  let builder = new WasmModuleBuilder();
  let js_tag = new WebAssembly.Tag({parameters: []});
  let js_func_index = builder.addImport('m', 'js_func', kSig_v_v);
  let js_tag_index = builder.addImportedTag("m", "js_tag", kSig_v_v);
  let tag_index = builder.addTag(kSig_v_v);
  builder.addExportOfKind("wasm_tag", kExternalTag, tag_index);
  builder.addFunction("catch", kSig_i_v)
      .addBody([
        kExprTry, kWasmI32,
        kExprCallFunction, js_func_index,
        kExprI32Const, 0,
        kExprCatch, js_tag_index,
        kExprI32Const, 1,
        kExprCatch, tag_index,
        kExprI32Const, 2,
        kExprEnd
      ]).exportFunc();
  let tag;
  function js_func() {
    throw new WebAssembly.Exception(tag, []);
  }
  let instance = builder.instantiate({m: {js_func, js_tag}});
  tag = js_tag;
  assertEquals(1, instance.exports.catch());
  tag = instance.exports.wasm_tag;
  assertEquals(2, instance.exports.catch());
})();

function TestCatchJS(types_str, types, values) {
  // Create a JS exception, catch it in wasm and check the unpacked value(s).
  let builder = new WasmModuleBuilder();
  let js_tag = new WebAssembly.Tag({parameters: types_str});
  let js_func_index = builder.addImport('m', 'js_func', kSig_v_v);
  let sig1 = makeSig(types, []);
  let sig2 = makeSig([], types);
  let js_tag_index = builder.addImportedTag("m", "js_tag", sig1);
  let tag_index = builder.addTag(sig1);
  let return_type = builder.addType(sig2);
  builder.addExportOfKind("wasm_tag", kExternalTag, tag_index);
  builder.addFunction("catch", sig2)
      .addBody([
        kExprTry, return_type,
        kExprCallFunction, js_func_index,
        kExprUnreachable,
        kExprCatch, js_tag_index,
        kExprCatch, tag_index,
        kExprEnd
      ]).exportFunc();
  let exception;
  function js_func() {
    throw exception;
  }
  let expected = values.length == 1 ? values[0] : values;
  let instance = builder.instantiate({m: {js_func, js_tag}});
  exception = new WebAssembly.Exception(js_tag, values);
  assertEquals(expected, instance.exports.catch());
  exception = new WebAssembly.Exception(instance.exports.wasm_tag, values);
  assertEquals(expected, instance.exports.catch());
}

(function TestCatchJSExceptionWithPayload() {
  TestCatchJS(['i32'], [kWasmI32], [1]);
  TestCatchJS(['i64'], [kWasmI64], [2n]);
  TestCatchJS(['f32'], [kWasmF32], [3]);
  TestCatchJS(['f64'], [kWasmF64], [4]);
  TestCatchJS(['externref'], [kWasmExternRef], [{value: 5}]);
  TestCatchJS(['i32', 'i64', 'f32', 'f64', 'externref'],
              [kWasmI32, kWasmI64, kWasmF32, kWasmF64, kWasmExternRef],
              [6, 7n, 8, 9, {value: 10}]);
})();

function TestGetArgHelper(types_str, types, values) {
  let tag = new WebAssembly.Tag({parameters: types_str});
  let exception = new WebAssembly.Exception(tag, values);
  for (i = 0; i < types.length; ++i) {
    assertEquals(exception.getArg(tag, i), values[i]);
  }

  let builder = new WasmModuleBuilder();
  let sig = makeSig(types, []);
  let tag_index = builder.addImportedTag("m", "t", sig);
  let body = [];
  for (i = 0; i < types.length; ++i) {
    body.push(kExprLocalGet, i);
  }
  body.push(kExprThrow, tag_index);
  builder.addFunction("throw", sig)
      .addBody(body).exportFunc();
  let instance = builder.instantiate({'m': {'t': tag}});
  try {
    instance.exports.throw(...values);
  } catch (e) {
    for (i = 0; i < types.length; ++i) {
      assertEquals(e.getArg(tag, i), values[i]);
    }
  }
}

(function TestGetArg() {
  // Check errors.
  let tag = new WebAssembly.Tag({parameters: ['i32']});
  let exception = new WebAssembly.Exception(tag, [0]);
  assertThrows(() => exception.getArg(0, 0), TypeError,
      /First argument must be a WebAssembly.Tag/);
  assertThrows(() => exception.getArg({}, 0), TypeError,
      /First argument must be a WebAssembly.Tag/);
  //assertThrows(() => exception.getArg(tag, undefined), TypeError,
      ///Index must be convertible to a valid number/);
  assertThrows(() => exception.getArg(tag, 0xFFFFFFFF), RangeError,
      /Index out of range/);
  let wrong_tag = new WebAssembly.Tag({parameters: ['i32']});
  assertThrows(() => exception.getArg(wrong_tag, 0), TypeError,
      /First argument does not match the exception tag/);

  // Check decoding.
  TestGetArgHelper(['i32'], [kWasmI32], [1]);
  TestGetArgHelper(['i64'], [kWasmI64], [2n]);
  TestGetArgHelper(['f32'], [kWasmF32], [3]);
  TestGetArgHelper(['f64'], [kWasmF64], [4]);
  TestGetArgHelper(['externref'], [kWasmExternRef], [{val: 5}]);
  TestGetArgHelper(['i32', 'i64', 'f32', 'f64', 'externref'], [kWasmI32, kWasmI64, kWasmF32, kWasmF64, kWasmExternRef], [5, 6n, 7, 8, {val: 9}]);
})();

(function TestExceptionIs() {
  let tag1 = new WebAssembly.Tag({parameters: []});
  let tag2 = new WebAssembly.Tag({parameters: []});
  assertThrows(() => new WebAssembly.Exception({}, []), TypeError,
      /expects the first argument to be a WebAssembly.Tag/);

  let exception = new WebAssembly.Exception(tag1, []);
  assertTrue(exception.is(tag1));
  assertFalse(exception.is(tag2));

  assertThrows(() => exception.is.apply({}, tag1), TypeError,
      /WebAssembly.Exception operation called on non-Exception object/);
})();
