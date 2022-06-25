//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
    if (message !== undefined) {
        if (Object.prototype.toString.call(message) === '[object RegExp]') {
            if (!message.test(String(error)))
                throw new Error(`expected '${String(error)}' to match ${message}!`);
        } else {
            shouldBe(String(error), message);
        }
    }
}


// Invalid Date

{
    const date = new Date(NaN);
    shouldThrow(() => {date.toTemporalInstant()}, RangeError)
}

// Epoch 
{
    const epochDateInstant = new Date(0).toTemporalInstant();
    const epochInstant = new Temporal.Instant(0n);
    shouldBe(epochDateInstant.toString(), epochInstant.toString());
    shouldBe(epochDateInstant.epochNanoseconds, epochInstant.epochNanoseconds);
    
    shouldBe(epochDateInstant.epochSeconds, 0);
    shouldBe(epochDateInstant.epochMilliseconds, 0);
    shouldBe(epochDateInstant.epochMicroseconds, 0n);
    shouldBe(epochDateInstant.epochNanoseconds, 0n);

}

// Positive value: 10^18
{
    const dateToInstant = new Date(1_000_000_000_000).toTemporalInstant();
    const temporalInstant = new Temporal.Instant(1_000_000_000_000_000_000n);
    shouldBe(dateToInstant.toString(), temporalInstant.toString());
    shouldBe(dateToInstant.epochNanoseconds, temporalInstant.epochNanoseconds);
    
    shouldBe(dateToInstant.epochSeconds, 1_000_000_000);
    shouldBe(dateToInstant.epochMilliseconds, 1_000_000_000_000);
    shouldBe(dateToInstant.epochMicroseconds, 1_000_000_000_000_000n);
    shouldBe(dateToInstant.epochNanoseconds, 1_000_000_000_000_000_000n);
}

// Negative value: -10^18
{
    const dateToInstant = new Date(-1_000_000_000_000).toTemporalInstant();
    const temporalInstant = new Temporal.Instant(-1_000_000_000_000_000_000n);
    shouldBe(dateToInstant.toString(), temporalInstant.toString());
    shouldBe(dateToInstant.epochNanoseconds, temporalInstant.epochNanoseconds);

    shouldBe(dateToInstant.epochSeconds, -1_000_000_000);
    shouldBe(dateToInstant.epochMilliseconds, -1_000_000_000_000);
    shouldBe(dateToInstant.epochMicroseconds, -1_000_000_000_000_000n);
    shouldBe(dateToInstant.epochNanoseconds, -1_000_000_000_000_000_000n);
}

// Max instant
{
    const dateToInstant = new Date(86400_0000_0000_000).toTemporalInstant();
    const temporalInstant = new Temporal.Instant(86400_0000_0000_000_000_000n);
    shouldBe(dateToInstant.toString(), temporalInstant.toString());
    shouldBe(dateToInstant.epochNanoseconds, temporalInstant.epochNanoseconds);
    
    shouldBe(dateToInstant.epochSeconds, 86400_0000_0000);
    shouldBe(dateToInstant.epochMilliseconds, 86400_0000_0000_000);
    shouldBe(dateToInstant.epochMicroseconds, 86400_0000_0000_000_000n);
    shouldBe(dateToInstant.epochNanoseconds, 86400_0000_0000_000_000_000n);
}

//Min instant
{
    const dateToInstant = new Date(-86400_0000_0000_000).toTemporalInstant();
    const temporalInstant = new Temporal.Instant(-86400_0000_0000_000_000_000n);
    shouldBe(dateToInstant.toString(), temporalInstant.toString());
    shouldBe(dateToInstant.epochNanoseconds, temporalInstant.epochNanoseconds);
    
    shouldBe(dateToInstant.epochSeconds, -86400_0000_0000);
    shouldBe(dateToInstant.epochMilliseconds, -86400_0000_0000_000);
    shouldBe(dateToInstant.epochMicroseconds, -86400_0000_0000_000_000n);
    shouldBe(dateToInstant.epochNanoseconds, -86400_0000_0000_000_000_000n);
}