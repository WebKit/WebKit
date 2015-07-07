// Attempts to induce a crash resulting from the FTL emitting code that clobbers the tag registers and then
// throwing an exception without restoring those tag registers' values.

function ftlFunction(array, callee) {
    // Gotta use lots of gprs.
    var x0 = array[0];
    var x1 = array[1];
    var x2 = array[2];
    var x3 = array[3];
    var x4 = array[4];
    var x5 = array[5];
    var x6 = array[6];
    var x7 = array[7];
    var x8 = array[8];
    var x9 = array[9];
    var x10 = array[10];
    var x11 = array[11];
    var x12 = array[12];
    var x13 = array[13];
    var x14 = array[14];
    var x15 = array[15];
    var x16 = array[16];
    var x17 = array[17];
    var x18 = array[18];
    var x19 = array[19];
    var x20 = array[20];
    var x21 = array[21];
    var x22 = array[22];
    var x23 = array[23];
    var x24 = array[24];
    var x25 = array[25];
    var x26 = array[26];
    var x27 = array[27];
    var x28 = array[28];
    var x29 = array[29];
    var x30 = array[30];
    var x31 = array[31];
    var x32 = array[32];
    var x33 = array[33];
    var x34 = array[34];
    var x35 = array[35];
    var x36 = array[36];
    var x37 = array[37];
    var x38 = array[38];
    var x39 = array[39];
    var x40 = array[40];
    var x41 = array[41];
    var x42 = array[42];
    var x43 = array[43];
    var x44 = array[44];
    var x45 = array[45];
    var x46 = array[46];
    var x47 = array[47];
    var x48 = array[48];
    var x49 = array[49];
    var x50 = array[50];
    var x51 = array[51];
    var x52 = array[52];
    var x53 = array[53];
    var x54 = array[54];
    var x55 = array[55];
    var x56 = array[56];
    var x57 = array[57];
    var x58 = array[58];
    var x59 = array[59];
    var x60 = array[60];
    var x61 = array[61];
    var x62 = array[62];
    var x63 = array[63];
    
    // Make a call that will throw, when we ask it to.
    callee("hello");
    
    // Use all of those crazy values.
    return [x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47, x48, x49, x50, x51, x52, x53, x54, x55, x56, x57, x58, x59, x60, x61, x62, x63]
}

noInline(ftlFunction);

// Create some callees that are too crazy to get inlined or devirtualized, but that don't have effects.

function happyCallee0() { return 0 };
function happyCallee1() { return 1 };
function happyCallee2() { return 2 };
function happyCallee3() { return 3 };
function happyCallee4() { return 4 };
function happyCallee5() { return 5 };
function happyCallee6() { return 6 };
function happyCallee7() { return 7 };
function happyCallee8() { return 8 };
function happyCallee9() { return 9 };
function happyCallee10() { return 10 };
function happyCallee11() { return 11 };
function happyCallee12() { return 12 };
function happyCallee13() { return 13 };
function happyCallee14() { return 14 };
function happyCallee15() { return 15 };
function happyCallee16() { return 16 };
function happyCallee17() { return 17 };
function happyCallee18() { return 18 };
function happyCallee19() { return 19 };
function happyCallee20() { return 20 };
function happyCallee21() { return 21 };
function happyCallee22() { return 22 };
function happyCallee23() { return 23 };
function happyCallee24() { return 24 };
function happyCallee25() { return 25 };
function happyCallee26() { return 26 };
function happyCallee27() { return 27 };
function happyCallee28() { return 28 };
function happyCallee29() { return 29 };
function happyCallee30() { return 30 };
function happyCallee31() { return 31 };
function happyCallee32() { return 32 };
function happyCallee33() { return 33 };
function happyCallee34() { return 34 };
function happyCallee35() { return 35 };
function happyCallee36() { return 36 };
function happyCallee37() { return 37 };
function happyCallee38() { return 38 };
function happyCallee39() { return 39 };
function happyCallee40() { return 40 };
function happyCallee41() { return 41 };
function happyCallee42() { return 42 };
function happyCallee43() { return 43 };
function happyCallee44() { return 44 };
function happyCallee45() { return 45 };
function happyCallee46() { return 46 };
function happyCallee47() { return 47 };
function happyCallee48() { return 48 };
function happyCallee49() { return 49 };
function happyCallee50() { return 50 };
function happyCallee51() { return 51 };
function happyCallee52() { return 52 };
function happyCallee53() { return 53 };
function happyCallee54() { return 54 };
function happyCallee55() { return 55 };
function happyCallee56() { return 56 };
function happyCallee57() { return 57 };
function happyCallee58() { return 58 };
function happyCallee59() { return 59 };
function happyCallee60() { return 60 };
function happyCallee61() { return 61 };
function happyCallee62() { return 62 };
function happyCallee63() { return 63 };

var happyCallees = [happyCallee0, happyCallee1, happyCallee2, happyCallee3, happyCallee4, happyCallee5, happyCallee6, happyCallee7, happyCallee8, happyCallee9, happyCallee10, happyCallee11, happyCallee12, happyCallee13, happyCallee14, happyCallee15, happyCallee16, happyCallee17, happyCallee18, happyCallee19, happyCallee20, happyCallee21, happyCallee22, happyCallee23, happyCallee24, happyCallee25, happyCallee26, happyCallee27, happyCallee28, happyCallee29, happyCallee30, happyCallee31, happyCallee32, happyCallee33, happyCallee34, happyCallee35, happyCallee36, happyCallee37, happyCallee38, happyCallee39, happyCallee40, happyCallee41, happyCallee42, happyCallee43, happyCallee44, happyCallee45, happyCallee46, happyCallee47, happyCallee48, happyCallee49, happyCallee50, happyCallee51, happyCallee52, happyCallee53, happyCallee54, happyCallee55, happyCallee56, happyCallee57, happyCallee58, happyCallee59, happyCallee60, happyCallee61, happyCallee62, happyCallee63];

for (var i = 0; i < happyCallees.length; ++i)
    noInline(happyCallees[i]);

// Unlike the other test (throw-from-ftl-call-ic-slow-path.js), we want to populate the registers with cells in
// this test.
var array = new Array();
for (var i = 0; i < 64; ++i)
    array[i] = new Object();

// Now, do some warming up.
for (var i = 0; i < 100000; ++i) {
    var result = ftlFunction(array, happyCallees[i % happyCallees.length]);
    if (result.length != array.length)
        throw "Error: bad length: " + result;
    for (var j = 0; j < result.length; ++j) {
        if (result[j] != array[j])
            throw "Error: bad entry at j = " + j + ": " + result;
    }
}

// Finally, attempt to trigger the bug.
var notACell = 42;
for (var i = 0; i < 100; ++i) {
    try {
        ftlFunction(array, Int8Array);
    } catch (e) {
        if (e.message.indexOf("not a function") < 0)
            throw "Error: bad exception message: " + e.message;
        var result = notACell.f;
        if (result !== void 0) {
            print("Bad outcome of accessing f on notACell.");
            print("Here's notACell:", notACell, describe(notACell));
            print("Here's the result:", result, describe(result));
            throw "Error: bad outcome of accessing f on " + notACell + ": " + result;
        }
        var result2 = result + 5;
        var result3 = notACell + 5;
        if ("" + result2 != "NaN")
            throw "Error: bad outcome of adding 5 to result: " + result2;
        if (result3 != 47)
            throw "Error: bad outcome of adding 5 to 42: " + result3;
    }
}

