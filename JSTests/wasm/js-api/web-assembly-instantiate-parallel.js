import * as assert from '../assert.js';
import Builder from '../Builder.js';

async function test() {
    const loopDepth = 100;
    const numCompilations = 1;
    const numVars = 30;
    const params = [];
    params.length = numVars;
    params.fill("i32");

    let builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("imp", "memory", { initial: 0 })
        .End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
        .Function("foo", { params, ret: "i32" });

    const makeLoop = (builder, depth) => {
        if (depth === 0)
            return builder;

        builder = builder
            .Loop("i32", (b) => {
                b.GetLocal(0)
                    .I32Const(1)
                    .I32Sub()
                    .TeeLocal(0)
                    .GetLocal(0)
                    .I32Eqz()
                    .BrIf(1);

                return makeLoop(b, depth - 1).Br(0);
            });
        return builder;
    }

    builder = makeLoop(builder, loopDepth);
    builder = builder.End().End();

    const bin = builder.WebAssembly().get();
    const memory = new WebAssembly.Memory({ initial: 0 });
    const importObject = { "imp": { "memory": memory } };

    // Compile a bunch of instances in parallel.
    let compilations = [];
    for (let i = 0; i < numCompilations; ++i) {
        compilations.push(WebAssembly.instantiate(bin, importObject));
    }

    let [inst] = await Promise.all(compilations);
    let module = inst.module;

    // Compile a bunch of instances from modules in parallel.
    compilations = [];
    for (let i = 0; i < numCompilations; ++i) {
        compilations.push(WebAssembly.instantiate(module, importObject));
    }

    await Promise.all(compilations);

    // Compile an instance from a module in parallel, have sync compilation steal it.
    compilations = [];
    compilations.push(WebAssembly.instantiate(module, importObject));
    inst = new WebAssembly.Instance(module, importObject);

    await Promise.all(compilations);
}

assert.asyncTest(test());
