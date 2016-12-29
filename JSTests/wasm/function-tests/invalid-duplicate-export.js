import Builder from '../Builder.js';
import * as assert from '../assert.js';

{
    const builder = (new Builder())
        .Type().End()
        .Function().End()
        .Export()
            .Function("foo")
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: [], ret: "void"})
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    assert.throws(() => new WebAssembly.Module(bin), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 31 / 39: duplicate export: 'foo'");
}
