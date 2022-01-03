// Copyright 2021 the V8 project authors. All rights reserved.
// Copyright 2021 Apple Inc. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + " " + expected);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name} but got ${String(error)}!`);
}

function shouldNotThrow(func) {
    func();
}

const validRanges = [[-12345, -5678], [-12345, 56789], [12345, 56789]];

const nf = new Intl.NumberFormat("en", {signDisplay: "exceptZero"});
if (nf.formatRange || nf.formatRangeToParts) {
    let methods = [];
    if (nf.formatRange)
        methods.push("formatRange");
    if (nf.formatRangeToParts)
        methods.push("formatRangeToParts");
    methods.forEach(function(method) {
        shouldBe("function", typeof nf[method]);

        // 2. Perform ? RequireInternalSlot(nf, [[InitializedNumberFormat]]).
        // Assert if called without nf
        let f = nf[method];
        shouldThrow(() => { f(1, 23) }, TypeError);
        shouldBe(f.length, 2);

        // Assert normal call success
        shouldNotThrow(() => nf[method](1, 23));

        // 3. If start is undefined ..., throw a TypeError exception.
        shouldThrow(() => { nf[method](undefined, 23) }, TypeError);
        // 3. If ... end is undefined, throw a TypeError exception.
        shouldThrow(() => { nf[method](1, undefined) }, TypeError);

        // 4. Let x be ? ToNumeric(start).
        // Verify it won't throw error
        shouldNotThrow(() => nf[method](null, 23));
        shouldNotThrow(() => nf[method](false, 23));
        shouldNotThrow(() => nf[method](true, 23));
        shouldNotThrow(() => nf[method]("12", 23));
        shouldNotThrow(() => nf[method]("-123456789012345678901234567890", 23));
        shouldThrow(() => { nf[method](Symbol(12), 23) }, TypeError);
        shouldNotThrow(() => nf[method](12, 23));
        shouldNotThrow(() => nf[method](12n, 23));
        shouldThrow(() => { nf[method]({}, -23) }, RangeError);
        shouldNotThrow(() => { nf[method]([], 23) });

        // 5. Let y be ? ToNumeric(end).
        // Verify it won't throw error
        shouldNotThrow(() => nf[method](-12, null));
        shouldNotThrow(() => nf[method](-12, false));
        shouldNotThrow(() => nf[method](-12, true));
        shouldNotThrow(() => nf[method](12, "23"));
        shouldNotThrow(() => nf[method](12, "23456789012345678901234567890"));
        shouldThrow(() => { nf[method](12, Symbol(23)) }, TypeError);
        shouldNotThrow(() => nf[method](12, 23));
        shouldNotThrow(() => nf[method](12, 23n));
        shouldThrow(() => { nf[method](-12, {}) }, RangeError);
        shouldNotThrow(() => { nf[method](-12, []) });

        // 6. If x is NaN ..., throw a RangeError exception.
        shouldThrow(() => { nf[method](NaN, 23) }, RangeError);
        shouldThrow(() => { nf[method]("NaN", 23) }, RangeError);
        // 6. If ... y is NaN, throw a RangeError exception.
        shouldThrow(() => { nf[method](12, NaN) }, RangeError);
        shouldThrow(() => { nf[method](12, "NaN") }, RangeError);
        shouldThrow(() => { nf[method](12, "xyz") }, RangeError);

        // 7. If x is a non-finite Number ..., throw a RangeError exception.
        shouldNotThrow(() => { nf[method](-12/0, 12/0) });
        shouldThrow(() => { nf[method](12/0, -12/0) }, RangeError);

        // 8. If x is greater than y, throw a RangeError exception.
        // neither x nor y are bigint.
        shouldThrow(() => { nf[method](23, 12) }, RangeError);
        shouldNotThrow(() => nf[method](12, 23));
        // x is not bigint but y is.
        shouldThrow(() => { nf[method](23, 12n) }, RangeError);
        shouldNotThrow(() => nf[method](12, 23n));
        // x is bigint but y is not.
        shouldThrow(() => { nf[method](23n, 12) }, RangeError);
        shouldNotThrow(() => nf[method](12n, 23));
        // both x and y are bigint.
        shouldThrow(() => { nf[method](23n, 12n) }, RangeError);
        shouldNotThrow(() => nf[method](12n, 23n));

        validRanges.forEach(
            function([x, y]) {
                const X = BigInt(x);
                const Y = BigInt(y);
                const formatted_x_y = nf[method](x, y);
                const formatted_X_y = nf[method](X, y);
                const formatted_x_Y = nf[method](x, Y);
                const formatted_X_Y = nf[method](X, Y);
                shouldBe(JSON.stringify(formatted_x_y), JSON.stringify(formatted_X_y));
                shouldBe(JSON.stringify(formatted_x_y), JSON.stringify(formatted_x_Y));
                shouldBe(JSON.stringify(formatted_x_y), JSON.stringify(formatted_X_Y));
            });
    });
}

