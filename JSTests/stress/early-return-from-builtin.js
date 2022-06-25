//@ runDefault("--earlyReturnFromInfiniteLoopsLimit=10", "--returnEarlyFromInfiniteLoopsForFuzzing=1", "--watchdog=1000", "--watchdog-exception-ok")

let o = {
  get value() {
    for (let i=0; i<1; i++) {}
  }
};


let i = 0;
let iter = {
  [Symbol.iterator]() {
    return {
      next() {
        ++i
        if (i < 1e6)
            return o;
        return {
            done: true
        };
      }
    }
  }
};

try {
    [...iter];
} catch {}
