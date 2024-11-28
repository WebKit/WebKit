//@ runWebAssemblySuite("--useWasmGC=true")
import { instantiate } from "./wast-wrapper.js";

try {
    /*
        (module
            (rec
                (type (;0;) (array i8))
            )
            (tag (;0;) (type 0))
        )
    */
    new WebAssembly.Module(new Uint8Array([0,97,115,109,1,0,0,0,1,132,128,128,128,0,1,94,120,0,13,131,128,128,128,0,1,0,0]));
} catch (e) {
    if (!(e instanceof WebAssembly.CompileError))
        throw new Error("bad");
}
