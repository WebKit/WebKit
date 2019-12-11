function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(JSON.stringify('𝌆'), `"𝌆"`);
shouldBe(JSON.stringify('\uD834\uDF06'), `"𝌆"`);
shouldBe(JSON.stringify('\uD834'), `"\\ud834"`);
shouldBe(JSON.stringify('\uDF06'), `"\\udf06"`);
shouldBe(JSON.stringify('\uDF06\uD834'), `"\\udf06\\ud834"`);
shouldBe(JSON.stringify('\uDEAD'), `"\\udead"`);
shouldBe(JSON.stringify('\uD834\uD834\uDF06'), `"\\ud834𝌆"`);
shouldBe(JSON.stringify('\uD834a'), `"\\ud834a"`);
shouldBe(JSON.stringify('\uD834\u0400'), `"\\ud834Ѐ"`);
