import * as assert from '../assert.js';
import Builder from '../Builder.js';

let done = false;
async function testPromiseAPI() {
    {
        // Can't declare more than one memory.
        const builder = (new Builder())
            .Type().End()
            .Import().Memory("imp", "memory", {initial: 20}).End()
            .Function().End()
            .Memory().InitialMaxPages(1, 1).End()
            .Export().End()
            .Code()
            .End();

        let threw = false;
        try {
            await WebAssembly.compile(builder.WebAssembly().get());
        } catch(e) {
            threw = true;
            assert.truthy(e instanceof WebAssembly.CompileError);
            assert.truthy(e.message === "WebAssembly.Module doesn't parse at byte 34 / 43: Memory section cannot exist if an Import has a memory (evaluating 'WebAssembly.compile(builder.WebAssembly().get())')");
        }
        assert.truthy(threw);
    }

    {
        let threw = false;
        try {
            await WebAssembly.compile(20);
        } catch(e) {
            threw = true;
            assert.truthy(e instanceof TypeError);
            assert.eq(e.message, "first argument must be an ArrayBufferView or an ArrayBuffer (evaluating 'WebAssembly.compile(20)')");
        }
        assert.truthy(threw);
    }

    {
        const builder = (new Builder())
            .Type().End()
            .Import().Memory("imp", "memory", {initial: 20}).End()
            .Function().End()
            .Export().End()
            .Code()
            .End();

        let module = await WebAssembly.compile(builder.WebAssembly().get());
        assert.truthy(module instanceof WebAssembly.Module);
    }

    done = true;
}

testPromiseAPI();
drainMicrotasks();
assert.truthy(done);
