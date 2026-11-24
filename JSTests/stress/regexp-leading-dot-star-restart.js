//@ runDefault("--useRegExpJIT=false", "--useRegExpInterpreterBodyRestart=true")

const tests = [
    [/.*a/, "b\nxa", 0, [2,"xa"], 0],
    [/.*a/, "xxa", 1, [0,"xxa"], 1],
    [/.*a/g, "xa\nya", 2, [3,"ya"], 5],
    [/.*a/g, "xxa\nb", 1, [1,"xa"], 3],
    [/.*?a/, "b\r\nca", 0, [3,"ca"], 0],
    [/.+b/, "a\u2028ab", 0, [2,"ab"], 0],
    [/.+b/, "a\u2029b", 0, null, 0],
    [/.{3,}c/, "ab\nxyzc", 0, [3,"xyzc"], 0],
    [/.{3,}c/, "ab\nxyc", 0, null, 0],
    [/.{2,}?c/g, "zzc\nyc", 1, null, 0],
    [/.*@(.*):(\d*)/, "u@h:1\nv@w:22", 0, [0,"u@h:1","h","1"], 0],
    [/.*@(.*):(\d*)/g, "u@h:1\nv@w:22", 6, [6,"v@w:22","w","22"], 12],
    [/.+!(.*?$)/, "a.b!c", 0, [0,"a.b!c","c"], 0],
    [/.+!(.*?$)/m, "x\n!a\nb!c", 0, [5,"b!c","c"], 0],
    [/.*\b\w/, "  \n a", 0, [3," a"], 0],
    [/.*(?<=\n)a/, "x\na", 0, [2,"a"], 0],
    [/.*(?<!x)b/, "xb\nxb", 0, null, 0],
    [/.*$/, "a\n", 2, [2,""], 2],
    [/.*/g, "ab\ncd", 3, [3,"cd"], 5],
    [/.*/, "", 0, [0,""], 0],
    [/[\s\S]*a/, "x\nya", 1, [0,"x\nya"], 1],
    [/[\s\S]*a/, "x\nyb", 1, null, 1],
    [/[\s\S]*a/g, "x\nya\nb", 2, [2,"ya"], 4],
    [/[\s\S]*a/g, "xa\nyb", 2, null, 0],
    [/.*a/gs, "xa\nyb", 2, null, 0],
    [/.*?a/gs, "b\nxa\nya", 1, [1,"\nxa"], 4],
    [/[^]*a/g, "xa\nyb", 1, [1,"a"], 2],
    [/.*a/s, "x\nya", 1, [0,"x\nya"], 1],
    [/.*a/s, "x\nyb", 0, null, 0],
    [/.*?a/is, "B\nXA", 2, [0,"B\nXA"], 2],
    [/.*a/i, "x\nyA", 0, [2,"yA"], 0],
    [/.*a/y, "xa\nb", 1, [1,"a"], 2],
    [/.*a/y, "x\na", 1, null, 0],
    [/.*b/u, "a\uD83D\uDE00b", 2, [0,"a\uD83D\uDE00b"], 2],
    [/.*b/gu, "a\uD83D\uDE00b", 2, [3,"b"], 4],
    [/.{0,2}a/, "xxxxa", 0, [2,"xxa"], 0],
    [/.?a/, "xxa", 0, [1,"xa"], 0],
    [/(.*)\1x/, "abab\nccx", 0, [5,"ccx","c"], 0],
    [/.*(?:^a)/m, "x\na", 0, [2,"a"], 0],
    [/.*a|b/, "xxb", 0, [2,"b"], 0],
    [/(?<n>.*)\k<n>z/, "q\naaz", 0, [2,"aaz","a"], 0],
    [/.*\uDE00/, "a\uD83D\uDE00", 1, [0,"a\uD83D\uDE00"], 1],
    [/.*$/m, "a\nb", 3, [0,"a"], 3],
    [/.*.*b.*/, "xx\nab\nc", 0, [3,"ab"], 0],
    [/.*.*b.*/g, "ab\nxb\nc", 1, [1,"b"], 2],
    [/.*.*a\nb.*/g, "xa\nb\nya\nbz", 2, [5,"ya\nbz"], 10],
    [/.*.*b.*/gs, "x\nab\nc", 3, [3,"b\nc"], 6],
];

for (const [regexp, input, lastIndex, expected, expectedLastIndex] of tests) {
    regexp.lastIndex = lastIndex;
    const match = regexp.exec(input);
    const actual = match ? [match.index, ...match] : null;
    if (JSON.stringify(actual) !== JSON.stringify(expected) || regexp.lastIndex !== expectedLastIndex)
        throw new Error(`${regexp} on ${JSON.stringify(input)} from ${lastIndex}: expected ${JSON.stringify(expected)} and lastIndex ${expectedLastIndex}, got ${JSON.stringify(actual)} and lastIndex ${regexp.lastIndex}`);
}

const long = "x".repeat(5000) + "\n" + "y".repeat(5000);
if (/.*z/.test(long) || /.+!(.*?$)/.test(long) || /.*z/s.test(long) || /[\s\S]*z/.test(long))
    throw new Error("unexpected match on long input");
if (/.*y$/.exec(long).index !== 5001)
    throw new Error("wrong match on long input");

const limit = ("a".repeat(600) + "\n").repeat(3) + "c";
if (/.*(?:a|b)*c/.exec(limit)?.index !== limit.length - 1)
    throw new Error("wrong match on match limit input");
