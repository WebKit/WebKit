// FIXME: use the assert library: https://bugs.webkit.org/show_bug.cgi?id=165684
import Builder from '../Builder.js';

function assert(b) {
    if (!b) {
        throw new Error("Bad assertion");
    }
}

const pageSize = 64 * 1024;
const maxPageCount = (2**32) / pageSize;

function binaryShouldNotParse(builder) {
    const bin = builder.WebAssembly().get();
    let threw = false;
    try {
        const module = new WebAssembly.Module(bin);
    } catch(e) {
        threw = true;
    }
    assert(threw);
}

{
    // Can't declare more than one memory.
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", {initial: 20}).End()
        .Function().End()
        .Memory().InitialMaxPages(1, 1).End()
        .Export().End()
        .Code()
        .End();
    binaryShouldNotParse(builder);
}

{
    // Can't declare more than one memory.
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("imp", "memory", {initial: 20})
            .Memory("imp", "memory", {initial: 30})
        .End()
        .Function().End()
        .Export().End()
        .Code()
        .End();
    binaryShouldNotParse(builder);
}

{
    // initial must be <= maximum.
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("imp", "memory", {initial: 20, maximum: 19})
        .End()
        .Function().End()
        .Export().End()
        .Code()
        .End();
    binaryShouldNotParse(builder);
}

{
    // No loads when no memory defined.
    const builder = (new Builder())
        .Type().End()
        .Import().End()
        .Function().End()
        .Export().Function("foo").End()
        .Code()
            .Function("foo", { params: ["i32"], ret: "i32" })
                .GetLocal(0)
                .I32Load(2, 0)
                .Return()
            .End()
        .End();

    binaryShouldNotParse(builder);
}

{
    // No stores when no memory defined.
    const builder = (new Builder())
        .Type().End()
        .Import()
        .End()
        .Function().End()
        .Export().Function("foo").End()
        .Code()
            .Function("foo", { params: ["i32"] })
                .GetLocal(0)
                .GetLocal(0)
                .I32Store(2, 0)
                .Return()
            .End()
        .End();

    binaryShouldNotParse(builder);
}

{
    let threw = false;
    try {
        new WebAssembly.Memory(20);
    } catch(e) {
        assert(e instanceof TypeError);
        assert(e.message === "WebAssembly.Memory expects its first argument to be an object");
        threw = true;
    }
    assert(threw);
}

{
    let threw = false;
    try {
        new WebAssembly.Memory({}, {});
    } catch(e) {
        assert(e instanceof TypeError);
        assert(e.message === "WebAssembly.Memory expects exactly one argument");
        threw = true;
    }
    assert(threw);
}

function test(f) {
    noInline(f);
    for (let i = 0; i < 2; i++) {
        f(i);
    }
}

