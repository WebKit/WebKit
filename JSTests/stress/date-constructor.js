function shouldBe(actual, expected) {
    if (!Object.is(actual, expected)) {
        throw new Error(`Expected: ${expected}, actual value: ${actual}`);
    }
}

for (var i = 0; i < testLoopCount; i++) {
    // Date constructor
    shouldBe(new Date(1, 1, 1, 1, 1, 1, 1).valueOf(), -2174741938999);
    shouldBe(new Date(NaN, 1, 1, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1, NaN, 1, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1, 1, NaN, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1, 1, 1, NaN, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1, 1, 1, 1, NaN, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1, 1, 1, 1, 1, NaN, 1).valueOf(), NaN);
    shouldBe(new Date(1, 1, 1, 1, 1, 1, NaN).valueOf(), NaN);
    shouldBe(new Date(1, 1, 1, NaN).valueOf(), NaN);
    shouldBe(new Date(1, 1, 1, NaN, 10).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, 1, 1, 1).valueOf(), 633862861001);
    shouldBe(new Date(NaN, 1, 1, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, NaN, 1, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, NaN, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, NaN, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, NaN, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, 1, NaN, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, 1, 1, NaN).valueOf(), NaN);
    shouldBe(new Date(Infinity, 1, 1, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, Infinity, 1, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, Infinity, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, Infinity, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, Infinity, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, 1, Infinity, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, 1, 1, Infinity).valueOf(), NaN);
    shouldBe(new Date(-Infinity, 1, 1, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, -Infinity, 1, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, -Infinity, 1, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, -Infinity, 1, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, -Infinity, 1, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, 1, -Infinity, 1).valueOf(), NaN);
    shouldBe(new Date(1990, 1, 1, 1, 1, 1, -Infinity).valueOf(), NaN);
    shouldBe(new Date(NaN).valueOf(), NaN);
    shouldBe(new Date(Infinity).valueOf(), NaN);
    shouldBe(new Date(-Infinity).valueOf(), NaN);
    shouldBe(new Date(0).valueOf(), 0);
    shouldBe(new Date(-0).valueOf(), 0);
    shouldBe(new Date(-10).valueOf(), -10);

    shouldBe(new Date("1990", 1, 1, 3, 10, 10).valueOf(), 633870610000);
    shouldBe(new Date("1990", 1, "1", 3, NaN, 10).valueOf(), NaN);
    shouldBe(new Date("1990", 1, "1", 3, Infinity, 10).valueOf(), NaN);
    shouldBe(new Date("1990", 1, "1", 3, "Infinity", 10).valueOf(), NaN);
    shouldBe(new Date("1990", 1, "1", 3, "NaN", 10).valueOf(), NaN);
    shouldBe(new Date("1990", 1, "1", 3, "random", 10).valueOf(), NaN);
    shouldBe(new Date("1990", 1, "1", 3, "", 10).valueOf(), 633870010000);
    shouldBe(new Date(1990, 1, "1", 3, "9", 10).valueOf(), 633870550000);
    shouldBe(new Date("1990", "1", "1", "3", "9", "10").valueOf(), 633870550000);

    // Date.UTC
    shouldBe(Date.UTC(1, 1, 1, 1, 10).valueOf(), -2174770200000);
    shouldBe(Date.UTC(NaN, 1, 1, 1, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1, NaN, 1, 1, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1, 1, NaN, 1, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1, 1, 1, NaN, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1, 1, 1, 1, NaN, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1, 1, 1, 1, 10, NaN).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, 10, 10, 10).valueOf(), 633841810010);
    shouldBe(Date.UTC(NaN, 1, 1, 3, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, NaN, 1, 3, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, NaN, 3, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, NaN, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, NaN, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, 10, NaN, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, 10, 10, NaN).valueOf(), NaN);
    shouldBe(Date.UTC(Infinity, 1, 1, 3, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, Infinity, 1, 3, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, Infinity, 3, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, Infinity, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, Infinity, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, 10, Infinity, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, 10, 10, Infinity).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, 10, 10, Infinity).valueOf(), NaN);
    shouldBe(Date.UTC(-Infinity, 1, 1, 3, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, -Infinity, 1, 3, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, -Infinity, 3, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, -Infinity, 10, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, -Infinity, 10, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, 10, -Infinity, 10).valueOf(), NaN);
    shouldBe(Date.UTC(1990, 1, 1, 3, 10, 10, -Infinity).valueOf(), NaN);
    shouldBe(Date.UTC(NaN).valueOf(), NaN);
    shouldBe(Date.UTC(Infinity).valueOf(), NaN);
    shouldBe(Date.UTC(-Infinity).valueOf(), NaN);
    shouldBe(Date.UTC(0).valueOf(), -2208988800000);
    shouldBe(Date.UTC(-0).valueOf(), -2208988800000);
    shouldBe(Date.UTC(-10).valueOf(), -62482752000000);

    shouldBe(Date.UTC("1990", 1, 1, 3, 10, 10).valueOf(), 633841810000);
    shouldBe(Date.UTC("1990", 1, "1", 3, NaN, 10).valueOf(), NaN);
    shouldBe(Date.UTC("1990", 1, "1", 3, Infinity, 10).valueOf(), NaN);
    shouldBe(Date.UTC("1990", 1, "1", 3, "Infinity", 10).valueOf(), NaN);
    shouldBe(Date.UTC("1990", 1, "1", 3, "NaN", 10).valueOf(), NaN);
    shouldBe(Date.UTC("1990", 1, "1", 3, "random", 10).valueOf(), NaN);
    shouldBe(Date.UTC("1990", 1, "1", 3, "", 10).valueOf(), 633841210000);
    shouldBe(Date.UTC(1990, 1, "1", 3, "9", 10).valueOf(), 633841750000);
    shouldBe(Date.UTC("1990", "1", "1", "3", "9", "10").valueOf(), 633841750000);

    // valueOf
    var mutable = 0;
    function generateObj(value) {
        return {
            valueOf: () => {
                mutable += 10;
                return value;
            },
        };
    }

    shouldBe(Date.UTC(generateObj(1)).valueOf(), -2177452800000);
    shouldBe(mutable, 10);

    shouldBe(Date.UTC(generateObj(1990)).valueOf(), 631152000000);
    shouldBe(mutable, 20);

    shouldBe(Date.UTC(generateObj(NaN)).valueOf(), NaN);
    shouldBe(mutable, 30);

    shouldBe(Date.UTC(generateObj(Infinity)).valueOf(), NaN);
    shouldBe(mutable, 40);

    shouldBe(Date.UTC(generateObj(-Infinity)).valueOf(), NaN);
    shouldBe(mutable, 50);

    shouldBe(Date.UTC(1990, NaN, generateObj(1)).valueOf(), NaN);
    shouldBe(mutable, 60);

    shouldBe(Date.UTC(1990, Infinity, generateObj(1)).valueOf(), NaN);
    shouldBe(mutable, 70);

    shouldBe(Date.UTC(1990, -Infinity, generateObj(1)).valueOf(), NaN);
    shouldBe(mutable, 80);

    shouldBe(new Date(generateObj(1)).valueOf(), 1);
    shouldBe(mutable, 90);

    shouldBe(new Date(generateObj(1990)).valueOf(), 1990);
    shouldBe(mutable, 100);

    shouldBe(new Date(generateObj(NaN)).valueOf(), NaN);
    shouldBe(mutable, 110);

    shouldBe(new Date(1990, NaN, generateObj(1)).valueOf(), NaN);
    shouldBe(mutable, 120);

    shouldBe(new Date(1990, Infinity, generateObj(1)).valueOf(), NaN);
    shouldBe(mutable, 130);

    shouldBe(new Date(1990, -Infinity, generateObj(1)).valueOf(), NaN);
    shouldBe(mutable, 140);

    shouldBe(new Date(generateObj(1990), generateObj(1), generateObj(1)).valueOf(), 633859200000);
    shouldBe(mutable, 170);

    shouldBe(new Date(generateObj(1990), generateObj(NaN), generateObj(1)).valueOf(), NaN);
    shouldBe(mutable, 200);

    shouldBe(new Date(generateObj(NaN), generateObj(1), generateObj(1)).valueOf(), NaN);
    shouldBe(mutable, 230);

    shouldBe(new Date(generateObj(NaN), 1, 1).valueOf(), NaN);
    shouldBe(mutable, 240);
}
