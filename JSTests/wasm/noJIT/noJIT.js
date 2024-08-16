//@ skip unless $isWasmPlatform
//@ runDefaultWasm("--useJIT=0")
if (typeof WebAssembly == "undefined")
    throw new Error("Expect WebAssembly global object is defined");
