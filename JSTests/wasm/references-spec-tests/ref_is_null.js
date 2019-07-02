//@ runWebAssemblySuite("--useWebAssemblyReferences=true")
'use strict';
let console = { log: print }
let hostrefs = {};
let hostsym = Symbol("hostref");
function hostref(s) {
  if (! (s in hostrefs)) hostrefs[s] = {[hostsym]: s};
  return hostrefs[s];
}
function is_hostref(x) {
  return (x !== null && hostsym in x) ? 1 : 0;
}
function is_funcref(x) {
  return typeof x === "function" ? 1 : 0;
}
function eq_ref(x, y) {
  return x === y ? 1 : 0;
}

let spectest = {
  hostref: hostref,
  is_hostref: is_hostref,
  is_funcref: is_funcref,
  eq_ref: eq_ref,
  print: console.log.bind(console),
  print_i32: console.log.bind(console),
  print_i32_f32: console.log.bind(console),
  print_f64_f64: console.log.bind(console),
  print_f32: console.log.bind(console),
  print_f64: console.log.bind(console),
  global_i32: 666,
  global_f32: 666,
  global_f64: 666,
  table: new WebAssembly.Table({initial: 10, maximum: 20, element: 'anyfunc'}),
  memory: new WebAssembly.Memory({initial: 1, maximum: 2})
};

let handler = {
  get(target, prop) {
    return (prop in target) ?  target[prop] : {};
  }
};
let registry = new Proxy({spectest}, handler);

function register(name, instance) {
  registry[name] = instance.exports;
}

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  let validated;
  try {
    validated = WebAssembly.validate(buffer);
  } catch (e) {
    throw new Error("Wasm validate throws");
  }
  if (validated !== valid) {
    throw new Error("Wasm validate failure" + (valid ? "" : " expected"));
  }
  return new WebAssembly.Module(buffer);
}

function instance(bytes, imports = registry) {
  return new WebAssembly.Instance(module(bytes), imports);
}

function call(instance, name, args) {
  return instance.exports[name](...args);
}

function get(instance, name) {
  let v = instance.exports[name];
  return (v instanceof WebAssembly.Global) ? v.value : v;
}

function exports(instance) {
  return {module: instance.exports, spectest: spectest};
}

function run(action) {
  action();
}

function assert_malformed(bytes) {
  try { module(bytes, false) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm decoding failure expected");
}

function assert_invalid(bytes) {
  try { module(bytes, false) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm validation failure expected");
}

function assert_unlinkable(bytes) {
  let mod = module(bytes);
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof WebAssembly.LinkError) return;
  }
  throw new Error("Wasm linking failure expected");
}

function assert_uninstantiable(bytes) {
  let mod = module(bytes);
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return;
  }
  throw new Error("Wasm trap expected");
}

function assert_trap(action) {
  try { action() } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return;
  }
  throw new Error("Wasm trap expected");
}

let StackOverflow;
try { (function f() { 1 + f() })() } catch (e) { StackOverflow = e.constructor }

function assert_exhaustion(action) {
  try { action() } catch (e) {
    if (e instanceof StackOverflow) return;
  }
  throw new Error("Wasm resource exhaustion expected");
}

function assert_return(action, expected) {
  let actual = action();
  if (!Object.is(actual, expected)) {
    throw new Error("Wasm return value " + expected + " expected, got " + actual);
  };
}

function assert_return_canonical_nan(action) {
  let actual = action();
  // Note that JS can't reliably distinguish different NaN values,
  // so there's no good way to test that it's a canonical NaN.
  if (!Number.isNaN(actual)) {
    throw new Error("Wasm return value NaN expected, got " + actual);
  };
}

function assert_return_arithmetic_nan(action) {
  // Note that JS can't reliably distinguish different NaN values,
  // so there's no good way to test for specific bitpatterns here.
  let actual = action();
  if (!Number.isNaN(actual)) {
    throw new Error("Wasm return value NaN expected, got " + actual);
  };
}

function assert_return_ref(action) {
  let actual = action();
  if (actual === null || typeof actual !== "object" && typeof actual !== "function") {
    throw new Error("Wasm reference return value expected, got " + actual);
  };
}

function assert_return_func(action) {
  let actual = action();
  if (typeof actual !== "function") {
    throw new Error("Wasm function return value expected, got " + actual);
  };
}

// ref_is_null.wast:1
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x97\x80\x80\x80\x00\x05\x60\x01\x6f\x01\x7f\x60\x01\x70\x01\x7f\x60\x00\x00\x60\x01\x6f\x00\x60\x01\x7f\x01\x7f\x03\x88\x80\x80\x80\x00\x07\x00\x01\x02\x03\x02\x04\x04\x04\x87\x80\x80\x80\x00\x02\x6f\x00\x02\x70\x00\x02\x07\xc1\x80\x80\x80\x00\x06\x06\x61\x6e\x79\x72\x65\x66\x00\x00\x07\x66\x75\x6e\x63\x72\x65\x66\x00\x01\x04\x69\x6e\x69\x74\x00\x03\x06\x64\x65\x69\x6e\x69\x74\x00\x04\x0b\x61\x6e\x79\x72\x65\x66\x2d\x65\x6c\x65\x6d\x00\x05\x0c\x66\x75\x6e\x63\x72\x65\x66\x2d\x65\x6c\x65\x6d\x00\x06\x09\x88\x80\x80\x80\x00\x01\x02\x01\x41\x01\x0b\x01\x02\x0a\xd4\x80\x80\x80\x00\x07\x85\x80\x80\x80\x00\x00\x20\x00\xd1\x0b\x85\x80\x80\x80\x00\x00\x20\x00\xd1\x0b\x82\x80\x80\x80\x00\x00\x0b\x88\x80\x80\x80\x00\x00\x41\x01\x20\x00\x26\x00\x0b\x8c\x80\x80\x80\x00\x00\x41\x01\xd0\x26\x00\x41\x01\xd0\x26\x01\x0b\x88\x80\x80\x80\x00\x00\x20\x00\x25\x00\x10\x00\x0b\x88\x80\x80\x80\x00\x00\x20\x00\x25\x01\x10\x01\x0b");

// ref_is_null.wast:29
assert_return(() => call($1, "anyref", [null]), 1);

// ref_is_null.wast:30
assert_return(() => call($1, "funcref", [null]), 1);

// ref_is_null.wast:32
assert_return(() => call($1, "anyref", [hostref(1)]), 0);

// ref_is_null.wast:34
run(() => call($1, "init", [hostref(0)]));

// ref_is_null.wast:36
assert_return(() => call($1, "anyref-elem", [0]), 1);

// ref_is_null.wast:37
assert_return(() => call($1, "funcref-elem", [0]), 1);

// ref_is_null.wast:39
assert_return(() => call($1, "anyref-elem", [1]), 0);

// ref_is_null.wast:40
assert_return(() => call($1, "funcref-elem", [1]), 0);

// ref_is_null.wast:42
run(() => call($1, "deinit", []));

// ref_is_null.wast:44
assert_return(() => call($1, "anyref-elem", [0]), 1);

// ref_is_null.wast:45
assert_return(() => call($1, "funcref-elem", [0]), 1);

// ref_is_null.wast:47
assert_return(() => call($1, "anyref-elem", [1]), 1);

// ref_is_null.wast:48
assert_return(() => call($1, "funcref-elem", [1]), 1);
