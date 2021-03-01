function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const num = 3;
const count = 1e5;

let buffer = new SharedArrayBuffer(128);
let array = new BigInt64Array(buffer);

for (let i = 0; i < num; ++i) {
    $.agent.start(`
        $262.agent.receiveBroadcast(function(buffer) {
            let array = new BigInt64Array(buffer);
            $262.agent.sleep(1);
            for (var i = 0; i < ${count}; ++i)
                Atomics.add(array, 0, 1n);
            $262.agent.report(0);
            $262.agent.leaving();
        });
    `);
}

$262.agent.broadcast(buffer);
let done = 0;
while (true) {
    let report = $262.agent.getReport();
    if (report !== null)
        done++;
    if (done === num)
        break;
    $262.agent.sleep(1);
}
shouldBe(array[0], BigInt(count * num));
