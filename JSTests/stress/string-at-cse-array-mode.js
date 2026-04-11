//@ runDefault("--jitPolicyScale=0.1")

function opt(s, i) {
    let a = s.at(i);
    let b = s.at(i);
    return `${a}_${b}_end`;
}
noInline(opt);

for (let j = 0; j < 20000; j++) opt("hello", 1);
for (let j = 0; j < 200; j++) opt("hello", 100);
