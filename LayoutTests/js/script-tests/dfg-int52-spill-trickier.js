description(
"Tests that spilling an int52 works for a program that is more tricky and wouldn't be subject to reassociation."
);

function foo(x, a) {
    var y0 = a[47 + 0] + 3000000000;
    var y1 = a[47 + 1] + 3000000000;
    var y2 = a[47 + 2] + 3000000000;
    var y3 = a[47 + 3] + 3000000000;
    var y4 = a[47 + 4] + 3000000000;
    var y5 = a[47 + 5] + 3000000000;
    var y6 = a[47 + 6] + 3000000000;
    var y7 = a[47 + 7] + 3000000000;
    var y8 = a[47 + 8] + 3000000000;
    var y9 = a[47 + 9] + 3000000000;
    var y10 = a[47 + 10] + 3000000000;
    var y11 = a[47 + 11] + 3000000000;
    var y12 = a[47 + 12] + 3000000000;
    var y13 = a[47 + 13] + 3000000000;
    var y14 = a[47 + 14] + 3000000000;
    var y15 = a[47 + 15] + 3000000000;
    var y16 = a[47 + 16] + 3000000000;
    var y17 = a[47 + 17] + 3000000000;
    var y18 = a[47 + 18] + 3000000000;
    var y19 = a[47 + 19] + 3000000000;
    var y20 = a[47 + 20] + 3000000000;
    var y21 = a[47 + 21] + 3000000000;
    var y22 = a[47 + 22] + 3000000000;
    var y23 = a[47 + 23] + 3000000000;
    var y24 = a[47 + 24] + 3000000000;
    var y25 = a[47 + 25] + 3000000000;
    var y26 = a[47 + 26] + 3000000000;
    var y27 = a[47 + 27] + 3000000000;
    var y28 = a[47 + 28] + 3000000000;
    var y29 = a[47 + 29] + 3000000000;
    var y30 = a[47 + 30] + 3000000000;
    var y31 = a[47 + 31] + 3000000000;
    var y32 = a[47 + 32] + 3000000000;
    var y33 = a[47 + 33] + 3000000000;
    var y34 = a[47 + 34] + 3000000000;
    var y35 = a[47 + 35] + 3000000000;
    var y36 = a[47 + 36] + 3000000000;
    var y37 = a[47 + 37] + 3000000000;
    var y38 = a[47 + 38] + 3000000000;
    var y39 = a[47 + 39] + 3000000000;

    var b = a[1];
    var c = a[2];
    var d = a[3];
    var e = a[4];
    var f = a[5];
    var g = a[6];
    var h = a[7];
    var i = a[8];
    var j = a[9];
    var k = a[10];
    var l = a[11];
    var m = a[12];
    var n = a[13];
    var o = a[14];
    var p = a[15];
    var q = a[16];
    var r = a[17];
    var s = a[18];
    var t = a[19];
    var u = a[20];
    var v = a[21];
    var w = a[22];
    var A = a[23];
    var B = a[24];
    var C = a[25];
    var D = a[26];
    var E = a[27];
    var F = a[28];
    var G = a[29];
    var H = a[30];
    var I = a[31];
    var J = a[32];
    var K = a[33];
    var L = a[34];
    var M = a[35];
    var N = a[36];
    var O = a[37];
    var P = a[38];
    var Q = a[39];
    var R = a[40];
    var S = a[41];
    var T = a[42];
    var U = a[43];
    var V = a[44];
    var W = a[45];
    var X = a[46];
    var Y = a[47];
    var Z = a[0];
    
    return b + c + d + e + f + g + h + i + j + k + l + m + n + o + p + q + r + s + t + u + v + w + A + B + C + D + E + F + G + H + I + J + K + L + M + N + O + P + Q + R + S + T + U + V + W + X + Y + Z + y0 + y1 + y2 + y3 + y4 + y5 + y6 + y7 + y8 + y9 + y10 + y11 + y12 + y13 + y14 + y15 + y16 + y17 + y18 + y19 + y20 + y21 + y22 + y23 + y24 + y25 + y26 + y27 + y28 + y29 + y30 + y31 + y32 + y33 + y34 + y35 + y36 + y37 + y38 + y39;
}

var array = [];
for (var i = 0; i < 100; ++i)
    array[i] = i;

dfgShouldBe(foo, "foo(2000000000, array)", "120000003788");
