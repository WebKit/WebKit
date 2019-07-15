import * as assert from '../assert.js';
import Builder from '../Builder.js';

assert.isFunction(WebAssembly.compile);
assert.isFunction(WebAssembly.__proto__.compile);
assert.eq(WebAssembly.compile.length, 1);

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

        try {
            await WebAssembly.compile(builder.WebAssembly().get());
        } catch(e) {
            assert.truthy(e instanceof WebAssembly.CompileError);
            assert.truthy(e.message === "WebAssembly.Module doesn't parse at byte 34: there can at most be one Memory section for now");
        }
    }

    {
        try {
            await WebAssembly.compile();
        } catch(e) {
            assert.truthy(e instanceof TypeError);
            assert.eq(e.message, "first argument must be an ArrayBufferView or an ArrayBuffer (evaluating 'WebAssembly.compile()')");
        }
    }

    {
        try {
            await WebAssembly.compile(20);
        } catch(e) {
            assert.truthy(e instanceof TypeError);
            assert.eq(e.message, "first argument must be an ArrayBufferView or an ArrayBuffer (evaluating 'WebAssembly.compile(20)')");
        }
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
}

assert.asyncTest(testPromiseAPI());
