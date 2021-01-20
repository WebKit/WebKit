import Builder from '../Builder.js'
import * as assert from '../assert.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Export().Function('f0').End()
    .Code()
        .Function('f0', { params: ["i32"], ret: "i32" }, ["i32"])
        .Block("void", (b) =>
               b.Block("void", (b) =>
                       b.GetLocal(0)
                       .BrTable(1, 0)
                       .I32Const(21)
                       .Return()
                      ).I32Const(20)
               .Return()
              ).I32Const(22)
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
testWasmModuleFunctions([[{type: "i32", value: 22 }, [{ type: "i32", value: 0 }]],
                                       [{type: "i32", value: 20 }, [{ type: "i32", value: 1 }]],
                                       [{type: "i32", value: 20 }, [{ type: "i32", value: 11 }]],
                                       [{type: "i32", value: 20 }, [{ type: "i32", value: -100 }]]
                                      ]);
