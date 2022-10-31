//@ runDefault("--jitPolicyScale=0")

function foo() {
  let fillString = (() => {}).toString();
  let s0 = ''.padStart(2 ** 31 - 1, fillString);
  try {
    ''.padStart(1, s0);
  } catch {}
}

let a = [10, foo];
let s1 = a.toLocaleString();
let bar = eval(s1);
for (let i = 0; i < 100000; i++)
  bar();
