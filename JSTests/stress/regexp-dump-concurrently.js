//@ runDefault("--jitPolicyScale=0", "--validateAbstractInterpreterState=1")
let code = `
function foo() {
  let o = /a/;
  o.lastIndex = undefined;
  o.toString();
}
for (let i = 0; i < 400; ++i) {
  foo();
}
`
for (let i=0; i<100; i++) {
  runString(code);
}
