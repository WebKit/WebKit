import Builder from '../Builder.js';
import * as assert from '../assert.js';


const tableDescription = {initial: 1, element: "anyfunc"};
const builder = new Builder()
    .Type().End()
    .Import()
        .Table("imp", "table", tableDescription)
    .End()
    .Function().End()
    .Element()
        .Element({tableIndex: 0, offset: 0, functionIndices: [0]})
    .End()
    .Code()
        .Function("foo", {params: ["i32"], ret: "i32"})
            .GetLocal(0)
            .I32Const(42)
            .I32Add()
            .Return()
        .End()
    .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const table = new WebAssembly.Table(tableDescription);
new WebAssembly.Instance(module, {imp: {table}});
const foo = table.get(0);
const objs = [];
for (let i = 0; i < 10000; i++) {
    objs.push(new String("foo"));
    if (foo(20) !== 20 + 42)
        throw new Error("bad!!!");
}
