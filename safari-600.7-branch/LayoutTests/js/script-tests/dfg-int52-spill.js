description(
"Tests that spilling an int52 works."
);

function foo(x, a) {
    var y = x + 3000000000;
    var y0 = y + 0;
    var y1 = y + 1;
    var y2 = y + 2;
    var y3 = y + 3;
    var y4 = y + 4;
    var y5 = y + 5;
    var y6 = y + 6;
    var y7 = y + 7;
    var y8 = y + 8;
    var y9 = y + 9;
    var y10 = y + 10;
    var y11 = y + 11;
    var y12 = y + 12;
    var y13 = y + 13;
    var y14 = y + 14;
    var y15 = y + 15;
    var y16 = y + 16;
    var y17 = y + 17;
    var y18 = y + 18;
    var y19 = y + 19;
    var y20 = y + 20;
    var y21 = y + 21;
    var y22 = y + 22;
    var y23 = y + 23;
    var y24 = y + 24;
    var y25 = y + 25;
    var y26 = y + 26;
    var y27 = y + 27;
    var y28 = y + 28;
    var y29 = y + 29;
    var y30 = y + 30;
    var y31 = y + 31;
    var y32 = y + 32;
    var y33 = y + 33;
    var y34 = y + 34;
    var y35 = y + 35;
    var y36 = y + 36;
    var y37 = y + 37;
    var y38 = y + 38;
    var y39 = y + 39;

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
    
    return y + 4000000000 + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p + q + r + s + t + u + v + w + A + B + C + D + E + F + G + H + I + J + K + L + M + N + O + P + Q + R + S + T + U + V + W + X + Y + Z + y0 + y1 + y2 + y3 + y4 + y5 + y6 + y7 + y8 + y9 + y10 + y11 + y12 + y13 + y14 + y15 + y16 + y17 + y18 + y19 + y20 + y21 + y22 + y23 + y24 + y25 + y26 + y27 + y28 + y29 + y30 + y31 + y32 + y33 + y34 + y35 + y36 + y37 + y38 + y39;
}

var array = [];
for (var i = 0; i < 48; ++i)
    array[i] = i;

dfgShouldBe(foo, "foo(2000000000, array)", "209000001908");
