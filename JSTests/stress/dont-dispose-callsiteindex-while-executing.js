//@ runDefault("--jitPolicyScale=0")

function emptyFunction() {}

function foo(a0) {
  function bar(a1) {
    let o = Object(a0);
    try {
      o.x = 0;
      new Set().values();
      gc();
      Object.defineProperty(a1, 'x', { set: bar });
    } catch {}

    try {
      Function('function');
    } catch {}
  }
  bar(emptyFunction);
}

foo(0);
for (let i=0; i<100; i++)
  foo(emptyFunction);
