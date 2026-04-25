import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: ["i32"]}).End()
        .Export().Exception("foo", 0).End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.instanceof(instance.exports.foo, WebAssembly.Tag);
}
