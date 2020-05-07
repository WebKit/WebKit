//@ runDefault("--validateAbstractInterpreterState=1")

function foo(x) {
  if (x > 0n)
    return
}

for (let i = 0; i < 10000000; i++) {
    foo(1);
}
