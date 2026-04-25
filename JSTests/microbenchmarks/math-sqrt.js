function sqrt(arg) {
  return Math.sqrt(arg);
}

noInline(sqrt);

for (var i = 0; i < testLoopCount; ++i) {
  sqrt(3);
  sqrt(9);
  sqrt(400.2);
  sqrt(-24.3);
}
