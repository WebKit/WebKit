import * as assert from '../assert.js'
import { return42 } from "./wasm-js-cycle/entry.wasm"

assert.eq(return42(), 42);
