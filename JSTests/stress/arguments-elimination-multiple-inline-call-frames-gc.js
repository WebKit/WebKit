//@ skip if $buildType == "debug"
//@ runDefault("--useConcurrentJIT=false", "--collectContinuously=true", "--maximumFunctionForCallInlineCandidateBytecodeCostForFTL=500", "--thresholdForFTLOptimizeAfterWarmUp=200", "--thresholdForOptimizeAfterWarmUp=50", "--thresholdForJITAfterWarmUp=25")

let g = 0;
function restY(c, ...r) { g = c ? 1 : 2; return r; }
function h(c, ...r) { return r; }
function h2(...r) { return r; }
function sink() { let out = []; for (let i = 0; i < arguments.length; i++) out.push(arguments[i]); return out; }
noInline(sink);
function restX(c, ...rx) {
    let arr = [...rx, ...restY(c, ...rx)];
    let dummy = [0, 0, 0, 0, 0, 0, 0, h(50, 9.9, 8.8)];
    return [sink.apply(null, arr), dummy];
}
for (let i = 0; i < 1000; i++) restX(i & 1, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2);
eval(`(function() {
    function victim0(c1) { let q = restX(c1, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2); let z = h2(7.7, 6.6); return [q, z]; }
    noInline(victim0);
    for (let i = 0; i < 1000; i++) victim0(i & 1);
})()`);
eval(`(function() {
    function victim1(c1) { let q = restX(c1, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2); let z = h2(7.7, 6.6); return [q, z]; }
    noInline(victim1);
    for (let i = 0; i < 1000; i++) victim1(i & 1);
})()`);
eval(`(function() {
    function victim2(c1) { let q = restX(c1, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2); let z = h2(7.7, 6.6); return [q, z]; }
    noInline(victim2);
    for (let i = 0; i < 1000; i++) victim2(i & 1);
})()`);
eval(`(function() {
    function victim3(c1) { let q = restX(c1, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2); let z = h2(7.7, 6.6); return [q, z]; }
    noInline(victim3);
    for (let i = 0; i < 1000; i++) victim3(i & 1);
})()`);
eval(`(function() {
    function victim4(c1) { let q = restX(c1, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2); let z = h2(7.7, 6.6); return [q, z]; }
    noInline(victim4);
    for (let i = 0; i < 1000; i++) victim4(i & 1);
})()`);
eval(`(function() {
    function victim5(c1) { let q = restX(c1, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2); let z = h2(7.7, 6.6); return [q, z]; }
    noInline(victim5);
    for (let i = 0; i < 1000; i++) victim5(i & 1);
})()`);
eval(`(function() {
    function victim6(c1) { let q = restX(c1, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2); let z = h2(7.7, 6.6); return [q, z]; }
    noInline(victim6);
    for (let i = 0; i < 1000; i++) victim6(i & 1);
})()`);
eval(`(function() {
    function victim7(c1) { let q = restX(c1, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2); let z = h2(7.7, 6.6); return [q, z]; }
    noInline(victim7);
    for (let i = 0; i < 1000; i++) victim7(i & 1);
})()`);
