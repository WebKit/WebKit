// A substring rope caches its base's 8-bit flag, but atomizing the base can swap in an equal 16-bit StringImpl.

function test(s) { return s === "abcde"; }
noInline(test);

const warmBase = "abcdeXYZWVUTSRQPONMLKJIHGFEDCBA".split("").join("");
warmBase.indexOf("#");
const warmWide = "\u3042" + warmBase;
warmWide.indexOf("#");
const warm = [
    "abcde", "abcdf", warmBase.slice(0, 5), warmBase.slice(1, 6), warmWide.slice(1, 6),
    "xyz", warmBase.slice(0, 4) + "e",
];
const expected = [true, false, true, false, true, false, true];
for (let i = 0; i < testLoopCount * 20; ++i) {
    const index = i % warm.length;
    if (test(warm[index]) !== expected[index])
        throw new Error("bad result for " + JSON.stringify(warm[index]));
}

let content = "abcde";
for (let i = 0; i < 75; ++i)
    content += String.fromCharCode(102 + ((i * 7) % 20));
content.indexOf("#");

const sub = content.slice(0, 5);

const wide16 = ["", ("\u3042" + content).replace("\u3042", "")].join("");
const object = {};
object[wide16] = 1;
object[content] = 2;

for (let i = 0; i < testLoopCount; ++i) {
    if (test(sub) !== true)
        throw new Error("substring of a base swapped to a 16-bit atom compared unequal");
}
