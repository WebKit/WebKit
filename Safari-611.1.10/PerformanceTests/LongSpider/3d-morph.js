/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

var loops = 3500
var nx = 120
var nz = 120

function morph(a, f) {
    var PI2nx = Math.PI * 8/nx
    var sin = Math.sin
    var f30 = -(50 * sin(f*Math.PI*2))
    
    for (var i = 0; i < nz; ++i) {
        for (var j = 0; j < nx; ++j) {
            a[3*(i*nx+j)+1]    = sin((j-1) * PI2nx ) * -f30
        }
    }
}

    
var a = Array()
for (var i=0; i < nx*nz*3; ++i) 
    a[i] = 0

for (var i = 0; i < loops; ++i) {
    morph(a, i/loops)
}

// This has to be an approximate test since ECMAscript doesn't formally specify
// what sin() returns. Even if it did specify something like for example what Java 7
// says - that sin() has to return a value within 1 ulp of exact - then we still
// would not be able to do an exact test here since that would allow for just enough
// low-bit slop to create possibly big errors due to testOutput being a sum.

var expectedResults = [
    { value: 0.018662099703860055, total: 0.018662099703860055 }, // 0
    { value: 0, total: 0.018662099703860055 }, // 1
    { value: -0.018662099703860055, total: 0 }, // 2
    { value: -0.03650857609997156, total: -0.03650857609997156 }, // 3
    { value: -0.05275945253292943, total: -0.08926802863290098 }, // 4
    { value: -0.06670448772225647, total: -0.15597251635515746 }, // 5
    { value: -0.07773421671447613, total: -0.23370673306963358 }, // 6
    { value: -0.08536658742611654, total: -0.3190733204957501 }, // 7
    { value: -0.08926802863290098, total: -0.40834134912865105 }, // 8
    { value: -0.089268028632901, total: -0.49760937776155206 }, // 9
    { value: -0.08536658742611655, total: -0.5829759651876686 }, // 10
    { value: -0.07773421671447614, total: -0.6607101819021447 }, // 11
    { value: -0.06670448772225651, total: -0.7274146696244013 }, // 12
    { value: -0.052759452532929435, total: -0.7801741221573307 }, // 13
    { value: -0.03650857609997158, total: -0.8166826982573023 }, // 14
    { value: -0.018662099703860093, total: -0.8353447979611625 }, // 15
    { value: -1.0992398059873222e-17, total: -0.8353447979611625 }, // 16
    { value: 0.018662099703860034, total: -0.8166826982573024 }, // 17
    { value: 0.036508576099971525, total: -0.780174122157331 }, // 18
    { value: 0.052759452532929414, total: -0.7274146696244015 }, // 19
    { value: 0.06670448772225647, total: -0.660710181902145 }, // 20
    { value: 0.0777342167144761, total: -0.5829759651876689 }, // 21
    { value: 0.08536658742611654, total: -0.4976093777615524 }, // 22
    { value: 0.08926802863290098, total: -0.4083413491286514 }, // 23
    { value: 0.089268028632901, total: -0.3190733204957504 }, // 24
    { value: 0.08536658742611655, total: -0.23370673306963383 }, // 25
    { value: 0.07773421671447615, total: -0.1559725163551577 }, // 26
    { value: 0.06670448772225653, total: -0.08926802863290116 }, // 27
    { value: 0.052759452532929435, total: -0.036508576099971726 }, // 28
    { value: 0.03650857609997163, total: -9.71445146547012e-17 }, // 29
    { value: 0.018662099703860107, total: 0.01866209970386001 }, // 30
    { value: 2.1984796119746444e-17, total: 0.01866209970386003 }, // 31
    { value: -0.018662099703859986, total: 4.5102810375396984e-17 }, // 32
    { value: -0.03650857609997152, total: -0.036508576099971476 }, // 33
    { value: -0.05275945253292941, total: -0.08926802863290088 }, // 34
    { value: -0.06670448772225643, total: -0.1559725163551573 }, // 35
    { value: -0.0777342167144761, total: -0.2337067330696334 }, // 36
    { value: -0.08536658742611652, total: -0.31907332049574993 }, // 37
    { value: -0.08926802863290098, total: -0.40834134912865094 }, // 38
    { value: -0.089268028632901, total: -0.49760937776155195 }, // 39
    { value: -0.08536658742611655, total: -0.5829759651876685 }, // 40
    { value: -0.07773421671447617, total: -0.6607101819021446 }, // 41
    { value: -0.06670448772225658, total: -0.7274146696244012 }, // 42
    { value: -0.05275945253292945, total: -0.7801741221573306 }, // 43
    { value: -0.03650857609997164, total: -0.8166826982573022 }, // 44
    { value: -0.018662099703860194, total: -0.8353447979611625 }, // 45
    { value: -3.2977194179619666e-17, total: -0.8353447979611625 }, // 46
    { value: 0.018662099703859975, total: -0.8166826982573024 }, // 47
    { value: 0.036508576099971435, total: -0.7801741221573311 }, // 48
    { value: 0.0527594525329294, total: -0.7274146696244017 }, // 49
    { value: 0.06670448772225643, total: -0.6607101819021453 }, // 50
    { value: 0.07773421671447606, total: -0.5829759651876691 }, // 51
    { value: 0.08536658742611652, total: -0.4976093777615526 }, // 52
    { value: 0.08926802863290097, total: -0.40834134912865167 }, // 53
    { value: 0.08926802863290101, total: -0.31907332049575066 }, // 54
    { value: 0.08536658742611655, total: -0.2337067330696341 }, // 55
    { value: 0.07773421671447617, total: -0.15597251635515794 }, // 56
    { value: 0.06670448772225658, total: -0.08926802863290136 }, // 57
    { value: 0.052759452532929456, total: -0.0365085760999719 }, // 58
    { value: 0.03650857609997165, total: -2.498001805406602e-16 }, // 59
    { value: 0.018662099703860208, total: 0.018662099703859958 }, // 60
    { value: 4.396959223949289e-17, total: 0.018662099703860003 }, // 61
    { value: -0.01866209970385996, total: 4.163336342344337e-17 }, // 62
    { value: -0.03650857609997142, total: -0.03650857609997138 }, // 63
    { value: -0.0527594525329294, total: -0.08926802863290079 }, // 64
    { value: -0.06670448772225641, total: -0.1559725163551572 }, // 65
    { value: -0.07773421671447604, total: -0.23370673306963324 }, // 66
    { value: -0.08536658742611652, total: -0.31907332049574977 }, // 67
    { value: -0.08926802863290097, total: -0.4083413491286507 }, // 68
    { value: -0.08926802863290101, total: -0.49760937776155173 }, // 69
    { value: -0.08536658742611655, total: -0.5829759651876683 }, // 70
    { value: -0.07773421671447618, total: -0.6607101819021445 }, // 71
    { value: -0.06670448772225658, total: -0.727414669624401 }, // 72
    { value: -0.05275945253292948, total: -0.7801741221573305 }, // 73
    { value: -0.036508576099971664, total: -0.8166826982573022 }, // 74
    { value: -0.018662099703860215, total: -0.8353447979611625 }, // 75
    { value: -5.496199029936611e-17, total: -0.8353447979611625 }, // 76
    { value: 0.01866209970385995, total: -0.8166826982573026 }, // 77
    { value: 0.036508576099971414, total: -0.7801741221573312 }, // 78
    { value: 0.05275945253292938, total: -0.7274146696244018 }, // 79
    { value: 0.0667044877222563, total: -0.6607101819021455 }, // 80
    { value: 0.07773421671447604, total: -0.5829759651876695 }, // 81
    { value: 0.08536658742611652, total: -0.49760937776155295 }, // 82
    { value: 0.08926802863290097, total: -0.408341349128652 }, // 83
    { value: 0.08926802863290101, total: -0.319073320495751 }, // 84
    { value: 0.08536658742611655, total: -0.23370673306963444 }, // 85
    { value: 0.07773421671447626, total: -0.1559725163551582 }, // 86
    { value: 0.0667044877222566, total: -0.08926802863290159 }, // 87
    { value: 0.05275945253292948, total: -0.036508576099972115 }, // 88
    { value: 0.036508576099971816, total: -2.983724378680108e-16 }, // 89
    { value: 0.018662099703860225, total: 0.018662099703859927 }, // 90
    { value: 6.595438835923933e-17, total: 0.018662099703859993 }, // 91
    { value: -0.018662099703859788, total: 2.0469737016526324e-16 }, // 92
    { value: -0.0365085760999714, total: -0.0365085760999712 }, // 93
    { value: -0.052759452532929366, total: -0.08926802863290056 }, // 94
    { value: -0.06670448772225629, total: -0.15597251635515685 }, // 95
    { value: -0.07773421671447604, total: -0.2337067330696329 }, // 96
    { value: -0.08536658742611652, total: -0.31907332049574944 }, // 97
    { value: -0.08926802863290095, total: -0.4083413491286504 }, // 98
    { value: -0.08926802863290101, total: -0.4976093777615514 }, // 99
    { value: -0.08536658742611657, total: -0.5829759651876679 }, // 100
    { value: -0.07773421671447626, total: -0.6607101819021441 }, // 101
    { value: -0.06670448772225661, total: -0.7274146696244007 }, // 102
    { value: -0.052759452532929484, total: -0.7801741221573302 }, // 103
    { value: -0.03650857609997182, total: -0.816682698257302 }, // 104
    { value: -0.01866209970386024, total: -0.8353447979611622 }, // 105
    { value: -7.694678641911255e-17, total: -0.8353447979611623 }, // 106
    { value: 0.018662099703859774, total: -0.8166826982573026 }, // 107
    { value: 0.0365085760999714, total: -0.7801741221573312 }, // 108
    { value: 0.05275945253292936, total: -0.7274146696244018 }, // 109
    { value: 0.06670448772225629, total: -0.6607101819021455 }, // 110
    { value: 0.07773421671447603, total: -0.5829759651876695 }, // 111
    { value: 0.08536658742611652, total: -0.49760937776155295 }, // 112
    { value: 0.08926802863290095, total: -0.408341349128652 }, // 113
    { value: 0.08926802863290101, total: -0.319073320495751 }, // 114
    { value: 0.08536658742611657, total: -0.2337067330696344 }, // 115
    { value: 0.07773421671447626, total: -0.15597251635515813 }, // 116
    { value: 0.06670448772225661, total: -0.08926802863290152 }, // 117
    { value: 0.0527594525329295, total: -0.036508576099972025 }, // 118
    { value: 0.03650857609997184, total: -1.8735013540549517e-16 }, // 119
];

var valueError = 0.00000000000001;
var totalError = 0.0000000000001;

function expect(i, value, total) {
    var data = expectedResults;
    var valueDiff = Math.abs(value - data[i].value);
    var totalDiff = Math.abs(total - data[i].total);

    var err = null;
    if (valueDiff > valueError && totalDiff > totalError)
        err = "    [" + i + "] value a:" + value + " e:" + data[i].value + " | d:" + valueDiff + ", total a:" + total + " e:" + data[i].total + " | d: " + totalDiff;
    else if (valueDiff > valueError)
        err = "    [" + i + "] value a:" + value + " e:" + data[i].value + " | d:" + valueDiff;
    else if (totalDiff > totalError)
        err = "    [" + i + "] total a:" + total + " e:" + data[i].total + " | d: " + totalDiff;

    if (err)
        throw err;
}

testOutput = 0;
for (var i = 0; i < nx; i++) {
    var value = a[3*(i*nx+i)+1];
    testOutput += value;
    expect(i, value, testOutput);
}
a = null;
