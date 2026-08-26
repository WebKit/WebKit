//@ requireOptions("--useWasmCompactImportSection=true")
import * as assert from "../assert.js";

function leb(n)
{
    const bytes = [];
    do {
        let byte = n & 0x7f;
        n >>>= 7;
        if (n)
            byte |= 0x80;
        bytes.push(byte);
    } while (n);
    return bytes;
}

function name(string)
{
    const bytes = [];
    for (let i = 0; i < string.length; ++i)
        bytes.push(string.charCodeAt(i));
    return [...leb(bytes.length), ...bytes];
}

function section(id, payload)
{
    return [id, ...leb(payload.length), ...payload];
}

function moduleBytes(...sections)
{
    return new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, ...sections.flat()]);
}

const typeVoidToI32 = section(1, [1, 0x60, 0, 1, 0x7f]);
const typeI32ToVoid = section(1, [1, 0x60, 1, 0x7f, 0]);

{
    const bytes = moduleBytes(
        typeVoidToI32,
        section(2, [
            1,
            ...name("env"),
            0x00,
            0x7e,
            0x00,
            0x00,
            2,
            ...name("a"),
            ...name("b"),
        ]),
        section(7, [
            2,
            ...name("a"), 0x00, 0x00,
            ...name("b"), 0x00, 0x01,
        ]),
    );
    const module = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(module);
    assert.eq(imports.length, 2);
    assert.eq(imports[0].module, "env");
    assert.eq(imports[0].name, "a");
    assert.eq(imports[0].kind, "function");
    assert.eq(imports[1].module, "env");
    assert.eq(imports[1].name, "b");
    assert.eq(imports[1].kind, "function");

    const instance = new WebAssembly.Instance(module, { env: { a: () => 1, b: () => 2 } });
    assert.eq(instance.exports.a(), 1);
    assert.eq(instance.exports.b(), 2);
}

{
    const bytes = moduleBytes(
        typeVoidToI32,
        section(2, [
            1,
            ...name("env"),
            0x00,
            0x7f,
            2,
            ...name("a"), 0x00, 0x00,
            ...name("b"), 0x00, 0x00,
        ]),
        section(7, [
            2,
            ...name("a"), 0x00, 0x00,
            ...name("b"), 0x00, 0x01,
        ]),
    );
    const module = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(module);
    assert.eq(imports.length, 2);
    assert.eq(imports[0].name, "a");
    assert.eq(imports[1].name, "b");
    const instance = new WebAssembly.Instance(module, { env: { a: () => 3, b: () => 4 } });
    assert.eq(instance.exports.a(), 3);
    assert.eq(instance.exports.b(), 4);
}

{
    const bytes = moduleBytes(
        typeVoidToI32,
        section(2, [
            2,
            ...name("env"),
            0x00,
            0x7e,
            0x00,
            0x00,
            2,
            ...name("a"),
            ...name("b"),
            ...name("other"),
            ...name("c"),
            0x00,
            0x00,
        ]),
        section(7, [
            3,
            ...name("a"), 0x00, 0x00,
            ...name("b"), 0x00, 0x01,
            ...name("c"), 0x00, 0x02,
        ]),
    );
    const imports = WebAssembly.Module.imports(new WebAssembly.Module(bytes));
    assert.eq(imports.length, 3);
    assert.eq(imports[0].module, "env");
    assert.eq(imports[0].name, "a");
    assert.eq(imports[1].name, "b");
    assert.eq(imports[2].module, "other");
    assert.eq(imports[2].name, "c");
}

{
    const bytes = moduleBytes(
        typeVoidToI32,
        section(2, [
            1,
            ...name("env"),
            0x00,
            0x00,
            0x00,
        ]),
        section(7, [
            1,
            ...name("empty"), 0x00, 0x00,
        ]),
    );
    const imports = WebAssembly.Module.imports(new WebAssembly.Module(bytes));
    assert.eq(imports.length, 1);
    assert.eq(imports[0].module, "env");
    assert.eq(imports[0].name, "");
    assert.eq(imports[0].kind, "function");
    const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), { env: { "": () => 5 } });
    assert.eq(instance.exports.empty(), 5);
}

{
    const bytes = moduleBytes(
        section(2, [
            1,
            ...name("env"),
            0x00,
            0x7e,
            0x03,
            0x7f, 0x00,
            2,
            ...name("g0"),
            ...name("g1"),
        ]),
        section(7, [
            2,
            ...name("g0"), 0x03, 0x00,
            ...name("g1"), 0x03, 0x01,
        ]),
    );
    const module = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(module);
    assert.eq(imports.length, 2);
    assert.eq(imports[0].kind, "global");
    assert.eq(imports[1].kind, "global");
    const instance = new WebAssembly.Instance(module, {
        env: {
            g0: new WebAssembly.Global({ value: "i32" }, 8),
            g1: new WebAssembly.Global({ value: "i32" }, 9),
        },
    });
    assert.eq(instance.exports.g0.value, 8);
    assert.eq(instance.exports.g1.value, 9);
}

{
    const bytes = moduleBytes(
        section(2, [
            1,
            ...name("env"),
            0x00,
            0x7e,
            0x01,
            0x70, 0x00, 0x01,
            2,
            ...name("t0"),
            ...name("t1"),
        ]),
        section(7, [
            2,
            ...name("t0"), 0x01, 0x00,
            ...name("t1"), 0x01, 0x01,
        ]),
    );
    const module = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(module);
    assert.eq(imports.length, 2);
    assert.eq(imports[0].kind, "table");
    assert.eq(imports[1].kind, "table");
    const instance = new WebAssembly.Instance(module, {
        env: {
            t0: new WebAssembly.Table({ element: "funcref", initial: 1 }),
            t1: new WebAssembly.Table({ element: "funcref", initial: 1 }),
        },
    });
    assert.eq(instance.exports.t0.length, 1);
    assert.eq(instance.exports.t1.length, 1);
}

{
    const bytes = moduleBytes(
        typeI32ToVoid,
        section(2, [
            1,
            ...name("env"),
            0x00,
            0x7f,
            2,
            ...name("f"), 0x00, 0x00,
            ...name("m"), 0x02, 0x00, 0x01,
        ]),
        section(7, [
            2,
            ...name("f"), 0x00, 0x00,
            ...name("m"), 0x02, 0x00,
        ]),
    );
    const module = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(module);
    assert.eq(imports.length, 2);
    assert.eq(imports[0].kind, "function");
    assert.eq(imports[1].kind, "memory");
    const memory = new WebAssembly.Memory({ initial: 1 });
    const instance = new WebAssembly.Instance(module, { env: { f: () => { }, m: memory } });
    assert.eq(instance.exports.m.buffer.byteLength, 65536);
}

{
    const bytes = moduleBytes(
        typeVoidToI32,
        section(2, [
            1,
            ...name("env"),
            0x00,
            0x7f,
            0,
        ]),
    );
    assert.eq(WebAssembly.Module.imports(new WebAssembly.Module(bytes)).length, 0);
}

assert.throws(() => new WebAssembly.Module(moduleBytes(
    typeVoidToI32,
    section(2, [
        1,
        ...name("env"),
        ...name("not-empty"),
        0x7e,
        0x00,
        0x00,
        1,
        ...name("a"),
    ]),
)), WebAssembly.CompileError, "kind");