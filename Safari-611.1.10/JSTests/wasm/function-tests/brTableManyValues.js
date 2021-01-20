import Builder from '../Builder.js'
import * as assert from '../assert.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Export().Function('f0').End()
    .Code()
    .Function('f0', { params: ["i32"], ret: "i32" }, ["i32"])
    .Block("i32", (b) =>
           b.Block("i32", (b) =>
                   b.Block("i32", (b) =>
                           b.Block("i32", (b) =>
                                   b.Block("i32", (b) =>
                                           b.I32Const(200)
                                           .GetLocal(0)
                                           .BrTable(3, 2, 1, 0, 4)
                                          ).I32Const(10)
                                   .I32Add()
                                   .Return()
                                  ).I32Const(11)
                           .I32Add()
                           .Return()
                          ).I32Const(12)
                   .I32Add()
                   .Return()
                  ).I32Const(13)
           .I32Add()
           .Return()
          ).I32Const(14)
    .I32Add()
    .Return()
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
testWasmModuleFunctions([[{type: "i32", value: 213 }, [{ type: "i32", value: 0 }]],
                                       [{type: "i32", value: 212 }, [{ type: "i32", value: 1 }]],
                                       [{type: "i32", value: 211 }, [{ type: "i32", value: 2 }]],
                                       [{type: "i32", value: 210 }, [{ type: "i32", value: 3 }]],
                                       [{type: "i32", value: 214 }, [{ type: "i32", value: 4 }]],
                                       [{type: "i32", value: 214 }, [{ type: "i32", value: 5 }]],
                                       [{type: "i32", value: 214 }, [{ type: "i32", value: -1 }]],
                                       [{type: "i32", value: 214 }, [{ type: "i32", value: -1000 }]]
                                      ]);
