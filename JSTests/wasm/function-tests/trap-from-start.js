import * as assert from '../assert.js';
import Builder from '../Builder.js';

const memoryInfo = { initial: 2 };
const tableInfo = { element: "anyfunc", initial: 8 };

(function StartTraps() {
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("imp", "memory", memoryInfo)
            .Table("imp", "table", tableInfo)
            .Function("imp", "func", { params: ["i32"] })
        .End()
        .Function().End()
        .Start("startMeUp").End()
        .Element()
          .Element({ offset: 4, functionIndices: [0, 1] })
        .End()
        .Code()
            .Function("startMeUp", { params: [] })
                .I32Const(0).I32Load(2, 0).I32Const(0xfeedface).I32Store(2, 0)
                .I32Const(4).I32Load(2, 0).I32Const(0xc0fec0fe).I32Store(2, 0) // This will trap.
                // This is unreachable:
                .I32Const(42).Call(0) // Calls func(42).
            .End()
        .End()
        .Data()
          .Segment([0xef, 0xbe, 0xad, 0xde]).Offset(1024).End()
        .End();

    const memory = new WebAssembly.Memory(memoryInfo);
    const buffer = new Uint32Array(memory.buffer);

    const table = new WebAssembly.Table(tableInfo);

    // The instance will use these as addresses for stores.
    buffer[0] = 128;
    buffer[1] = 0xc0defefe; // This is out of bounds.

    // This function shouldn't get called because the trap occurs before the call.
    let value = 0;
    const func = v => value = v;

    const module = new WebAssembly.Module(builder.WebAssembly().get());
    const imp = { imp: { memory, table, func } };

    assert.throws(() => new WebAssembly.Instance(module, imp), WebAssembly.RuntimeError, `Out of bounds memory access`);

    assert.eq(value, 0);

    for (let i = 0; i < buffer.length; ++i) {
        switch (i) {
        case   0: assert.eq(buffer[i], 128);        break; // Initial ArrayBuffer store.
        case   1: assert.eq(buffer[i], 0xc0defefe); break; // Initial ArrayBuffer store.
        case  32: assert.eq(buffer[i], 0xfeedface); break; // First store from start function.
        case 256: assert.eq(buffer[i], 0xdeadbeef); break; // Data segment.
        default:  assert.eq(buffer[i], 0);          break; // The rest.
        }
    }

    for (let i = 0; i < table.length; ++i) {
        switch (i) {
        case 4:  assert.isFunction(table.get(i)); break;
        case 5:  assert.isFunction(table.get(i)); break;
        default: assert.eq(table.get(i), null); break;
        }
    }

    // Call the imported `func`.
    table.get(4)(0xf00f);
    assert.eq(value, 0xf00f);
    value = 0;

    // Call the start function again on the instance. The instance is otherwise inaccessible!
    buffer[32] = 0; // Reset the location which will be set by the first store.
    assert.throws(() => table.get(5)(), WebAssembly.RuntimeError, `Out of bounds memory access`);
    assert.eq(buffer[32], 0xfeedface); // The first store should still succeed.
    assert.eq(value, 0);
})();
