//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import * as assert from '../assert.js';

const num = 3;
const count = 1e5;

let memory = new WebAssembly.Memory({
    initial: 1,
    maximum: 1,
    shared: true
});
let buffer = new Int32Array(memory.buffer);

for (let i = 0; i < num; ++i) {
    $.agent.start(`
        import("../wabt-wrapper.js").then(({ instantiate }) => {
            $262.agent.receiveBroadcast(function(memory) {
                $262.agent.sleep(1);
                (async function () {
                    let wat = \`
                        (module
                            (memory (import "env" "memory") 1 1 shared)
                            (func (export "add") (result i32) (i32.atomic.rmw.add (i32.const 0) (i32.const 1)))
                        )\`;
                    let instance = await instantiate(wat, {
                        env: { memory }
                    }, { threads: true });
                    for (var i = 0; i < ${count}; ++i)
                        instance.exports.add();
                    $262.agent.report(0);
                    $262.agent.leaving();
                })();
            });
        }, (error) => { print(error); });
    `, import.meta.filename);
}

$262.agent.broadcast(memory);
let done = 0;
while (true) {
    let report = $262.agent.getReport();
    if (report !== null)
        done++;
    if (done === num)
        break;
    $262.agent.sleep(1);
}
assert.eq(buffer[0], count * num);
