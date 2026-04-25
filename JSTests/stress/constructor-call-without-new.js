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
    class Foo { }
    shouldThrow(() => {
        Foo();
    }, "TypeError: Cannot call a class constructor Foo without |new|");
}

{
  shouldThrow(() => {
    (class { })()
  }, "TypeError: Cannot call a class constructor without |new|");
}
