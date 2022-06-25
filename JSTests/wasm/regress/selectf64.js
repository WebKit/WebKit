import { test } from './selectf64.wasm'
import * as assert from '../assert.js';

assert.eq(test(), 43)
