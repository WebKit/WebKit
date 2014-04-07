description(
"This test checks date values at the limits set by the ES5 15.9.1.14 TimeClip specification and ensures that we don't crash on any assertions."
);

function testDateFromSetDateAdjustement(initialMs, adjustMs) {
    var date = new Date(initialMs);
    debug("(new Date(" + initialMs + ")).setDate(" + adjustMs + ") ==> " + date.setDate(adjustMs) + " ms, " + date.toDateString());
}

testDateFromSetDateAdjustement(1396547803766, 1396549003769);
testDateFromSetDateAdjustement(1396547803766, 100000000);
testDateFromSetDateAdjustement(1396547803766, 99983839+1);
testDateFromSetDateAdjustement(1396547803766, 99983839);
testDateFromSetDateAdjustement(1396547803766, 10000000);
debug("");

function testDateFromSetTimeWithMilliseconds(ms) {
    var date = new Date(0);
    debug("(new Date(0)).setTime(" + ms + ") ==> " + date.setTime(ms) + " ms, " + date.toUTCString() + " " + date.getUTCMilliseconds() + " ms");
}

testDateFromSetTimeWithMilliseconds(8640000000000001);
testDateFromSetTimeWithMilliseconds(8640000000000000);
testDateFromSetTimeWithMilliseconds(-8640000000000000);
testDateFromSetTimeWithMilliseconds(-8640000000000001);
debug("");

function testDateFromString(str) {
    var date = new Date(str);
    debug("(new Date(" + str + ") ==> " + date.getTime() + " ms, " + date.toUTCString() + " " + date.getUTCMilliseconds() + " ms");
}

testDateFromString("13 Sep 275760 00:00:00 -0001");
testDateFromString("13 Sep 275760 00:00:00 +0000");
testDateFromString("13 Sep 275760 00:00:00 +0001");
testDateFromString("20 Apr -271821 00:00:00 -0001");
testDateFromString("20 Apr -271821 00:00:00 +0000");
testDateFromString("20 Apr -271821 00:00:00 +0001");
debug("");

testDateFromString("19 Apr -271821 23:59:59");
debug("");

testDateFromString("275760-09-13T00:00:00.001");
testDateFromString("275760-09-13T00:00:00.000");
testDateFromString("-271821-04-20T00:00:00.0000");
testDateFromString("-271821-04-19T23:59:59.999");
debug("");

testDateFromString("Sat, 13 Sep 275760 00:00:00 UTC-2");
testDateFromString("Sat, 13 Sep 275760 00:00:00 UTC");
testDateFromString("Sat, 13 Sep 275760 00:00:00 UTC+2");
testDateFromString("Tue, 20 Apr -271821 00:00:00 UTC-2");
testDateFromString("Tue, 20 Apr -271821 00:00:00 UTC");
testDateFromString("Tue, 20 Apr -271821 00:00:00 UTC+2");
debug("");

