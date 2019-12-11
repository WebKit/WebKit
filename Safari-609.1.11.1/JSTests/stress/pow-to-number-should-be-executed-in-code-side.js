function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let value = {
        valueOf()
        {
            throw new Error("NG");
        }
    };
    let error = null;

    try {
        2 ** value;
    } catch (e) {
        error = e;
    }
    // global, and valueOf.
    shouldBe(error.stack.split("\n").length, 2);
}

{
    let value = {
        valueOf()
        {
            throw new Error("NG");
        }
    };
    let error = null;

    try {
        value ** 2;
    } catch (e) {
        error = e;
    }
    // global, and valueOf.
    shouldBe(error.stack.split("\n").length, 2);
}

{
    let value = {
        valueOf()
        {
            throw new Error("NG");
        }
    };
    let error = null;

    try {
        Math.pow(value, 2);
    } catch (e) {
        error = e;
    }
    // global, Math.pow, and valueOf.
    shouldBe(error.stack.split("\n").length, 3);
}

{
    let value = {
        valueOf()
        {
            throw new Error("NG");
        }
    };
    let error = null;

    try {
        Math.pow(2, value);
    } catch (e) {
        error = e;
    }
    // global, Math.pow, and valueOf.
    shouldBe(error.stack.split("\n").length, 3);
}
