
'use strict';

console = { log : print };

let hostrefs = {};
let hostsym = Symbol("hostref");
function hostref(s) {
  if (! (s in hostrefs)) hostrefs[s] = {[hostsym]: s};
  return hostrefs[s];
}
function eq_ref(x, y) {
  return x === y ? 1 : 0;
}

let spectest = {
  hostref: hostref,
  eq_ref: eq_ref,
  print: console.log.bind(console),
  print_i32: console.log.bind(console),
  print_i64: console.log.bind(console),
  print_i32_f32: console.log.bind(console),
  print_f64_f64: console.log.bind(console),
  print_f32: console.log.bind(console),
  print_f64: console.log.bind(console),
  global_i32: 666,
  global_i64: 666n,
  global_f32: 666.6,
  global_f64: 666.6,
  table: new WebAssembly.Table({initial: 10, maximum: 20, element: 'anyfunc'}),
  memory: new WebAssembly.Memory({initial: 1, maximum: 2}),
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

function instance(mod, imports = registry) {
  return new WebAssembly.Instance(mod, imports);
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

function assert_malformed_custom(bytes) {
  return;
}

function assert_invalid(bytes) {
  try { module(bytes, false) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm validation failure expected");
}

function assert_invalid_custom(bytes) {
  return;
}

function assert_unlinkable(mod) {
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof WebAssembly.LinkError) return;
  }
  throw new Error("Wasm linking failure expected");
}

function assert_uninstantiable(mod) {
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

function assert_exception(action) {
  try { action() } catch (e) { return; }
  throw new Error("exception expected");
}

let StackOverflow;
try { (function f() { 1 + f() })() } catch (e) { StackOverflow = e.constructor }

function assert_exhaustion(action) {
  try { action() } catch (e) {
    if (e instanceof StackOverflow) return;
  }
  throw new Error("Wasm resource exhaustion expected");
}

function assert_return(action, ...expected) {
  let actual = action();
  if (actual === undefined) {
    actual = [];
  } else if (!Array.isArray(actual)) {
    actual = [actual];
  }
  if (actual.length !== expected.length) {
    throw new Error(expected.length + " value(s) expected, got " + actual.length);
  }
  for (let i = 0; i < actual.length; ++i) {
    switch (expected[i]) {
      case "nan:canonical":
      case "nan:arithmetic":
      case "nan:any":
        // Note that JS can't reliably distinguish different NaN values,
        // so there's no good way to test that it's a canonical NaN.
        if (!Number.isNaN(actual[i])) {
          throw new Error("Wasm NaN return value expected, got " + actual[i]);
        };
        return;
      case "ref.i31":
        if (typeof actual[i] !== "number" || (actual[i] & 0x7fffffff) !== actual[i]) {
          throw new Error("Wasm i31 return value expected, got " + actual[i]);
        };
        return;
      case "ref.any":
      case "ref.eq":
      case "ref.struct":
      case "ref.array":
        // For now, JS can't distinguish exported Wasm GC values,
        // so we only test for object.
        if (typeof actual[i] !== "object") {
          throw new Error("Wasm object return value expected, got " + actual[i]);
        };
        return;
      case "ref.func":
        if (typeof actual[i] !== "function") {
          throw new Error("Wasm function return value expected, got " + actual[i]);
        };
        return;
      case "ref.extern":
        if (actual[i] === null) {
          throw new Error("Wasm reference return value expected, got " + actual[i]);
        };
        return;
      case "ref.null":
        if (actual[i] !== null) {
          throw new Error("Wasm null return value expected, got " + actual[i]);
        };
        return;
      default:
        if (!Object.is(actual[i], expected[i])) {
          throw new Error("Wasm return value " + expected[i] + " expected, got " + actual[i]);
        };
    }
  }
}

/*
(module
  (type $a (array (mut i32)))
  (type $b (array (mut i31ref)))
  (type $c (array i16))
  (type $s (struct (field i16)))
  (type $t (struct (field i32)))
  (elem $e i31ref (ref.i31 (i32.const 1)))
  (data $d)

  (func (param $e externref)
    (return)
    (i32.const 5) (struct.new $s)
    (i32.const 5) (struct.new $s)
    (ref.eq)
    (drop)

    (i32.const 5) (struct.new $t)
    (struct.get $t 0) (drop)
    (i32.const 5) (struct.new $s)
    (struct.get_s $s 0) (drop)
    (struct.new_default $s)
    (struct.get_u $s 0) (drop)

    (i32.const 1) (i32.const 1) (array.new $a)
    (i32.const 0) (array.get $a)
    (i32.const 1) (array.new_default $c)
    (i32.const 0) (array.get_s $c)
    (array.new_data $c $d)
    (i32.const 0) (array.get_u $c)
    (array.new_elem $b $e)
    (i32.const 0) (ref.i31 (i32.const 0)) (array.set $b)
    (array.new_fixed $a 0) (array.len) (drop)

    (i32.const 1) (i32.const 1) (array.new $a)
    (i32.const 0) (i32.const 0) (i32.const 1) (array.fill $a)

    (i32.const 1) (i32.const 1) (array.new $a)
    (i32.const 0)
    (i32.const 1) (i32.const 1) (array.new $a)
    (i32.const 0) (i32.const 1) (array.copy $a $a)

    (ref.i31 (i32.const 1)) (i32.const 1) (array.new $b)
    (i32.const 0) (i32.const 0) (i32.const 1) (array.init_elem $b $e)

    (i32.const 1) (i32.const 1) (array.new $a)
    (i32.const 0) (i32.const 0) (i32.const 1) (array.init_data $a $d)

    (local.get $e) (ref.test externref) (drop)
    (local.get $e) (ref.cast externref) (drop)

    (local.get $e)
    (any.convert_extern)
    (extern.convert_any)
    (drop)
    (block (result externref)
        (local.get $e)
        (br_on_cast 0 externref externref)
    )
    (drop)
    (block (result externref)
        (local.get $e)
        (br_on_cast_fail 0 externref externref)
    )
    (ref.i31 (i32.const 5)) (i31.get_s) (drop)
    (ref.i31 (i32.const 5)) (i31.get_u) (drop)
    (drop)
  )
)
*/

// .:0
let $$1 = module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x96\x80\x80\x80\x00\x06\x5e\x7f\x01\x5e\x6c\x01\x5e\x77\x00\x5f\x01\x77\x00\x5f\x01\x7f\x00\x60\x01\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x05\x09\x89\x80\x80\x80\x00\x01\x05\x6c\x01\x41\x01\xfb\x1c\x0b\x0c\x81\x80\x80\x80\x00\x01\x0a\xe9\x81\x80\x80\x00\x01\xe3\x81\x80\x80\x00\x00\x0f\x41\x05\xfb\x00\x03\x41\x05\xfb\x00\x03\xd3\x1a\x41\x05\xfb\x00\x04\xfb\x02\x04\x00\x1a\x41\x05\xfb\x00\x03\xfb\x03\x03\x00\x1a\xfb\x01\x03\xfb\x04\x03\x00\x1a\x41\x01\x41\x01\xfb\x06\x00\x41\x00\xfb\x0b\x00\x41\x01\xfb\x07\x02\x41\x00\xfb\x0c\x02\xfb\x09\x02\x00\x41\x00\xfb\x0d\x02\xfb\x0a\x01\x00\x41\x00\x41\x00\xfb\x1c\xfb\x0e\x01\xfb\x08\x00\x00\xfb\x0f\x1a\x41\x01\x41\x01\xfb\x06\x00\x41\x00\x41\x00\x41\x01\xfb\x10\x00\x41\x01\x41\x01\xfb\x06\x00\x41\x00\x41\x01\x41\x01\xfb\x06\x00\x41\x00\x41\x01\xfb\x11\x00\x00\x41\x01\xfb\x1c\x41\x01\xfb\x06\x01\x41\x00\x41\x00\x41\x01\xfb\x13\x01\x00\x41\x01\x41\x01\xfb\x06\x00\x41\x00\x41\x00\x41\x01\xfb\x12\x00\x00\x20\x00\xfb\x15\x6f\x1a\x20\x00\xfb\x17\x6f\x1a\x20\x00\xfb\x1a\xfb\x1b\x1a\x02\x6f\x20\x00\xfb\x18\x03\x00\x6f\x6f\x0b\x1a\x02\x6f\x20\x00\xfb\x19\x03\x00\x6f\x6f\x0b\x41\x05\xfb\x1c\xfb\x1d\x1a\x41\x05\xfb\x1c\xfb\x1e\x1a\x1a\x0b\x0b\x83\x80\x80\x80\x00\x01\x01\x00");
