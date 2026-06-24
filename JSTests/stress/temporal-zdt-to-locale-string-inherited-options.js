//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function shouldThrow(func, errorType) {
    let threw = false;
    try { func(); } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error(`Expected ${errorType.name} but got ${e.constructor.name}: ${e.message}`);
    }
    if (!threw)
        throw new Error(`Expected ${errorType.name} but no exception was thrown`);
}

const zdt = Temporal.ZonedDateTime.from("1970-01-01T00:00:00+00:00[UTC]");

// Inherited month option.
{
    const zdt2 = new Temporal.ZonedDateTime(0n, "UTC");
    shouldBe(zdt2.toLocaleString("en-US", Object.create({ month: "long" })), "January");
}

// Inherited option must be honored (spec uses Get(), not own-only access).
{
    const opts = Object.create({ month: "long" });
    shouldBe(zdt.toLocaleString("en-US", opts), "January");
}

// Inherited dateStyle must be honored.
{
    const opts = Object.create({ dateStyle: "full" });
    shouldBe(zdt.toLocaleString("en-US", opts), "Thursday, January 1, 1970");
}

// Own property overrides inherited one.
{
    const opts = Object.create({ month: "long" });
    opts.month = "short";
    shouldBe(zdt.toLocaleString("en-US", opts), "Jan");
}

// timeZone in inherited options must also throw TypeError.
{
    shouldThrow(() => zdt.toLocaleString("en-US", Object.create({ timeZone: "America/New_York" })), TypeError);
}

// null-prototype object (no inherited props) works like a plain object.
{
    const opts = Object.create(null);
    opts.month = "numeric";
    shouldBe(zdt.toLocaleString("en-US", opts), "1");
}
