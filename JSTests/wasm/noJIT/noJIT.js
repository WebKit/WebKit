if (typeof WebAssembly !== "undefined")
    throw new Error("Expect WebAssembly global object is undefined if JIT is off");
