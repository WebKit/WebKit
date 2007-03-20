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
