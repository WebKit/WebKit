//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { ok } from "./v128-import.wasm"
import * as assert from '../assert.js';

assert.eq(ok(), 1);
