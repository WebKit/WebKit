// This test should not crash.
x = Reflect;
delete this.Reflect;

for (var i = 0; i < 10000; ++i) { }
