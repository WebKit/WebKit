import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export()
            .Function("throw")
            .Exception("tag", 0)
        .End()
        .Code()
            .Function("throw", { params: [], ret: "void" })
                .Try("void")
                    .Throw(0)
                .Catch(0)
                .End()
            .End()
        .End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    {
        let inst = new WebAssembly.Instance(module);
        inst.exports.throw();
    }
    {
        let inst = new WebAssembly.Instance(module);
        inst.exports.throw();
    }
}
