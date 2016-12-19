import Builder from '../Builder.js'

function assert(b) {
    if (!b)
        throw new Error("Bad")
}

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

    for (let i = 0; i < 10000; i++) {
        let threw = false;
        try {
            foo(i, numPages * pageSize + 1);
        } catch(e) {
            assert(e instanceof WebAssembly.RuntimeError);
            assert(e.message === "Out of bounds memory access");
            threw = true;
        }
        assert(threw);
    }
}
