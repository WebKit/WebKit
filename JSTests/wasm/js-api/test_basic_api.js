if (WebAssembly === undefined)
    throw new Error("Couldn't find WebAssembly global object");

const functionProperties = ["validate", "compile"];
const constructorProperties = ["Module", "Instance", "Memory", "Table", "CompileError"];

for (const f of functionProperties)
    if (WebAssembly[f] === undefined)
        throw new Error(`Couldn't find WebAssembly function property "${f}"`);

for (const c of constructorProperties)
    if (WebAssembly[c] === undefined)
        throw new Error(`Couldn't find WebAssembly constructor property "${c}"`);

// FIXME https://bugs.webkit.org/show_bug.cgi?id=159775 Implement and test these APIs further. For now they just throw.

for (const f of functionProperties) {
    try {
        WebAssembly[f]();
    } catch (e) {
        if (e instanceof Error)
            continue;
        throw new Error(`Expected WebAssembly.${f}() to throw an Error, got ${e}`);
    }
    throw new Error(`Expected WebAssembly.${f}() to throw an Error`);
}

for (const c of constructorProperties) {
    try {
        let v = new WebAssembly[c]();
    } catch (e) {
        if (e instanceof Error)
            continue;
        throw new Error(`Expected new WebAssembly.${f}() to throw an Error, got ${e}`);
    }
    throw new Error(`Expected new WebAssembly.${f}() to throw an Error`);
}
