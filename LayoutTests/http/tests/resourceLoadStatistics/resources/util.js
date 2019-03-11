function setEnableFeature(enable, completionHandler) {
    if (typeof completionHandler !== "function")
        testFailed("setEnableFeature() requires a completion handler function.");
    testRunner.setStatisticsNotifyPagesWhenDataRecordsWereScanned(enable);
    if (enable) {
        internals.setResourceLoadStatisticsEnabled(true);
        testRunner.setStatisticsIsRunningTest(true);
        completionHandler();
    } else {
        testRunner.statisticsResetToConsistentState(function() {
            testRunner.setStatisticsIsRunningTest(false);
            internals.setResourceLoadStatisticsEnabled(false);
            completionHandler();
        });
    }
}
