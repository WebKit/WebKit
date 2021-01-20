import * as assert from '../assert.js';
import Builder from '../Builder.js';

async function throwExn() {
    throw new Error();
}

async function test() {
    const loopDepth = 10;
    const numCompilations = 1;
    const numVars = 30;
    const params = [];
    params.length = numVars;
    params.fill("i32");

    let builder = (new Builder())
          .Type().End()
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
        return builder

    }

    builder = makeLoop(builder, loopDepth);
    builder = builder.End().End();

    const bin = builder.WebAssembly().get();

    let compilations = [];
    for (let i = 0; i < numCompilations; ++i) {
        compilations.push(WebAssembly.compile(bin));
    }

    await Promise.all(compilations);
}

assert.asyncTest(test());
