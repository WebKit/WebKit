function testArrayConcat() {
    var array = createRuntimeArray(1, 2, 3);
    var result = array.concat();

    if (result.length != 3)
        throw new Error("Runtime array length is incorrect");
    for (var i = 0; i < result.length; i++) {
        if (result[i] != i + 1)
            throw new Error("Runtime array concat result is incorrect");
    }
};

testArrayConcat();
