//@ runDefault("--useConcurrentJIT=0")

let caughtInOpt = false;
let caughtInMain = false;

function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

function opt(proxy) {
    try {
        "x" in proxy;
    } catch {
        caughtInOpt = true;
    }
}

function main() {
    const handler = {};
    const proxy = new Proxy({}, handler);

    for (let i = 0; i < 100000; i++) {
        opt(proxy);
    }

    handler.has = () => {
        throw 1;
    };

    try {
        opt(proxy);
    } catch {
        caughtInMain = true;
    }
}

main();

assert(caughtInOpt);
assert(!caughtInMain);
