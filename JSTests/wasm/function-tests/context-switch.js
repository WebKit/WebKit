import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    function makeInstance() {
        const tableDescription = {initial: 1, element: "anyfunc"};
        const builder = new Builder()
            .Type()
                .Func([], "void")
            .End()
            .Import()
                .Table("imp", "table", tableDescription)
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", {params:["i32"], ret:"void"})
                    .GetLocal(0) // parameter to call
                    .GetLocal(0) // call index
                    .CallIndirect(0, 0) // calling function of type [] => 'void'
                    .Return()
                .End()
            .End();


        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        const table = new WebAssembly.Table(tableDescription);
        return {instance: new WebAssembly.Instance(module, {imp: {table}}), table};
    }

    function makeInstance2(f) {
        const builder = new Builder()
            .Type()
            .End()
            .Import()
                .Function("imp", "f", {params:[], ret:"void"})
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", {params: [], ret: "void" })
                    .Call(0)
                    .Return()
                .End()
            .End();


        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        return new WebAssembly.Instance(module, {imp: {f}});
    }

    const {instance: i1, table: t1} = makeInstance();
    const foo = i1.exports.foo;

    let called = false;
    let shouldThrow = false;
    const i2 = makeInstance2(function() {
        called = true;
        if (shouldThrow)
            throw new Error("Threw");
    });

    t1.set(0, i2.exports.foo);
    for (let i = 0; i < 1000; ++i) {
        foo(0);
        assert.eq(called, true);
        called = false;
    }
    shouldThrow = true;
    for (let i = 0; i < 1000; ++i) {
        assert.throws(() => foo(0), Error, "Threw");
        assert.eq(called, true);
        called = false;
    }
}

{
    function makeInstance() {
        const tableDescription = {initial: 1, element: "anyfunc"};
        const builder = new Builder()
            .Type()
                .Func(["i32"], "void")
            .End()
            .Import()
                .Table("imp", "table", tableDescription)
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", {params:["i32", "i32"], ret:"void"})
                    .GetLocal(1) // parameter to call
                    .GetLocal(0) // call index
                    .CallIndirect(0, 0) // calling function of type ['i32'] => 'void'
                    .Return()
                .End()
            .End();

        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        const table = new WebAssembly.Table(tableDescription);
        return {instance: new WebAssembly.Instance(module, {imp: {table}}), table};
    }

    function makeInstance2(memory, f) {
        const builder = new Builder()
            .Type()
            .End()
            .Import()
                .Function("imp", "f", {params:['i32', 'i32'], ret:"void"})
                .Memory("imp", "memory", memoryDescription)
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", {params: ["i32"], ret: "void" })
                    .GetLocal(0)
                    .GetLocal(0)
                    .I32Load(2, 0)
                    .Call(0)
                    .Return()
                .End()
            .End();


        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        return new WebAssembly.Instance(module, {imp: {f, memory}});
    }

    const {instance: i1, table: t1} = makeInstance();
    const foo = (memOffset) => i1.exports.foo(0, memOffset);

    const memoryDescription = {initial:1};
    const mem = new WebAssembly.Memory(memoryDescription);
    let called = false;

    const pageSize = 64 * 1024;
    let buf = new Uint32Array(mem.buffer);
    const i2 = makeInstance2(mem, function(i, value) {
        assert.eq(i & 3, 0);
        assert.eq(buf[i/4], value);
        called = true;
    });
    t1.set(0, i2.exports.foo);
    
    for (let i = 0; i < pageSize/4; ++i) {
        buf[i] = i+1;
    }

    for (let i = 0; i < pageSize/4; ++i) {
        foo(i*4);
        assert.eq(called, true);
        called = false;
    }

    for (let i = pageSize/4; i < 2*pageSize/4; ++i) {
        assert.throws(() => foo(i*4), WebAssembly.RuntimeError, "Out of bounds memory access");
        assert.eq(called, false);
    }
}

{
    function makeInstance() {
        const tableDescription = {initial: 1, element: "anyfunc"};
        const builder = new Builder()
            .Type()
                .Func(["i32"], "void")
            .End()
            .Import()
                .Table("imp", "table", tableDescription)
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", {params:["i32", "i32"], ret:"void"})
                    .GetLocal(1) // parameter to call
                    .GetLocal(0) // call index
                    .CallIndirect(0, 0) // calling function of type ['i32'] => 'void'
                    .Return()
                .End()
            .End();

        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        const table = new WebAssembly.Table(tableDescription);
        return {instance: new WebAssembly.Instance(module, {imp: {table}}), table};
    }

    function makeInstance2(memory, f) {
        const builder = new Builder()
            .Type()
            .End()
            .Import()
                .Function("imp", "f", {params:['i32', 'i32'], ret:"void"})
                .Memory("imp", "memory", memoryDescription)
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", {params: ["i32"], ret: "void" })
                    .GetLocal(0)
                    .GetLocal(0)
                    .I32Load(2, 0)
                    .Call(0)
                    .Return()
                .End()
            .End();


        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        return new WebAssembly.Instance(module, {imp: {f, memory}});
    }

    function exportImport(f) {
        let builder = (new Builder())
            .Type().End()
            .Import()
                .Function("imp", "f", {params: ['i32'], ret:"void"})
            .End()
            .Function().End()
            .Export()
                .Function("func", {module: "imp", field: "f"})
            .End()
            .Code().End();
        return (new WebAssembly.Instance(new WebAssembly.Module(builder.WebAssembly().get()), {imp: {f}})).exports.func;
    }

    const {instance: i1, table: t1} = makeInstance();
    const foo = (memOffset) => i1.exports.foo(0, memOffset);

    const memoryDescription = {initial:1};
    const mem = new WebAssembly.Memory(memoryDescription);
    let called = false;

    const pageSize = 64 * 1024;
    let buf = new Uint32Array(mem.buffer);
    const i2 = makeInstance2(mem, function(i, value) {
        assert.eq(i & 3, 0);
        assert.eq(buf[i/4], value);
        called = true;
    });

    const exportedImport = exportImport(function(offset) {
        i2.exports.foo(offset);
    });
    t1.set(0, exportedImport);
    
    for (let i = 0; i < pageSize/4; ++i) {
        buf[i] = i+1;
    }

    for (let i = 0; i < pageSize/4; ++i) {
        foo(i*4);
        assert.eq(called, true);
        called = false;
    }

    for (let i = pageSize/4; i < 2*pageSize/4; ++i) {
        assert.throws(() => foo(i*4), WebAssembly.RuntimeError, "Out of bounds memory access");
        assert.eq(called, false);
    }
}
