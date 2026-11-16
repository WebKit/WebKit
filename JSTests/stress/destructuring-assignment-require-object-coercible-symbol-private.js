function shouldThrow(testFunction, expectedError) {
    let actualError = null;
    try {
        testFunction();
    } catch (e) {
        actualError = e;
    }

    if (!actualError) {
        throw new Error(
            `Expected to throw "${expectedError}" but no error was thrown`,
        );
    }

    const actualMessage = String(actualError);
    if (actualMessage !== expectedError) {
        throw new Error(
            `Expected error: "${expectedError}"\nActual error: "${actualMessage}"`,
        );
    }
}

{
    const destructToPrivateName = $vm.createBuiltin(
        "(function (value) { var { @Number } = value; })",
    );
    shouldThrow(() => {
        destructToPrivateName(null);
    }, "TypeError: Cannot destructure null or undefined value");
}

{
    const symbol = Symbol("symbol");
    shouldThrow(() => {
        ({ [symbol]: x } = null);
    }, "TypeError: Cannot destructure null or undefined value");
}

