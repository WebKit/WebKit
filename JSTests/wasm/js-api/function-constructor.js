//@ requireOptions("--useWasmJSTypes=true")
import * as assert from "../assert.js";

function addxy(x, y) {
    return x + y;
}

function doNothing() {}

{
    assert.eq(typeof WebAssembly.Function, "function");
    assert.eq(WebAssembly.Function.name, "Function");
    assert.eq(WebAssembly.Function.length, 2);
}

{
    const fun = new WebAssembly.Function({ parameters: ["i32", "i32"], results: ["i32"] }, addxy);
    assert.eq(fun instanceof WebAssembly.Function, true);
    assert.eq(fun(1, 2), 3);
    assert.throws(() => new fun(1, 2), TypeError, "");
    const type = fun.type();
    assert.eq(type.parameters.length, 2);
    assert.eq(type.parameters[0], "i32");
    assert.eq(type.parameters[1], "i32");
    assert.eq(type.results.length, 1);
    assert.eq(type.results[0], "i32");
}

{
    const fun = new WebAssembly.Function({ parameters: [], results: [] }, doNothing);
    assert.eq(fun(), undefined);
    const type = fun.type();
    assert.eq(type.parameters.length, 0);
    assert.eq(type.results.length, 0);
}

{
    const fun = new WebAssembly.Function({ parameters: ["i64"], results: ["i64"] }, x => x + 1n);
    assert.eq(fun(1n), 2n);
}

{
    const table = new WebAssembly.Table({ element: "anyfunc", initial: 1 });
    const fun = new WebAssembly.Function({ parameters: ["i32"], results: [] }, doNothing);
    table.set(0, fun);
    assert.eq(table.get(0), fun);
}

assert.throws(() => WebAssembly.Function({ parameters: [], results: [] }, doNothing), TypeError, "");
assert.throws(() => new WebAssembly.Function(), TypeError, "");
assert.throws(() => new WebAssembly.Function({ parameters: [] }, doNothing), TypeError, "");
assert.throws(() => new WebAssembly.Function({ results: [] }, doNothing), TypeError, "");
assert.throws(() => new WebAssembly.Function({ parameters: ["nope"], results: [] }, doNothing), TypeError, "");
assert.throws(() => new WebAssembly.Function({ parameters: [], results: [] }, {}), TypeError, "");
assert.throws(() => new WebAssembly.Function({ parameters: [], results: [] }, 72), TypeError, "");

{
    const multi = new WebAssembly.Function({ parameters: [], results: ["i32", "i32"] }, () => [1, 2]);
    const values = multi();
    assert.eq(values[0], 1);
    assert.eq(values[1], 2);
    const tooFew = new WebAssembly.Function({ parameters: [], results: ["i32", "i32"] }, () => [1]);
    assert.throws(() => tooFew(), TypeError, "");
}

{
    class F extends WebAssembly.Function {}
    const fun = new F({ parameters: [], results: [] }, doNothing);
    assert.eq(fun instanceof F, true);
}

{
    const wrapped = new WebAssembly.Function({ parameters: [], results: [] }, doNothing);
    assert.eq(new WebAssembly.Function({ parameters: [], results: [] }, wrapped), wrapped);
    assert.throws(() => new WebAssembly.Function({ parameters: ["i32"], results: [] }, wrapped), TypeError, "");
}

{
    const fun = new WebAssembly.Function({ parameters: ["i32"], results: ["i32"] }, x => x + 1);
    {
        const unused = new WebAssembly.Module(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0]));
        unused;
    }
    fullGC();
    assert.eq(fun.type().parameters[0], "i32");
    assert.eq(fun.type().results[0], "i32");
    assert.eq(fun(1), 2);
}
