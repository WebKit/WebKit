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

$$ = instance("\x00\x61\x73\x6d\x0d\x00\x00\x00\x01\x1b\x07\x60\x00\x00\x60\x00\x00\x60\x00\x00\x60\x00\x01\x7f\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x01\x7f\x00\x02\x12\x01\x08\x73\x70\x65\x63\x74\x65\x73\x74\x05\x70\x72\x69\x6e\x74\x00\x06\x03\x07\x06\x00\x01\x04\x05\x05\x06\x07\x1c\x04\x03\x6f\x6e\x65\x00\x03\x03\x74\x77\x6f\x00\x04\x05\x74\x68\x72\x65\x65\x00\x05\x04\x66\x6f\x75\x72\x00\x06\x0a\x23\x06\x02\x00\x0b\x02\x00\x0b\x04\x00\x41\x0d\x0b\x07\x00\x20\x00\x41\x01\x6a\x0b\x07\x00\x20\x00\x41\x02\x6b\x0b\x06\x00\x20\x00\x10\x00\x0b");
assert_return(() => $$.exports["one"](), 13);
assert_return(() => $$.exports["two"](13), 14);
assert_return(() => $$.exports["three"](13), 11);
$$.exports["four"](83);
assert_invalid("\x00\x61\x73\x6d\x0d\x00\x00\x00");
assert_invalid("\x00\x61\x73\x6d\x0d\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x0a\x04\x01\x02\x00\x0b");
assert_invalid("\x00\x61\x73\x6d\x0d\x00\x00\x00\x04\x04\x01\x70\x00\x01\x09\x06\x01\x00\x42\x00\x0b\x00");
assert_invalid("\x00\x61\x73\x6d\x0d\x00\x00\x00\x04\x04\x01\x70\x00\x01\x09\x06\x01\x00\x41\x00\x0b\x00");
assert_invalid("\x00\x61\x73\x6d\x0d\x00\x00\x00\x04\x04\x01\x70\x00\x01\x09\x05\x01\x00\x01\x0b\x00");
assert_invalid("\x00\x61\x73\x6d\x0d\x00\x00\x00\x03\x02\x01\x2a\x0a\x04\x01\x02\x00\x0b");
assert_invalid("\x00\x61\x73\x6d\x0d\x00\x00\x00\x02\x12\x01\x08\x73\x70\x65\x63\x74\x65\x73\x74\x05\x70\x72\x69\x6e\x74\x00\x2b");
$$ = instance("\x00\x61\x73\x6d\x0d\x00\x00\x00\x01\x0e\x03\x60\x00\x01\x7f\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x03\x08\x07\x00\x00\x00\x01\x01\x02\x02\x04\x05\x01\x70\x01\x07\x07\x07\x11\x02\x05\x63\x61\x6c\x6c\x74\x00\x05\x05\x63\x61\x6c\x6c\x75\x00\x06\x09\x0d\x01\x00\x41\x00\x0b\x07\x00\x01\x02\x03\x04\x00\x02\x0a\x2a\x07\x04\x00\x41\x01\x0b\x04\x00\x41\x02\x0b\x04\x00\x41\x03\x0b\x04\x00\x41\x04\x0b\x04\x00\x41\x05\x0b\x07\x00\x20\x00\x11\x00\x00\x0b\x07\x00\x20\x00\x11\x01\x00\x0b");
assert_return(() => $$.exports["callt"](0), 1);
assert_return(() => $$.exports["callt"](1), 2);
assert_return(() => $$.exports["callt"](2), 3);
assert_return(() => $$.exports["callt"](3), 4);
assert_return(() => $$.exports["callt"](4), 5);
assert_return(() => $$.exports["callt"](5), 1);
assert_return(() => $$.exports["callt"](6), 3);
assert_trap(() => $$.exports["callt"](7));
assert_trap(() => $$.exports["callt"](100));
assert_trap(() => $$.exports["callt"](-1));
assert_return(() => $$.exports["callu"](0), 1);
assert_return(() => $$.exports["callu"](1), 2);
assert_return(() => $$.exports["callu"](2), 3);
assert_return(() => $$.exports["callu"](3), 4);
assert_return(() => $$.exports["callu"](4), 5);
assert_return(() => $$.exports["callu"](5), 1);
assert_return(() => $$.exports["callu"](6), 3);
assert_trap(() => $$.exports["callu"](7));
assert_trap(() => $$.exports["callu"](100));
assert_trap(() => $$.exports["callu"](-1));
$$ = instance("\x00\x61\x73\x6d\x0d\x00\x00\x00\x01\x0a\x02\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x03\x04\x03\x00\x00\x01\x04\x05\x01\x70\x01\x02\x02\x07\x09\x01\x05\x63\x61\x6c\x6c\x74\x00\x02\x09\x08\x01\x00\x41\x00\x0b\x02\x00\x01\x0a\x13\x03\x04\x00\x41\x01\x0b\x04\x00\x41\x02\x0b\x07\x00\x20\x00\x11\x00\x00\x0b");
assert_return(() => $$.exports["callt"](0), 1);
assert_return(() => $$.exports["callt"](1), 2);
