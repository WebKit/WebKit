function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Bad value: " + actual + " expected: " + expected);
}

function shouldThrow(func, errorType) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (!(error instanceof errorType))
            throw new Error("Bad error: " + error);
    }
    if (!errorThrown)
        throw new Error("Should have thrown");
}

async function asyncReturn42() {
    return 42;
}

async function asyncReturnUndefined() {
    let x = 1;
    x++;
}

async function asyncThrow() {
    throw new Error("test error");
}

const asyncArrowReturn = async () => {
    return "arrow result";
};

const asyncArrowExpr = async () => "expression result";

async function asyncWithLocals() {
    let a = 1;
    let b = 2;
    let c = a + b;
    return c;
}

function helper() {
    return "helper result";
}

async function asyncCallsHelper() {
    return helper();
}

async function asyncWithTryCatch() {
    try {
        throw new Error("caught");
    } catch (e) {
        return "caught: " + e.message;
    }
}

async function asyncWithControlFlow(x) {
    if (x > 0) {
        return "positive";
    } else if (x < 0) {
        return "negative";
    } else {
        return "zero";
    }
}

class AsyncClass {
    async asyncMethod() {
        return "method result";
    }

    static async staticAsyncMethod() {
        return "static method result";
    }
}

async function asyncWithAwait() {
    return await Promise.resolve(100);
}

async function runTests() {
    let result1 = await asyncReturn42();
    shouldBe(result1, 42);

    let result2 = await asyncReturnUndefined();
    shouldBe(result2, undefined);

    let caught3 = false;
    try {
        await asyncThrow();
    } catch (e) {
        caught3 = true;
        shouldBe(e.message, "test error");
    }
    shouldBe(caught3, true);

    let result4 = await asyncArrowReturn();
    shouldBe(result4, "arrow result");

    let result5 = await asyncArrowExpr();
    shouldBe(result5, "expression result");

    let result6 = await asyncWithLocals();
    shouldBe(result6, 3);

    let result7 = await asyncCallsHelper();
    shouldBe(result7, "helper result");

    let result8 = await asyncWithTryCatch();
    shouldBe(result8, "caught: caught");

    shouldBe(await asyncWithControlFlow(5), "positive");
    shouldBe(await asyncWithControlFlow(-5), "negative");
    shouldBe(await asyncWithControlFlow(0), "zero");

    let instance = new AsyncClass();
    shouldBe(await instance.asyncMethod(), "method result");
    shouldBe(await AsyncClass.staticAsyncMethod(), "static method result");

    let result11 = await asyncWithAwait();
    shouldBe(result11, 100);

    for (let i = 0; i < 10000; i++) {
        await asyncReturn42();
        await asyncArrowReturn();
        await asyncWithLocals();
    }
}

runTests().catch($vm.abort);
