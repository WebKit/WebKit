function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

(async function () {
    {
        let errorMessage = null;
        try {
            await import("./resources/error-module.js");
        } catch (error) {
            errorMessage = String(error);
        }
        shouldBe(errorMessage, `SyntaxError: Importing binding name 'x' is not found.`);
    }
    {
        let errorMessage = null;
        try {
            await import("./resources/error-module.js");
        } catch (error) {
            errorMessage = String(error);
        }
        shouldBe(errorMessage, `SyntaxError: Importing binding name 'x' is not found.`);
    }
}()).catch(abort);
