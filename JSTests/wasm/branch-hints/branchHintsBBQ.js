//@ $skipModes << "wasm-no-jit".to_sym
//@ $skipModes << "wasm-no-wasm-jit".to_sym
//@ requireOptions("--useOMGJIT=0", "--useWasmIPInt=0")
import * as assert from '../assert.js';
import Builder from '../Builder.js';

const existing = new WebAssembly.Instance(new WebAssembly.Module(
    typeof readbuffer !== "undefined" ? readbuffer('branchHintsModule.wasm') : read('branchHintsModule.wasm', 'binary')
));
assert.eq(existing.exports._fun(-1), 10);
assert.eq(existing.exports._fun(0), 10);
assert.eq(existing.exports._fun(1), 10);
assert.eq(existing.exports._fun(2), 20);

const Op = { Block: 0x02, End: 0x0b, BrIf: 0x0d, Return: 0x0f, Drop: 0x1a, LocalGet: 0x20, I32Const: 0x41 };

function readLeb(bytes, i)
{
    let value = 0;
    let shift = 0;
    while (true) {
        const byte = bytes[i++];
        value |= (byte & 0x7f) << shift;
        if (!(byte & 0x80))
            return [value >>> 0, i];
        shift += 7;
    }
}

function skipOp(bytes, i)
{
    const op = bytes[i++];
    switch (op) {
    case Op.End:
    case Op.Drop:
    case Op.Return:
        return i;
    case Op.Block:
        return i + 1;
    case Op.BrIf:
    case Op.LocalGet:
        [, i] = readLeb(bytes, i);
        return i;
    case Op.I32Const:
        [, i] = readLeb(bytes, i);
        return i;
    default:
        throw new Error(`unhandled opcode 0x${op.toString(16)}`);
    }
}

function firstFunctionOpcodeOffset(wasmBytes, opcode)
{
    const bytes = new Uint8Array(wasmBytes);
    let i = 8;
    while (i < bytes.length) {
        const id = bytes[i++];
        let size;
        [size, i] = readLeb(bytes, i);
        const end = i + size;
        if (id === 10) {
            [, i] = readLeb(bytes, i);
            let bodySize;
            [bodySize, i] = readLeb(bytes, i);
            const bodyStart = i;
            const bodyEnd = i + bodySize;
            let localDeclCount;
            [localDeclCount, i] = readLeb(bytes, i);
            for (let n = 0; n < localDeclCount; ++n) {
                [, i] = readLeb(bytes, i);
                i++;
            }
            while (i < bodyEnd) {
                if (bytes[i] === opcode)
                    return i - bodyStart;
                i = skipOp(bytes, i);
            }
            throw new Error("opcode not found in function body");
        }
        i = end;
    }
    throw new Error("missing code section");
}

function instantiateWithHint(buildCode, opcode, likely, functionIndex, imports)
{
    const unsigned = buildCode(new Builder()).WebAssembly().get();
    const offset = firstFunctionOpcodeOffset(unsigned, opcode);
    const builder = new Builder();
    builder.Unknown("metadata.code.branch_hint")
        .Byte(1)
        .Byte(functionIndex)
        .Byte(1)
        .Byte(offset)
        .Byte(1)
        .Byte(likely ? 1 : 0)
        .End();
    const module = new WebAssembly.Module(buildCode(builder).WebAssembly().get());
    assert.eq(WebAssembly.Module.customSections(module, "metadata.code.branch_hint").length, 1);
    return new WebAssembly.Instance(module, imports);
}

function voidBrIfCode(builder)
{
    return builder
        .Type().End()
        .Function().End()
        .Export()
            .Function("select")
        .End()
        .Code()
            .Function("select", { params: ["i32"], ret: "i32" })
                .Block("void", b =>
                    b.GetLocal(0)
                    .BrIf(0)
                    .I32Const(7)
                    .Return()
                )
                .I32Const(3)
            .End()
        .End();
}

function valuedBrIfCode(builder)
{
    return builder
        .Type().End()
        .Function().End()
        .Export()
            .Function("select")
        .End()
        .Code()
            .Function("select", { params: ["i32"], ret: "i32" })
                .Block("i32", b =>
                    b.I32Const(7)
                    .GetLocal(0)
                    .BrIf(0)
                    .Drop()
                    .I32Const(3)
                )
            .End()
        .End();
}

function importedVoidBrIf(builder)
{
    return builder
        .Type().End()
        .Import()
            .Function("env", "imp", { params: [], ret: "void" })
        .End()
        .Function().End()
        .Export()
            .Function("select")
        .End()
        .Code()
            .Function("select", { params: ["i32"], ret: "i32" })
                .Block("void", b =>
                    b.GetLocal(0)
                    .BrIf(0)
                    .I32Const(7)
                    .Return()
                )
                .I32Const(3)
            .End()
        .End();
}

for (const likely of [false, true]) {
    const voidBrIf = instantiateWithHint(voidBrIfCode, Op.BrIf, likely, 0);
    assert.eq(voidBrIf.exports.select(0), 7);
    assert.eq(voidBrIf.exports.select(1), 3);

    const valued = instantiateWithHint(valuedBrIfCode, Op.BrIf, likely, 0);
    assert.eq(valued.exports.select(0), 3);
    assert.eq(valued.exports.select(1), 7);

    const imported = instantiateWithHint(importedVoidBrIf, Op.BrIf, likely, 1, { env: { imp() { } } });
    assert.eq(imported.exports.select(0), 7);
    assert.eq(imported.exports.select(1), 3);
}
