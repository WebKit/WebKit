function shouldBe(actual, expected) {
    if (actual != expected) throw new Error(`bad value: ${actual}`);
}

const nonContinentalTimeZones = [
    "Etc/GMT+1",
    "Etc/GMT+2",
    "Etc/GMT+3",
    "Etc/GMT+4",
    "Etc/GMT+5",
    "Etc/GMT+6",
    "Etc/GMT+7",
    "Etc/GMT+8",
    "Etc/GMT+9",
    "Etc/GMT+10",
    "Etc/GMT+11",
    "Etc/GMT+12",
    "Etc/GMT-1",
    "Etc/GMT-2",
    "Etc/GMT-3",
    "Etc/GMT-4",
    "Etc/GMT-5",
    "Etc/GMT-6",
    "Etc/GMT-7",
    "Etc/GMT-8",
    "Etc/GMT-9",
    "Etc/GMT-10",
    "Etc/GMT-11",
    "Etc/GMT-12",
    "Etc/GMT-13",
    "Etc/GMT-14",
    "UTC",
];

const supportedTimeZones = Intl.supportedValuesOf("timeZone");

for (let i = 0; i < testLoopCount; ++i) {
    for (const tz of nonContinentalTimeZones) {
        shouldBe(supportedTimeZones.includes(tz), true);
    }
}
