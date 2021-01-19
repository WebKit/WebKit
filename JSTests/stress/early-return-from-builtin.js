//@ skip if $architecture != "arm64" and $architecture != "x86-64"
//@ runDefault("--earlyReturnFromInfiniteLoopsLimit=10", "--returnEarlyFromInfiniteLoopsForFuzzing=1", "--watchdog=1000", "--watchdog-exception-ok")

let o = {
  get value() {
    for (let i=0; i<1; i++) {}
  }
};


let iter = {
  [Symbol.iterator]() {
    return {
      next() {
        return o;
      }
    }
  }
};

[...iter];
