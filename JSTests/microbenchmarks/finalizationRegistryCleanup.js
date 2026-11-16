asyncTestStart(1);
let sum = 0;
let testPassed = false;

async function callback(value) {
    // Do something somewhat interesting so we can't eliminate the entire callback.
    sum += value;
    if (!testPassed) {
        testPassed = true;
        await Promise.resolve();
        asyncTestPassed();
    }
}

let finalizationRegistry = new FinalizationRegistry(callback);

for (let i = 0; i < 1e6; ++i)
    finalizationRegistry.register({ }, 1);

gc();
