//@ $skipModes << :lockdown if ($buildType == "debug")

function empty() {}
function empty2() {}

function test(arr) {
    empty.apply(undefined, arr);
    empty2();
}

for (let i = 0; i < testLoopCount; i++) {
    let arr = [];
    for (let j = 0; j < i+testLoopCount; j++) {
        arr.push(undefined);
    }
    test(arr);
}
