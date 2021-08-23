import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

async function test() {
    await instantiate(`
        (module
          (func
            (local externref)
            (local externref)
          )
        )
    `);
}

assert.asyncTest(test());
