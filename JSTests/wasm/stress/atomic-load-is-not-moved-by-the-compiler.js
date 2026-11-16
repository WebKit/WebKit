//@ skip if $useCompressedHeap
//@ skip if $architecture != "arm64" && $architecture != "x86_64"

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

const memory = new WebAssembly.Memory({
    initial: 1,
    maximum: 1,
    shared: true
});
const buffer = new Int32Array(memory.buffer);

const wat = `
    (module
        (memory (import "env" "memory") 1 1 shared)
        (func (export "wait") (param $limit i32) (result i32) (local $spins i32)
            (block $done
                (loop $continue
                    (br_if $done (i32.atomic.load8_u (i32.const 0)))
                    (local.set $spins (i32.add (local.get $spins) (i32.const 1)))
                    (br_if $continue (i32.ne (local.get $spins) (local.get $limit)))))
            (local.get $spins))
    )`;

const instance = await instantiate(wat, { env: { memory } }, { threads: true });
const { wait } = instance.exports;

for (let i = 0; i < wasmTestLoopCount; ++i)
    assert.eq(wait(10), 10);

$.agent.start(`
    $262.agent.receiveBroadcast(function(memory) {
        $262.agent.sleep(20);
        Atomics.store(new Int32Array(memory.buffer), 0, 1);
        $262.agent.leaving();
    });
`);
$262.agent.broadcast(memory);

const limit = 0x7fffffff;
while (wait(limit) === limit)
    assert.eq(Atomics.load(buffer, 0), 0);
