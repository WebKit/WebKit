//@ runDefault("--returnEarlyFromInfiniteLoopsForFuzzing=1", "--earlyReturnFromInfiniteLoopsLimit=1000000")
function foo() {
  while (1);
}

if ($vm.useJIT()) {
    let x = foo();
    let handler = {
      'get': () => async () => {},
    };
    let proxy = new Proxy(x, handler);

    try {
        for (let i = 0; i < 1000; i++) {
          [] = proxy;
        }
    } catch { }
}
