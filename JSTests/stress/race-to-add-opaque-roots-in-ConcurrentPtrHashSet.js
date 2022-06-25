//@ skip if $memoryLimited
//@ runDefault("--verifyGC=true", "--slowPathAllocsBetweenGCs=2")
//@ slow!

let array = [];
for (let i = 0; i < 100000; i++)
    array[i] = new DataView(new ArrayBuffer());
