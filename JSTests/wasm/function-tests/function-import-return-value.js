import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    let called = false;

    const tests = {
        i32: [
            [20, (x) => assert.eq(x, 20)],
            [20.888, (x) => assert.eq(x, 20.888 | 0)],
            [Math.PI, (x) => assert.eq(x, Math.PI | 0)],
            [{valueOf() { assert.truthy(!called); called = true; return 25; } }, (x) => { assert.truthy(called); assert.eq(x, 25); called = false; }],
            [NaN, (x) => assert.eq(x, NaN | 0)],
            [-0.0, (x) => assert.eq(1/x, Infinity)],
            [undefined, (x) => assert.eq(x, undefined | 0)],
            [null, (x) => assert.eq(x, null | 0)],
            [Number.MAX_SAFE_INTEGER, (x) => assert.eq(x, Number.MAX_SAFE_INTEGER | 0)],
            [2**32 - 1, (x) => assert.eq(x, (2**32 - 1) | 0)],
            [2**32 - 1000, (x) => assert.eq(x, (2**32 - 1000) | 0)],
            [-1000, (x) => assert.eq(x, -1000)],
        ],
        f32: [
            [20, (x) => assert.eq(x, 20)],
            [20.888, (x) => assert.eq(x, Math.fround(20.888))],
            [Math.PI, (x) => assert.eq(x, Math.fround(Math.PI))],
            [{valueOf() { assert.truthy(!called); called = true; return 25.82; } }, (x) => { assert.truthy(called); assert.eq(x, Math.fround(25.82)); called = false; }],
            [NaN, (x) => assert.truthy(isNaN(x))],
            [-0.0, (x) => assert.eq(1/x, -Infinity)],
            [undefined, (x) => assert.truthy(isNaN(x))],
            [null, (x) => assert.eq(x, 0)],
            [Number.MAX_SAFE_INTEGER, (x) => assert.eq(x, Math.fround(Number.MAX_SAFE_INTEGER))],
            [-1000, (x) => assert.eq(x, -1000)],
        ],
        f64: [
            [20, (x) => assert.eq(x, 20)],
            [2**24, (x) => assert.eq(x, 2**24)],
            [2**52, (x) => assert.eq(x, 2**52)],
            [20.8888888, (x) => assert.eq(x, 20.8888888)],
            [Math.PI, (x) => assert.eq(x, Math.PI)],
            [{valueOf() { assert.truthy(!called); called = true; return 25.82; } }, (x) => { assert.truthy(called); assert.eq(x, 25.82); called = false; }],
            [NaN, (x) => assert.truthy(isNaN(x))],
            [-0.0, (x) => assert.eq(1/x, -Infinity)],
            [undefined, (x) => assert.truthy(isNaN(x))],
            [null, (x) => assert.eq(x, 0)],
            [Number.MAX_SAFE_INTEGER, (x) => assert.eq(x, Number.MAX_SAFE_INTEGER)],
            [-1000, (x) => assert.eq(x, -1000)],
        ]
    };

    for (let type of Reflect.ownKeys(tests)) {
        const builder = new Builder()
            .Type().End()
            .Import()
                .Function("imp", "func", { params: [], ret: type})
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", {params: [], ret: type})
                    .Call(0)
                    .Return()
                .End()
            .End();


        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);

        for (let test of tests[type]) {
            const func = () => {
                return test[0];
            };

            const instance = new WebAssembly.Instance(module, {imp: {func}});
            const ret = instance.exports.foo();
            test[1](ret);
        }
    }
}

{
    let types = ["i32", "f32", "f64"];
    for (let type of types) {
        const builder = new Builder()
            .Type().End()
            .Import()
                .Function("imp", "func", { params: [], ret: type})
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", {params: [], ret: type})
                    .Call(0)
                    .Unreachable()
                .End()
            .End();


        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        let error = null;
        const func = () => {
            return {
                valueOf() {
                    error = new Error;
                    throw error;
                }
            };
        };

        const instance = new WebAssembly.Instance(module, {imp: {func}});
        for (let i = 0; i < 100; i++) {
            let threw = false;
            try {
                instance.exports.foo();
            } catch(e) {
                assert.eq(e, error);
                threw = true;
                error = null;
            }
            assert.truthy(threw);
        }
    }
}

{
    const builder = new Builder()
        .Type().End()
        .Import()
            .Function("imp", "func", { params: [], ret: "i64"})
        .End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: [], ret: "void"})
                .Call(0)
                .Drop()
                .Return()
            .End()
        .End();


    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const func = () => 20;
    const instance = new WebAssembly.Instance(module, {imp: {func}});
    for (let i = 0; i < 100; i++) {
        assert.throws(() => instance.exports.foo(), TypeError, "i64 not allowed as return type or argument to an imported function");
    }
}

{
    const builder = new Builder()
        .Type().End()
        .Import()
            .Function("imp", "func", { params: ["i64"], ret: "void"})
        .End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: [], ret: "void"})
                 .I64Const(20)
                .Call(0)
                .Return()
            .End()
        .End();


    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const func = () => 20;
    const instance = new WebAssembly.Instance(module, {imp: {func}});
    for (let i = 0; i < 100; i++) {
        assert.throws(() => instance.exports.foo(), TypeError, "i64 not allowed as return type or argument to an imported function");
    }
}

{
    const builder = new Builder()
        .Type().End()
        .Import()
            .Function("imp", "func", { params: ["i64"], ret: "void"})
        .End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: [], ret: "void"})
                 .I64Const(20)
                .Call(0)
                .Return()
            .End()
        .End();


    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    let called = false;
    const func = () => {
        called = true;
    }
    const instance = new WebAssembly.Instance(module, {imp: {func}});
    for (let i = 0; i < 100; i++) {
        assert.throws(() => instance.exports.foo(), TypeError, "i64 not allowed as return type or argument to an imported function");
        assert.eq(called, false);
    }
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: ["i64"], ret: "void"})
                 .I64Const(20)
                .Call(0)
                .Return()
            .End()
        .End();


    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    let called = false;
    let value = {
        valueOf() {
            called = true;
        }
    };
    const instance = new WebAssembly.Instance(module);
    for (let i = 0; i < 100; i++) {
        assert.throws(() => instance.exports.foo(value), Error, "WebAssembly function with an i64 argument can't be called from JavaScript");
        assert.eq(called, false);
    }
}
