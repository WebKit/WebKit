import * as assert from '../assert.js'

/*
This test loads a WebAssembly file compiled with wat2wasm with support for code annotations:
wat2wasm --enable-annotations --enable-code-metadata branchHintsModule

From the following .wat:
(module
  (func $fun (param i32) (result i32)
        (local i32)
        i32.const 10
        local.tee 1
        local.get 0
        i32.mul
        local.tee 0
        i32.const 10
        i32.gt_s
        (@metadata.code.branch_hint "\01") if
          local.get 0
          return
        end
        (block
          local.get 0
          i32.const 0
          i32.le_s
          (@metadata.code.branch_hint "\00") br_if 0
          local.get 0
          return
        )
        local.get 1
        return
  )
  (export "_fun" (func $fun)))
*/

const verbose = false;
const wasmFile = 'branchHintsModule.wasm';

const module = (location) => {
    if (verbose)
        print(`Processing ${location}`);
    let buf = typeof readbuffer !== "undefined"? readbuffer(location) : read(location, 'binary');
    if (verbose)
        print(`  Size: ${buf.byteLength}`);
    let module = new WebAssembly.Module(buf);
    return module;
};

const branchHintsModule = module(wasmFile);
const parsedBranchHintsSection = WebAssembly.Module.customSections(branchHintsModule, "metadata.code.branch_hint");
assert.eq(parsedBranchHintsSection.length, 1);
const instance = new WebAssembly.Instance(branchHintsModule);
const fun = instance.exports._fun;
assert.truthy(fun(-1));
assert.truthy(fun(0));
assert.truthy(fun(1));
assert.truthy(fun(2));
