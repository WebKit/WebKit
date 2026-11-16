// Stresses isDefinitelyNonThenable() through a `.then()` handler chain that returns
// plain object literals — every link must check whether the returned object is a
// thenable.

let p = Promise.resolve({ a: 1, b: 2, c: 3 });
for (let i = 0; i < 1e6; i++)
    p = p.then((x) => ({ a: x.a + 1, b: x.b + 1, c: x.c + 1 }));
drainMicrotasks();
