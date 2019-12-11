import Builder from '../Builder.js'
import * as assert from '../assert.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Export().Function("f0").End()
    .Code()
    .Function("f0", { params: ["i32", "i32"], ret: "i32" })
    .Loop("void")
    .Block("void", b =>
           b.Block("void", b =>
                   b.GetLocal(0)
                   .GetLocal(1)
                   .I32Eq()
                   .BrIf(0)
                   .Br(1)
                  )
           .I32Const(0)
           .Return()
          )
    .Block("void", b =>
           b.Block("void", b =>
                   b.GetLocal(0)
                   .I32Const(0)
                   .I32Eq()
                   .BrIf(0)
                   .Br(1)
                  )
           .I32Const(1)
           .Return()
          )
    .GetLocal(0)
    .I32Const(1)
    .I32Sub()
    .SetLocal(0)
    .Br(0)
    .End()
    .Unreachable()
    .End()
    .End()

const bin = b.WebAssembly().get();
const instance = new WebAssembly.Instance(new WebAssembly.Module(bin));

function testWasmModuleFunctions(...tests) {
    for (let i = 0; i < tests.length; i++) {
        const func = instance.exports['f' + i];
        for (let test of tests[i]) {
            let result = test[0].value;
            let args = test[1].map(x => x.value);
            assert.eq(result, func(...args));
        }
    }
}

testWasmModuleFunctions([[{type: "i32", value: 1 }, [{ type: "i32", value: 0 }, { type: "i32", value: 1 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 1 }, { type: "i32", value: 0 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 2 }, { type: "i32", value: 1 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 1 }, { type: "i32", value: 2 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 2 }, { type: "i32", value: 2 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 1 }, { type: "i32", value: 1 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 2 }, { type: "i32", value: 6 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 100 }, { type: "i32", value: 6 }]]
                                      ]);
