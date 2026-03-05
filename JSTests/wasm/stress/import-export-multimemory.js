//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// exercise code paths dealing with imported and exported memories

let watExEx = `
(module
  (memory (export "mem0") 2 3)
  (memory (export "mem1") 2 4)
)
`

let watImEx = `
(module
  (import "js" "mem0" (memory 1))
  (memory (export "mem1") 1)
)
`

let watImIm = `
(module
  (import "js" "mem0" (memory 1))
  (import "js" "mem1" (memory 1))
)
`

let watNoMemories = `
(module
  (func (export "unused") (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add
  )
)
`

const importedMemory0 = new WebAssembly.Memory({ initial: 1 });
const importedMemory1 = new WebAssembly.Memory({ initial: 1 });

async function testMultipleExportedMemories() {
    const instance = await instantiate(watExEx, {}, { multi_memory: true });
    if(instance.exports.mem0 == undefined) { throw new Error('mem0 undefined'); }
    if(instance.exports.mem1 == undefined) { throw new Error('mem1 undefined'); }
}

async function testImportAndExportMemories() {
    const instance = await instantiate(watImEx, { js: { mem0: importedMemory0 } }, { multi_memory: true });
    if(instance.exports.mem1 == undefined) { throw new Error('mem1 undefined'); }
}

async function testMultipleImportedMemories() {
    const instance = await instantiate(watImIm, { js: { mem0: importedMemory0, mem1: importedMemory1 } }, { multi_memory: true });
}

async function testNoMemories() {
    const instance = await instantiate(watNoMemories, {}, { multi_memory: true });
}

await assert.asyncTest(testMultipleExportedMemories());
await assert.asyncTest(testImportAndExportMemories());
await assert.asyncTest(testMultipleImportedMemories());
await assert.asyncTest(testNoMemories());
