load("libwabt.js", "caller relative");

export async function compile(wat, options = {}) {
    const wabtModule = await WabtModule();
    // we need a filename for whatever reason...
    let parseResult = wabtModule.parseWat("filenamesAreCool", wat, options);
    let binaryResult = parseResult.toBinary(options);
    if (options.log) {
        print("log for compilation:");
        print(binaryResult.log);
    }
    if (options.logBinary) {
        print("binary:");
        print(binaryResult.buffer);
    }
    return new WebAssembly.Module(binaryResult.buffer);
}

export async function watToWasm(wat, options = {}) {
    const wabtModule = await WabtModule();
    let parseResult = wabtModule.parseWat("noFilename", wat, options);
    return parseResult.toBinary(options).buffer;
}

export async function instantiate(wat, imports = {}, options = {}) {
    const module = await compile(wat, options);
    return new WebAssembly.Instance(module, imports);
}
