//@ skip if $memoryLimited
//@ runDefault if !$memoryLimited
// This test should not crash.

var error;
let str = '';
let arr = [{}, 2, 3];
try {
    for (let z = 0; z < 30; z++)
        str = arr.join(str); // exponentially grow length of string.
} catch(e) {
    error = e;
}

if (!error)
    throw Error("Failed");
