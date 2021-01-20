function foo() {
      bar = 4;
}
function get() {
      return bar;
}
for (var i = 0; i < 1000000; ++i)
      foo();
for (var i = 0; i < 1000000; ++i)
      get();
$.evalScript('let bar = 3;');
for (var i = 0; i < 1000000; ++i)
      get();
for (var i = 0; i < 1000000; ++i)
      foo();
