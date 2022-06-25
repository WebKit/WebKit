// infinite recursion
try {
  var v = [];
  v[0] = v;
  v.toString();
} catch (e) {
  debug("OK. Caught an exception.");
}
