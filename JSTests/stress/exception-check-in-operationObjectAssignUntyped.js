//@ requireOptions("--useRandomizingFuzzerAgent=true")

for (let i = 0; i < 10000; ++i) {
  Object.assign({}, [[]][0]);
}
