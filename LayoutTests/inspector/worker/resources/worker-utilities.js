TestPage.registerInitializer(() => {
    window.awaitTarget = async function(filter) {
        while (true) {
            let target = WI.targets.find(filter);
            if (target)
                return target;

            await WI.targetManager.awaitEvent(WI.TargetManager.Event.TargetAdded);
        }
    };

    window.awaitTargetMainResource = function(workerTarget) {
        if (workerTarget.mainResource)
            return Promise.resolve();
        return workerTarget.awaitEvent(WI.Target.Event.MainResourceAdded);
    }
});
