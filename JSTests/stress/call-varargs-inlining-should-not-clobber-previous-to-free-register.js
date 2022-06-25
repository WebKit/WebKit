//@ skip if $memoryLimited
//@ runDefault("--useConcurrentJIT=0", "--watchdog=4000", "--watchdog-exception-ok")
(function __v0() {
    try {
        __v0(__v0.apply(this, arguments));
    } catch (e) {
        for (let i = 0; i < 10000; i++) {
        }
    }
})(2);
