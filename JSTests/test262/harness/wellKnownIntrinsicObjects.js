// Copyright (C) 2018 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    An Array of all representable Well-Known Intrinsic Objects
---*/

const WellKnownIntrinsicObjects = [
  {
    intrinsicName: "%Array%",
    globalNameOrSource: "Array"
  },
  {
    intrinsicName: "%ArrayBuffer%",
    globalNameOrSource: "ArrayBuffer"
  },
  {
    intrinsicName: "%ArrayBufferPrototype%",
    globalNameOrSource: "ArrayBuffer.prototype"
  },
  {
    intrinsicName: "%ArrayIteratorPrototype%",
    globalNameOrSource: "Object.getPrototypeOf([][Symbol.iterator]())"
  },
  {
    intrinsicName: "%ArrayPrototype%",
    globalNameOrSource: "Array.prototype"
  },
  {
    intrinsicName: "%ArrayProto_entries%",
    globalNameOrSource: "Array.prototype.entries"
  },
  {
    intrinsicName: "%ArrayProto_forEach%",
    globalNameOrSource: "Array.prototype.forEach"
  },
  {
    intrinsicName: "%ArrayProto_keys%",
    globalNameOrSource: "Array.prototype.keys"
  },
  {
    intrinsicName: "%ArrayProto_values%",
    globalNameOrSource: "Array.prototype.values"
  },
  {
    intrinsicName: "%AsyncFromSyncIteratorPrototype%",
    globalNameOrSource: "undefined"
  },
  {
    intrinsicName: "%AsyncFunction%",
    globalNameOrSource: "(async function() {}).constructor"
  },
  {
    intrinsicName: "%AsyncFunctionPrototype%",
    globalNameOrSource: "(async function() {}).constructor.prototype"
  },
  {
    intrinsicName: "%AsyncGenerator%",
    globalNameOrSource: "Object.getPrototypeOf((async function * () {})())"
  },
  {
    intrinsicName: "%AsyncGeneratorFunction%",
    globalNameOrSource: "Object.getPrototypeOf(async function * () {})"
  },
  {
    intrinsicName: "%AsyncGeneratorPrototype%",
    globalNameOrSource: "Object.getPrototypeOf(async function * () {}).prototype"
  },
  {
    intrinsicName: "%AsyncIteratorPrototype%",
    globalNameOrSource: "((async function * () {})())[Symbol.asyncIterator]()"
  },
  {
    intrinsicName: "%Atomics%",
    globalNameOrSource: "Atomics"
  },
  {
    intrinsicName: "%Boolean%",
    globalNameOrSource: "Boolean"
  },
  {
    intrinsicName: "%BooleanPrototype%",
    globalNameOrSource: "Boolean.prototype"
  },
  {
    intrinsicName: "%DataView%",
    globalNameOrSource: "DataView"
  },
  {
    intrinsicName: "%DataViewPrototype%",
    globalNameOrSource: "DataView.prototype"
  },
  {
    intrinsicName: "%Date%",
    globalNameOrSource: "Date"
  },
  {
    intrinsicName: "%DatePrototype%",
    globalNameOrSource: "Date.prototype"
  },
  {
    intrinsicName: "%decodeURI%",
    globalNameOrSource: "decodeURI"
  },
  {
    intrinsicName: "%decodeURIComponent%",
    globalNameOrSource: "decodeURIComponent"
  },
  {
    intrinsicName: "%encodeURI%",
    globalNameOrSource: "encodeURI"
  },
  {
    intrinsicName: "%encodeURIComponent%",
    globalNameOrSource: "encodeURIComponent"
  },
  {
    intrinsicName: "%Error%",
    globalNameOrSource: "Error"
  },
  {
    intrinsicName: "%ErrorPrototype%",
    globalNameOrSource: "Error.prototype"
  },
  {
    intrinsicName: "%eval%",
    globalNameOrSource: "eval"
  },
  {
    intrinsicName: "%EvalError%",
    globalNameOrSource: "EvalError"
  },
  {
    intrinsicName: "%EvalErrorPrototype%",
    globalNameOrSource: "EvalError.prototype"
  },
  {
    intrinsicName: "%Float32Array%",
    globalNameOrSource: "Float32Array"
  },
  {
    intrinsicName: "%Float32ArrayPrototype%",
    globalNameOrSource: "Float32Array.prototype"
  },
  {
    intrinsicName: "%Float64Array%",
    globalNameOrSource: "Float64Array"
  },
  {
    intrinsicName: "%Float64ArrayPrototype%",
    globalNameOrSource: "Float64Array.prototype"
  },
  {
    intrinsicName: "%Function%",
    globalNameOrSource: "Function"
  },
  {
    intrinsicName: "%FunctionPrototype%",
    globalNameOrSource: "Function.prototype"
  },
  {
    intrinsicName: "%Generator%",
    globalNameOrSource: "Object.getPrototypeOf((function * () {})())"
  },
  {
    intrinsicName: "%GeneratorFunction%",
    globalNameOrSource: "Object.getPrototypeOf(function * () {})"
  },
  {
    intrinsicName: "%GeneratorPrototype%",
    globalNameOrSource: "Object.getPrototypeOf(function * () {}).prototype"
  },
  {
    intrinsicName: "%Int8Array%",
    globalNameOrSource: "Int8Array"
  },
  {
    intrinsicName: "%Int8ArrayPrototype%",
    globalNameOrSource: "Int8Array.prototype"
  },
  {
    intrinsicName: "%Int16Array%",
    globalNameOrSource: "Int16Array"
  },
  {
    intrinsicName: "%Int16ArrayPrototype%",
    globalNameOrSource: "Int16Array.prototype"
  },
  {
    intrinsicName: "%Int32Array%",
    globalNameOrSource: "Int32Array"
  },
  {
    intrinsicName: "%Int32ArrayPrototype%",
    globalNameOrSource: "Int32Array.prototype"
  },
  {
    intrinsicName: "%isFinite%",
    globalNameOrSource: "isFinite"
  },
  {
    intrinsicName: "%isNaN%",
    globalNameOrSource: "isNaN"
  },
  {
    intrinsicName: "%IteratorPrototype%",
    globalNameOrSource: "Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()))"
  },
  {
    intrinsicName: "%JSON%",
    globalNameOrSource: "JSON"
  },
  {
    intrinsicName: "%JSONParse%",
    globalNameOrSource: "JSON.parse"
  },
  {
    intrinsicName: "%Map%",
    globalNameOrSource: "Map"
  },
  {
    intrinsicName: "%MapIteratorPrototype%",
    globalNameOrSource: "Object.getPrototypeOf(new Map()[Symbol.iterator]())"
  },
  {
    intrinsicName: "%MapPrototype%",
    globalNameOrSource: "Map.prototype"
  },
  {
    intrinsicName: "%Math%",
    globalNameOrSource: "Math"
  },
  {
    intrinsicName: "%Number%",
    globalNameOrSource: "Number"
  },
  {
    intrinsicName: "%NumberPrototype%",
    globalNameOrSource: "Number.prototype"
  },
  {
    intrinsicName: "%Object%",
    globalNameOrSource: "Object"
  },
  {
    intrinsicName: "%ObjectPrototype%",
    globalNameOrSource: "Object.prototype"
  },
  {
    intrinsicName: "%ObjProto_toString%",
    globalNameOrSource: "Object.prototype.toString"
  },
  {
    intrinsicName: "%ObjProto_valueOf%",
    globalNameOrSource: "Object.prototype.valueOf"
  },
  {
    intrinsicName: "%parseFloat%",
    globalNameOrSource: "parseFloat"
  },
  {
    intrinsicName: "%parseInt%",
    globalNameOrSource: "parseInt"
  },
  {
    intrinsicName: "%Promise%",
    globalNameOrSource: "Promise"
  },
  {
    intrinsicName: "%PromisePrototype%",
    globalNameOrSource: "Promise.prototype"
  },
  {
    intrinsicName: "%PromiseProto_then%",
    globalNameOrSource: "Promise.prototype.then"
  },
  {
    intrinsicName: "%Promise_all%",
    globalNameOrSource: "Promise.all"
  },
  {
    intrinsicName: "%Promise_reject%",
    globalNameOrSource: "Promise.reject"
  },
  {
    intrinsicName: "%Promise_resolve%",
    globalNameOrSource: "Promise.resolve"
  },
  {
    intrinsicName: "%Proxy%",
    globalNameOrSource: "Proxy"
  },
  {
    intrinsicName: "%RangeError%",
    globalNameOrSource: "RangeError"
  },
  {
    intrinsicName: "%RangeErrorPrototype%",
    globalNameOrSource: "RangeError.prototype"
  },
  {
    intrinsicName: "%ReferenceError%",
    globalNameOrSource: "ReferenceError"
  },
  {
    intrinsicName: "%ReferenceErrorPrototype%",
    globalNameOrSource: "ReferenceError.prototype"
  },
  {
    intrinsicName: "%Reflect%",
    globalNameOrSource: "Reflect"
  },
  {
    intrinsicName: "%RegExp%",
    globalNameOrSource: "RegExp"
  },
  {
    intrinsicName: "%RegExpPrototype%",
    globalNameOrSource: "RegExp.prototype"
  },
  {
    intrinsicName: "%Set%",
    globalNameOrSource: "Set"
  },
  {
    intrinsicName: "%SetIteratorPrototype%",
    globalNameOrSource: "Object.getPrototypeOf(new Set()[Symbol.iterator]())"
  },
  {
    intrinsicName: "%SetPrototype%",
    globalNameOrSource: "Set.prototype"
  },
  {
    intrinsicName: "%SharedArrayBuffer%",
    globalNameOrSource: "SharedArrayBuffer"
  },
  {
    intrinsicName: "%SharedArrayBufferPrototype%",
    globalNameOrSource: "SharedArrayBuffer.prototype"
  },
  {
    intrinsicName: "%String%",
    globalNameOrSource: "String"
  },
  {
    intrinsicName: "%StringIteratorPrototype%",
    globalNameOrSource: "Object.getPrototypeOf(new String()[Symbol.iterator]())"
  },
  {
    intrinsicName: "%StringPrototype%",
    globalNameOrSource: "String.prototype"
  },
  {
    intrinsicName: "%Symbol%",
    globalNameOrSource: "Symbol"
  },
  {
    intrinsicName: "%SymbolPrototype%",
    globalNameOrSource: "Symbol.prototype"
  },
  {
    intrinsicName: "%SyntaxError%",
    globalNameOrSource: "SyntaxError"
  },
  {
    intrinsicName: "%SyntaxErrorPrototype%",
    globalNameOrSource: "SyntaxError.prototype"
  },
  {
    intrinsicName: "%ThrowTypeError%",
    globalNameOrSource: "(function() { 'use strict'; return Object.getOwnPropertyDescriptor(arguments, 'callee').get })()"
  },
  {
    intrinsicName: "%TypedArray%",
    globalNameOrSource: "Object.getPrototypeOf(Uint8Array)"
  },
  {
    intrinsicName: "%TypedArrayPrototype%",
    globalNameOrSource: "Object.getPrototypeOf(Uint8Array).prototype"
  },
  {
    intrinsicName: "%TypeError%",
    globalNameOrSource: "TypeError"
  },
  {
    intrinsicName: "%TypeErrorPrototype%",
    globalNameOrSource: "TypeError.prototype"
  },
  {
    intrinsicName: "%Uint8Array%",
    globalNameOrSource: "Uint8Array"
  },
  {
    intrinsicName: "%Uint8ArrayPrototype%",
    globalNameOrSource: "Uint8Array.prototype"
  },
  {
    intrinsicName: "%Uint8ClampedArray%",
    globalNameOrSource: "Uint8ClampedArray"
  },
  {
    intrinsicName: "%Uint8ClampedArrayPrototype%",
    globalNameOrSource: "Uint8ClampedArray.prototype"
  },
  {
    intrinsicName: "%Uint16Array%",
    globalNameOrSource: "Uint16Array"
  },
  {
    intrinsicName: "%Uint16ArrayPrototype%",
    globalNameOrSource: "Uint16Array.prototype"
  },
  {
    intrinsicName: "%Uint32Array%",
    globalNameOrSource: "Uint32Array"
  },
  {
    intrinsicName: "%Uint32ArrayPrototype%",
    globalNameOrSource: "Uint32Array.prototype"
  },
  {
    intrinsicName: "%URIError%",
    globalNameOrSource: "URIError"
  },
  {
    intrinsicName: "%URIErrorPrototype%",
    globalNameOrSource: "URIError.prototype"
  },
  {
    intrinsicName: "%WeakMap%",
    globalNameOrSource: "WeakMap"
  },
  {
    intrinsicName: "%WeakMapPrototype%",
    globalNameOrSource: "WeakMap.prototype"
  },
  {
    intrinsicName: "%WeakSet%",
    globalNameOrSource: "WeakSet"
  },
  {
    intrinsicName: "%WeakSetPrototype%",
    globalNameOrSource: "WeakSet.prototype"
  }
];


WellKnownIntrinsicObjects.forEach(wkio => {
  var actual;

  try {
    actual = new Function("return " + wkio.globalNameOrSource)();
  } catch (exception) {
    // Nothing to do here.
  }

  wkio.reference = actual;
});
