import * as assert from '../assert.js';
import Builder from '../Builder.js';

{
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", {initial: 20}).End()
        .Function().End()
        .Memory().InitialMaxPages(1, 1).End()
        .Export().End()
        .Code()
        .End();

    assert.truthy(!WebAssembly.validate(builder.WebAssembly().get()));
}

{
    const builder = (new Builder())
        .Type().End()
        .Import().Memory("imp", "memory", {initial: 20}).End()
        .Function().End()
        .Export().End()
        .Code()
        .End();

    assert.truthy(WebAssembly.validate(builder.WebAssembly().get()));
}
