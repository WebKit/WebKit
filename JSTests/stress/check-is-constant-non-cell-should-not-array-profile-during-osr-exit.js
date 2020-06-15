//@ skip if ["arm", "mips"].include?($architecture)

function foo() {
  function bar(a0) {
    try {
      for (let q of a0) {}
      p.isFrozen();
    } catch {}
    return bar;
  }
  let p = new Proxy([], { get: bar });
  bar(p);
}


for (let i=0; i<100; i++) {
  foo();
}
