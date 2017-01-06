import Builder from '../Builder.js'
import * as assert from '../assert.js'

const pageSize = 64 * 1024;
const numPages = 10;

{
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("a", "b", {initial: numPages})
        .End()
        .Function().End()
        .Export().Function("foo").End()
        .Code()
            .Function("foo", {params: ["i32", "i32"]})
                .GetLocal(1)
                .GetLocal(0)
                .I32Store(2, 0)
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const foo = new WebAssembly.Instance(module, {a: {b: new WebAssembly.Memory({initial: numPages})}}).exports.foo;

    for (let i = 0; i < 10000; i++)
        assert.throws(() => foo(i, numPages * pageSize + 1), WebAssembly.RuntimeError, "Out of bounds memory access");
}


{
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("imp", "mem", {initial: numPages})
            .Function("imp", "func", { params: ["i32"] })
        .End()
        .Function().End()
        .Export().Function("foo").End()
        .Code()
            .Function("foo", {params: ["i32", "i32"]})
                .GetLocal(0)
                .I32Const(0)
                .I32Eq()
                .If("void", b =>
                    b.GetLocal(1)
                    .GetLocal(0)
                    .I32Store(2, 0)
                    .Br(0)
                    .Else()
                        .GetLocal(0)
                        .Call(0)
                    .Br(0)
                   )
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const imp = {
        imp: {
            mem: new WebAssembly.Memory({initial: numPages}),
            func: continuation
        }
    };
    const foo = new WebAssembly.Instance(module, imp).exports.foo;
    const address = numPages*pageSize + 1;
    function continuation(x) {
        foo(x - 1, address);
    }

    for (let i = 0; i < 10000; i++) {
        let threw = false;
        try {
            foo(25, address);
        } catch(e) {
            assert(e instanceof WebAssembly.RuntimeError);
            assert(e.message === "Out of bounds memory access");
            threw = true;
        }
        assert(threw);
    }
}
