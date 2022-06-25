import * as assert from '../assert.js';
import Builder from '../Builder.js';

assert.isFunction(WebAssembly.validate);
assert.truthy(WebAssembly.hasOwnProperty('validate'));
assert.eq(WebAssembly.validate.length, 1);

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

{
    const builder = (new Builder());
    builder.setChecked(false);

    builder.Type().End()
        .Import().Memory("imp", "memory", {initial: 20}).End()
        .Unknown("test").End()
        .Import().Memory("imp", "memory", {initial: 20}).End()
        .Function().End()
        .Export().End()
        .Code()
        .End();

    assert.falsy(WebAssembly.validate(builder.WebAssembly().get()));
}
