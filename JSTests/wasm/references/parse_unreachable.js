import * as assert from '../assert.js';

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

async function validTableInitUnreachable() {
  /*
  (module
    (type $type1 (func (result i32)))
    (table $t 10 funcref)
    (func $f (result i32) i32.const 37)
    (elem $elem0 funcref (ref.null func) (ref.func $f) (ref.func $f))
    (func (export "run")
      (return)
      (table.init $t $elem0 (i32.const 1) (i32.const 2) (i32.const 3))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x08\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x03\x02\x00\x01\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x01\x09\x0d\x01\x05\x70\x03\xd0\x70\x0b\xd2\x00\x0b\xd2\x00\x0b\x0a\x14\x02\x04\x00\x41\x25\x0b\x0d\x00\x0f\x41\x01\x41\x02\x41\x03\xfc\x0c\x00\x00\x0b"));
  instance.exports.run();
}

function invalidTableInitUnreachable() {
  /*
  (module
    (type $type1 (func (result i32)))
    (table $t 10 funcref)
    (func $f (result i32) i32.const 37)
    (elem $elem0 funcref (ref.null func) (ref.func $f) (ref.func $f))
    (func (export "run")
      (return)
      (table.init $t 11 (i32.const 1) (i32.const 2) (i32.const 3))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x08\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x03\x02\x00\x01\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x01\x09\x0d\x01\x05\x70\x03\xd0\x70\x0b\xd2\x00\x0b\xd2\x00\x0b\x0a\x14\x02\x04\x00\x41\x25\x0b\x0d\x00\x0f\x41\x01\x41\x02\x41\x0b\xfc\x0c\x0b\x00\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't validate: element index 11 is invalid, limit is 1, in function at index 1 (evaluating 'new WebAssembly.Module(buffer)')");

  /*
  (module
    (type $type1 (func (result i32)))
    (table $t 10 funcref)
    (func $f (result i32) i32.const 37)
    (elem $elem0 funcref (ref.null func) (ref.func $f) (ref.func $f))
    (func (export "run")
      (return)
      (table.init 11 $elem0 (i32.const 1) (i32.const 2) (i32.const 3))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x08\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x03\x02\x00\x01\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x01\x09\x0d\x01\x05\x70\x03\xd0\x70\x0b\xd2\x00\x0b\xd2\x00\x0b\x0a\x14\x02\x04\x00\x41\x25\x0b\x0d\x00\x0f\x41\x01\x41\x02\x41\x0b\xfc\x0c\x00\x0b\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't validate: table index 11 is invalid, limit is 1, in function at index 1 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validTableSizeUnreachable() {
  /*
  (module
    (table $t 10 funcref)
    (func (export "run")
      (return)
      (drop (table.size $t))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x09\x01\x07\x00\x0f\xfc\x10\x00\x1a\x0b"));
  instance.exports.run();
}

function invalidTableSizeUnreachable() {
  /*
  (module
    (table $t 10 funcref)
    (func (export "run")
      (return)
      (drop (table.size 10))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x09\x01\x07\x00\x0f\xfc\x10\x0a\x1a\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't validate: table index 10 is invalid, limit is 1, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validElemDropUnreachable() {
  /*
  (module
    (func $f (result i32) (i32.const 37))
    (func $g (result i32) (i32.const 42))
    (table $t0 10 funcref)
    (elem $e0 func $f $f $g $g)

    (func (export "run")
      (return)
      (elem.drop $e0)
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x08\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x04\x03\x00\x00\x01\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x02\x09\x08\x01\x01\x00\x04\x00\x00\x01\x01\x0a\x12\x03\x04\x00\x41\x25\x0b\x04\x00\x41\x2a\x0b\x06\x00\x0f\xfc\x0d\x00\x0b"));
  instance.exports.run();
}

function invalidElemDropUnreachable() {
  /*
  (module
    (func $f (result i32) (i32.const 37))
    (func $g (result i32) (i32.const 42))
    (table $t0 10 funcref)
    (elem $e0 func $f $f $g $g)

    (func (export "run")
      (return)
      (elem.drop 10)
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x08\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x04\x03\x00\x00\x01\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x02\x09\x08\x01\x01\x00\x04\x00\x00\x01\x01\x0a\x12\x03\x04\x00\x41\x25\x0b\x04\x00\x41\x2a\x0b\x06\x00\x0f\xfc\x0d\x0a\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't validate: element index 10 is invalid, limit is 1, in function at index 2 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validTableGrowUnreachable() {
  /*
  (module
    (table $t0 10 funcref)
    (func (export "run")
      (return)
      (drop (table.grow $t0 (i32.const 37)))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0b\x01\x09\x00\x0f\x41\x25\xfc\x0f\x00\x1a\x0b"));
  instance.exports.run();
}

function invalidTableGrowUnreachable() {
  /*
  (module
    (table $t0 10 funcref)
    (func (export "run")
      (return)
      (drop (table.grow 10 (i32.const 37)))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0b\x01\x09\x00\x0f\x41\x25\xfc\x0f\x0a\x1a\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't validate: table index 10 is invalid, limit is 1, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validTableFillUnreachable() {
  /*
  (module
    (table $t0 10 funcref)
    (func (export "run")
      (return)
      (table.fill $t0 (i32.const 0) (ref.null func) (i32.const 7))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0e\x01\x0c\x00\x0f\x41\x00\xd0\x70\x41\x07\xfc\x11\x00\x0b"));
  instance.exports.run();
}

function invalidTableFillUnreachable() {
  /*
  (module
    (table $t0 10 funcref)
    (func (export "run")
      (return)
      (drop (table.grow 10 (i32.const 37)))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x04\x04\x01\x70\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0e\x01\x0c\x00\x0f\x41\x00\xd0\x70\x41\x07\xfc\x11\x0a\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't validate: table index 10 is invalid, limit is 1, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validTableCopyUnreachable() {
  /*
  (module
    (func $f (result i32) (i32.const 37))
    (func $g (result i32) (i32.const 42))
    (table $t0 10 funcref)
    (elem (table $t0) (i32.const 0) func $f $f $g $g $f)
    (table $t1 20 funcref)
    (func (export "run")
      (return)
      (table.copy $t1 $t0 (i32.const 1) (i32.const 0) (i32.const 5))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x08\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x04\x03\x00\x00\x01\x04\x07\x02\x70\x00\x0a\x70\x00\x14\x07\x07\x01\x03\x72\x75\x6e\x00\x02\x09\x0b\x01\x00\x41\x00\x0b\x05\x00\x00\x01\x01\x00\x0a\x19\x03\x04\x00\x41\x25\x0b\x04\x00\x41\x2a\x0b\x0d\x00\x0f\x41\x01\x41\x00\x41\x05\xfc\x0e\x01\x00\x0b"));
  instance.exports.run();
}

function invalidTableCopyUnreachable() {
  /*
  (module
    (func $f (result i32) (i32.const 37))
    (func $g (result i32) (i32.const 42))
    (table $t0 10 funcref)
    (elem (table $t0) (i32.const 0) func $f $f $g $g $f)
    (table $t1 20 funcref)
    (func (export "run")
      (return)
      (table.copy 10 $t0 (i32.const 1) (i32.const 0) (i32.const 5))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x08\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x04\x03\x00\x00\x01\x04\x07\x02\x70\x00\x0a\x70\x00\x14\x07\x07\x01\x03\x72\x75\x6e\x00\x02\x09\x0b\x01\x00\x41\x00\x0b\x05\x00\x00\x01\x01\x00\x0a\x19\x03\x04\x00\x41\x25\x0b\x04\x00\x41\x2a\x0b\x0d\x00\x0f\x41\x01\x41\x00\x41\x05\xfc\x0e\x0a\x00\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't validate: table index 10 is invalid, limit is 2, in function at index 2 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validAnnotatedSelectUnreachable() {
  /*
  (module
    (table $t 10 externref)
    (func (export "run")
      (return)
      (drop
        (select (result externref) (ref.null extern) (ref.null extern) (i32.const 1)))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x04\x04\x01\x6f\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0f\x01\x0d\x00\x0f\xd0\x6f\xd0\x6f\x41\x01\x1c\x01\x6f\x1a\x0b"));
  instance.exports.run();
}

function invalidAnnotatedSelectUnreachable() {
  /*
  (module
    (table $t 10 externref)
    (func (export "run")
      (return)
      (drop
        (select (result (size = 2) externref) (ref.null extern) (ref.null extern) (i32.const 1)))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x04\x04\x01\x6f\x00\x0a\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0f\x01\x0d\x00\x0f\xd0\x6f\xd0\x6f\x41\x01\x1c\x02\x6f\x1a\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't parse at byte 10: select invalid result arity for, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validMemoryFillUnreachable() {
  /*
  (module
    (memory $mem0 1)
    (func (export "run")
      (return)
      (memory.fill (i32.const 0) (i32.const 11) (i32.const 3))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x05\x03\x01\x00\x01\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0e\x01\x0c\x00\x0f\x41\x00\x41\x0b\x41\x03\xfc\x0b\x00\x0b"));
  instance.exports.run();
}

function invalidMemoryFillUnreachable() {
  /*
  (module
    (memory $mem0 1)
    (func (export "run")
      (return)
      (memory.fill (unused = 1) (i32.const 0) (i32.const 11) (i32.const 3))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x05\x03\x01\x00\x01\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0e\x01\x0c\x00\x0f\x41\x00\x41\x0b\x41\x03\xfc\x0b\x01\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't parse at byte 11: auxiliary byte for memory.fill should be zero, but got 1, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validMemoryCopyUnreachable() {
  /*
  (module
    (memory $mem0 1)
    (func (export "run")
      (return)
      (memory.copy (i32.const 3) (i32.const 0) (i32.const 1))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x05\x03\x01\x00\x01\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0f\x01\x0d\x00\x0f\x41\x03\x41\x00\x41\x01\xfc\x0a\x00\x00\x0b"));
  instance.exports.run();
}

function invalidMemoryCopyUnreachable() {
  /*
  (module
    (memory $mem0 1)
    (func (export "run")
      (return)
      (memory.copy (unsued_1 = 1) (i32.const 3) (i32.const 0) (i32.const 1))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x05\x03\x01\x00\x01\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0f\x01\x0d\x00\x0f\x41\x03\x41\x00\x41\x01\xfc\x0a\x01\x00\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't parse at byte 11: auxiliary byte for memory.copy should be zero, but got 1, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')");

  /*
  (module
    (memory $mem0 1)
    (func (export "run")
      (return)
      (memory.copy (unsued_1 = 0, unused_2 = 1) (i32.const 3) (i32.const 0) (i32.const 1))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x05\x03\x01\x00\x01\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0a\x0f\x01\x0d\x00\x0f\x41\x03\x41\x00\x41\x01\xfc\x0a\x00\x01\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't parse at byte 12: auxiliary byte for memory.copy should be zero, but got 1, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validMemoryInitUnreachable() {
  /*
  (module
    (memory $mem0 1)
    (data $data0 "\02\03\05\07")
    (func (export "run")
      (return)
      (memory.init $data0 (i32.const 4) (i32.const 0) (i32.const 4))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x05\x03\x01\x00\x01\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0c\x01\x01\x0a\x0f\x01\x0d\x00\x0f\x41\x04\x41\x00\x41\x04\xfc\x08\x00\x00\x0b\x0b\x07\x01\x01\x04\x02\x03\x05\x07"));
  instance.exports.run();
}

function invalidMemoryInitUnreachable() {
  /*
  (module
    (memory $mem0 1)
    (data $data0 "\02\03\05\07")
    (func (export "run")
      (return)
      (memory.init 10 (i32.const 4) (i32.const 0) (i32.const 4))
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x05\x03\x01\x00\x01\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0c\x01\x01\x0a\x0f\x01\x0d\x00\x0f\x41\x04\x41\x00\x41\x04\xfc\x08\x0a\x00\x0b\x0b\x07\x01\x01\x04\x02\x03\x05\x07"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't validate: data segment index 10 is invalid, limit is 1, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')");
}

function validDataDropUnreachable() {
  /*
  (module
    (memory 1)
    (data $data0 "\37")
    (func (export "run")
      (return)
      (data.drop $data0)
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x05\x03\x01\x00\x01\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0c\x01\x01\x0a\x08\x01\x06\x00\x0f\xfc\x09\x00\x0b\x0b\x04\x01\x01\x01\x37"));
  instance.exports.run();
}

function invalidDataDropUnreachable() {
  /*
  (module
    (memory 1)
    (data $data0 "\37")
    (func (export "run")
      (return)
      (data.drop 10)
    )
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x05\x03\x01\x00\x01\x07\x07\x01\x03\x72\x75\x6e\x00\x00\x0c\x01\x01\x0a\x08\x01\x06\x00\x0f\xfc\x09\x0a\x0b\x0b\x04\x01\x01\x01\x37"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't validate: data segment index 10 is invalid, limit is 1, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')");
}

validTableInitUnreachable();
invalidTableInitUnreachable();

validTableSizeUnreachable();
invalidTableSizeUnreachable();

validElemDropUnreachable();
invalidElemDropUnreachable();

validTableGrowUnreachable();
invalidTableGrowUnreachable();

validTableFillUnreachable();
invalidTableFillUnreachable();

validTableCopyUnreachable();
invalidTableCopyUnreachable();

validAnnotatedSelectUnreachable();
invalidAnnotatedSelectUnreachable();

validMemoryFillUnreachable();
invalidMemoryFillUnreachable();

validMemoryCopyUnreachable();
invalidMemoryCopyUnreachable();

validMemoryInitUnreachable();
invalidMemoryInitUnreachable();

validDataDropUnreachable();
invalidDataDropUnreachable();
