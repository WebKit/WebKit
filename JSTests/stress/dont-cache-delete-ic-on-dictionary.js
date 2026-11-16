//@ runDefault("--repatchBufferingCountdown=8", "--useDFGJIT=0", "--jitPolicyScale=0", "--useConcurrentJIT=0")

function assert(b) {
    if (!b)
        throw new Error;
}

function foo(i) {
    Object.defineProperty(Reflect, "get", {});
    for (let i = 0; i < 2; i++) {
        delete Reflect.get;
    }
}

for (let i=0; i<100; i++) {
    foo(i);
}
