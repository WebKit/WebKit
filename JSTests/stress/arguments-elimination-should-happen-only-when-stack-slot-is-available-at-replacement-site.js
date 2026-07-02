//@ runDefault("--jitPolicyScale=0")
function empty() {}

function bar2(...a0) {
  return a0;
}

function foo() {
  let xs = bar2(undefined);
  '' == 1 && 0;
  return empty(...xs, undefined);
}
function main () {
    for (let i = 0; i < 1_000_000; i++) {
      let a = foo();
      Array.isArray(a);
    }
}
main();
