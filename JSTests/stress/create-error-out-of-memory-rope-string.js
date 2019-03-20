var foo = 'yy?x\uFFFD$w    5?\uFFFDo\uFFFD?\uFFFD\'i?\uFFFDE-N\uFFFD\uFFFD6_\uFFFD\\ d';
foo = foo.padEnd(2147483644, 1);
eval('foo()');
