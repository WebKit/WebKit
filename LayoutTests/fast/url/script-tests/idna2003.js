description("IDNA2003 handling in domain name labels.");

debug("The PASS/FAIL results of this test are set to the behavior in IDNA2003.");

cases = [ 
  // For IDNA Compatibility test material see
  // http://www.unicode.org/reports/tr46/
  // 1) Deviant character tests (deviant from IDNA2008)
  // U+00DF normalizes to "ss" during IDNA2003's mapping phase
  ["fa\u00DF.de","fass.de"],
  // The ς U+03C2 GREEK SMALL LETTER FINAL SIGMA
  ["\u03B2\u03CC\u03BB\u03BF\u03C2.com","xn--nxasmq6b.com"],
  // The ZWJ U+200D ZERO WIDTH JOINER
  ["\u0DC1\u0DCA\u200D\u0DBB\u0DD3.com","xn--10cl1a0b.com"],
  // The ZWNJ U+200C ZERO WIDTH NON-JOINER
  ["\u0646\u0627\u0645\u0647\u200C\u0627\u06CC.com","xn--mgba3gch31f.com"],
  // 2) Normalization tests
  ["www.loo\u0138out.net","www.xn--looout-5bb.net"],
  ["\u15EF\u15EF\u15EF.lookout.net","xn--1qeaa.lookout.net"],
  ["www.lookout.\u0441\u043E\u043C","www.lookout.xn--l1adi"],
  ["www.lookout.net\uFF1A80","www.lookout.net:80"],
  ["www\u2025lookout.net","www..lookout.net"],
  ["www.lookout\u2027net","www.xn--lookoutnet-406e"],
  // using Latin letter kra ‘ĸ’ in domain
  ["www.loo\u0138out.net","www.xn--looout-5bb.net"],
  // \u2A74 decomposes into ::=
  ["www.lookout.net\u2A7480","www.lookout.net::%3D80"],
  // 3) Prohibited code points 
  //   Using prohibited high-ASCII \u00A0
  ["www\u00A0.lookout.net","www%20.lookout.net"],
  //   using prohibited non-ASCII space chars 1680 (Ogham space mark)
  ["\u1680lookout.net","%E1%9A%80lookout.net"],
  //   Using prohibited lower ASCII control character \u001F
  ["\u001Flookout.net","%1Flookout.net"],
  //   Using prohibited U+06DD ARABIC END OF AYAH
  ["look\u06DDout.net","look%DB%9Dout.net"],
  //   Using prohibited U+180E MONGOLIAN VOWEL SEPARATOR
  ["look\u180Eout.net","look%E1%A0%8Eout.net"],
  //   Using prohibited U+2060 WORD JOINER
  ["look\u2060out.net","look%E2%81%A0out.net"],
  //   Using prohibited U+FEFF ZERO WIDTH NO-BREAK SPACE
  ["look\uFEFFout.net","look%EF%BB%BFout.net"],
  //   Using prohibited Non-character code points 1FFFE [NONCHARACTER CODE POINTS]
  ["look\uD83F\uDFFEout.net","look%F0%9F%BF%BEout.net"],
  //   Using prohibited U+DEAD half surrogate code point
  // FIXME: ["look\uDEADout.net","look%ED%BA%ADout.net"],
  //   Using prohibited Inappropriate for plain text U+FFFA; INTERLINEAR ANNOTATION SEPARATOR
  ["look\uFFFAout.net","look%EF%BF%BAout.net"],
  //   Using prohibited Inappropriate for canonical representation 2FF0-2FFB; [IDEOGRAPHIC DESCRIPTION CHARACTERS]
  ["look\u2FF0out.net","look%E2%BF%B0out.net"],
  //   Using prohibited Change display properties or are deprecated 0341; COMBINING ACUTE TONE MARK
  ["look\u0341out.net","look%CD%81out.net"],
  //   Using prohibited Change display properties or are deprecated 202E; RIGHT-TO-LEFT OVERRIDE
  ["look\u202Eout.net","look%E2%80%AEout.net"],
  //   Using prohibited Change display properties or are deprecated 206B; ACTIVATE SYMMETRIC SWAPPING
  ["look\u206Bout.net","look%E2%81%ABout.net"],
  //   Using prohibited Tagging characters E0001; LANGUAGE TAG
  ["look\uDB40\uDC01out.net","look%F3%A0%80%81out.net"],
  //   Using prohibited Tagging characters E0020-E007F; [TAGGING CHARACTERS]
  ["look\uDB40\uDC20out.net","look%F3%A0%80%A0out.net"],
  //   Using prohibited Characters with bidirectional property 05BE
  ["look\u05BEout.net","look%D6%BEout.net"]
];

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  shouldBe("canonicalize('http://" + test_vector + "/')",
           "'http://" + expected_result + "/'");
}
