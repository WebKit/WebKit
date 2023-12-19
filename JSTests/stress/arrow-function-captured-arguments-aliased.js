function createOptAll(count) {
    count = 0;

    return () => {
        arguments[0]++;

        return count;
    };
}

function createOpt500(count) {
    count = 0;

    return () => {
        arguments[0]++;

        if (arguments[0] < 500)
            return arguments[0];

        return count;
    };
}

function createOpt2000(count) {
    count = 0;

    return () => {
        arguments[0]++;

        if (arguments[0] < 2000)
            return arguments[0];

        return count;
    };
}

function createOpt5000(count) {
    count = 0;

    return () => {
        arguments[0]++;

        if (arguments[0] < 5000)
            return arguments[0];

        return count;
    };
}

function main() {
    const testCount = 10000;
    const createOptFuncs = [createOptAll, createOpt500, createOpt2000, createOpt5000];
    for (createOptFunc of createOptFuncs) {
    const opt = createOptFunc(0);
        for (let i = 0; i < testCount; i++) {
            opt();
        }

        const expectedResult = testCount+1;
        let actualResult = opt();
        if (actualResult != expectedResult)
            print("Expected " + expectedResult + ", got " + actualResult);
    }
}

main();
