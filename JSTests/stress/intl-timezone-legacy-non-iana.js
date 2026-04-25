function shouldThrowRangeError(func) {
    try {
        func();
    } catch (err) {
        if (err instanceof RangeError) {
            return;
        }
    }
    throw new Error("should throw RangeError");
}

// https://github.com/unicode-org/icu/blob/main/icu4c/source/tools/tzcode/icuzones
const invalidTimeZones = [
    "ACT",
    "AET",
    "AGT",
    "ART",
    "AST",
    "BET",
    "BST",
    "CAT",
    "CNT",
    "CST",
    "CTT",
    "EAT",
    "ECT",
    "IET",
    "IST",
    "JST",
    "MIT",
    "NET",
    "NST",
    "PLT",
    "PNT",
    "PRT",
    "PST",
    "SST",
    "VST",
];

for (const timeZone of invalidTimeZones) {
    shouldThrowRangeError(() => {
        new Intl.DateTimeFormat(undefined, { timeZone });
    });
}
