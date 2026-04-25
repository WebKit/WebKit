const ROUNDS = 10;
const ITERATIONS_PER_ROUND = 5000;

for (let round = 0; round < ROUNDS; round++) {
    const N = 16;
    const maxBytes = N * 4 + 4096;
    const sab = new SharedArrayBuffer(N * 4, { maxByteLength: maxBytes });
    const ta = new Int32Array(sab);

    $.agent.start(`
        $.agent.receiveBroadcast((sab, idx) => {
            for (let i = 0; i < 50000; i++) {
                try {
                    if (sab.byteLength < ${maxBytes}) {
                        sab.grow(sab.byteLength + 64);
                    }
                } catch (e) {}
            }
            $.agent.report("done");
            $.agent.leaving();
        });
    `);

    $.agent.broadcast(sab, 0);

    for (let i = 0; i < ITERATIONS_PER_ROUND; i++) {
        try {
            ta.with(0, 0x41414141);
            ta.toReversed();
            ta.toSorted();
        } catch (e) {}
    }

    $.agent.sleep(50);
    $.agent.getReport();
}
