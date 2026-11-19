//@ skip if $useCompressedHeap
//@ runDefault

// Another thread flips one element between 0 and 1 while this thread searches for 1.
// All other elements stay 0, so indexOf and lastIndexOf can return only that index or -1.

const length = 1024;
const firstIndex = 512;
const indexCount = 64;
const iterations = Math.min(testLoopCount, 1000);

const sab = new SharedArrayBuffer(64 + length);
const control = new Int32Array(sab, 0, 16);
const array = new Uint8Array(sab, 64, length);
const flipIndex = 1;
const acknowledged = 2;

Atomics.store(control, flipIndex, firstIndex);
Atomics.store(control, acknowledged, -1);

$262.agent.start(`
    $262.agent.receiveBroadcast(function(sab) {
        const control = new Int32Array(sab, 0, 16);
        const array = new Uint8Array(sab, 64, ${length});
        for (;;) {
            const index = Atomics.load(control, ${flipIndex});
            if (index < 0)
                break;
            Atomics.store(control, ${acknowledged}, index);
            for (let i = 0; i < 1000; ++i) {
                array[index] = 1;
                array[index] = 0;
            }
        }
        $262.agent.report("done");
        $262.agent.leaving();
    });
`);
$262.agent.broadcast(sab);

for (let index = firstIndex; index < firstIndex + indexCount; ++index) {
    Atomics.store(control, flipIndex, index);
    while (Atomics.load(control, acknowledged) !== index) { }
    for (let i = 0; i < iterations; ++i) {
        let result = array.indexOf(1);
        if (result !== index && result !== -1)
            throw new Error(`indexOf returned ${result} while only ${index} could be 1`);
        result = array.lastIndexOf(1);
        if (result !== index && result !== -1)
            throw new Error(`lastIndexOf returned ${result} while only ${index} could be 1`);
    }
}

Atomics.store(control, flipIndex, -1);
while ($262.agent.getReport() === null)
    $262.agent.sleep(1);
