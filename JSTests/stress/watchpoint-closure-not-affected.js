//@ runDefault("--useDollarVM=1", "--useConcurrentJIT=0")

async function sleepAsync(secs) {
    return new Promise(resolve => setTimeout(resolve, secs * 1000.0));
}

async function test() {
    var obj = (function() {
        let x = 1;
        function opt() { return x; }
        return { opt, set(v) { x = v; } };
    })();

    $vm.deleteAllCodeWhenIdle();
    await sleepAsync(0.0);

    for (let i = 0; i < 1000; i++)
        obj.opt();

    obj.set(2);

    if (obj.opt() !== 2)
        throw new Error("Expected 2, got " + obj.opt());
}

test().then(undefined, e => {
    print(e);
    $vm.abort();
});
