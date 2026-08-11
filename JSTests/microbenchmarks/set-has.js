var set = new Set();

for (var i = 0; i < testLoopCount; i++) {
  set.add(i);
  set.has(i);
  set.has(i * 2);
}
