//@ $skipModes << :lockdown if ($buildType == "debug") or ($architecture == "mips")

function empty() {}
function empty2() {}

function test(arr) {
    empty.apply(undefined, arr);
    empty2();
}

for (let i = 0; i < 10000; i++) {
    let arr = [];
    for (let j = 0; j < i+10000; j++) {
        arr.push(undefined);
    }
    test(arr);
}
