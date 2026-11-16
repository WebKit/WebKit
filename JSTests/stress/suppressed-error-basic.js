//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

shouldBe(SuppressedError.prototype.constructor, SuppressedError);
shouldBe(SuppressedError.prototype.message, "");
shouldBe(SuppressedError.prototype.name, "SuppressedError");

{
    class OurSuppressedError extends SuppressedError {
        constructor(...args) {
            super(...args);
        }
    }
    const e1 = new OurSuppressedError();
    shouldBe(e1 instanceof Error, true);
    shouldBe(e1 instanceof SuppressedError, true);
}

{
    let toStringCallCount = 0;
    const obj = {
        toString() {
            toStringCallCount++;
            return "this is error message";
        }
    };
    const e1 = new SuppressedError(undefined, undefined, obj);
    shouldBe(toStringCallCount, 1);
    shouldBe(e1.message, "this is error message");

    const e2 = SuppressedError(undefined, undefined, obj);
    shouldBe(toStringCallCount, 2);
    shouldBe(e2.message, "this is error message");

}

{
    const error = {};
    const suppressed = {};
    const message = "message";

    const e1 = new SuppressedError(error, suppressed, message);
    shouldBe(e1.error, error);
    shouldBe(e1.suppressed, suppressed);
    shouldBe(e1.message, message);

    const e2 = SuppressedError(error, suppressed, message);
    shouldBe(e2.error, error);
    shouldBe(e2.suppressed, suppressed);
    shouldBe(e2.message, message);
}
