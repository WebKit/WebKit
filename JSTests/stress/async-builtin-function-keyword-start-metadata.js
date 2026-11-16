//@ runDefault("--useDollarVM=1")

var asyncBuiltin = $vm.createBuiltin("(async function (x) { return await x + 1; })", "private");

var result;
asyncBuiltin(Promise.resolve(41)).then(v => { result = v; });
drainMicrotasks();

if (result !== 42)
    throw new Error("Expected 42, got " + result);
