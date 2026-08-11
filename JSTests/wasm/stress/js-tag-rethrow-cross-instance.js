import { instantiate as legacyInstantiate } from "../wabt-wrapper.js";
import { instantiate } from "../exception-wast-wrapper.js";
import * as assert from "../assert.js";

let legacyWat = `
(module
  (import "env" "throw" (func $jsThrow))

  (func (export "rethrow")
    (try $try
      (do
        (call $jsThrow)
      )

      (catch_all
        (rethrow $try)
      )
    )
  )
)
`;

let tryTableWat = `
(module
  (import "exports" "rethrow" (func $callRethrow))

  (func $throwRef (param $exnref exnref)
    (throw_ref (local.get $exnref))
  )

  (func (export "trigger")
    (block $block (result exnref)
      (try_table (catch_all_ref $block)
        (call $callRethrow)
        (return)
      )
      unreachable
    )
    (call $throwRef)
  )
)
`;


async function main() {
    let shouldThrow = false;
    const legacyInstance = await legacyInstantiate(legacyWat, {
        env: {
            throw() {
                if (shouldThrow)
                    throw 1234;
            }
        }
    }, { exceptions: true });

    const tryTableInstance = await instantiate(tryTableWat, legacyInstance);

    for (let i = 0; i < 1000; i++)
        tryTableInstance.exports.trigger();

    shouldThrow = true;
    tryTableInstance.exports.trigger();
}

try {
    await main();
} catch (e) {
    assert.eq(e, 1234);
}
