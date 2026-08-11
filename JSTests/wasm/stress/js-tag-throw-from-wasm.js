import { instantiate } from "../exception-wast-wrapper.js";
import * as assert from "../assert.js";

let tryTableWat = `
(module
  (type $e (func (param externref)))
  (tag $jsTag (import "env" "jstag") (type $e))
  (func (export "trigger") (param externref)
    (local.get 0)
    (throw $jsTag)
  )
)
`;


async function main() {
    const tryTableInstance = await instantiate(tryTableWat, {
        env: {
            jstag: WebAssembly.JSTag
        }
    });
    let ok = new Error(42);
    for (let i = 0; i < testLoopCount; ++i) {
        assert.throws(() => {
            tryTableInstance.exports.trigger(ok);
        }, Error, `42`);
    }
}

await main();