test(function() {
    const memoryDescription = {initial: 20, maximum: 20};
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", { params: ["i32", "i32"], ret: "i32" })
                .GetLocal(1)
                .GetLocal(0)
                .I32Store(2, 0)
                .GetLocal(1)
                .I32Load(2, 0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    const instance = new WebAssembly.Instance(module, { imp: { memory: memory } });
    const foo = instance.exports.foo;
    // foo(value, address)

    const bytes = memoryDescription.initial * pageSize;
    for (let i = 0; i < (bytes/4); i++) {
        let value = i + 1;
        let address = i * 4;
        let result = foo(value, address);
        assert(result === value);
        let arrayBuffer = memory.buffer;
        let buffer = new Uint32Array(arrayBuffer);
        assert(buffer[i] === value);
    }
});

test(function() {
    const memoryDescription = {initial: 20, maximum: 20};
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", { params: ["i32", "i32"], ret: "i32"})
                .GetLocal(1)
                .GetLocal(0)
                .I32Store8(0, 0)
                .GetLocal(1)
                .I32Load8U(0, 0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    const instance = new WebAssembly.Instance(module, { imp: { memory: memory } });
    const foo = instance.exports.foo;
    // foo(value, address)

    const bytes = memoryDescription.initial * pageSize;
    for (let i = 0; i < bytes; i++) {
        let value = (i + 1);
        let address = i;
        let result = foo(value, address);
        let expectedValue = (value & ((2**8) - 1)); 
        assert(result === expectedValue);
        let arrayBuffer = memory.buffer;
        let buffer = new Uint8Array(arrayBuffer);
        assert(buffer[i] === expectedValue);
    }
});

test(function() {
    const memoryDescription = {initial: 20, maximum: 20};
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", { params: ["f32", "i32"], ret: "f32"})
                .GetLocal(1)
                .GetLocal(0)
                .F32Store(2, 0)
                .GetLocal(1)
                .F32Load(2, 0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    const instance = new WebAssembly.Instance(module, { imp: { memory: memory } });
    const foo = instance.exports.foo;
    // foo(value, address)

    const bytes = memoryDescription.initial * pageSize;
    for (let i = 0; i < (bytes/4); i++) {
        let value = i + 1 + .0128213781289;
        assert(value !== Math.fround(value));
        let address = i * 4;
        let result = foo(value, address);
        let expectedValue = Math.fround(result);
        assert(result === expectedValue);
        let arrayBuffer = memory.buffer;
        let buffer = new Float32Array(arrayBuffer);
        assert(buffer[i] === expectedValue);
    }
});

test(function() {
    const memoryDescription = {initial: 20, maximum: 20};
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", { params: ["f64", "i32"], ret: "f64"})
                .GetLocal(1)
                .GetLocal(0)
                .F64Store(3, 0)
                .GetLocal(1)
                .F64Load(3, 0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    const instance = new WebAssembly.Instance(module, { imp: { memory: memory } });
    const foo = instance.exports.foo;
    // foo(value, address)

    const bytes = memoryDescription.initial * pageSize;
    for (let i = 0; i < (bytes/8); i++) {
        let value = i + 1 + .0128213781289;
        let address = i * 8;
        let result = foo(value, address);
        let expectedValue = result;
        assert(result === expectedValue);
        let arrayBuffer = memory.buffer;
        let buffer = new Float64Array(arrayBuffer);
        assert(buffer[i] === expectedValue);
    }
});

test(function() {
    const memoryDescription = {initial: 20, maximum: 25};
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", { params: ["f64", "i32"], ret: "f64"})
                .GetLocal(1)
                .GetLocal(0)
                .F64Store(3, 0)
                .GetLocal(1)
                .F64Load(3, 0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);

    function testMemImportError(instanceObj, expectedError) {
        let threw = false;
        try {
            new WebAssembly.Instance(module, instanceObj);
        } catch(e) {
            assert(e instanceof TypeError);
            threw = true;
            if (expectedError) {
                assert(e.message === expectedError);
            }
        }
        assert(threw);
    }

    testMemImportError(20);
    testMemImportError({ });
    testMemImportError({imp: { } });
    testMemImportError({imp: { memory: 20 } });
    testMemImportError({imp: { memory: [] } });
    testMemImportError({imp: { memory: new WebAssembly.Memory({initial: 19, maximum: 25}) } }, "Memory import provided an 'initial' that is too small");
    testMemImportError({imp: { memory: new WebAssembly.Memory({initial: 20}) } }, "Memory import did not have a 'maximum' but the module requires that it does");
    testMemImportError({imp: { memory: new WebAssembly.Memory({initial: 20, maximum: 26}) } }, "Memory imports 'maximum' is larger than the module's expected 'maximum");
});

test(function() {
    const memoryDescription = {initial: 20};
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", memoryDescription).End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", { params: ["f64", "i32"], ret: "f64"})
                .GetLocal(1)
                .GetLocal(0)
                .F64Store(3, 0)
                .GetLocal(1)
                .F64Load(3, 0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);

    function testMemImportError(instanceObj, expectedError) {
        let threw = false;
        try {
            new WebAssembly.Instance(module, instanceObj);
        } catch(e) {
            assert(e instanceof TypeError);
            threw = true;
            if (expectedError) {
                assert(e.message === expectedError);
            }
        }
        assert(threw);
    }

    testMemImportError({imp: { memory: new WebAssembly.Memory({initial: 19, maximum: 25}) } }, "Memory import provided an 'initial' that is too small");
    testMemImportError({imp: { memory: new WebAssembly.Memory({initial: 19}) } }, "Memory import provided an 'initial' that is too small");

    // This should not throw.
    new WebAssembly.Instance(module, {imp: {memory: new WebAssembly.Memory({initial:20})}});
    new WebAssembly.Instance(module, {imp: {memory: new WebAssembly.Memory({initial:20, maximum:20})}});
});
