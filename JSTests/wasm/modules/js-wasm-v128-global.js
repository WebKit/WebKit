//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import * as ns from "./v128-global.wasm"
import * as assert from '../assert.js';

assert.throws(() => {
    ns.v;
}, ReferenceError, `Cannot access 'v' before initialization.`);
