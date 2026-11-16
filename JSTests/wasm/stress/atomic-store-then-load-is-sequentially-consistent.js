//@ skip if $useCompressedHeap
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
//@ requireOptions("--useOMGJIT=0", "--wasmFunctionIndexRangeToCompile=0:0")

const rounds = 100;
const count = wasmTestLoopCount;
const stride = 32;
const barrier = 0;
const variables = [stride, stride + count * stride];
const results = [stride + 2 * count * stride, stride + 2 * count * stride + rounds * count];
const pages = Math.ceil((results[1] + rounds * count) * 4 / 65536);

const memory = new WebAssembly.Memory({
    initial: pages,
    maximum: pages,
    shared: true
});
const buffer = new Int32Array(memory.buffer);

for (let id = 0; id < 2; ++id) {
    $.agent.start(`
        import("../wabt-wrapper.js").then(({ instantiate }) => {
            $262.agent.receiveBroadcast(function(memory) {
                (async function () {
                    let wat = \`
                        (module
                            (memory (import "env" "memory") ${pages} ${pages} shared)
                            (func (export "store") (param i32 i32) (i32.atomic.store (local.get 0) (local.get 1)))
                            (func (export "load") (param i32) (result i32) (i32.atomic.load (local.get 0)))
                        )\`;
                    const instance = await instantiate(wat, {
                        env: { memory }
                    }, { threads: true });
                    const { store, load } = instance.exports;
                    const buffer = new Int32Array(memory.buffer);

                    function run(mine, other, result, value) {
                        for (let i = 0; i < ${count}; ++i) {
                            store((mine + i * ${stride}) * 4, value);
                            buffer[result + i] = load((other + i * ${stride}) * 4);
                        }
                    }

                    for (let round = 1; round <= ${rounds}; ++round) {
                        Atomics.add(buffer, ${barrier}, 1);
                        while (Atomics.load(buffer, ${barrier}) < round * 2);
                        run(${variables[id]}, ${variables[1 - id]}, ${results[id]} + (round - 1) * ${count}, round);
                    }
                    $262.agent.report(0);
                    $262.agent.leaving();
                })();
            });
        }, (error) => { print(error); });
    `, import.meta.filename);
}

$262.agent.broadcast(memory);
for (let done = 0; done < 2;) {
    if ($262.agent.getReport() !== null)
        done++;
    else
        $262.agent.sleep(1);
}

for (let round = 1; round <= rounds; ++round) {
    for (let i = 0; i < count; ++i) {
        const index = (round - 1) * count + i;
        if (buffer[results[0] + index] !== round && buffer[results[1] + index] !== round)
            throw new Error(`Neither agent saw the store of the other in round ${round} at ${i}`);
    }
}
