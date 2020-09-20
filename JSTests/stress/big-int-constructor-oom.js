//@ skip if $buildType == "debug" or $memoryLimited

function assert(a) {
    if (!a)
        throw new Error("Bad");
}

var longStr = "f";
for (var i = 0; i < 30; ++i)
   longStr = longStr + longStr;

let sub = longStr.substring(0, longStr.length - 4)
let sNumber = "0x" + longStr + sub + "f";

try {
    BigInt(sNumber);
    assert(false);
} catch(e) {
    assert(e.message == "Out of memory: BigInt generated from this operation is too big")
}

