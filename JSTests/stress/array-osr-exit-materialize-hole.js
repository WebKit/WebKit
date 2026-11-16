function test(flag) {
    let arr = new Array(2);
    arr[0] = {};
    if (!flag) {
        leaked = arr; // trigger InadequateCoverage OSR exit
    }
}
noInline(test);

for(let i = 0; i < testLoopCount; i++) {
    test(true);
}

test(false);
if (leaked[1] !== undefined)
    throw new Error();