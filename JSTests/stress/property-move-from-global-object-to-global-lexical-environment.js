function foo() {
      bar = 4;
}
function get() {
      return bar;
}
for (var i = 0; i < testLoopCount; ++i)
      foo();
for (var i = 0; i < testLoopCount; ++i)
      get();
$.evalScript('let bar = 3;');
for (var i = 0; i < testLoopCount; ++i)
      get();
for (var i = 0; i < testLoopCount; ++i)
      foo();
