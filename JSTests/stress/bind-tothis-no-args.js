//@ requireOptions("--forceEagerCompilation=1", "--useConcurrentJIT=0")
function f0(x) {
    try {
      anything();eval();
    } catch (error) {
        if (x < 1e3)
          f0.bind()(x+1);
    }
}
noInline(f0);
try {
  f0(1);
} catch (error) {
  // This test shouldn't crash or throw, but in certain configurations it might
  // run out of stack
}
