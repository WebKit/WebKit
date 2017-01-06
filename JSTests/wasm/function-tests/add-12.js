import Builder from '../Builder.js'
import * as assert from '../assert.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Export().Function("f0").Function("f1").End()
    .Code()
        .Function("f0", { params: ["i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32"], ret: "i32" })
            .GetLocal(0)
            .GetLocal(1)
            .I32Add()
            .GetLocal(2)
            .I32Add()
            .GetLocal(3)
            .I32Add()
            .GetLocal(4)
            .I32Add()
            .GetLocal(5)
            .I32Add()
            .GetLocal(6)
            .I32Add()
            .GetLocal(7)
            .I32Add()
            .GetLocal(8)
            .I32Add()
            .GetLocal(9)
            .I32Add()
            .GetLocal(10)
            .I32Add()
            .GetLocal(11)
            .I32Add()
            .Return()
        .End()
        .Function("f1", { params: ["i32"], ret: "i32" })
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .GetLocal(0)
            .Call(0)
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

testWasmModuleFunctions([[{ type: "i32", value: 78 }, [{ type: "i32", value: 1 }, { type: "i32", value: 2 }, { type: "i32", value: 3 }, { type: "i32", value: 4 }, { type: "i32", value: 5 }, { type: "i32", value: 6 }, { type: "i32", value: 7 }, { type: "i32", value: 8 }, { type: "i32", value: 9 }, { type: "i32", value: 10 }, { type: "i32", value: 11 }, { type: "i32", value: 12 }]],
                         [{ type: "i32", value: 166 }, [{ type: "i32", value: 1 }, { type: "i32", value: 2 }, { type: "i32", value: 3 }, { type: "i32", value: 4 }, { type: "i32", value: 5 }, { type: "i32", value: 6 }, { type: "i32", value: 7 }, { type: "i32", value: 8 }, { type: "i32", value: 9 }, { type: "i32", value: 10 }, { type: "i32", value: 11 }, { type: "i32", value: 100 }]]
                        ],
                        [[{ type: "i32", value: 12 }, [{ type: "i32", value: 1 }]],
                         [{ type: "i32", value: 1200 }, [{ type: "i32", value: 100 }]],
                         [{ type: "i32", value: 0 }, [{ type: "i32", value: 0 }]],
                        ]);
