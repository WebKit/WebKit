window.animations = [];

function createAnimation(selector, keyframes = [], options = {}) {
    let animation = null;

    let target = document.querySelector(selector);
    if (target)
        animation = target.animate(keyframes, options);
    else
        animation = new Animation(new KeyframeEffect(null, keyframes, options));

    window.animations.push(animation);
}

function destroyAnimations() {
    for (let animation of window.animations) {
        if (!animation)
            return;

        animation.cancel();

        if (animation.effect) {
            if (animation.effect.target) {
                animation.effect.target.remove();
                animation.effect.target = null;
            }

            animation.effect = null;
        }
    }

    window.animations = [];

    // Force GC to make sure the animation and it's target are both destroyed, as otherwise the
    // frontend will not receive Animation.animationDestroyed events.
    setTimeout(() => {
        GCController.collect();
    });
}

TestPage.registerInitializer(() => {
    function jsonKeyframeFilter(key, value) {
        if (key === "easing" && value instanceof WI.CubicBezier)
            return value.toString();
        return value;
    }

    InspectorTest.AnimationLifecycleUtilities = {};

    InspectorTest.AnimationLifecycleUtilities.awaitAnimationCreated = async function(animationType) {
        let nameChangedPromise = null;
        if (animationType === WI.Animation.Type.WebAnimation)
            nameChangedPromise = WI.Animation.awaitEvent(WI.Animation.Event.NameChanged);

        let animationCollectionItemAddedEvent = await WI.animationManager.animationCollection.awaitEvent(WI.Collection.Event.ItemAdded);

        let animation = animationCollectionItemAddedEvent.data.item;

        await nameChangedPromise;

        InspectorTest.pass(`Animation created '${animation.displayName}'.`);

        for (let i = 0; i < animation.backtrace.length; ++i) {
            let callFrame = animation.backtrace[i];
            let traceText = `  ${i}: `;
            traceText += callFrame.functionName || "(anonymous function)";

            if (callFrame.nativeCode)
                traceText += " - [native code]";
            else if (callFrame.programCode)
                traceText += " - [program code]";
            else if (callFrame.sourceCodeLocation) {
                let location = callFrame.sourceCodeLocation;
                traceText += " - " + sanitizeURL(location.sourceCode.url) + `:${location.lineNumber}:${location.columnNumber}`;
            }

            InspectorTest.log(traceText);
        }

        InspectorTest.expectEqual(animation.animationType, animationType, `Animation type should be ${WI.Animation.displayNameForAnimationType(animationType)}.`);

        if (animation.startDelay)
            InspectorTest.log("startDelay: " + JSON.stringify(animation.startDelay));
        if (animation.endDelay)
            InspectorTest.log("endDelay: " + JSON.stringify(animation.endDelay));
        if (animation.iterationCount)
            InspectorTest.log("iterationCount: " + (animation.iterationCount === Infinity ? "Infinity" : JSON.stringify(animation.iterationCount)));
        if (animation.iterationStart)
            InspectorTest.log("iterationStart: " + JSON.stringify(animation.iterationStart));
        if (animation.iterationDuration)
            InspectorTest.log("iterationDuration: " + JSON.stringify(animation.iterationDuration));
        if (animation.timingFunction)
            InspectorTest.log("timingFunction: " + JSON.stringify(String(animation.timingFunction)));
        if (animation.playbackDirection)
            InspectorTest.log("playbackDirection: " + JSON.stringify(animation.playbackDirection));
        if (animation.fillMode)
            InspectorTest.log("fillMode: " + JSON.stringify(animation.fillMode));
        if (animation.keyframes.length) {
            InspectorTest.log("keyframes:");
            InspectorTest.json(animation.keyframes, jsonKeyframeFilter);
        }

        InspectorTest.newline();

        return animation
    };

    InspectorTest.AnimationLifecycleUtilities.awaitAnimationDestroyed = async function(animationIdentifier) {
        let animationCollectionItemRemovedEvent = await WI.animationManager.animationCollection.awaitEvent(WI.Collection.Event.ItemRemoved)

        InspectorTest.pass("Animation destroyed.");

        let animation = animationCollectionItemRemovedEvent.data.item;
        InspectorTest.expectEqual(animation.animationId, animationIdentifier, "Removed animation has expected ID.");
    };

    InspectorTest.AnimationLifecycleUtilities.createAnimation = async function(animationType, {selector, keyframes, options} = {}) {
        InspectorTest.log("Creating animation...\n");

        let stringifiedSelector = JSON.stringify(selector || null);
        let stringifiedKeyframes = JSON.stringify(keyframes || []);
        let stringifiedOptions = JSON.stringify(options || {});

        let [animation] = await Promise.all([
            InspectorTest.AnimationLifecycleUtilities.awaitAnimationCreated(animationType),
            InspectorTest.evaluateInPage(`createAnimation(${stringifiedSelector}, ${stringifiedKeyframes}, ${stringifiedOptions})`)
        ]);

        return animation;
    };

    InspectorTest.AnimationLifecycleUtilities.destroyAnimations = async function() {
        InspectorTest.log("Destroying animations...\n");
        await InspectorTest.evaluateInPage(`destroyAnimations()`);
    };
});
