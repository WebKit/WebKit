//@ memoryHog!
//@ requireOptions("--useConcurrentJIT=0", "--useZombieMode=1", "--slowPathAllocsBetweenGCs=16")

function opt(s) {
    const o = {};

    try {
        return s + s;
    } catch {
        return o;
    }
}

function main() {
    noDFG(main);
    noFTL(main);

    for (let i = 0; i < 100; i++) {
        opt("hello");
    }

    const s = 's'.repeat(0x40000000);
    const a = [opt(s), opt(s), opt(s), opt(s), opt(s), opt(s), opt(s), opt(s)];

    setTimeout(() => {
        a.toString();
    }, 100);
}

main();
