//@ skip if $buildType == "debug" or $memoryLimited or $addressBits <= 32
//@ slow!
//@ runDefault

let n = 44739242;
let s = 'a();'.repeat(n);
let f = new Function('a', s);

let caught = false;
try {
    f(function() { });
} catch (e) {
    caught = true;
    if (!(e instanceof RangeError))
        throw new Error("Expected RangeError but got: " + e);
}
if (!caught)
    throw new Error("Expected RangeError to be thrown");
