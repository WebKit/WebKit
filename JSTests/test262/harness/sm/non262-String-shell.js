/*---
defines: [runNormalizeTest]
allow_unused: True
---*/

function runNormalizeTest(test) {
  function codePointsToString(points) {
    return points.map(x => String.fromCodePoint(x)).join("");
  }
  function stringify(points) {
    return points.map(x => x.toString(16)).join();
  }

  var source = codePointsToString(test.source);
  var NFC = codePointsToString(test.NFC);
  var NFD = codePointsToString(test.NFD);
  var NFKC = codePointsToString(test.NFKC);
  var NFKD = codePointsToString(test.NFKD);
  var sourceStr = stringify(test.source);
  var nfcStr = stringify(test.NFC);
  var nfdStr = stringify(test.NFD);
  var nfkcStr = stringify(test.NFKC);
  var nfkdStr = stringify(test.NFKD);

  /* NFC */
  assertEq(source.normalize(), NFC, "NFC of " + sourceStr);
  assertEq(NFC.normalize(), NFC, "NFC of " + nfcStr);
  assertEq(NFD.normalize(), NFC, "NFC of " + nfdStr);
  assertEq(NFKC.normalize(), NFKC, "NFC of " + nfkcStr);
  assertEq(NFKD.normalize(), NFKC, "NFC of " + nfkdStr);

  assertEq(source.normalize(undefined), NFC, "NFC of " + sourceStr);
  assertEq(NFC.normalize(undefined), NFC, "NFC of " + nfcStr);
  assertEq(NFD.normalize(undefined), NFC, "NFC of " + nfdStr);
  assertEq(NFKC.normalize(undefined), NFKC, "NFC of " + nfkcStr);
  assertEq(NFKD.normalize(undefined), NFKC, "NFC of " + nfkdStr);

  assertEq(source.normalize("NFC"), NFC, "NFC of " + sourceStr);
  assertEq(NFC.normalize("NFC"), NFC, "NFC of " + nfcStr);
  assertEq(NFD.normalize("NFC"), NFC, "NFC of " + nfdStr);
  assertEq(NFKC.normalize("NFC"), NFKC, "NFC of " + nfkcStr);
  assertEq(NFKD.normalize("NFC"), NFKC, "NFC of " + nfkdStr);

  /* NFD */
  assertEq(source.normalize("NFD"), NFD, "NFD of " + sourceStr);
  assertEq(NFC.normalize("NFD"), NFD, "NFD of " + nfcStr);
  assertEq(NFD.normalize("NFD"), NFD, "NFD of " + nfdStr);
  assertEq(NFKC.normalize("NFD"), NFKD, "NFD of " + nfkcStr);
  assertEq(NFKD.normalize("NFD"), NFKD, "NFD of " + nfkdStr);

  /* NFKC */
  assertEq(source.normalize("NFKC"), NFKC, "NFKC of " + sourceStr);
  assertEq(NFC.normalize("NFKC"), NFKC, "NFKC of " + nfcStr);
  assertEq(NFD.normalize("NFKC"), NFKC, "NFKC of " + nfdStr);
  assertEq(NFKC.normalize("NFKC"), NFKC, "NFKC of " + nfkcStr);
  assertEq(NFKD.normalize("NFKC"), NFKC, "NFKC of " + nfkdStr);

  /* NFKD */
  assertEq(source.normalize("NFKD"), NFKD, "NFKD of " + sourceStr);
  assertEq(NFC.normalize("NFKD"), NFKD, "NFKD of " + nfcStr);
  assertEq(NFD.normalize("NFKD"), NFKD, "NFKD of " + nfdStr);
  assertEq(NFKC.normalize("NFKD"), NFKD, "NFKD of " + nfkcStr);
  assertEq(NFKD.normalize("NFKD"), NFKD, "NFKD of " + nfkdStr);
}
