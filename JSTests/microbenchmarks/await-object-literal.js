// Stresses isDefinitelyNonThenable() through `await object`, the dominant pattern
// in async functions returning plain objects.

async function run() {
    let acc = 0;
    for (let i = 0; i < 1e6; i++) {
        const o = await { a: i, b: i + 1, c: i + 2 };
        acc += o.a;
    }
    return acc;
}

run();
drainMicrotasks();
