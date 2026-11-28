function test(v) {
    if (typeof v === "object")
        return 1;
    return 0;
}
noInline(test);
noDFG(test);

const values = [{}, [], () => { }, Map, null, "string", 42, undefined];
let sum = 0;
for (let i = 0; i < 2e6; ++i)
    sum += test(values[i & 7]);
if (sum !== 3 * 250000)
    throw new Error("bad value " + sum);
