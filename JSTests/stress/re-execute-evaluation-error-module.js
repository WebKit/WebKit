var abort = $vm.abort;

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

(async function () {
    {
        let errorMessage = null;
        try {
            await import("./resources/evaluation-error-module.js");
        } catch (error) {
            errorMessage = String(error);
        }
        shouldBe(errorMessage, `Error: evaluation error`);
    }
    {
        let errorMessage = null;
        try {
            await import("./resources/evaluation-error-module.js");
        } catch (error) {
            errorMessage = String(error);
        }
        shouldBe(errorMessage, `Error: evaluation error`);
    }
}()).catch(abort);
