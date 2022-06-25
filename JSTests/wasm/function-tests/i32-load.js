import Builder from '../Builder.js'
import * as assert from '../assert.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Memory().InitialMaxPages(1, 1).End()
    .Export().Function('f0').End()
    .Code()
    .Function('f0', { params: ["i32", "i32"], ret: "i32" })
    .GetLocal(1)
    .GetLocal(0)
    .I32Store(2, 0)
    .GetLocal(1)
    .I32Load(2, 0)
    .Return()
    .End()
    .End();

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
testWasmModuleFunctions([[{type: "i32", value: 0 }, [{ type: "i32", value: 0 }, { type: "i32", value: 10 }]],
                                       [{type: "i32", value: 100 }, [{ type: "i32", value: 100 }, { type: "i32", value: 112 }]],
                                       [{type: "i32", value: 1000000 }, [{ type: "i32", value: 1000000 }, { type: "i32", value: 10 }]]
                                      ]);
