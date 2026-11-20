//@ $skipModes << :lockdown if $buildType == "debug"

function callee(a, b, c) {
    return a === undefined && b === undefined && c === undefined ? 1 : 0;
}

function test(args) {
    return callee.apply(null, args);
}
noInline(test);

let result = 0;
for (let i = 0; i < 4e6; ++i)
    result += test(i & 1 ? null : undefined);
if (result !== 4e6)
    throw new Error(result);
