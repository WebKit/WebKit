// This test is taken almost literally from the bug report: https://bugs.webkit.org/show_bug.cgi?id=141883.
// The only change is to use a loop bound of 1e4 instead of 1e5 to make the test run faster. This
// change still caused a reliable crash in every optimizing JIT configuration prior to the fix.

(function() {
var b=!2;
var n = 1e4;
for(i = 0; i< n; i++) {
b[b=this];
for (var i = 0; i < n; i++) {
  if (a = b*3) {
  }
}
}
})()
