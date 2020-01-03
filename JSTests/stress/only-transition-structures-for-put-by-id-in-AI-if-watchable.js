//@ runDefault("--useAccessInlining=0", "--jitPolicyScale=0")

let s = `
function Ctor() {
  this.b = 0;
}

function test2() {
  let a = new Ctor();
  ~a.b;
}

test2();
test2();
gc();
test2();
Object.defineProperty(Ctor.prototype, 'b', {});
Object.defineProperty(Ctor.prototype, '0', {});
test2();
`
for (let i = 0; i < 1000; i++) {
    runString(s);
}
