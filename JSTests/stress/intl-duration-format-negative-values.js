function sameValue(a, b) {
  if (a !== b)
      throw new Error(`Expected ${b} but got ${a}`);
}

{
  const duration = { hours: 0, seconds: -1 };
  const df = new Intl.DurationFormat("en", { hoursDisplay: "always" });
  const formatted = df.format(duration);
  sameValue(formatted, "-0 hr, 1 sec");
}

{
  const duration = { years: -1, months: -2 };
  const df = new Intl.DurationFormat("en");
  const formatted = df.format(duration);
  sameValue(formatted, "-1 yr, 2 mths");
}
