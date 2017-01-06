/* Copied from *
 * https://github.com/WebAssembly/spec/blob/master/interpreter/host/js.ml */
'use strict';

let soft_validate = true;

let spectest = {
  print: print || ((...xs) => console.log(...xs)),
  global: 666,
  table: new WebAssembly.Table({initial: 10, maximum: 20, element: 'anyfunc'}),
  memory: new WebAssembly.Memory({initial: 1, maximum: 2}),};

let registry = {spectest};
let $$;

function register(name, instance) {
  registry[name] = instance.exports;
}

function module(bytes) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

function instance(bytes, imports = registry) {
  return new WebAssembly.Instance(module(bytes), imports);
}

function assert_malformed(bytes) {
  try { module(bytes) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm decoding failure expected");
}

function assert_invalid(bytes) {
  try { module(bytes) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm validation failure expected");
}

function assert_soft_invalid(bytes) {
  try { module(bytes) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
    throw new Error("Wasm validation failure expected");
  }
  if (soft_validate)
    throw new Error("Wasm validation failure expected");
}

function assert_unlinkable(bytes) {
  let mod = module(bytes);
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof TypeError) return;
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

function assert_return(action, expected) {
  let actual = action();
  if (!Object.is(actual, expected)) {
    throw new Error("Wasm return value " + expected + " expected, got " + actual);
  };
}

function assert_return_nan(action) {
  let actual = action();
  if (!Number.isNaN(actual)) {
    throw new Error("Wasm return value NaN expected, got " + actual);
  };
}

let f32 = Math.fround;

$$ = instance("\x00\x61\x73\x6d\x0d\x00\x00\x00");
$$ = instance("\x00\x61\x73\x6d\x0d\x00\x00\x00");
$$ = instance("\x00\x61\x73\x6d\x0d\x00\x00\x00\x01\x07\x01\x60\x02\x7f\x7f\x01\x7f\x03\x02\x01\x00\x07\x0a\x01\x06\x61\x64\x64\x54\x77\x6f\x00\x00\x0a\x09\x01\x07\x00\x20\x00\x20\x01\x6a\x0b");
assert_malformed("\x00\x61\x73\x6d\x0d\x00\x00\x00\x00\x00");
assert_malformed("\x00\x61\x73\x6d\x0d\x00\x00\x00\x00\x26\x10\x61\x20\x63\x75\x73\x74\x6f\x6d\x20\x73\x65\x63\x74\x69\x6f\x6e\x74\x68\x69\x73\x20\x69\x73\x20\x74\x68\x65\x20\x70\x61\x79\x6c\x6f\x61\x64");
assert_malformed("\x00\x61\x73\x6d\x0d\x00\x00\x00\x00\x25\x10\x61\x20\x63\x75\x73\x74\x6f\x6d\x20\x73\x65\x63\x74\x69\x6f\x6e\x74\x68\x69\x73\x20\x69\x73\x20\x74\x68\x65\x20\x70\x61\x79\x6c\x6f\x61\x64\x00\x24\x10\x61\x20\x63\x75\x73\x74\x6f\x6d\x20\x73\x65\x63\x74\x69\x6f\x6e\x74\x68\x69\x73\x20\x69\x73\x20\x74\x68\x65\x20\x70\x61\x79\x6c\x6f\x61\x64");
assert_malformed("\x00\x61\x73\x6d\x0d\x00\x00\x00\x01\x07\x01\x60\x02\x7f\x7f\x01\x7f\x00\x25\x10\x61\x20\x63\x75\x73\x74\x6f\x6d\x20\x73\x65\x63\x74\x69\x6f\x6e\x74\x68\x69\x73\x20\x69\x73\x20\x74\x68\x65\x20\x70\x61\x79\x6c\x6f\x61\x64\x03\x02\x01\x00\x0a\x09\x01\x07\x00\x20\x00\x20\x01\x6a\x0b\x00\x1b\x07\x63\x75\x73\x74\x6f\x6d\x32\x74\x68\x69\x73\x20\x69\x73\x20\x74\x68\x65\x20\x70\x61\x79\x6c\x6f\x61\x64");
