import Builder from '../Builder.js';
import * as assert from '../assert.js';

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
    assert.truthy(threw);
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
    // Can't export an undefined memory.
    const builder = (new Builder())
        .Type().End()
        .Function().End()
        .Export()
            .Memory("memory", 0)
        .End()
        .Code()
        .End();
    binaryShouldNotParse(builder);
}

{
    // Can't export a non-zero memory index.
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", {initial: 20}).End()
        .Function().End()
        .Export()
            .Memory("memory", 1)
        .End()
        .Code()
        .End();
    binaryShouldNotParse(builder);
}

{
    assert.throws(() => new WebAssembly.Memory(20), TypeError, "WebAssembly.Memory expects its first argument to be an object"); 
    assert.throws(() => new WebAssembly.Memory({}, {}), TypeError,  "WebAssembly.Memory expects exactly one argument");
}

function test(f) {
    noInline(f);
    for (let i = 0; i < 2; i++) {
        f(i);
    }
}

test(function() {
    const memoryDescription = {initial: 2, maximum: 2};
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
        assert.truthy(result === value);
        let arrayBuffer = memory.buffer;
        let buffer = new Uint32Array(arrayBuffer);
        assert.truthy(buffer[i] === value);
    }
});

test(function() {
    const memoryDescription = {initial: 2, maximum: 2};
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
        assert.truthy(result === expectedValue);
        let arrayBuffer = memory.buffer;
        let buffer = new Uint8Array(arrayBuffer);
        assert.truthy(buffer[i] === expectedValue);
    }
});

test(function() {
    const memoryDescription = {initial: 2, maximum: 2};
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
        assert.truthy(value !== Math.fround(value));
        let address = i * 4;
        let result = foo(value, address);
        let expectedValue = Math.fround(result);
        assert.truthy(result === expectedValue);
        let arrayBuffer = memory.buffer;
        let buffer = new Float32Array(arrayBuffer);
        assert.truthy(buffer[i] === expectedValue);
    }
});

test(function() {
    const memoryDescription = {initial: 2, maximum: 2};
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
        assert.truthy(result === expectedValue);
        let arrayBuffer = memory.buffer;
        let buffer = new Float64Array(arrayBuffer);
        assert.truthy(buffer[i] === expectedValue);
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

    assert.throws(() => new WebAssembly.Instance(module, 20), TypeError, `second argument to WebAssembly.Instance must be undefined or an Object (evaluating 'new WebAssembly.Instance(module, 20)')`);
    assert.throws(() => new WebAssembly.Instance(module, {}), TypeError, `import must be an object (evaluating 'new WebAssembly.Instance(module, {})')`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { } }), WebAssembly.LinkError, `Memory import is not an instance of WebAssembly.Memory (evaluating 'new WebAssembly.Instance(module, {imp: { } })')`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: 20 } }), WebAssembly.LinkError, `Memory import is not an instance of WebAssembly.Memory (evaluating 'new WebAssembly.Instance(module, {imp: { memory: 20 } })')`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: [] } }), WebAssembly.LinkError, `Memory import is not an instance of WebAssembly.Memory (evaluating 'new WebAssembly.Instance(module, {imp: { memory: [] } })')`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: new WebAssembly.Memory({initial: 19, maximum: 25}) } }), WebAssembly.LinkError, `Memory import provided an 'initial' that is too small (evaluating 'new WebAssembly.Instance(module, {imp: { memory: new WebAssembly.Memory({initial: 19, maximum: 25}) } })')`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: new WebAssembly.Memory({initial: 20}) } }), WebAssembly.LinkError, `Memory import did not have a 'maximum' but the module requires that it does (evaluating 'new WebAssembly.Instance(module, {imp: { memory: new WebAssembly.Memory({initial: 20}) } })')`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: new WebAssembly.Memory({initial: 20, maximum: 26}) } }), WebAssembly.LinkError, `Memory imports 'maximum' is larger than the module's expected 'maximum' (evaluating 'new WebAssembly.Instance(module, {imp: { memory: new WebAssembly.Memory({initial: 20, maximum: 26}) } })')`);
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
        assert.throws(() => new WebAssembly.Instance(module, instanceObj), WebAssembly.LinkError, expectedError);
    }

    testMemImportError({imp: { memory: new WebAssembly.Memory({initial: 19, maximum: 25}) } }, "Memory import provided an 'initial' that is too small (evaluating 'new WebAssembly.Instance(module, instanceObj)')");
    testMemImportError({imp: { memory: new WebAssembly.Memory({initial: 19}) } }, "Memory import provided an 'initial' that is too small (evaluating 'new WebAssembly.Instance(module, instanceObj)')");

    // This should not throw.
    new WebAssembly.Instance(module, {imp: {memory: new WebAssembly.Memory({initial:20})}});
    new WebAssembly.Instance(module, {imp: {memory: new WebAssembly.Memory({initial:20, maximum:20})}});
});

{
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", {initial: 20}).End()
        .Function().End()
        .Export()
            .Memory("memory", 0)
            .Memory("memory2", 0)
        .End()
        .Code()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory({initial: 20});
    const instance = new WebAssembly.Instance(module, {imp: {memory}});
    assert.truthy(memory === instance.exports.memory);
    assert.truthy(memory === instance.exports.memory2);
}

{
    const builder = (new Builder())
        .Type().End()
        .Function().End()
        .Memory().InitialMaxPages(20, 25).End()
        .Export()
            .Memory("memory", 0)
            .Memory("memory2", 0)
        .End()
        .Code()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);
    assert.eq(instance.exports.memory, instance.exports.memory2);
    assert.eq(instance.exports.memory.buffer.byteLength, 20 * pageSize);
    assert.truthy(instance.exports.memory instanceof WebAssembly.Memory);
}
