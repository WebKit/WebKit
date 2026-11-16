//@ requireOptions("--useDollarVM=true")
//@ runDefault("--useSharedHelperThreadPool=true")
//@ runDefault("--useSharedHelperThreadPool=true", "--helperThreadPoolTargetCapacity=1", "--collectContinuously=true")

function makeFunction(i) {
    return new Function("a", "b", `let s = 0; for (let j = 0; j < a; ++j) s += (j * ${i}) ^ b; return s;`);
}

let functions = [];
for (let i = 0; i < 200; ++i)
    functions.push(makeFunction(i));

for (let iteration = 0; iteration < 100; ++iteration) {
    for (let f of functions)
        f(50, iteration);
    let garbage = [];
    for (let k = 0; k < 1000; ++k)
        garbage.push({ k, s: "x" + k });
}
fullGC();

let statistics;
for (let i = 0; i < 1000; ++i) {
    statistics = $vm.helperThreadPoolStatistics();
    if (!statistics.running)
        break;
    sleepSeconds(0.01);
}

if (statistics.running || statistics.blocked)
    throw new Error(`Pool did not become idle: ${JSON.stringify(statistics)}`);
for (let name of ["GC", "JIT", "Wasm"]) {
    if (statistics[name].running)
        throw new Error(`${name} is still running tasks: ${JSON.stringify(statistics)}`);
}
