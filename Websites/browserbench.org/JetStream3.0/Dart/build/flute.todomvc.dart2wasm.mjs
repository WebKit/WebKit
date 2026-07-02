// Compiles a dart2wasm-generated main module from `source` which can then
// instantiatable via the `instantiate` method.
//
// `source` needs to be a `Response` object (or promise thereof) e.g. created
// via the `fetch()` JS API.
export async function compileStreaming(source) {
  const builtins = {builtins: ['js-string']};
  return new CompiledApp(
      await WebAssembly.compileStreaming(source, builtins), builtins);
}

// Compiles a dart2wasm-generated wasm modules from `bytes` which is then
// instantiatable via the `instantiate` method.
export async function compile(bytes) {
  const builtins = {builtins: ['js-string']};
  return new CompiledApp(await WebAssembly.compile(bytes, builtins), builtins);
}

// DEPRECATED: Please use `compile` or `compileStreaming` to get a compiled app,
// use `instantiate` method to get an instantiated app and then call
// `invokeMain` to invoke the main function.
export async function instantiate(modulePromise, importObjectPromise) {
  var moduleOrCompiledApp = await modulePromise;
  if (!(moduleOrCompiledApp instanceof CompiledApp)) {
    moduleOrCompiledApp = new CompiledApp(moduleOrCompiledApp);
  }
  const instantiatedApp = await moduleOrCompiledApp.instantiate(await importObjectPromise);
  return instantiatedApp.instantiatedModule;
}

// DEPRECATED: Please use `compile` or `compileStreaming` to get a compiled app,
// use `instantiate` method to get an instantiated app and then call
// `invokeMain` to invoke the main function.
export const invoke = (moduleInstance, ...args) => {
  moduleInstance.exports.$invokeMain(args);
}

class CompiledApp {
  constructor(module, builtins) {
    this.module = module;
    this.builtins = builtins;
  }

