import * as assert from '../assert.js';
import Builder from '../Builder.js';

const verbose = false;

const initial = 0;
const max = 0;

const builder = (new Builder())
    .Type().End()
    .Function().End()
    .Memory().InitialMaxPages(initial, max).End()
    .Export().Function("current").Function("grow").End()
    .Code()
        .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
        .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
    .End();

let instance = new WebAssembly.Instance(new WebAssembly.Module(builder.WebAssembly().get()));

const current = instance.exports.current();
const by = 2;
const result = instance.exports.grow(current + by);
if (verbose)
    print(`Grow from ${current} (max ${max}) to ${current + by} returned ${result}, current now ${instance.exports.current()}`);

assert.eq(result, -1);
assert.eq(current, instance.exports.current());
assert.le(instance.exports.current(), max);
