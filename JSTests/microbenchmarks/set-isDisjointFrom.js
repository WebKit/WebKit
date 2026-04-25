var set1 = new Set();
var set2 = new Set();

for (var i = 0; i < 50; i++) {
  set1.add(i);
  set2.add((i + 1) * 100);
}

for (var i = 0; i < testLoopCount; i++) {
  set1.isDisjointFrom(set2);
  set2.isDisjointFrom(set1);
}
