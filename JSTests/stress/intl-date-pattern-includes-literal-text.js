function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

shouldBe(new Intl.DateTimeFormat("fr", {hour: "numeric", hour12: false}).resolvedOptions().hour12, false);
shouldBe(new Intl.DateTimeFormat("fr", {hour: "numeric", hour12: false}).format(new Date(2021, 2, 3, 23)), `23 h`);

shouldBe(new Intl.DateTimeFormat("fr", {hour: "numeric", hourCycle: 'h24'}).format(new Date(2021, 2, 3, 23)), '23 h');
shouldBe(new Intl.DateTimeFormat("fr", {hour: "numeric", hourCycle: 'h23'}).format(new Date(2021, 2, 3, 23)), '23 h');

shouldBe(JSON.stringify(new Intl.Locale("fr", {hourCycle: 'h24'}).hourCycles), `["h24"]`);
shouldBe(JSON.stringify(new Intl.Locale("fr", {hourCycle: 'h23'}).hourCycles), `["h23"]`);
shouldBe(JSON.stringify(new Intl.Locale("fr").hourCycles), `["h23"]`);
