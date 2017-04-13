import Builder from '../Builder.js';
import * as assert from '../assert.js';
import * as LLB from '../LowLevelBinary.js';
import * as WASM from '../WASM.js';
import * as util from '../utilities.js';

const verbose = false;

const pageSize = 64 * 1024;
const fourGiB = pageSize * 65536;
const initial = 64;
const maximum = 128;

// When using fast memories, we allocate a redzone after the 4GiB huge
// allocation. This redzone is used to trap reg+imm accesses which exceed
// 32-bits. Anything past 4GiB must trap, but we cannot know statically that it
// will.

const offsets = [
    0,
    1,
    2,
    1024,
    pageSize,
    pageSize + pageSize / 2,
    pageSize * 8,
    pageSize * 16,
    pageSize * 128,
    pageSize * 512,
    fourGiB / 4 - 4,
    fourGiB / 4 - 3,
    fourGiB / 4 - 2,
    fourGiB / 4 - 1,
    fourGiB / 4,
    fourGiB / 4 + 1,
    fourGiB / 4 + 2,
    fourGiB / 4 + 3,
    fourGiB / 4 + 4,
    fourGiB / 2 - 4,
    fourGiB / 2 - 3,
    fourGiB / 2 - 2,
    fourGiB / 2 - 1,
    fourGiB / 2,
    fourGiB / 2 + 1,
    fourGiB / 2 + 2,
    fourGiB / 2 + 3,
    fourGiB / 2 + 4,
    (fourGiB / 4) * 3,
    fourGiB - 4,
    fourGiB - 3,
    fourGiB - 2,
    fourGiB - 1,
];

for (let memoryDeclaration of [{ initial: initial }, { initial: initial, maximum: maximum }]) {
    fullGC();

    // Re-use a single memory so tests are more likely to get a fast memory.
    const memory = new WebAssembly.Memory(memoryDeclaration);
    if (verbose)
        print(WebAssemblyMemoryMode(memory));
    const buf = new Uint8Array(memory.buffer);

    // Enumerate all memory access types.
    for (const op of WASM.opcodes("memory")) {
        const info = WASM.memoryAccessInfo(op);

        // The accesses should fault even if only the last byte is off the end.
        let wiggles = [0];
        for (let wiggle = 0; wiggle !== info.width / 8; ++wiggle)
            wiggles.push(wiggle);

        let builder = (new Builder())
            .Type().End()
            .Import().Memory("imp", "memory", memoryDeclaration).End()
            .Function().End()
            .Export();

        for (let offset of offsets)
            switch (info.type) {
            case "load": builder = builder.Function("get_" + offset); break;
            case "store": builder = builder.Function("set_" + offset); break;
            default: throw new Error(`Implementation problem: unknown memory access type ${info.type}`);
            }

        builder = builder.End().Code();

        const align = 3; // No need to be precise, it's just a hint.
        const constInstr = util.toJavaScriptName(WASM.constForValueType(info.valueType));
        const instr = util.toJavaScriptName(op.name);
        for (let offset of offsets)
            switch (info.type) {
            case "load":
                builder = builder.Function("get_" + offset, { params: ["i32"] }).GetLocal(0)[instr](align, offset).Drop().End();
                break;
            case "store":
                builder = builder.Function("set_" + offset, { params: ["i32"] }).GetLocal(0)[constInstr](0xdead)[instr](align, offset).End();
                break;
            default:
                throw new Error(`Implementation problem: unknown memory access type ${info.type}`);
            }
        
        builder = builder.End();

        const instance = new WebAssembly.Instance(new WebAssembly.Module(builder.WebAssembly().get()), { imp: { memory: memory } });

        for (let offset of offsets) {
            for (let wiggle of wiggles) {
                const address = LLB.varuint32Max - offset - wiggle;
                if (verbose)
                    print(`${op.name.padStart(16, ' ')}: base address ${address > 0 ? '0x' : '  '}${address.toString(16).padStart(8, address > 0 ? '0' : ' ')} + offset 0x${offset.toString(16).padStart(8, '0')} - wiggle ${wiggle} = effective address 0x${(address + offset - wiggle).toString(16).padStart(16, '0')}`);
                switch (info.type) {
                case "load":
                    assert.throws(() => instance.exports["get_" + offset](address), WebAssembly.RuntimeError, `Out of bounds memory access`);
                    break;
                case "store":
                    assert.throws(() => instance.exports["set_" + offset](address), WebAssembly.RuntimeError, `Out of bounds memory access`);
                    break;
                default: throw new Error(`Implementation problem: unknown memory access type ${info.type}`);
                }
            }
        }

        fullGC();
    }

    // Only check that the memory was untouched at the very end, before throwing it away entirely.
    for (let idx = 0; idx < buf.byteLength; ++idx)
        assert.eq(buf[idx], 0);
}
