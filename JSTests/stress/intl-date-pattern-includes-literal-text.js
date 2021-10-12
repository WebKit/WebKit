function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

shouldBe(new Intl.DateTimeFormat("fr", {hour: "numeric", hour12: false}).resolvedOptions().hour12, false);
shouldBe(new Intl.DateTimeFormat("fr", {hour: "numeric", hour12: false}).format(new Date(2021, 2, 3, 23)), `23 h`);

shouldBe(new Intl.DateTimeFormat("fr", {hour: "numeric", hourCycle: 'h24'}).format(new Date(2021, 2, 3, 23)), '23 h');
shouldBe(new Intl.DateTimeFormat("fr", {hour: "numeric", hourCycle: 'h23'}).format(new Date(2021, 2, 3, 23)), '23 h');
