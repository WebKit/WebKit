
const animationAcceleration = async animation => {
    // If the animation is already ready, it should already
    // have been considered for acceleration.
    if (!animation.pending)
        return;

    // An animation must be ready to be considered for acceleration.
    await animation.ready;

    // We only need to wait for the next JS run loop here
    // because composition will happen during the animation frame,
    // but after the requestAnimationFrame callbacks have been serviced.
    await new Promise(setTimeout);
};

const legacyAnimationCommit = async () => {
    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);
};

const threadedAnimationsCommit = async () => {
    await new Promise(requestAnimationFrame);
    await new Promise(setTimeout);
};
