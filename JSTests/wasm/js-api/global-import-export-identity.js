import * as assert from '../assert.js';
import Builder from '../Builder.js';

function moduleImportingAndExportingI32() {
    const builder = new Builder();
    builder.Type().End()
        .Import()
            .Global().I32("imp", "global", "immutable").End()
        .End()
        .Export()
            .Global("global", 0)
        .End();
    const bin = builder.WebAssembly();
    bin.trim();
    return new WebAssembly.Module(bin.get());
}

{
    const module = moduleImportingAndExportingI32();
    const imported = new WebAssembly.Global({ value: "i32", mutable: false }, 7);
    const instance = new WebAssembly.Instance(module, { imp: { global: imported } });
    assert.eq(instance.exports.global, imported);
    assert.eq(instance.exports.global.value, 7);
}

{
    const module = moduleImportingAndExportingI32();
    const instance = new WebAssembly.Instance(module, { imp: { global: 11 } });
    assert.eq(instance.exports.global.value, 11);
}

{
    const producer = new Builder();
    producer.Type().End()
        .Global().I32(13, "immutable").End()
        .Export()
            .Global("global", 0)
        .End();
    const producedBin = producer.WebAssembly();
    producedBin.trim();
    const produced = new WebAssembly.Instance(new WebAssembly.Module(producedBin.get()));
    const module = moduleImportingAndExportingI32();
    const instance = new WebAssembly.Instance(module, { imp: { global: produced.exports.global } });
    assert.eq(instance.exports.global, produced.exports.global);
    assert.eq(instance.exports.global.value, 13);
}