  // The second argument is an options object containing:
  // `loadDeferredWasm` is a JS function that takes a module name matching a
  //   wasm file produced by the dart2wasm compiler and returns the bytes to
  //   load the module. These bytes can be in either a format supported by
  //   `WebAssembly.compile` or `WebAssembly.compileStreaming`.
  // `loadDynamicModule` is a JS function that takes two string names matching,
  //   in order, a wasm file produced by the dart2wasm compiler during dynamic
  //   module compilation and a corresponding js file produced by the same
  //   compilation. It should return a JS Array containing 2 elements. The first
  //   should be the bytes for the wasm module in a format supported by
  //   `WebAssembly.compile` or `WebAssembly.compileStreaming`. The second
  //   should be the result of using the JS 'import' API on the js file path.
  async instantiate(additionalImports, {loadDeferredWasm, loadDynamicModule} = {}) {
    let dartInstance;

    // Prints to the console
    function printToConsole(value) {
      if (typeof dartPrint == "function") {
        dartPrint(value);
        return;
      }
      if (typeof console == "object" && typeof console.log != "undefined") {
        console.log(value);
        return;
      }
      if (typeof print == "function") {
        print(value);
        return;
      }

      throw "Unable to print message: " + value;
    }

    // A special symbol attached to functions that wrap Dart functions.
    const jsWrappedDartFunctionSymbol = Symbol("JSWrappedDartFunction");

    function finalizeWrapper(dartFunction, wrapped) {
      wrapped.dartFunction = dartFunction;
      wrapped[jsWrappedDartFunctionSymbol] = true;
      return wrapped;
    }

    // Imports
    const dart2wasm = {
            _6: (o,s,v) => o[s] = v,
      _39: x0 => x0.length,
      _41: (x0,x1) => x0[x1],
      _69: () => Symbol("jsBoxedDartObjectProperty"),
      _70: (decoder, codeUnits) => decoder.decode(codeUnits),
      _71: () => new TextDecoder("utf-8", {fatal: true}),
      _72: () => new TextDecoder("utf-8", {fatal: false}),
      _73: (s) => +s,
      _74: Date.now,
      _76: s => new Date(s * 1000).getTimezoneOffset() * 60,
      _77: s => {
        if (!/^\s*[+-]?(?:Infinity|NaN|(?:\.\d+|\d+(?:\.\d*)?)(?:[eE][+-]?\d+)?)\s*$/.test(s)) {
          return NaN;
        }
        return parseFloat(s);
      },
      _78: () => {
        let stackString = new Error().stack.toString();
        let frames = stackString.split('\n');
        let drop = 2;
        if (frames[0] === 'Error') {
            drop += 1;
        }
        return frames.slice(drop).join('\n');
      },
      _79: () => typeof dartUseDateNowForTicks !== "undefined",
      _80: () => 1000 * performance.now(),
      _81: () => Date.now(),
      _84: () => new WeakMap(),
      _85: (map, o) => map.get(o),
      _86: (map, o, v) => map.set(o, v),
      _99: s => JSON.stringify(s),
      _100: s => printToConsole(s),
      _101: (o, p, r) => o.replaceAll(p, () => r),
      _103: Function.prototype.call.bind(String.prototype.toLowerCase),
      _104: s => s.toUpperCase(),
      _105: s => s.trim(),
      _106: s => s.trimLeft(),
      _107: s => s.trimRight(),
      _108: (string, times) => string.repeat(times),
      _109: Function.prototype.call.bind(String.prototype.indexOf),
      _110: (s, p, i) => s.lastIndexOf(p, i),
      _111: (string, token) => string.split(token),
      _112: Object.is,
      _113: o => o instanceof Array,
      _118: a => a.pop(),
      _119: (a, i) => a.splice(i, 1),
      _120: (a, s) => a.join(s),
      _121: (a, s, e) => a.slice(s, e),
      _124: a => a.length,
      _126: (a, i) => a[i],
      _127: (a, i, v) => a[i] = v,
      _130: (o, offsetInBytes, lengthInBytes) => {
        var dst = new ArrayBuffer(lengthInBytes);
        new Uint8Array(dst).set(new Uint8Array(o, offsetInBytes, lengthInBytes));
        return new DataView(dst);
      },
      _132: o => o instanceof Uint8Array,
      _133: (o, start, length) => new Uint8Array(o.buffer, o.byteOffset + start, length),
      _135: (o, start, length) => new Int8Array(o.buffer, o.byteOffset + start, length),
      _137: (o, start, length) => new Uint8ClampedArray(o.buffer, o.byteOffset + start, length),
      _139: (o, start, length) => new Uint16Array(o.buffer, o.byteOffset + start, length),
      _141: (o, start, length) => new Int16Array(o.buffer, o.byteOffset + start, length),
      _143: (o, start, length) => new Uint32Array(o.buffer, o.byteOffset + start, length),
      _145: (o, start, length) => new Int32Array(o.buffer, o.byteOffset + start, length),
      _147: (o, start, length) => new BigInt64Array(o.buffer, o.byteOffset + start, length),
      _149: (o, start, length) => new Float32Array(o.buffer, o.byteOffset + start, length),
      _151: (o, start, length) => new Float64Array(o.buffer, o.byteOffset + start, length),
      _152: (t, s) => t.set(s),
      _154: (o) => new DataView(o.buffer, o.byteOffset, o.byteLength),
      _156: o => o.buffer,
      _157: o => o.byteOffset,
      _158: Function.prototype.call.bind(Object.getOwnPropertyDescriptor(DataView.prototype, 'byteLength').get),
      _159: (b, o) => new DataView(b, o),
      _160: (b, o, l) => new DataView(b, o, l),
      _161: Function.prototype.call.bind(DataView.prototype.getUint8),
      _162: Function.prototype.call.bind(DataView.prototype.setUint8),
      _163: Function.prototype.call.bind(DataView.prototype.getInt8),
      _164: Function.prototype.call.bind(DataView.prototype.setInt8),
      _165: Function.prototype.call.bind(DataView.prototype.getUint16),
      _166: Function.prototype.call.bind(DataView.prototype.setUint16),
      _167: Function.prototype.call.bind(DataView.prototype.getInt16),
      _168: Function.prototype.call.bind(DataView.prototype.setInt16),
      _169: Function.prototype.call.bind(DataView.prototype.getUint32),
      _170: Function.prototype.call.bind(DataView.prototype.setUint32),
      _171: Function.prototype.call.bind(DataView.prototype.getInt32),
      _172: Function.prototype.call.bind(DataView.prototype.setInt32),
      _175: Function.prototype.call.bind(DataView.prototype.getBigInt64),
      _176: Function.prototype.call.bind(DataView.prototype.setBigInt64),
      _177: Function.prototype.call.bind(DataView.prototype.getFloat32),
      _178: Function.prototype.call.bind(DataView.prototype.setFloat32),
      _179: Function.prototype.call.bind(DataView.prototype.getFloat64),
      _180: Function.prototype.call.bind(DataView.prototype.setFloat64),
      _193: (ms, c) =>
      setTimeout(() => dartInstance.exports.$invokeCallback(c),ms),
      _194: (handle) => clearTimeout(handle),
      _195: (ms, c) =>
      setInterval(() => dartInstance.exports.$invokeCallback(c), ms),
      _196: (handle) => clearInterval(handle),
      _197: (c) =>
      queueMicrotask(() => dartInstance.exports.$invokeCallback(c)),
      _198: () => Date.now(),
      _232: (x0,x1) => x0.matchMedia(x1),
      _233: (s, m) => {
        try {
          return new RegExp(s, m);
        } catch (e) {
          return String(e);
        }
      },
      _234: (x0,x1) => x0.exec(x1),
      _235: (x0,x1) => x0.test(x1),
      _236: x0 => x0.pop(),
      _238: o => o === undefined,
      _240: o => typeof o === 'function' && o[jsWrappedDartFunctionSymbol] === true,
      _243: o => o instanceof RegExp,
      _244: (l, r) => l === r,
      _245: o => o,
      _246: o => o,
      _247: o => o,
      _249: o => o.length,
      _251: (o, i) => o[i],
      _252: f => f.dartFunction,
      _253: () => ({}),
      _263: o => String(o),
      _265: o => {
        if (o === undefined) return 1;
        var type = typeof o;
        if (type === 'boolean') return 2;
        if (type === 'number') return 3;
        if (type === 'string') return 4;
        if (o instanceof Array) return 5;
        if (ArrayBuffer.isView(o)) {
          if (o instanceof Int8Array) return 6;
          if (o instanceof Uint8Array) return 7;
          if (o instanceof Uint8ClampedArray) return 8;
          if (o instanceof Int16Array) return 9;
          if (o instanceof Uint16Array) return 10;
          if (o instanceof Int32Array) return 11;
          if (o instanceof Uint32Array) return 12;
          if (o instanceof Float32Array) return 13;
          if (o instanceof Float64Array) return 14;
          if (o instanceof DataView) return 15;
        }
        if (o instanceof ArrayBuffer) return 16;
        // Feature check for `SharedArrayBuffer` before doing a type-check.
        if (globalThis.SharedArrayBuffer !== undefined &&
            o instanceof SharedArrayBuffer) {
            return 17;
        }
        return 18;
      },
      _271: (jsArray, jsArrayOffset, wasmArray, wasmArrayOffset, length) => {
        const setValue = dartInstance.exports.$wasmI8ArraySet;
        for (let i = 0; i < length; i++) {
          setValue(wasmArray, wasmArrayOffset + i, jsArray[jsArrayOffset + i]);
        }
      },
      _275: (jsArray, jsArrayOffset, wasmArray, wasmArrayOffset, length) => {
        const setValue = dartInstance.exports.$wasmI32ArraySet;
        for (let i = 0; i < length; i++) {
          setValue(wasmArray, wasmArrayOffset + i, jsArray[jsArrayOffset + i]);
        }
      },
      _277: (jsArray, jsArrayOffset, wasmArray, wasmArrayOffset, length) => {
        const setValue = dartInstance.exports.$wasmF32ArraySet;
        for (let i = 0; i < length; i++) {
          setValue(wasmArray, wasmArrayOffset + i, jsArray[jsArrayOffset + i]);
        }
      },
      _279: (jsArray, jsArrayOffset, wasmArray, wasmArrayOffset, length) => {
        const setValue = dartInstance.exports.$wasmF64ArraySet;
        for (let i = 0; i < length; i++) {
          setValue(wasmArray, wasmArrayOffset + i, jsArray[jsArrayOffset + i]);
        }
      },
      _283: x0 => x0.index,
      _285: x0 => x0.flags,
      _286: x0 => x0.multiline,
      _287: x0 => x0.ignoreCase,
      _288: x0 => x0.unicode,
      _289: x0 => x0.dotAll,
      _290: (x0,x1) => { x0.lastIndex = x1 },
      _295: x0 => x0.random(),
      _298: () => globalThis.Math,
      _299: Function.prototype.call.bind(Number.prototype.toString),
      _300: Function.prototype.call.bind(BigInt.prototype.toString),
      _301: Function.prototype.call.bind(Number.prototype.toString),
      _302: (d, digits) => d.toFixed(digits),
      _2062: () => globalThis.window,
      _8706: x0 => x0.matches,
      _12689: x0 => { globalThis.window.flutterCanvasKit = x0 },

    };

    const baseImports = {
      dart2wasm: dart2wasm,
      Math: Math,
      Date: Date,
      Object: Object,
      Array: Array,
      Reflect: Reflect,
      S: new Proxy({}, { get(_, prop) { return prop; } }),

    };

    const jsStringPolyfill = {
      "charCodeAt": (s, i) => s.charCodeAt(i),
      "compare": (s1, s2) => {
        if (s1 < s2) return -1;
        if (s1 > s2) return 1;
        return 0;
      },
      "concat": (s1, s2) => s1 + s2,
      "equals": (s1, s2) => s1 === s2,
      "fromCharCode": (i) => String.fromCharCode(i),
      "length": (s) => s.length,
      "substring": (s, a, b) => s.substring(a, b),
      "fromCharCodeArray": (a, start, end) => {
        if (end <= start) return '';

        const read = dartInstance.exports.$wasmI16ArrayGet;
        let result = '';
        let index = start;
        const chunkLength = Math.min(end - index, 500);
        let array = new Array(chunkLength);
        while (index < end) {
          const newChunkLength = Math.min(end - index, 500);
          for (let i = 0; i < newChunkLength; i++) {
            array[i] = read(a, index++);
          }
          if (newChunkLength < chunkLength) {
            array = array.slice(0, newChunkLength);
          }
          result += String.fromCharCode(...array);
        }
        return result;
      },
      "intoCharCodeArray": (s, a, start) => {
        if (s === '') return 0;

        const write = dartInstance.exports.$wasmI16ArraySet;
        for (var i = 0; i < s.length; ++i) {
          write(a, start++, s.charCodeAt(i));
        }
        return s.length;
      },
      "test": (s) => typeof s == "string",
    };


    

    dartInstance = await WebAssembly.instantiate(this.module, {
      ...baseImports,
      ...additionalImports,
      
      "wasm:js-string": jsStringPolyfill,
    });

    return new InstantiatedApp(this, dartInstance);
  }
}

class InstantiatedApp {
  constructor(compiledApp, instantiatedModule) {
    this.compiledApp = compiledApp;
    this.instantiatedModule = instantiatedModule;
  }

  // Call the main function with the given arguments.
  invokeMain(...args) {
    this.instantiatedModule.exports.$invokeMain(args);
  }
}
