import * as assert from '../assert.js';
import Builder from '../Builder.js';

(function EmptyModule() {
    const builder = new Builder();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);
    assert.instanceof(instance, WebAssembly.Instance);
})();
