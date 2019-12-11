import * as assert from '../assert.js';
import Builder from '../Builder.js';

(function EmptyModule() {
    const builder = new Builder();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    assert.instanceof(module, WebAssembly.Module);
})();

(function ModuleWithImports() {
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Function("foo", "bar", { params: [] })
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
})();
