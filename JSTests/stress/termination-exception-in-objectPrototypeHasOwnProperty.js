//@ runDefault("--watchdog=10", "--watchdog-exception-ok", "--useJIT=0")

function f1() {}
for (let i = 0; i < 1000000; ++i) {
    f1.hasOwnProperty('name');
}
