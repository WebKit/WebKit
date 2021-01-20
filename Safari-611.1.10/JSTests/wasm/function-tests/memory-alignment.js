import Builder from '../Builder.js';
import * as assert from '../assert.js';
import * as WASM from '../WASM.js';
import * as util from '../utilities.js';

const offset = 0;

const memoryDeclaration = { initial: 1 };
const memory = new WebAssembly.Memory(memoryDeclaration);

for (const op of WASM.opcodes("memory")) {
    const info = WASM.memoryAccessInfo(op);
    const maxAlignLog2 = Math.log2(info.width / 8);
    const constInstr = util.toJavaScriptName(WASM.constForValueType(info.valueType));
    const instr = util.toJavaScriptName(op.name);
    for (let alignLog2 = 0; alignLog2 < 16; ++alignLog2) {
        let builder = (new Builder())
            .Type().End()
            .Import().Memory("imp", "memory", memoryDeclaration).End()
            .Function().End()
            .Code();
        let start, end;
        switch (info.type) {
        case "load":
            builder = builder.Function({ params: ["i32"] }).GetLocal(0)[instr](alignLog2, offset).Drop().End();
            start = 5;
            end = 8;
            break;
        case "store":
            builder = builder.Function({ params: ["i32", info.valueType] }).GetLocal(0).GetLocal(1)[instr](alignLog2, offset).End();
            start = 7;
            end = 9;
            break;
        default:
            throw new Error(`Implementation problem: unknown memory access type ${info.type}`);
        }
        builder = builder.End();
        const instance = () => {
            const module = new WebAssembly.Module(builder.WebAssembly().get());
            return new WebAssembly.Instance(module, { imp: { memory: memory } });
        };
        if (alignLog2 <= maxAlignLog2)
            instance();
        else
            assert.throws(instance, WebAssembly.CompileError, `WebAssembly.Module doesn't parse at byte ${start}: byte alignment ${1 << alignLog2} exceeds ${info.type}'s natural alignment ${1 << maxAlignLog2}, in function at index 0`);

    }
}
