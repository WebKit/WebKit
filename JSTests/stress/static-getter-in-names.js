function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(JSON.stringify(Object.getOwnPropertyNames(RegExp.prototype).sort()), '["compile","constructor","dotAll","exec","flags","global","hasIndices","ignoreCase","multiline","source","sticky","test","toString","unicode"]');
shouldBe(JSON.stringify(Object.getOwnPropertyNames(/Cocoa/).sort()), '["lastIndex"]');
