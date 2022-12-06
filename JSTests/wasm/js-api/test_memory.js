//@ skip if $architecture != "arm64" && $architecture != "x86_64"
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
    new WebAssembly.Memory({initial:1}, {});
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

    assert.throws(() => new WebAssembly.Instance(module, 20), TypeError, `second argument to WebAssembly.Instance must be undefined or an Object`);
    assert.throws(() => new WebAssembly.Instance(module, {}), TypeError, `import imp:memory must be an object`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { } }), WebAssembly.LinkError, `Memory import imp:memory is not an instance of WebAssembly.Memory`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: 20 } }), WebAssembly.LinkError, `Memory import imp:memory is not an instance of WebAssembly.Memory`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: [] } }), WebAssembly.LinkError, `Memory import imp:memory is not an instance of WebAssembly.Memory`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: new WebAssembly.Memory({initial: 19, maximum: 25}) } }), WebAssembly.LinkError, `Memory import imp:memory provided an 'initial' that is smaller than the module's declared 'initial' import memory size`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: new WebAssembly.Memory({initial: 20}) } }), WebAssembly.LinkError, `Memory import imp:memory did not have a 'maximum' but the module requires that it does`);
    assert.throws(() => new WebAssembly.Instance(module, {imp: { memory: new WebAssembly.Memory({initial: 20, maximum: 26}) } }), WebAssembly.LinkError, `Memory import imp:memory provided a 'maximum' that is larger than the module's declared 'maximum' import memory size`);
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

    testMemImportError({imp: { memory: new WebAssembly.Memory({initial: 19, maximum: 25}) } }, `Memory import imp:memory provided an 'initial' that is smaller than the module's declared 'initial' import memory size`);
    testMemImportError({imp: { memory: new WebAssembly.Memory({initial: 19}) } }, `Memory import imp:memory provided an 'initial' that is smaller than the module's declared 'initial' import memory size`);

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

test(function() {
    assert.throws(() => {
        const m = new WebAssembly.Memory({minimum:40, maximum: 100});
        m.type.call({});
    }, TypeError, "WebAssembly.Memory.prototype.buffer getter called with non WebAssembly.Memory |this| value");

    const memory = new WebAssembly.Memory({initial: 20});
    assert.eq(Object.keys(memory.type()).length, 2);
    assert.eq(memory.type().minimum, 20);
    assert.eq(memory.type().shared, false);

    const memory2 = new WebAssembly.Memory({minimum:40, maximum: 100});
    assert.eq(Object.keys(memory2.type()).length, 3);
    assert.eq(memory2.type().minimum, 40);
    assert.eq(memory2.type().maximum, 100);
    assert.eq(memory.type().shared, false);

    const memory3 = new WebAssembly.Memory(memory2.type());
    assert.eq(Object.keys(memory2.type()).length, Object.keys(memory3.type()).length);
    assert.eq(memory2.type().minimum, memory3.type().minimum);
    assert.eq(memory2.type().maximum, memory3.type().maximum);
    assert.eq(memory.type().shared, false);

    const memory4 = new WebAssembly.Memory({minimum: 10, maximum: 20, shared:true});
    assert.eq(Object.keys(memory4.type()).length, 3);
    assert.eq(memory4.type().minimum, 10);
    assert.eq(memory4.type().maximum, 20);
    assert.eq(memory4.type().shared, true);
})
