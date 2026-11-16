//@ runDefault("--watchdog=10", "--watchdog-exception-ok", "--useJIT=0")

function f1() {}
for (let i = 0; i < testLoopCount; ++i) {
    f1.hasOwnProperty('name');
}
