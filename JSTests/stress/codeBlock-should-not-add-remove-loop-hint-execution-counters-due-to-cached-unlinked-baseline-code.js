//@ runDefault("--returnEarlyFromInfiniteLoopsForFuzzing=true", "--forceCodeBlockToJettisonDueToOldAge=true", "--collectContinuously=true")

async function foo() {
    for (let i = 0; i < 1000; i++);
}

for (let i = 0; i < 1000; i++) {
   foo();
   edenGC();
}
