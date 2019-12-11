import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = new Builder();

builder.Type().End()
    .Import()
        .Global().I32("imp", "global", "immutable").End()
    .End()
    .Function().End()
    .Export()
        .Function("getGlobal")
        .Global("global", 0)
    .End()
    .Code()

    .Function("getGlobal", { params: [], ret: "i32" })
        .GetGlobal(0)
    .End()

    .End()

const bin = builder.WebAssembly();
bin.trim();

const module = new WebAssembly.Module(bin.get());
const instance = new WebAssembly.Instance(module, { imp: { global: 5 } });
assert.eq(instance.exports.global, 5);
