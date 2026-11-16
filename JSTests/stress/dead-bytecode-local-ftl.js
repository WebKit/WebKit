//@ runDefault("--jitPolicyScale=0")
function bar() {
  return;
  hello;
}

function foo() {
  let j = 0;
  let x = bar();
  do {} while (j++ < 1000);
  return x;
}

for (let i=0; i<1000; i++) {
  foo();
}
