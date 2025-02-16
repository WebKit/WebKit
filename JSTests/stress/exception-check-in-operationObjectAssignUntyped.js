//@ requireOptions("--useRandomizingFuzzerAgent=true")

for (let i = 0; i < testLoopCount; ++i) {
  Object.assign({}, [[]][0]);
}
