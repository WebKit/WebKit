import Builder from '../Builder.js';
import * as assert from '../assert.js';

const instantiate = (builder, importObject = undefined) => {
    return new WebAssembly.Instance(
        new WebAssembly.Module(
            builder.WebAssembly().get()),
        importObject);
};

const initial = 0;
const maximum = 2;

const builder0 = (new Builder())
      .Type().End()
      .Function().End()
      .Memory().InitialMaxPages(initial, maximum).End()
      .Export()
          .Memory("memory", 0)
      .End()
      .Code().End();

const builder1 = (new Builder())
      .Type().End()
      .Import().Memory("imp", "memory", { initial: initial, maximum: maximum }).End()
      .Function().End()
      .Memory().InitialMaxPages(initial, maximum).End()
      .Code().End();

const i0 = instantiate(builder0);
assert.throws(() => instantiate(builder1, { imp: { memory: i0.exports.memory } }), WebAssembly.CompileError, `WebAssembly.Module doesn't parse at byte 35: there can at most be one Memory section for now`);
