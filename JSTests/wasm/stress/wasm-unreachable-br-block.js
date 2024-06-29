import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
  (type (;0;) (func))
  (func (;0;) (type 0)
    unreachable
    block  ;; label = @1
      br 1 (;@0;)
    end)
)
`;

async function test() {
    await instantiate(wat);
}

assert.asyncTest(test());
