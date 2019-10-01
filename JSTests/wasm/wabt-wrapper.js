load("../libwabt.js");

export function compile(wat, options = {}) {
    // we need a filename for whatever reason...
    let parseResult = WabtModule().parseWat("filenamesAreCool", wat, options);
    let binaryResult = parseResult.toBinary(options);
    if (options.log) {
        print("log for compilation:");
        print(binaryResult.log);
    }
    return new WebAssembly.Module(binaryResult.buffer);
}

export function instantiate(wat, imports = {}, options = {}) {
    return new WebAssembly.Instance(compile(wat, options), imports);
}
