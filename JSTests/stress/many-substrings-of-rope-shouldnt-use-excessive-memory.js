//@ skip if $architecture == "arm" or $memoryLimited
//@ runDefault

function getLongRopeString() {
    let rope = "";
    for (let i = 0; i < 20000; ++i)
        rope += "a";
    return rope;
}

let string = getLongRopeString();

let result = [];
for (let i = 0; i < string.length; i += 5)
    result.push(string.slice(i, i + 5));

let footprint = MemoryFootprint();
if (footprint.peak > 14000000)
    throw new Error("This test used to use ~7000000 but is now using " + footprint.peak);

