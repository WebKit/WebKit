function checkSnapshot(comment, actual, expected) {
    if (actual.snapshotLength != expected.length) {
        testFailed(comment + " incorrect length (expected " + expected.length + ", actual " + actual.snapshotLength + ")");
        return;
    }
    
    for (i = 0; i < actual.snapshotLength; ++i) {
        if (actual.snapshotItem(i) != expected[i]) {
            testFailed(comment + " item " + i + " incorrect (expected " + expected[i].nodeName + ", actual " + actual.snapshotItem(i).nodeName + ")");
            return;
        }
    }
    
    testPassed(comment);
}

function test(doc, context, expr, expected, nsResolver)
{
    try {
        if (typeof(context) == "string")
            context = doc.evaluate(context, doc.documentElement, nsResolver, XPathResult.ANY_UNORDERED_NODE_TYPE, null).singleNodeValue;
        if (typeof(expected) == "object") {
            var result = doc.evaluate(expr, context, nsResolver, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
            checkSnapshot(expr,  result, expected);
        } else {
            var result = doc.evaluate(expr, context, nsResolver, XPathResult.ANY_TYPE, null);
            if (typeof(expected) == "number") {
                if (result.numberValue == expected)
                    testPassed(expr);
                else
                    testFailed(expr + ": expected " + expected + ", actual " + result.numberValue);
            } else if (typeof(expected) == "string") {
                if (result.stringValue == expected)
                    testPassed(expr);
                else
                    testFailed(expr + ": expected '" + expected + "', actual '" + result.stringValue + "'");
            } else if (typeof(expected) == "boolean") {
                if (result.booleanValue == expected)
                    testPassed(expr);
                else
                    testFailed(expr + ": expected '" + expected + "', actual '" + result.booleanValue + "'");
            }
        }
    } catch (ex) {
        testFailed(expr + ": raised " + ex);
    }
}
