description("Verify that changing a date with a delta that is too large for a date produces an invalid date/NaN");

debug("Testing date creating at the max boundary");
shouldBe("new Date(0).valueOf()", "0");
shouldBe("new Date(8.64e15) instanceof Date", "true");
shouldBe("new Date(8.64e15).valueOf()", "8.64e15");
shouldBe("new Date(8640000000000001) instanceof Date", "true");
shouldBe("new Date(8640000000000001).valueOf()", "NaN");
shouldBe("new Date(Infinity) instanceof Date", "true");
shouldBe("new Date(Infinity).valueOf()", "NaN");
shouldBe("new Date(-Infinity) instanceof Date", "true");
shouldBe("new Date(-Infinity).valueOf()", "NaN");

debug("Testing setMilliseconds()");
shouldBe("new Date(0).setMilliseconds(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setMilliseconds(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setMilliseconds(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setMilliseconds(new Date(8.64e15).getMilliseconds()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setMilliseconds(new Date(8.64e15).getMilliseconds() + 1).valueOf()", "NaN");

debug("Testing setSeconds()");
shouldBe("new Date(0).setSeconds(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setSeconds(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setSeconds(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setSeconds(new Date(8.64e15).getSeconds()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setSeconds(new Date(8.64e15).getSeconds() + 1).valueOf()", "NaN");

debug("Testing setMinutes()");
shouldBe("new Date(0).setMinutes(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setMinutes(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setMinutes(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setMinutes(new Date(8.64e15).getMinutes()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setMinutes(new Date(8.64e15).getMinutes() + 1).valueOf()", "NaN");

debug("Testing setHours()");
shouldBe("new Date(0).setHours(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setHours(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setHours(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setHours(new Date(8.64e15).getHours()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setHours(new Date(8.64e15).getHours() + 1).valueOf()", "NaN");

debug("Testing setDate()");
shouldBe("new Date(0).setDate(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setDate(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setDate(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setDate(9e15 / (24 * 60 * 60 * 1000)).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setDate(new Date(8.64e15).getDate()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setDate(new Date(8.64e15).getDate() + 1).valueOf()", "NaN");

debug("Testing setMonth()");
shouldBe("new Date(0).setMonth(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setMonth(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setMonth(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setMonth(new Date(8.64e15).getMonth()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setMonth(new Date(8.64e15).getMonth() + 1).valueOf()", "NaN");

debug("Testing setYear()");
shouldBe("new Date(0).setYear(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setYear(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setYear(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setYear(new Date(8.64e15).getFullYear()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setYear(new Date(8.64e15).getFullYear() + 1).valueOf()", "NaN");

debug("Testing setFullYear()");
shouldBe("new Date(0).setFullYear(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setFullYear(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setFullYear(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setFullYear(new Date(8.64e15).getFullYear()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setFullYear(new Date(8.64e15).getFullYear() + 1).valueOf()", "NaN");

debug("Testing setUTCMilliseconds()");
shouldBe("new Date(0).setUTCMilliseconds(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setUTCMilliseconds(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setUTCMilliseconds(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setUTCMilliseconds(new Date(8.64e15).getUTCMilliseconds()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setUTCMilliseconds(new Date(8.64e15).getUTCMilliseconds() + 1).valueOf()", "NaN");

debug("Testing setUTCSeconds()");
shouldBe("new Date(0).setUTCSeconds(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setUTCSeconds(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setUTCSeconds(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setUTCSeconds(new Date(8.64e15).getUTCSeconds()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setUTCSeconds(new Date(8.64e15).getUTCSeconds() + 1).valueOf()", "NaN");

debug("Testing setUTCMinutes()");
shouldBe("new Date(0).setUTCMinutes(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setUTCMinutes(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setUTCMinutes(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setUTCMinutes(new Date(8.64e15).getUTCMinutes()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setUTCMinutes(new Date(8.64e15).getUTCMinutes() + 1).valueOf()", "NaN");

debug("Testing setUTCHours()");
shouldBe("new Date(0).setUTCHours(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setUTCHours(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setUTCHours(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setUTCHours(new Date(8.64e15).getUTCHours()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setUTCHours(new Date(8.64e15).getUTCHours() + 1).valueOf()", "NaN");

debug("Testing setUTCDate()");
shouldBe("new Date(0).setUTCDate(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setUTCDate(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setUTCDate(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setUTCDate(9e15 / (24 * 60 * 60 * 1000)).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setUTCDate(new Date(8.64e15).getUTCDate()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setUTCDate(new Date(8.64e15).getUTCDate() + 1).valueOf()", "NaN");

debug("Testing setUTCMonth()");
shouldBe("new Date(0).setUTCMonth(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setUTCMonth(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setUTCMonth(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setUTCMonth(new Date(8.64e15).getUTCMonth()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setUTCMonth(new Date(8.64e15).getUTCMonth() + 1).valueOf()", "NaN");

debug("Testing setUTCFullYear()");
shouldBe("new Date(0).setUTCFullYear(Infinity).valueOf()", "NaN");
shouldBe("new Date(0).setUTCFullYear(1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(0).setUTCFullYear(-1.79769e+308).valueOf()", "NaN");
shouldBe("new Date(8.64e15).setUTCFullYear(new Date(8.64e15).getUTCFullYear()).valueOf()", "8.64e15");
shouldBe("new Date(8.64e15).setUTCFullYear(new Date(8.64e15).getUTCFullYear() + 1).valueOf()", "NaN");
