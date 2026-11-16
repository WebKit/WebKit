//@ runDefault("--useDollarVM=1", "--useConcurrentJIT=0")

async function sleepAsync(secs) {
    return new Promise(resolve => setTimeout(resolve, secs * 1000.0));
}

async function main() {
    let x = 1;

    $vm.deleteAllCodeWhenIdle();
    await sleepAsync(0.0);

    function opt() {
        return x;
    }

    for (let i = 0; i < testLoopCount; i++) {
        opt();
    }

    x = 2;

    if (opt() !== 2)
        throw new Error("Expected 2, got " + opt());
}

main().then(undefined, e => {
    print(e);
    $vm.abort();
});
