import Builder from '../Builder.js';
import * as assert from '../assert.js';

const memSizeInPages = 1;
const pageSizeInBytes = 64 * 1024;
const memoryDescription = { initial: memSizeInPages, maximum: memSizeInPages };

(function ElementBeforeData() {
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("imp", "memory", memoryDescription)
            .Table("imp", "table", {element: "funcref", initial: 19}) // unspecified maximum.
        .End()
        .Function().End()
        .Element()
            .Element({offset: 19, functionIndices: [0, 0, 0, 0, 0]})
        .End()
        .Code()
            .Function("foo", {params: ["i32"], ret: "i32"})
                .GetLocal(0)
                .I32Const(42)
                .I32Add()
                .Return()
            .End()
        .End()
        .Data()
          .Segment([0xde, 0xad, 0xbe, 0xef]).Offset(0).End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    const table = new WebAssembly.Table({element: "funcref", initial: 19});
    const imports = {
        imp: {
            memory: memory,
            table: table,
        }
    };
    assert.throws(() => new WebAssembly.Instance(module, imports), WebAssembly.RuntimeError, `Element is trying to set an out of bounds table index (evaluating 'new WebAssembly.Instance(module, imports)')`);
    // On Element failure, the Data section shouldn't have executed.
    const buffer = new Uint8Array(memory.buffer);
    for (let idx = 0; idx < memSizeInPages * pageSizeInBytes; ++idx) {
        const value = buffer[idx];
        assert.eq(value, 0x00);
    }
})();
