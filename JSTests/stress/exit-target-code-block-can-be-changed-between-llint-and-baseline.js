//@ runDefault("--useConcurrentJIT=0", "--useFTLJIT=0")
function bar() {
    try {
        throw new Error();
    } catch {}
        +arguments;
}

function foo() {
    for (let i=0; i<100; i++) {}

    bar(...[]);

    for (let i=0; i<100; i++) {
        bar();
    }
}

for (let i=0; i<100; i++) {
    foo(Object);
    gc();
}
