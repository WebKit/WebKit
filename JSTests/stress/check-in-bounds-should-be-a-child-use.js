//@ runDefault("--useConcurrentJIT=0", "--thresholdForFTLOptimizeAfterWarmUp=100")

const hello = [1337,1337,1337,1337];
const arr = [1337,1337];

function func(arg) {
    for (let p in arg) {
        arg.a = 42;
        const val = arg[-698666199];
    }
}

for (let i = 0; i < 10000; ++i) {
    const a = func(arr);
    const b = func(1337);
}
