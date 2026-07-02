//@ skip if $buildType == "debug"
//@ runDefault("--useConcurrentJIT=false", "--jitPolicyScale=0", "--maximumFunctionForCallInlineCandidateBytecodeCostForFTL=500")

let g = 0;
function restY(c, ...r) { g = c ? 1 : 2; return r; }
function h(c, ...r) { return r; }
function h2(...r) { return r; }
function sink() {
    let out = [];
    for (let i = 0; i < arguments.length; i++) out.push(arguments[i]);
    return out;
}
noInline(sink);

function restX(c, ...rx) {
    let arr = [...rx, ...restY(c, ...rx)];
    let dummy = [0, 0, 0, 0, 0, 0, 0, h(50, 9.9, 8.8)];
    return [sink.apply(null, arr), dummy];
}
for (let i = 0; i < 1000000; i++) restX(i & 1, 0.1, 0.2);

function makeSrc(k) {
return `
(function() {
function victim${k}(c1) {
    let q = restX(c1, 0.1, 0.2);
    let z = h2(7.7, 6.6);
    return [q, z];
}
noInline(victim${k});
for (let i = 0; i < 1000000; i++) {
    victim${k}(i & 1);
}
})()
`;
}

for (let k = 0; k < 30; k++) eval(makeSrc(k));
