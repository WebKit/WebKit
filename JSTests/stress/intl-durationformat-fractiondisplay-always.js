function shouldThrow(op, errorConstructor) {
    try {
        op();
    } catch (e) {
        if (!(e instanceof errorConstructor)) {
            throw new Error(`threw ${e}, but should have thrown ${errorConstructor.name}`);
        }
        return;
    }
    throw new Error(`Expected to throw ${errorConstructor.name}, but no exception thrown`);
}

function shouldNotThrow(func) {
    func();
}

const fractionStyles = ["milliseconds", "microseconds", "nanoseconds"];

for (const fractionStyle of fractionStyles) {
    shouldThrow(() => {
        new Intl.DurationFormat("en", {
            [fractionStyle]: "numeric",
            [fractionStyle + "Display"]: "always",
        });
    }, RangeError);

    // With the "digital" style, fractional-second units default to the "numeric" unit style.
    // Setting their display to "always" must therefore throw a RangeError.
    shouldThrow(() => {
        new Intl.DurationFormat("en", {
            style: "digital",
            [fractionStyle + "Display"]: "always",
        });
    }, RangeError);

    shouldNotThrow(() => {
        new Intl.DurationFormat("en", {
            [fractionStyle]: "numeric",
        });
    });
}
