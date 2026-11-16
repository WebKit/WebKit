// A then-chain over distinct handler functions interleaves several functions
// in the microtask stream, stressing the microtask call cache in the drain loop.

const f1 = (x) => x + 1;
const f2 = (x) => x + 2;
const f3 = (x) => x + 3;

async function run() {
    let acc = 0;
    for (let i = 0; i < 2e5; i++)
        acc += await Promise.resolve(i).then(f1).then(f2).then(f3);
    return acc;
}

run();
drainMicrotasks();
