//@ runDefault("--useConcurrentJIT=false", "--jitPolicyScale=0.1", "--slowPathAllocsBetweenGCs=3")

function opt(escape) {
    const arr = new Array(2);

    arr[0] = {};
    arr[1] = {};

    if (escape) {
        return arr;
    }

    return 0;
}

function main() {
    noInline(opt);

    for (let i = 0; i < 1000; i++) {
        opt(!(i % 10));
    }

    for (let i = 0; i < 100; i++) {
        const arr = opt(true);

        for (let j = 0; j < 5; j++) {
            const object = {};
            if (arr[0] === object) {
                throw new Error("bad");
            }
        }
    }
}

main();
