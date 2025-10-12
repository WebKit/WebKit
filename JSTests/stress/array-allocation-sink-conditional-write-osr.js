function test(write, escape) {
    let arr = new Array(4);

    arr[1] = 1.1;

    if (escape) {
        return arr;
    }

    if (write) {
        arr[0] = 2;
    }

}
noInline(test);

function test2(write, escape) {
    let arr = new Array(4);

    arr[1] = 1;

    if (escape) {
        return arr;
    }

    if (write) {
        arr[0] = 2;
    }
}
noInline(test2);

function test3(write, escape) {
    let arr = new Array(4);

    arr[1] = {};

    if (escape) {
        return arr;
    }

    if (write) {
        arr[0] = 2;
    }
}
noInline(test3);

function test4(overwrite, escape) {
    let arr = new Array(4);
    if (escape) {
        return arr;
    }
    arr[0] = 1.1;
    if (overwrite) {
        arr[0] = 3.14;
    }
}
noInline(test4);

// JIT warmup loop
for(let i = 0; i < testLoopCount; i++) {
    test(true, false);
    test(false, false);
    test2(true, false);
    test2(false, false);
    test3(true, false);
    test3(false, false);
    test4(true, false);
    test4(false, false);
}

function check(functor) {
    let noWrite = functor(true, true);
    if (0 in noWrite)
        throw new Error(noWrite);
}

check(test);
check(test2);
check(test3);

if (0 in test4(true, true))
    throw new Error();

if (0 in test4(false, true))
    throw new Error();

