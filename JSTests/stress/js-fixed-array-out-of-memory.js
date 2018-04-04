//@ if $buildType == "debug" && !$memoryLimited then runDefault("--maxSingleAllocationSize=1048576") else skip end

var exception;

function foo() { };

function test(length) {
    try {
        foo([...new Array(length)].filter(() => { }));
    } catch (e) {
        exception = e;
    }

    if (exception && exception != "Error: Out of memory")
        throw "ERROR: length " + length + ": unexpected exception " + exception;
}

var sizes = [
    1, 10, 50, 100, 500, 1000, 5000, 10000, 50000, 100000, 500000,
    1000000, 5000000, 10000000, 50000000, 100000000, 500000000, 1000000000
];

for (size of sizes)
    test(size);

