
const requestFramesUntilTrue = async (resolveCondition, rejectCondition) => {
    return new Promise((resolve, reject) => {
        if (rejectCondition && rejectCondition()) {
            reject();
            return;
        }

        (function tryFrame () {
            if (resolveCondition())
                resolve();
            else
                requestAnimationFrame(tryFrame);
        })();
    });
};
