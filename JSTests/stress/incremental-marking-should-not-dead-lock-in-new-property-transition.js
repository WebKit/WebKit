//@ skip if $hostOS == "playstation" || $memoryLimited
//@ runDefault("--gcIncrementScale=100", "--gcIncrementBytes=10", "--numberOfGCMarkers=1")

let a = [];

for (let i = 0; i < 1000000; ++i) {
    let o = {};
    let p1 = `f${ (Math.random() * 10000000000) | 0 }`
    let p2 = `f${ (Math.random() * 10000000000) | 0 }`
    o[p1] = 20;
    o[p2] = 42;
    a.push(o);
}
