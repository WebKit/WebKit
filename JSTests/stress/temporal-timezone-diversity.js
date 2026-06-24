//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}
{
    const plus530 = new Temporal.ZonedDateTime(0n, "+05:30");
    shouldBe(plus530.offset, "+05:30", "+05:30 offset");
    shouldBe(plus530.hour, 5, "+05:30 hour at epoch 0");
    shouldBe(plus530.minute, 30, "+05:30 minute at epoch 0");
    shouldBe(plus530.hoursInDay, 24, "+05:30 always 24h days");
}
{
    const minus12 = new Temporal.ZonedDateTime(0n, "-12:00");
    shouldBe(minus12.offset, "-12:00", "-12:00 offset");
    shouldBe(minus12.day, 31, "-12:00 is still Dec 31 1969");
}
{
    // Asia/Kolkata: always +05:30
    const kolkata = Temporal.ZonedDateTime.from("2024-06-15T12:00[Asia/Kolkata]");
    shouldBe(kolkata.offset, "+05:30", "Kolkata offset summer");
    const kolkataWinter = Temporal.ZonedDateTime.from("2024-01-15T12:00[Asia/Kolkata]");
    shouldBe(kolkataWinter.offset, "+05:30", "Kolkata offset winter (same)");
    shouldBe(kolkata.hoursInDay, 24, "Kolkata always 24h");
}
{
    // Asia/Tokyo: always +09:00
    const tokyo = Temporal.ZonedDateTime.from("2024-06-15T12:00[Asia/Tokyo]");
    shouldBe(tokyo.offset, "+09:00", "Tokyo offset");
    shouldBe(tokyo.getTimeZoneTransition("next"), null, "Tokyo no future DST");
}
{
    // Asia/Kathmandu: +05:45 (not on hour or half-hour boundary)
    const ktm = Temporal.ZonedDateTime.from("2024-01-01T12:00[Asia/Kathmandu]");
    shouldBe(ktm.offset, "+05:45", "Kathmandu +05:45");
}
{
    // Pacific/Chatham: +12:45 / +13:45 (New Zealand's Chatham Islands)
    const chatham = Temporal.ZonedDateTime.from("2024-01-15T12:00[Pacific/Chatham]");
    // Summer in southern hemisphere — DST active
    shouldBe(chatham.offset, "+13:45", "Chatham summer +13:45");
}
{
    const epoch = 1718456400000000000n; // 2024-06-15T13:00:00Z
    const utc = new Temporal.ZonedDateTime(epoch, "UTC");
    const nyc = new Temporal.ZonedDateTime(epoch, "America/New_York");
    const tokyo = new Temporal.ZonedDateTime(epoch, "Asia/Tokyo");
    shouldBe(utc.hour, 13, "UTC 13:00");
    shouldBe(nyc.hour, 9, "NYC 09:00 (EDT)");
    shouldBe(tokyo.hour, 22, "Tokyo 22:00 (JST)");
    shouldBe(utc.epochNanoseconds, nyc.epochNanoseconds, "same instant UTC=NYC");
    shouldBe(utc.epochNanoseconds, tokyo.epochNanoseconds, "same instant UTC=Tokyo");
}
{
    // Apia is UTC+13 (ahead of date line)
    const apia = Temporal.ZonedDateTime.from("2024-01-01T00:00[Pacific/Apia]");
    shouldBe(apia.offset, "+13:00", "Apia +13:00");
}
{
    // Australia/Sydney: DST in southern summer (Oct-Apr)
    const sydSummer = Temporal.ZonedDateTime.from("2024-01-15T12:00[Australia/Sydney]");
    shouldBe(sydSummer.offset, "+11:00", "Sydney summer AEDT");
    const sydWinter = Temporal.ZonedDateTime.from("2024-07-15T12:00[Australia/Sydney]");
    shouldBe(sydWinter.offset, "+10:00", "Sydney winter AEST");
}
{
    const nyc = Temporal.ZonedDateTime.from("2024-06-15T12:00[America/New_York]");
    const nextTrans = nyc.getTimeZoneTransition("next");
    shouldBe(nextTrans !== null, true, "NYC has next transition");
    shouldBe(nextTrans instanceof Temporal.ZonedDateTime, true, "getTimeZoneTransition returns ZDT");
    shouldBe(nextTrans.month, 11, "Next NYC transition in November");
    const prevTrans = nyc.getTimeZoneTransition("previous");
    shouldBe(prevTrans !== null, true, "NYC has prev transition");
    shouldBe(prevTrans.month, 3, "Prev NYC transition in March");
}
{
    const utc = Temporal.ZonedDateTime.from("2024-06-15T12:00[UTC]");
    shouldBe(utc.offset, "+00:00", "UTC offset string");
    shouldBe(utc.hoursInDay, 24, "UTC always 24h");
    shouldBe(utc.getTimeZoneTransition("next"), null, "UTC no transitions");
    shouldBe(utc.getTimeZoneTransition("previous"), null, "UTC no transitions prev");
}
