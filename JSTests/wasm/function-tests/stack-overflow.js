import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    function makeInstance() {
        const tableDescription = {initial: 1, element: "funcref"};
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
                .Function("foo", 0 /*['i32'] => 'void'*/)
                    .GetLocal(0) // parameter to call
                    .GetLocal(0) // call index
                    .CallIndirect(0, 0) // calling function of type ['i32'] => 'i32'
                    .Return()
                .End()
            .End();


        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        const table = new WebAssembly.Table(tableDescription);
        return {instance: new WebAssembly.Instance(module, {imp: {table}}), table};
    }

    const {instance: i1, table: t1} = makeInstance();
    const {instance: i2, table: t2} = makeInstance();
    t2.set(0, i1.exports.foo);
    t1.set(0, i2.exports.foo);

    function assertOverflows(instance) {
        let stack;
        try {
            instance.exports.foo(0)
        } catch(e) {
            stack = e.stack;
        }
        stack = stack.split("\n");
        assert.truthy(stack.length > 50);
        for (let i = 0; i < 50; ++i) {
            let item = stack[stack.length - i - 1];
            assert.eq(item, '0@wasm-function[0]');
        } 
    }
    assertOverflows(i1);
    assertOverflows(i2);

}

{
    function makeInstance() {
        const tableDescription = {initial: 1, element: "funcref"};
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
                .Function("imp", "f", {params:['i32'], ret:"void"})
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Code()
                .Function("foo", {params: [], ret: "void" })
                    .I32Const(0)
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
    const i2 = makeInstance2(i1.exports.foo);
    t1.set(0, i2.exports.foo);

    function assertThrows(instance) {
        let stack;
        try {
            instance.exports.foo();
        } catch(e) {
            stack = e.stack;
        }
        assert.truthy(stack);

        stack = stack.split("\n");
        assert.truthy(stack.length > 50);
        const one = '1@wasm-function[1]';
        const zero = '0@wasm-function[0]';
        let currentIndex = (one === stack[stack.length - 1]) ? 1 : 0;
        for (let i = 0; i < 50; ++i) {
            let item = stack[stack.length - 1 - i];
            if (currentIndex === 1) {
                assert.eq(item, one);
                currentIndex = 0;
            } else {
                assert.eq(currentIndex, 0);
                assert.eq(item, zero);
                currentIndex = 1;
            }
        }
    }

    for (let i = 0; i < 20; ++i) {
        assertThrows(i2);
        assertThrows(i1);
    }

    for (let i = 0; i < 20; ++i) {
        assertThrows(i1);
        assertThrows(i2);
    }
}

{
    function test(numArgs) {
        function makeSignature() {
            let args = [];
            for (let i = 0; i < numArgs; ++i) {
                args.push("i32");
            }
            return {params: args};
        }
        function makeInstance(f) {
            let builder = new Builder()
                .Type()
                    .Func([], "void")
                .End()
                .Import()
                    .Function("imp", "f", makeSignature())
                .End()
                .Function().End()
                .Export()
                    .Function("foo")
                .End()
                .Code()
                    .Function("foo", {params:[], ret:"void"});
            for (let i = 0; i < numArgs; ++i) {
                builder = builder.I32Const(i);
            }

            builder = builder.Call(0).Return().End().End();
            const bin = builder.WebAssembly().get();
            const module = new WebAssembly.Module(bin);
            return new WebAssembly.Instance(module, {imp: {f}});
        }

        function f(...args) {
            assert.eq(args.length, numArgs);
            for (let i = 0; i < args.length; ++i)
                assert.eq(args[i], i);

            instance.exports.foo();
        }
        let instance = makeInstance(f);

        let stack;
        try {
            instance.exports.foo();
        } catch(e) {
            stack = e.stack;
        }
        assert.truthy(stack.split("\n").length > 25);
    }

    for (let i = 0; i < 70; ++i) {
        let r = Math.random() * 1000 | 0;
        test(r);
    }

    test(20);
    test(20);
    test(1000);
    test(2);
    test(1);
    test(0);
    test(700);
    test(433);
    test(42);
}
