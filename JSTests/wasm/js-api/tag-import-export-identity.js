import * as assert from '../assert.js';
import Builder from '../Builder.js';

function moduleImportingAndExportingTag() {
    const builder = new Builder();
    builder.Type().End()
        .Import()
            .Exception("imp", "tag", { params: ["i32"], ret: "void" })
        .End()
        .Export()
            .Exception("tag", 0)
        .End();
    const bin = builder.WebAssembly();
    bin.trim();
    return new WebAssembly.Module(bin.get());
}

{
    const module = moduleImportingAndExportingTag();
    const imported = new WebAssembly.Tag({ parameters: ["i32"] });
    const instance = new WebAssembly.Instance(module, { imp: { tag: imported } });
    assert.eq(instance.exports.tag, imported);
    const exception = new WebAssembly.Exception(imported, [1]);
    assert.eq(exception.is(instance.exports.tag), true);
}

{
    const producer = new Builder();
    producer.Type().End()
        .Exception().Signature({ params: ["i32"] }).End()
        .Export()
            .Exception("tag", 0)
        .End();
    const producedBin = producer.WebAssembly();
    producedBin.trim();
    const produced = new WebAssembly.Instance(new WebAssembly.Module(producedBin.get()));
    const module = moduleImportingAndExportingTag();
    const instance = new WebAssembly.Instance(module, { imp: { tag: produced.exports.tag } });
    assert.eq(instance.exports.tag, produced.exports.tag);
}

{
    const builder = new Builder();
    builder.Type().End()
        .Exception().Signature({ params: ["i32"] }).End()
        .Export()
            .Exception("a", 0)
            .Exception("b", 0)
        .End();
    const bin = builder.WebAssembly();
    bin.trim();
    const instance = new WebAssembly.Instance(new WebAssembly.Module(bin.get()));
    assert.eq(instance.exports.a, instance.exports.b);
}
