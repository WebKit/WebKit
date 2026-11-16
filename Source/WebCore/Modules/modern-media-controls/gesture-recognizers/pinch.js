
const MAXIMUM_TIME_FOR_RECORDING_GESTURES = 100;
const MAXIMUM_DECELERATION_TIME = 500;

class PinchGestureRecognizer extends GestureRecognizer
{

    constructor(target, delegate)
    {
        super(target, delegate);

        this.scaleThreshold = 0;
        this._scaledMinimumAmount = false;
    }

    // Public

    get velocity()
    {
        const lastGesture = this._gestures[this._gestures.length - 1];
        if (!lastGesture)
            return this._velocity;

        const elapsedTime = Date.now() - (lastGesture.timeStamp + MAXIMUM_TIME_FOR_RECORDING_GESTURES);
        if (elapsedTime <= 0)
            return this._velocity;

        const f = Math.max((MAXIMUM_DECELERATION_TIME - elapsedTime) / MAXIMUM_DECELERATION_TIME, 0);
        return this._velocity * f;
    }

    // Protected

    touchesBegan(event)
    {
        if (event.currentTarget !== this.target)
            return;

        // Additional setup for when the the platform doesn't natively
        // provide us with gesture events.
        if (!GestureRecognizer.SupportsGestures) {
            // A pinch gesture can only be performed with 2 fingers, anything more
            // and we failed our gesture.
            if (this.numberOfTouches > 2) {
                this.enterFailedState();
                return;
            }

            // We can only start tracking touches with 2 fingers.
            if (this.numberOfTouches !== 2)
                return;

            this._startDistance = this._distance();

            // We manually add a start value so that we always have 2 entries in the
            // _gestures array so that we don't have to check for the existence of 2
            // entries when computing velocity.
            this._recordGesture(1);

            this._scaledMinimumAmount = false;
            this._updateStateWithEvent(event);
        } else if (this.numberOfTouches !== 2) {
            // When we support gesture events, we only care about the case where we're
            // using two fingers.
            return;
        }

        super.touchesBegan(event);
    }

    touchesMoved(event)
    {
        // This method only needs to be overriden in the case where the platform
        // doesn't natively provide us with gesture events.
        if (GestureRecognizer.SupportsGestures)
            return;

        if (this.numberOfTouches !== 2)
            return;

        this._updateStateWithEvent(event);
    }

    touchesEnded(event)
    {
        // This method only needs to be overriden in the case where the platform
        // doesn't natively provide us with gesture events.
        if (GestureRecognizer.SupportsGestures)
            return;

        // If we don't have the required number of touches or have not event
        // obtained 2 fingers, then there's nothing for us to do.
        if (this.numberOfTouches >= 2 || !this._startDistance)
            return;

        if (this._scaledMinimumAmount)
            this.enterEndedState();
        else
            this.enterFailedState();
    }

    gestureBegan(event)
    {
        super.gestureBegan(event);

        // We manually add a start value so that we always have 2 entries in the
        // _gestures array so that we don't have to check for the existence of 2
        // entries when computing velocity.
        this._recordGesture(event.scale);

        this._scaledMinimumAmount = false;
        this._updateStateWithEvent(event);

        event.preventDefault();
    }

    gestureChanged(event)
    {
        event.preventDefault();

        this._updateStateWithEvent(event);
    }

    gestureEnded(event)
    {
        if (this._scaledMinimumAmount)
            this.enterEndedState();
        else
            this.enterFailedState();
    }

    reset()
    {
        this.scale = 1;
        this._velocity = 0;
        this._gestures = [];
        delete this._startDistance;
    }

    // Private

    _recordGesture(scale)
    {
        const currentTime = Date.now();
        const count = this._gestures.push({
            scale: scale,
            timeStamp: currentTime
        });

        // We want to keep at least two gestures at all times.
        if (count <= 2)
            return;

        const scaleDirection = this._gestures[count - 1].scale >= this._gestures[count - 2].scale;
        let i = count - 3;
        for (; i >= 0; --i) {
            let gesture = this._gestures[i];
            if (currentTime - gesture.timeStamp > MAXIMUM_TIME_FOR_RECORDING_GESTURES ||
                this._gestures[i + 1].scale >= gesture.scale !== scaleDirection)
                break;
        }

        if (i > 0)
            this._gestures = this._gestures.slice(i + 1);
    }

    _updateStateWithEvent(event)
    {
        const scaleSinceStart = GestureRecognizer.SupportsGestures ? event.scale : this._distance() / this._startDistance;

        if (!this._scaledMinimumAmount) {
            if (Math.abs(1 - scaleSinceStart) >= this.scaleThreshold) {
                this._scaledMinimumAmount = true;
                this.scale = 1;
                this.enterBeganState();
            }
            return;
        }

        this._recordGesture(scaleSinceStart);

        const oldestGesture = this._gestures[0];
        const ds = scaleSinceStart - oldestGesture.scale;
        const dt = Date.now() - oldestGesture.timeStamp;
        this._velocity = (dt === 0) ? 0 : ds / dt * 1000;

        this.scale *= scaleSinceStart / this._gestures[this._gestures.length - 2].scale;

        this.enterChangedState();
    }

    _distance()
    {
        console.assert(this.numberOfTouches === 2);

        const firstTouch = this._targetTouches[0];
        const firstTouchPoint = new DOMPoint(firstTouch.pageX, firstTouch.pageY);

        const secondTouch = this._targetTouches[1];
        const secondTouchPoint = new DOMPoint(secondTouch.pageX, secondTouch.pageY);

        return Math.sqrt(Math.pow(firstTouchPoint.x - secondTouchPoint.x, 2) + Math.pow(firstTouchPoint.y - secondTouchPoint.y, 2));
    }

}
