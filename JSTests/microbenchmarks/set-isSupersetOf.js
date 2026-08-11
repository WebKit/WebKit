var set1 = new Set();
var set2 = new Set();

for (var i = 0; i < 50; i++) {
  set1.add(i);
  set2.add(i);
  set2.add(i + 1);
}

for (var i = 0; i < testLoopCount; i++) {
  set1.isSupersetOf(set2);
  set2.isSupersetOf(set1);
}
