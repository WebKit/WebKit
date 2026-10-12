//@ $skipModes << :lockdown if $buildType == "debug"

function listener(a, b, c) {
    return arguments.length;
}

function notify(args) {
    return listener(...args);
}
noInline(notify);

const argumentsList = [[], [{}], [{}, 1], [{}, 1, "a"], [1.5], [1.5, 2.5, 3.5], [1, 2], [1, 2, 3]];
let result = 0;
for (let i = 0; i < 4e6; ++i)
    result += notify(argumentsList[i & 7]);
if (result !== 7.5e6)
    throw new Error(result);
