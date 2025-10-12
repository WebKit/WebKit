function test(write, escape) {
    let arr = new Array(4);

    arr[1] = 1.1;

    if (write) {
        arr[0] = 2;
    }

    if (escape) {
        return arr;
    }
}
noInline(test);

function test2(write, escape) {
    let arr = new Array(4);

    arr[1] = 1;

    if (write) {
        arr[0] = 2;
    }

    if (escape) {
        return arr;
    }
}
noInline(test2);

function test3(write, escape) {
    let arr = new Array(4);

    arr[1] = {};

    if (write) {
        arr[0] = 2;
    }

    if (escape) {
        return arr;
    }
}
noInline(test3);

function test4(overwrite, escape) {
    let arr = new Array(4);
    arr[0] = 1.1;
    if (overwrite) {
        arr[0] = 3.14;
    }
    if (escape) {
        return arr;
    }
}
noInline(test4);

// JIT warmup loop
for(let i = 0; i < testLoopCount; i++) {
    test(true, false);
    test(false, true);
    test2(true, false);
    test2(false, true);
    test3(true, false);
    test3(false, true);
    test4(true, false);
    test4(false, true);
}

function check(functor) {
    let write = functor(true, true);
    if (write[0] !== 2)
        throw new Error(write);

    let noWrite = functor(false, true);
    if (0 in noWrite)
        throw new Error(noWrite);
}

check(test);
check(test2);
check(test3);

if (test4(true, true)[0] !== 3.14)
    throw new Error();

if (test4(false, true)[0] !== 1.1)
    throw new Error();

