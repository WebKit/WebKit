import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

function generateRandomAlphanumericString(length) {
  if (length < 0) {
    throw new Error("Length must be non-negative");
  }

  const characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';

  for (let i = 0; i < length; i++) {
    result += characters.charAt(Math.floor(Math.random() * characters.length));
  }

  return result;
}

async function test() {
  let hugeStr1 = generateRandomAlphanumericString(500_000);
  let hugeStr2 = generateRandomAlphanumericString(500_000);
  let wat = `
  (module
    (import "ns" "${hugeStr2}" (global $const1 externref))
    (func (export "${hugeStr1}") (result i32)
      (return (i32.const 42))
    )
  )
  `;

  const ns = { [hugeStr2]: "foo" };

  let instance = await instantiate(wat, { ns }, {});

  assert.truthy(Object.hasOwn(instance.exports, hugeStr1));
}

await assert.asyncTest(test());
