//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let data = Temporal.PlainTime.from({
      hour: 0,
      minute: 0,
      second: 0,
      millisecond: 1000,
      microsecond: 0,
      nanosecond: 0,
    }).toString();

    shouldBe(data, `00:00:00.999`);
}
{
    let data = Temporal.PlainTime.from({
      hour: 0,
      minute: 0,
      second: 0,
      millisecond: 0,
      microsecond: 1000,
      nanosecond: 0,
    }).toString();

    shouldBe(data, `00:00:00.000999`);
}
{
    let data = Temporal.PlainTime.from({
      hour: 0,
      minute: 0,
      second: 0,
      millisecond: 0,
      microsecond: 0,
      nanosecond: 1000,
    }).toString();

    shouldBe(data, `00:00:00.000000999`);
}
