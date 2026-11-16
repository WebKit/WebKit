// Stresses isDefinitelyNonThenable() through Promise.resolve(object). Resolving a
// plain object literal must check whether it is a thenable, which walks the
// prototype chain unless cached on the Structure.

for (var i = 0; i < 1e6; ++i)
    Promise.resolve({ a: i, b: i + 1, c: i + 2 });
