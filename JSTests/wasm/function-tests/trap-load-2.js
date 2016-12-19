import Builder from '../Builder.js'

const pageSize = 64 * 1024;
const numPages = 10;

const builder = (new Builder())
    .Type().End()
    .Import()
        .Memory("a", "b", {initial: numPages})
    .End()
    .Function().End()
    .Export().Function("foo").End()
    .Code()
        .Function("foo", {params: ["i32"], ret: "i32"})
            .GetLocal(0)
            .I32Load(2, 0)
            .Return()
        .End()
    .End();

function assert(b) {
    if (!b)
        throw new Error("Bad")
}

function wasmFrameCountFromError(e) {
    let stackFrames = e.stack.split("\n").filter((s) => s.indexOf("<wasm>@[wasm code]") !== -1);
    return stackFrames.length;
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
                    .GetLocal(1)
                    .I32Load(2, 0)
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

    for (let i = 0; i < 5000; i++) {
        let threw = false;
        try {
            foo(5, address);
        } catch(e) {
            assert(e instanceof WebAssembly.RuntimeError);
            assert(e.message === "Out of bounds memory access");
            // There are 5 total calls, and each call does:
            // JS entry, wasm entry, js call stub.
            // The last call that traps just has JS entry and wasm entry.
            assert(wasmFrameCountFromError(e) === 5 * 3 + 2);
            threw = true;
        }
        assert(threw);
    }
}
