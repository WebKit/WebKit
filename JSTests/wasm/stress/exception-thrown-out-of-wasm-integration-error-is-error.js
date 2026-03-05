import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: [] }).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
            .Throw(0)
            .I32Const(1)
            .End()
        .End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    let hasCaught = false;
    try {
        instance.exports.call();
    } catch (e) {
        hasCaught = true;
        assert.instanceof(e, WebAssembly.Exception, 'must be an instance of WebAssembly.Exception');
        // By the current (22 November 2025) spec semantics, `WebAssembly.Exception` does not have `[[ErrorData]]` in ECMA262 concept.
        // This means `Error.isError(<a instance of WebAssembly.Exception>)` should be `false`.
        //
        // - https://webassembly.github.io/spec/js-api/#exceptions
        // - https://github.com/WebAssembly/spec/issues/1914
        // - https://tc39.es/ecma262/multipage/fundamental-objects.html#sec-properties-of-error-instances
        assert.eq(Error.isError(e), false, 'Error.isError(WebAssembly.Exception) should be false');
    }

    assert.truthy(hasCaught, 'expected assertion was not called');
}
