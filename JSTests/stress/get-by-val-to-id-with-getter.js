
// Test GetByVal => GetById conversion works correctly when inlining a getter in the DFG.
function foo(obj, val) {
    if (obj[val]) {
        return 1;
    }
    return 0;
}
noInline(foo);


o = { num: 0,
      get hello() {
          if (this.num === 1)
              return true;
          return false;
      }
    };

for(i = 0; i < 100000; ++i) {
    let num = i % 2;
    o.num = num;
    if (foo(o, "hello") !== num)
        throw new Error("bad result on iteration: " + i);
}
