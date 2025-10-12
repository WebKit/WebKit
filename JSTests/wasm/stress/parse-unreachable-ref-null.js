function shouldThrow(run, errorType, message) {
    let error;
    let threw = false;
    try {
        run();
    } catch (e) {
        threw = true;
        error = e;
    }
    if (!threw)
        throw new Error(`Expected to throw ${errorType.name}, but did not throw.`);
    if (!(error instanceof errorType))
        throw new Error(`Expected to throw ${errorType.name}, but threw '${error}'`);
    if (message !== void 0 && error.message !== message)
        throw new Error(`Expected to throw '${message}', but threw '${error.message}'`);
}

{
    // (module
    //   (type $0 (func))
    //   (type $1 (func))
    //   (type $2 (func))
    //   (func
    //     (return)
    //     (ref.null $2)
    //     (drop)
    //   )
    // )
    const wasm = new Uint8Array([
      0, 97, 115, 109, 1, 0, 0, 0, 1, 10, 3, 96, 0, 0, 96, 0, 0, 96, 0, 0, 3, 2, 1,
      0, 10, 8, 1, 6, 0, 15, 208, 2, 26, 11, 0, 17, 4, 110, 97, 109, 101, 4, 10, 3,
      0, 1, 48, 1, 1, 49, 2, 1, 50,
    ]);
    new WebAssembly.Module(wasm);
}

{
    // (module
    //   (type $0 (func))
    //   (type $1 (func))
    //   (type $2 (func))
    //   (func
    //     (return)
    //     (ref.null)
    //     (drop)
    //   )
    // )
    const wasm = new Uint8Array([
      0, 97, 115, 109, 1, 0, 0, 0, 1, 10, 3, 96, 0, 0, 96, 0, 0, 96, 0, 0, 3, 2, 1,
      0, 10, 7, 1, 5, 0, 15, 208, 26, 11, 0, 17, 4, 110, 97, 109, 101, 4, 10, 3, 0,
      1, 48, 1, 1, 49, 2, 1, 50,
    ]);
    shouldThrow(() => {
        new WebAssembly.Module(wasm);
    }, WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 4: can't get heap type for RefNull in unreachable context, in function at index 0 (evaluating 'new WebAssembly.Module(wasm)')")
}
