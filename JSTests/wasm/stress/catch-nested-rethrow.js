import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (tag $exc (param i32))

    (func (export "test")

        try $outer
            i32.const 777
            throw $exc
        catch $exc
            ;; Now do inner try-catch
            try $inner
                i32.const 888
                throw $exc
            catch $exc
                ;; Rethrow the outer exception (depth 1)
                rethrow 1
            end
            unreachable
        end
        unreachable
    )
)
`;

let watWithOSR = `
(module
    (tag $exc (param i32))

    (func (export "test") (param $loopCount i32)
        (local $counter i32)

        try $outer
            i32.const 777
            throw $exc
        catch $exc
            ;; Now do inner try-catch
            try $inner
                i32.const 888
                throw $exc
            catch $exc
                ;; Do a hot loop to trigger OSR tier-up before rethrowing
                i32.const 0
                local.set $counter

                (block $break
                    (loop $continue
                        ;; Increment counter
                        local.get $counter
                        i32.const 1
                        i32.add
                        local.set $counter

                        local.get $counter
                        local.get $loopCount
                        i32.lt_u
                        br_if $continue
                    )
                )

                ;; After the hot loop, rethrow the outer exception (depth 1)
                rethrow 1
            end
            unreachable
        end
        unreachable
    )
)
`;

async function testBasic() {
    const instance = await instantiate(wat, {}, { exceptions: true });

    for (let i = 0; i < wasmTestLoopCount; i++) {
        try {
            instance.exports.test();
            assert.fail("Expected WebAssembly.Exception to be thrown");
        } catch (e) {
            assert.instanceof(e, WebAssembly.Exception, "Should throw WebAssembly.Exception");
        }
    }
}

async function testWithOSR() {
    const instance = await instantiate(watWithOSR, {}, { exceptions: true });

    // Use a large loop count to ensure OSR tier-up happens
    const loopCount = 10000;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        try {
            instance.exports.test(loopCount);
            assert.fail("Expected WebAssembly.Exception to be thrown");
        } catch (e) {
            assert.instanceof(e, WebAssembly.Exception, "Should throw WebAssembly.Exception");
        }
    }
}

await assert.asyncTest(testBasic());
await assert.asyncTest(testWithOSR());
