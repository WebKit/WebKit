import * as assert from "../assert.js";

function testElemSegmentLimit() {
  // (module
  //   (type (;0;) (func))
  //   (type (;1;) (func (result i32)))
  //   (table (;0;) 10 funcref)
  //   (export "run" (func 1))
  //   (elem (;0;) func 0 ... (; repeated 10_000_001 times ;) )
  //   (func (;0;) (type 0))
  //   (func (;1;) (type 1) (result i32)
  //     i32.const 42
  //   )
  // )
  let bytes = [0,97,115,109,1,0,0,0,1,8,2,96,0,0,96,0,1,127,3,3,2,0,1,4,4,1,112,0,10,7,7,1,3,114,117,110,0,1,9,136,173,226,4,1,1,0,129,173,226,4];
  const count = 10_000_001;
  for (let i = 0; i < count; i++) {
    bytes.push(0);
  }
  bytes.push(...[10,9,2,2,0,11,4,0,65,42,11]);
  assert.throws(
      () => new WebAssembly.Module(new Uint8Array(bytes)),
      WebAssembly.CompileError,
      "WebAssembly.Module doesn't parse at byte 50: Element section's 0th index count of 10000001 is too big, maximum 10000000"
  );
}

testElemSegmentLimit();
