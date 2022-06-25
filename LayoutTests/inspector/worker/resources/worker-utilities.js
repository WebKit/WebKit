TestPage.registerInitializer(() => {
    window.awaitTargetMainResource = function(workerTarget) {
        if (workerTarget.mainResource)
            return Promise.resolve();
        return workerTarget.awaitEvent(WI.Target.Event.MainResourceAdded);
    }
});
