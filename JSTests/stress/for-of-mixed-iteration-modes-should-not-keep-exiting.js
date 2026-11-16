//@ skip if not $jitTests
//@ $skipModes << :lockdown

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

let siteCount = 0;
function makeSite() {
    let site = new Function("iterable", `
        let count = 0;
        for (let x of iterable)
            count++;
        return count;
        // ${siteCount++}`);
    noInline(site);
    return site;
}

let array = [1];
let map = new Map([[1, 2]]);
let set = new Set([1]);
let kinds = {
    arrayValues: () => array.values(),
    arrayKeys: () => array.keys(),
    map: () => map,
    set: () => set,
    string: () => "1",
};

let sites = [
    ["map", "set"],
    ["set", "string"],
    ["arrayValues", "arrayKeys"],
];

function test(names) {
    let site = makeSite();
    for (let i = 0; i < testLoopCount * 6; ++i)
        shouldBe(site(kinds[names[i % names.length]]()), 1, names);
    let compiles = numberOfDFGCompiles(site);
    if (compiles > 4)
        throw new Error(names + ": compiled " + compiles + " times");
}
noDFG(test);

for (let names of sites)
    test(names);
