//@ skip if $architecture == "arm"
function shouldBe(actual, expected) {
    if (!Object.is(actual, expected)) {
        throw new Error(`expected: ${expected}, actual: ${actual}`);
    }
}

for (var i = 0; i < testLoopCount; i++) {
    // basic
    shouldBe(Math.hypot(3, 4), 5);
    shouldBe(Math.hypot(-3, -4), 5);
    shouldBe(Math.hypot(5, 12), 13);
    shouldBe(Math.hypot(5, -12), 13);
    shouldBe(Math.hypot(1, 2, 2), 3);
    shouldBe(Math.hypot(1e308), 1e308);
    shouldBe(Math.hypot(-1e308), 1e308);
    shouldBe(Math.hypot(1e308, -1e308), 1.4142135623730951e308);
    shouldBe(Math.hypot(1e308, 1e308, 1e308), 1.7320508075688772e308);
    shouldBe(Math.hypot(-1e308, -1e308, -1e308), 1.7320508075688772e308);
    shouldBe(Math.hypot(1e308, 1e308, 1e308, 1e308), Infinity);
    shouldBe(Math.hypot(-1e308, -1e308, -1e308, -1e308), Infinity);
    shouldBe(Math.hypot(1e308, 1e308, 1e308, 1e308, -1e308), Infinity);
    shouldBe(Math.hypot(0.1), 0.1);
    shouldBe(Math.hypot(0.1, 0.1), 0.14142135623730953);
    shouldBe(Math.hypot(0.1, -0.1), 0.14142135623730953);
    shouldBe(Math.hypot(1e308, 1e308, 0.1, 0.1, 1e30, 0.1, -1e30, -1e308, -1e308), Infinity);

    // big and small numbers
    shouldBe(Math.hypot(...Array(100).fill(1)), 10);
    shouldBe(Math.hypot(...Array(10000).fill(1)), 100);
    shouldBe(Math.hypot(8.98846567431158e307, 8.988465674311579e307, -1.7976931348623157e308), Infinity);
    shouldBe(Math.hypot(-5.630637621603525e255, 9.565271205476345e307, 2.9937604643020797e292), 9.565271205476345e307);
    shouldBe(
        Math.hypot(
            6.739986666787661e66,
            2,
            -1.2689709186578243e-116,
            1.7046015739467354e308,
            -9.979201547673601e291,
            6.160926733208294e307,
            -3.179557053031852e234,
            -7.027282978772846e307,
            -0.7500000000000001
        ),
        Infinity
    );
    shouldBe(
        Math.hypot(
            0.31150493246968836,
            -8.988465674311582e307,
            1.8315037361673755e-270,
            -15.999999999999996,
            2.9999999999999996,
            7.345200721499384e164,
            -2.033582473639399,
            -8.98846567431158e307,
            -3.5737295155405993e292,
            4.13894772383715e-124,
            -3.6111186457260667e-35,
            2.387234887098013e180,
            7.645295562778372e-298,
            3.395189016861822e-103,
            -2.6331611115768973e-149
        ),
        1.2711610061536464e308
    );
    shouldBe(
        Math.hypot(
            -1.1442589134409902e308,
            9.593842098384855e138,
            4.494232837155791e307,
            -1.3482698511467367e308,
            4.494232837155792e307
        ),
        Infinity
    );
    shouldBe(
        Math.hypot(-1.1442589134409902e308, 4.494232837155791e307, -1.3482698511467367e308, 4.494232837155792e307),
        Infinity
    );
    shouldBe(
        Math.hypot(9.593842098384855e138, -6.948356297254111e307, -1.3482698511467367e308, 4.494232837155792e307),
        1.5819637896591838e308
    );
    shouldBe(Math.hypot(-2.534858246857893e115, 8.988465674311579e307, 8.98846567431158e307), 1.2711610061536462e308);
    // FIXME: This test result differs from Chrome and Firefox,
    // although it matches the behavior of Swift, C++, Rust, and Python. Commenting out this test case for now.
    // shouldBe(Math.hypot(1.3588124894186193e308, 1.4803986201152006e223, 6.741349255733684e307), 1.5168484694516577e308);
    shouldBe(Math.hypot(6.741349255733684e307, 1.7976931348623155e308, -7.388327292663961e41), Infinity);
    shouldBe(Math.hypot(-1.9807040628566093e28, 1.7976931348623157e308, 9.9792015476736e291), 1.7976931348623157e308);
    shouldBe(
        Math.hypot(-1.0214557991173964e61, 1.7976931348623157e308, 8.98846567431158e307, -8.988465674311579e307),
        Infinity
    );
    shouldBe(
        Math.hypot(
            1.7976931348623157e308,
            7.999999999999999,
            -1.908963895403937e-230,
            1.6445950082320264e292,
            2.0734856707605806e205
        ),
        1.7976931348623157e308
    );
    shouldBe(
        Math.hypot(6.197409167220438e-223, -9.979201547673601e291, -1.7976931348623157e308),
        1.7976931348623157e308
    );
    shouldBe(
        Math.hypot(
            4.49423283715579e307,
            8.944251746776101e307,
            -0.0002441406250000001,
            1.1752060710043817e308,
            4.940846717201632e292,
            -1.6836699406454528e308
        ),
        Infinity
    );
    shouldBe(
        Math.hypot(
            8.988465674311579e307,
            7.999999999999998,
            7.029158107234023e-308,
            -2.2303483759420562e-172,
            -1.7976931348623157e308,
            -8.98846567431158e307
        ),
        Infinity
    );
    shouldBe(Math.hypot(8.98846567431158e307, 8.98846567431158e307), 1.2711610061536464e308);

    // nan infinity
    shouldBe(Math.hypot(NaN), NaN);
    shouldBe(Math.hypot(NaN, 0), NaN);
    shouldBe(Math.hypot(NaN, 3), NaN);
    shouldBe(Math.hypot(NaN, 0, 0, 0), NaN);
    shouldBe(Math.hypot(NaN, 0, 0, 0, 0), NaN);
    shouldBe(Math.hypot(NaN, 0, 0, 0, 0, Infinity), Infinity);
    shouldBe(Math.hypot(0, 0, 0, 0, 0, Infinity), Infinity);
    shouldBe(Math.hypot(0, NaN, 0, 1e-308), NaN);
    shouldBe(Math.hypot(0, NaN, 0, Infinity), Infinity);
    shouldBe(Math.hypot(0, 0, 1, Infinity), Infinity);
    shouldBe(Math.hypot(Infinity, -Infinity), Infinity);
    shouldBe(Math.hypot(-Infinity, Infinity), Infinity);
    shouldBe(Math.hypot(Infinity, NaN), Infinity);
    shouldBe(Math.hypot(Infinity, Infinity, Infinity, NaN), Infinity);
    shouldBe(Math.hypot(Infinity, NaN, NaN, NaN), Infinity);
    shouldBe(Math.hypot(-Infinity, NaN), Infinity);
    shouldBe(Math.hypot(Infinity), Infinity);
    shouldBe(Math.hypot(Infinity, 1), Infinity);
    shouldBe(Math.hypot(Infinity, Infinity), Infinity);
    shouldBe(Math.hypot(-Infinity), Infinity);
    shouldBe(Math.hypot(-Infinity, -Infinity), Infinity);

    // string
    shouldBe(Math.hypot(""), 0);
    shouldBe(Math.hypot(" "), 0);
    shouldBe(Math.hypot("  "), 0);
    shouldBe(Math.hypot("\t"), 0);
    shouldBe(Math.hypot("\n"), 0);
    shouldBe(Math.hypot("w"), NaN);
    shouldBe(Math.hypot(13, "a"), NaN);
    shouldBe(Math.hypot(1, "1"), 1.4142135623730951);
    shouldBe(Math.hypot("3", "4"), 5);
    shouldBe(Math.hypot("3", "五"), NaN);
    shouldBe(Math.hypot(1, "🐟"), NaN);

    // object
    shouldBe(Math.hypot([]), 0);
    shouldBe(Math.hypot([1, 2, 3]), NaN);
    shouldBe(Math.hypot({}), NaN);
    shouldBe(Math.hypot({ valueOf: 1 }), NaN);

    // bool
    shouldBe(Math.hypot(true), 1);
    shouldBe(Math.hypot(false), 0);
    shouldBe(Math.hypot(false, true), 1);
    shouldBe(Math.hypot(false, true, 1, 1, 1), 2);
    shouldBe(Math.hypot(...Array(100).fill(true)), 10);

    // zero
    shouldBe(Math.hypot(), 0);
    shouldBe(Math.hypot(-0), 0);
    shouldBe(Math.hypot(-0, -0), 0);
    shouldBe(Math.hypot(-0, 0), 0);
    shouldBe(Math.hypot(0), 0);
    shouldBe(Math.hypot(0, 0), 0);

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

    shouldBe(Math.hypot(generateObj(3), 4), 5);
    shouldBe(mutable, 10);

    shouldBe(Math.hypot(generateObj(3), Infinity), Infinity);
    shouldBe(mutable, 20);

    shouldBe(Math.hypot(generateObj(3), generateObj(NaN), Infinity), Infinity);
    shouldBe(mutable, 40);

    shouldBe(Math.hypot(generateObj(3), generateObj(NaN)), NaN);
    shouldBe(mutable, 60);

    shouldBe(Math.hypot(generateObj(3), generateObj([])), NaN);
    shouldBe(mutable, 80);

    shouldBe(Math.hypot(generateObj(NaN), generateObj(3)), NaN);
    shouldBe(mutable, 100);

    shouldBe(Math.hypot(Infinity, generateObj(3)), Infinity);
    shouldBe(mutable, 110);

    shouldBe(Math.hypot(-Infinity, generateObj(3)), Infinity);
    shouldBe(mutable, 120);

    shouldBe(Math.hypot(Infinity, Infinity, Infinity, generateObj(3)), Infinity);
    shouldBe(mutable, 130);
    
    shouldBe(Math.hypot(NaN, Infinity, Infinity, Infinity, generateObj(3)), Infinity);
    shouldBe(mutable, 140);

    shouldBe(Math.hypot(NaN, generateObj(Infinity), Infinity, Infinity, generateObj(3)), Infinity);
    shouldBe(mutable, 160);
}
