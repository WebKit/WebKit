import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const count = 1000;
    const signature = [];
    for (let i = 0; i < count; ++i)
        signature.push("i32");

    let builder = new Builder()
        .Type()
        .End()
        .Import()
            .Function("imp", "f1", {params:signature, ret:"void"})
            .Function("imp", "f2", {params:signature, ret:"void"})
        .End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: signature, ret: "void" });

    for (let i = 0; i < count; ++i)
        builder = builder.GetLocal(i);

    builder = builder.Call(0);

    for (let i = count; i--; )
        builder = builder.GetLocal(i);

    builder = builder.Call(1).Return().End().End();

    let calledF1 = false;
    let calledF2 = false;

    function f1(...args) {
        calledF1 = true;
        let realArgs = [...args, ...args];
        return end(...realArgs);
    }
    noInline(f1);

    function end() {}
    noInline(end);


    function f2(...args) {
        calledF2 = true;
        let called = false;
        assert.eq(args.length, count);
        for (let i = 0; i < args.length; ++i) {
            assert.eq(args[i], args.length - i - 1);
        }
    }
    noInline(f2);

    let instance = new WebAssembly.Instance(new WebAssembly.Module(builder.WebAssembly().get()), {imp: {f1, f2}});

    const args = [];
    for (let i = 0; i < count; ++i)
        args.push(i);

    for (let i = 0; i < 50; ++i) {
        instance.exports.foo(...args);

        assert.eq(calledF1, true);
        assert.eq(calledF2, true);
        calledF1 = false;
        calledF2 = false;
    }
}
