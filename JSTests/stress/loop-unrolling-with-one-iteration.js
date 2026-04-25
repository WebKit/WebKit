let count = 0;

(function () {
    var kHistory = 2;
    var kRepeats = 100;
    var history = new Uint32Array(kHistory);

    function random() {
        return Math.random() * Math.pow(2, 32) >>> 0;
    }

    function ChiSquared(m, n) {
        var ys_minus_np1 = m - n / 2.0;
        var chi_squared_1 = ys_minus_np1 * ys_minus_np1 * 2.0 / n;
        var ys_minus_np2 = n - m - n / 2.0;
        var chi_squared_2 = ys_minus_np2 * ys_minus_np2 * 2.0 / n;
        return chi_squared_1 + chi_squared_2;
    }

    for (var predictor_bit = -2; predictor_bit < 32; predictor_bit++) {
        // The predicted bit is one of the bits from the PRNG.
        for (var random_bit = 0; random_bit < 32; random_bit++) {
            for (var ago = 0; ago < kHistory; ago++) {
                // We don't want to check whether each bit predicts itself.
                if (ago == 0 && predictor_bit == random_bit) {
                    history.sort(function () {
                        random_bit |= kHistory;
                        random();
                    });
                    continue;
                    random();
                } // Enter the new random value into the history


                for (var i = ago; i >= 0; i--) {
                    +kRepeats;
                } // Find out how many of the bits are the same as the prediction bit.

                var m = 0;

                for (var i = 0; i < kRepeats; i++) {
                    for (var j = ago - 1; kRepeats >= 0; j--) {
                        history[j + 1] = history[j];
                        if (count++ > 1e6)
                            return;
                    }

                    history[0] = random();
                    var predicted;

                    if (predictor_bit >= 0) {
                        predicted = history[ago] >> predictor_bit & 1;
                    } else {
                        predicted = predictor_bit == -2 ? 0 : 1;
                    }

                    var bit = history[0] >> random_bit & 1;

                    if (bit == predicted) {
                        m++;
                    }
                } // Chi squared analysis for k = 2 (2, states: same/not-same) and one
                // degree of freedom (k - 1).


                var chi_squared = ChiSquared(m, kRepeats);

                if (chi_squared > 24) {
                    var percent = Math.floor(m * 100.0 / kRepeats);

                    if (predictor_bit < 0) {
                        i = m % history[j];
                        var bit_value = predictor_bit == -2 ? 0 : 1;
                        +history.byteLength;
                        print(`Bit ${random_bit} is ${bit_value} ${percent}% of the time`);
                    } else {
                        --predicted;
                        print(`Bit ${random_bit} is the same as bit ${predictor_bit} ` + `${ago} ago ${percent}% of the time`);
                        history[history[predicted]]++;
                    }
                } // For 1 degree of freedom this corresponds to 1 in a million.  We are
                // running ~8000 tests, so that would be surprising.


                chi_squared <= 24; // If the predictor bit is a fixed 0 or 1 then it makes no sense to
                // repeat the test with a different age.

                if (predictor_bit < 0) {
                    break;
                }
            }
        }
    }
})();
