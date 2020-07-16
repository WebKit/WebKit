//@ skip if $memoryLimited
//@ runDefault("--returnEarlyFromInfiniteLoopsForFuzzing=1")
//@ slow!

// This test needs 3 seconds to reproduce the crash.

function foo() {
    let a=0;
    while(1) a++;
}

for (let i=0; i<10; i++) {
    runString(`${foo.toString()};foo();foo();`);
}
gc();
