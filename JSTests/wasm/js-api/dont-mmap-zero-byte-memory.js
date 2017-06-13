import Builder from '../Builder.js';
import * as assert from '../assert.js';

let mems = [];
function makeMem(initial) {
    const desc = {initial};
    mems.push([desc, new WebAssembly.Memory(desc)]);
}
for (let i = 0; i < 100; ++i) {
    makeMem(1);
}

// This loop should not OOM! This tests a bug where we
// would call mmap with zero bytes if we ran out of
// fast memories but created a slow memory with zero
// initial page count.
for (let i = 0; i < 100; ++i) {
    makeMem(0);
}

function testMems() {
    for (const [memDesc, mem] of mems) {
        const builder = (new Builder())
            .Type().End()
            .Import()
                .Memory("imp", "memory", memDesc)
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", { params: [], ret: "i32" })
                    .I32Const(0)
                    .I32Load8U(0, 0)
                    .Return()
                .End()
            .End();
        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        const instance = new WebAssembly.Instance(module, {imp: {memory: mem}});
        if (mem.buffer.byteLength > 0)
            assert.eq(instance.exports.foo(), 0);
        else
            assert.throws(() => instance.exports.foo(), WebAssembly.RuntimeError, "Out of bounds memory access");
    }
}

testMems();

for (const [_, mem] of mems)
    mem.grow(1);

testMems();
