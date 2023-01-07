import * as assert from '../assert.js';
import Builder from '../Builder.js';

const memSizeInPages = 1;
const pageSizeInBytes = 64 * 1024;
const memoryDescription = { initial: memSizeInPages, maximum: memSizeInPages };
const emptyMemory = { initial: 0, maximum: 2 };

// FIXME Some corner cases are ill-specified: https://github.com/WebAssembly/design/issues/897

const assertMemoryAllZero = memory => {
    const buffer = new Uint8Array(memory.buffer);
    for (let idx = 0; idx < buffer.length; ++idx) {
        const value = buffer[idx];
        assert.eq(value, 0x00);
    }
};

(function DataSection() {
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Data()
          .Segment([0xff, 0x2a]).Offset(4).End()
          .Segment([0xde, 0xad, 0xbe, 0xef]).Offset(24).End()
          .Segment([0xca, 0xfe]).Offset(25).End() // Overwrite.
          .Segment([]).Offset(4).End() // Empty.
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    const instance = new WebAssembly.Instance(module, { imp: { memory: memory } });
    const buffer = new Uint8Array(memory.buffer);
    for (let idx = 0; idx < memSizeInPages * pageSizeInBytes; ++idx) {
        const value = buffer[idx];
        switch (idx) {
        case 4: assert.eq(value, 0xff); break;
        case 5: assert.eq(value, 0x2a); break;
        case 24: assert.eq(value, 0xde); break;
        case 25: assert.eq(value, 0xca); break;
        case 26: assert.eq(value, 0xfe); break;
        case 27: assert.eq(value, 0xef); break;
        default: assert.eq(value, 0x00); break;
        }
    }
})();

(function DataSectionWithoutMemory() {
    const builder = (new Builder())
        .Type().End()
        .Data()
          .Segment([0xff]).Offset(0).End()
        .End();
    const bin = builder.WebAssembly().get();
    assert.throws(() => new WebAssembly.Module(bin), WebAssembly.CompileError, `WebAssembly.Module doesn't parse at byte 15: 0th Data segment has index 0 which exceeds the number of Memories 0`);
})();

(function EmptyDataSectionWithoutMemory() {
    const builder = (new Builder())
        .Type().End()
        .Data()
          .Segment([]).Offset(0).End()
        .End();
    const bin = builder.WebAssembly().get();
    assert.throws(() => new WebAssembly.Module(bin), WebAssembly.CompileError, `WebAssembly.Module doesn't parse at byte 15: 0th Data segment has index 0 which exceeds the number of Memories 0`);
})();

(function DataSectionBiggerThanMemory() {
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Data()
          .Segment(Array(memSizeInPages * pageSizeInBytes + 1).fill(0xff)).Offset(0).End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    assert.throws(() => new WebAssembly.Instance(module, { imp: { memory: memory } }), WebAssembly.RuntimeError, `Invalid data segment initialization: segment of 65537 bytes memory of 65536 bytes, at offset 0, segment is too big (evaluating 'new WebAssembly.Instance(module, { imp: { memory: memory } })')`);
    assertMemoryAllZero(memory);
})();

(function DataSectionOffTheEnd() {
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Data()
          .Segment([0xff]).Offset(memSizeInPages * pageSizeInBytes).End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    assert.throws(() => new WebAssembly.Instance(module, { imp: { memory: memory } }), WebAssembly.RuntimeError, `Invalid data segment initialization: segment of 1 bytes memory of 65536 bytes, at offset 65536, segment writes outside of memory (evaluating 'new WebAssembly.Instance(module, { imp: { memory: memory } })')`);
    assertMemoryAllZero(memory);
})();

(function DataSectionPartlyOffTheEnd() {
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Data()
          .Segment([0xff, 0xff]).Offset(memSizeInPages * pageSizeInBytes - 1).End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    assert.throws(() => new WebAssembly.Instance(module, { imp: { memory: memory } }), WebAssembly.RuntimeError, `Invalid data segment initialization: segment of 2 bytes memory of 65536 bytes, at offset 65535, segment writes outside of memory (evaluating 'new WebAssembly.Instance(module, { imp: { memory: memory } })')`);
    assertMemoryAllZero(memory);
})();

(function DataSectionEmptyOffTheEnd() {
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Data()
          .Segment([]).Offset(memSizeInPages * pageSizeInBytes).End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    const instance = new WebAssembly.Instance(module, { imp: { memory: memory } });
    assertMemoryAllZero(memory);
})();

(function DataSectionEmptyOffTheEndWithEmptyMemory() {
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", emptyMemory).End()
        .Data()
          .Segment([]).Offset(memSizeInPages * pageSizeInBytes).End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(emptyMemory);
    assert.throws(() => new WebAssembly.Instance(module, { imp: { memory: memory } }), WebAssembly.RuntimeError, `Invalid data segment initialization: segment of 0 bytes memory of 0 bytes, at offset 65536, segment writes outside of memory`);
    assert.eq(memory.buffer.byteLength, 0);
})();

(function DataSectionSeenByStart() {
    const offset = 1024;
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("imp", "memory", memoryDescription)
            .Function("imp", "func", { params: ["i32"] })
        .End()
        .Function().End()
        .Start("foo").End()
        .Code()
            .Function("foo", { params: [] })
                .I32Const(offset)
                .I32Load8U(0, 0)
                .Call(0) // Calls func((i8.load(offset), align=2, offset=0). This should observe 0xff as set by the data section.
            .End()
        .End()
        .Data()
          .Segment([0xff]).Offset(offset).End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    let value = 0;
    const setter = v => value = v;
    const instance = new WebAssembly.Instance(
        module,
        {
            imp: {
                memory: memory,
                func: setter
            }
        });
    assert.eq(value, 0xff);
    const buffer = new Uint8Array(memory.buffer);
    for (let idx = 0; idx < memSizeInPages * pageSizeInBytes; ++idx) {
        const value = buffer[idx];
        if (idx == offset)
            assert.eq(value, 0xff);
        else
            assert.eq(value, 0x00);
    }
})();
