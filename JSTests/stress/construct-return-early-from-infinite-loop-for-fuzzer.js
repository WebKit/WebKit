//@ skip if $architecture != "arm64" and $architecture != "x86-64"
//@ runDefault("--returnEarlyFromInfiniteLoopsForFuzzing=1")
function foo() {
  while(1);
}
Reflect.construct(foo, {});
