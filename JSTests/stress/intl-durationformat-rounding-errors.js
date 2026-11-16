function sameValue(actual, expected) {
  if (actual !== expected)
    throw new Error(`Expected ${expected} but got ${actual}`);
}

{
  // This is calculated as 9.123456788999999 in double
  const duration = {
    seconds: 9,
    milliseconds: 123,
    microseconds: 456,
    nanoseconds: 789,
  };

  const df = new Intl.DurationFormat('en', { style: "digital" });
  sameValue(df.format(duration), "0:00:09.123456789");
}

{
  // This is calculated as 10000000.000000002 in double
  const duration = {
    seconds: 10_000_000,
    nanoseconds: 1,
  };

  const df = new Intl.DurationFormat('en', { style: "digital" });
  sameValue(df.format(duration), "0:00:10000000.000000001");
}

{
  // This is calculated as 0:00:9,007,199,254,740,992 in double
  const duration = {
    // Actual value is: 4503599627370497024
    milliseconds: 4503599627370497_000,
    // Actual value is: 4503599627370494951424
    microseconds: 4503599627370495_000000,
  };

  const df = new Intl.DurationFormat('en', { style: 'digital' });
  sameValue(df.format(duration), '0:00:9007199254740991.975424');
}
