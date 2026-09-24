import * as assert from "../assert.js";
import { compile, instantiate, watToWasm } from "./wast-wrapper.js";

function testBlockType() {
  compile(`
    (module
      (rec (type (func (result (ref null 0)))))
      (func (block (type 0) (ref.null 0)) drop))
  `);

  assert.compileError(watToWasm(`
      (module
        (type (array i32))
        (func (block (type 0))))
    `),
    "WebAssembly.Module doesn't parse at byte 3: can't get block's signature, in function at index 0");

  assert.compileError(watToWasm(`
      (module
        (type (struct))
        (func (block (type 0))))
    `),
    "WebAssembly.Module doesn't parse at byte 3: can't get block's signature, in function at index 0");

  assert.compileError(watToWasm(`
      (module
        (type (array i32))
        (func (type 0)))
    `),
    "WebAssembly.Module doesn't parse at byte 26: 0th Function type 0 doesn't have a function signature");
}

testBlockType();
