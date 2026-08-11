//@ requireOptions("--useTemporal=1")

// Guards the UCalendar TinyLRUCache in TimeZoneICUBridge.cpp (timeZoneCacheEntry,
// capacity 16). Cycling 12 named zones fits the cache; a smaller capacity evicts
// before reuse and pays ucal_open (~900ns) on every access.
const zones = [
    "America/Vancouver", "America/Denver", "America/Chicago", "America/New_York",
    "America/Sao_Paulo", "Europe/London", "Europe/Paris", "Africa/Cairo",
    "Asia/Kolkata", "Asia/Tokyo", "Australia/Sydney", "Pacific/Auckland",
];
const instant = Temporal.Instant.fromEpochMilliseconds(1750000000000); // 2025-06-15T15:06:40Z
const zdts = zones.map(zone => instant.toZonedDateTimeISO(zone));

let acc = 0;
for (var i = 0; i < 2400000; ++i)
    acc += zdts[i % zdts.length].offsetNanoseconds / 900e9; // ICU offset lookup per iteration

if (acc !== 14000000)
    throw "Bad result: " + acc;
