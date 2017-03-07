import * as assert from '../assert.js';
import Builder from '../Builder.js';

assert.truthy(WebAssembly.instantiate instanceof Function);
assert.eq(WebAssembly.instantiate.length, 1);

{
    const builder = (new Builder())
          .Type().End()
          .Function().End()
          .Export()
              .Function("foo")
          .End()
          .Code()
              .Function("foo", { params: [], ret: "i32" })
                  .I32Const(1)
              .End()
          .End();

    const bin = builder.WebAssembly().get();

    let done = false;
    async function test() {
        let {module, instance} = await WebAssembly.instantiate(bin);
        assert.truthy(module instanceof WebAssembly.Module);
        assert.truthy(instance instanceof WebAssembly.Instance);
        assert.eq(instance.exports.foo(20), 1);
        done = true;
    }

    test();
    drainMicrotasks();
    assert.truthy(done);
}

{
    const builder = (new Builder())
          .Type().End()
          .Function().End()
          .Export()
              .Function("foo")
          .End()
          .Code()
              .Function("foo", { params: [], ret: "i32" })
                  .I32Const(1)
              .End()
          .End();

    const bin = builder.WebAssembly().get();

    let done = false;
    async function test() {
        try {
            let {module, instance} = await WebAssembly.instantiate(bin, null);
        } catch(e) {
            assert.eq(e.message, "second argument to WebAssembly.instantiate must be undefined or an Object (evaluating 'WebAssembly.instantiate(bin, null)')");
        }
        done = true;
    }

    test();
    drainMicrotasks();
    assert.truthy(done);
}

{
    const builder = (new Builder())
          .Type().End()
          .Function().End()
          .Export()
              .Function("foo")
          .End()
          .Code()
              .Function("foo", { params: [], ret: "i32" })
                  .F32Const(1)
              .End()
          .End();

    const bin = builder.WebAssembly().get();

    let done = false;
    async function test() {
        try {
            let {module, instance} = await WebAssembly.instantiate(bin);
        } catch(e) {
            assert.truthy(e instanceof WebAssembly.CompileError);
            assert.eq(e.message, "WebAssembly.Module doesn't validate: control flow returns with unexpected type, in function at index 0 (evaluating 'WebAssembly.instantiate(bin)')");
        }
        done = true;
    }

    test();
    drainMicrotasks();
    assert.truthy(done);
}

{
    const builder = (new Builder())
          .Type().End()
          .Import().Memory("imp", "memory", {initial:100}).End()
          .Function().End()
          .Export()
              .Function("foo")
          .End()
          .Code()
              .Function("foo", { params: [], ret: "i32" })
                  .I32Const(1)
              .End()
          .End();

    const bin = builder.WebAssembly().get();

    let done = false;
    async function test() {
        try {
            let {module, instance} = await WebAssembly.instantiate(bin, {imp: {memory: 20}});
        } catch(e) {
            assert.eq(e.message, "Memory import is not an instance of WebAssembly.Memory (evaluating 'WebAssembly.instantiate(bin, {imp: {memory: 20}})')");
        }
        done = true;
    }

    test();
    drainMicrotasks();
    assert.truthy(done);
}

{
    const builder = (new Builder())
          .Type().End()
          .Import().Memory("imp", "memory", {initial:100}).End()
          .Function().End()
          .Export()
              .Function("foo")
          .End()
          .Code()
              .Function("foo", { params: [], ret: "i32" })
                  .I32Const(1)
              .End()
          .End();

    const bin = builder.WebAssembly().get();

    let done = false;
    async function test() {
        try {
            const module = new WebAssembly.Module(bin);
            let instance = await WebAssembly.instantiate(bin, {imp: {memory: 20}});
        } catch(e) {
            assert.eq(e.message, "Memory import is not an instance of WebAssembly.Memory (evaluating 'WebAssembly.instantiate(bin, {imp: {memory: 20}})')");
        }
        done = true;
    }

    test();
    drainMicrotasks();
    assert.truthy(done);
}

{
    const builder = (new Builder())
          .Type().End()
          .Function().End()
          .Export()
              .Function("foo")
          .End()
          .Code()
              .Function("foo", { params: [], ret: "i32" })
                  .I32Const(1)
              .End()
          .End();

    const bin = builder.WebAssembly().get();

    let done = false;
    async function test() {
        let module = new WebAssembly.Module(bin);
        let instance = await WebAssembly.instantiate(module);
        assert.truthy(instance instanceof WebAssembly.Instance);
        assert.eq(instance.exports.foo(20), 1);
        done = true;
    }

    test();
    drainMicrotasks();
    assert.truthy(done);
}

{
    const builder = (new Builder())
          .Type().End()
          .Import().Memory("imp", "memory", {initial:100}).End()
          .Function().End()
          .Export()
              .Function("foo")
          .End()
          .Code()
              .Function("foo", { params: [], ret: "i32" })
                  .I32Const(1)
              .End()
          .End();

    const bin = builder.WebAssembly().get();

    let done = false;
    async function test() {
        try {
            await WebAssembly.instantiate(25);
        } catch(e) {
            // FIXME: Better error message here.
            assert.eq(e.message, "first argument must be an ArrayBufferView or an ArrayBuffer (evaluating 'WebAssembly.instantiate(25)')");
        }
        done = true;
    }

    test();
    drainMicrotasks();
    assert.truthy(done);
}