// Check the number of part with type: "plusSign" and "minusSign" are corre
if (nf.formatRangeToParts) {
    validRanges.forEach(
        function([x, y]) {
            const expectedPlus = (x > 0) ? ((y > 0) ? 2 : 1) : ((y > 0) ? 1 : 0);
            const expectedMinus = (x < 0) ? ((y < 0) ? 2 : 1) : ((y < 0) ? 1 : 0);
            let actualPlus = 0;
            let actualMinus = 0;
            const parts = nf.formatRangeToParts(x, y);
            parts.forEach(function(part) {
                if (part.type == "plusSign") actualPlus++;
                if (part.type == "minusSign") actualMinus++;
            });
            const method = "formatRangeToParts(" + x + ", " + y + "): ";
            shouldBe(expectedPlus, actualPlus,
                method + "Number of type: 'plusSign' in parts is incorrect");
            shouldBe(expectedMinus, actualMinus,
                method + "Number of type: 'minusSign' in parts is incorrect");
        });
}

// From https://github.com/tc39/proposal-intl-numberformat-v3#formatrange-ecma-402-393
const nf2 = new Intl.NumberFormat("en-US", {
    style: "currency",
    currency: "EUR",
    maximumFractionDigits: 0,
});

// README.md said it expect "€3–5"
if (nf2.formatRange)
    shouldBe("€3 – €5", nf2.formatRange(3, 5));

const nf3 = new Intl.NumberFormat("en-US", {
    style: "currency",
    currency: "EUR",
    maximumFractionDigits: 0,
});
if (nf3.formatRangeToParts) {
    const actual3 = nf3.formatRangeToParts(3, 5);
    /*
    [
      {type: "currency", value: "€", source: "startRange"}
      {type: "integer", value: "3", source: "startRange"}
      {type: "literal", value: "–", source: "shared"}
      {type: "integer", value: "5", source: "endRange"}
    ]
    */
    shouldBe(5, actual3.length);
    shouldBe("currency", actual3[0].type);
    shouldBe("€", actual3[0].value);
    shouldBe("startRange", actual3[0].source);
    shouldBe("integer", actual3[1].type);
    shouldBe("3", actual3[1].value);
    shouldBe("startRange", actual3[1].source);
    shouldBe("literal", actual3[2].type);
    shouldBe(" – ", actual3[2].value);
    shouldBe("shared", actual3[2].source);
    shouldBe("currency", actual3[3].type);
    shouldBe("€", actual3[3].value);
    shouldBe("endRange", actual3[3].source);
    shouldBe("integer", actual3[4].type);
    shouldBe("5", actual3[4].value);
    shouldBe("endRange", actual3[4].source);
}

const nf4 = new Intl.NumberFormat("en-US", {
    style: "currency",
    currency: "EUR",
    maximumFractionDigits: 0,
});
if (nf4.formatRange)
    shouldBe("~€3", nf4.formatRange(2.9, 3.1));

const nf5 = new Intl.NumberFormat("en-US", {
    style: "currency",
    currency: "EUR",
    signDisplay: "always",
});
if (nf5.formatRange)
    shouldBe("~+€3.00", nf5.formatRange(2.999, 3.001));

const nf6 = new Intl.NumberFormat("en");
if (nf6.formatRange) {
    shouldBe("3–∞", nf6.formatRange(3, 1/0));
    shouldThrow(() => { nf6.formatRange(3, 0/0); }, RangeError);
}
