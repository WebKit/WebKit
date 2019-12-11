
class GestureRecognizer
{

    constructor(target = null, delegate = null)
    {
        this._targetTouches = [];

        this.modifierKeys = {
            alt : false,
            ctrl : false,
            meta : false,
            shift : false
        };

        this._state = GestureRecognizer.States.Possible;
        this._enabled = true;

        this.target = target;
        this.delegate = delegate;
    }

    // Public

    get state()
    {
        return this._state;
    }

    set state(state)
    {
        if (this._state === state && state !== GestureRecognizer.States.Changed)
            return;

        this._state = state;
        if (this.delegate && typeof this.delegate.gestureRecognizerStateDidChange === "function")
            this.delegate.gestureRecognizerStateDidChange(this);
    }

    get target()
    {
        return this._target;
    }

    set target(target)
    {
        if (!target || this._target === target)
            return;

        this._target = target;
        this._initRecognizer();
    }

    get numberOfTouches()
    {
        return this._targetTouches.length;
    }

    get enabled()
    {
        return this._enabled;
    }

    set enabled(enabled)
    {
        if (this._enabled === enabled)
            return;

        this._enabled = enabled;

        if (!enabled) {
            if (this.numberOfTouches === 0) {
                this._removeTrackingListeners();
                this.reset();
            } else
                this.enterCancelledState();
        }

        this._updateBaseListeners();
    }

    reset()
    {
        // Implemented by subclasses.
    }

    locationInElement(element)
    {
        const p = new DOMPoint;
        const touches = this._targetTouches;
        const count = touches.length;
        for (let i = 0; i < count; ++i) {
            const touch = touches[i];
            p.x += touch.pageX;
            p.y += touch.pageY;
        }
        p.x /= count;
        p.y /= count;

        if (!element)
            return p;

        // FIXME: are WebKitPoint and DOMPoint interchangeable?
        const wkPoint = window.webkitConvertPointFromPageToNode(element, new WebKitPoint(p.x, p.y));
        return new DOMPoint(wkPoint.x, wkPoint.y);
    }

    locationInClient()
    {
        const p = new DOMPoint;
        const touches = this._targetTouches;
        const count = touches.length;
        for (let i = 0; i < count; ++i) {
            const touch = touches[i];
            p.x += touch.clientX;
            p.y += touch.clientY;
        }
        p.x /= count;
        p.y /= count;

        return p;
    }

    locationOfTouchInElement(touchIndex, element)
    {
        const touch = this._targetTouches[touchIndex];
        if (!touch)
            return new DOMPoint;

        const touchLocation = new DOMPoint(touch.pageX, touch.pageY);
        if (!element)
            return touchLocation;

        // FIXME: are WebKitPoint and DOMPoint interchangeable?
        const wkPoint = window.webkitConvertPointFromPageToNode(element, new WebKitPoint(touchLocation.x, touchLocation.y));
        return new DOMPoint(wkPoint.x, wkPoint.y);
    }

    touchesBegan(event)
    {
        if (event.currentTarget !== this._target)
            return;

        window.addEventListener(GestureRecognizer.Events.TouchMove, this, true);
        window.addEventListener(GestureRecognizer.Events.TouchEnd, this, true);
        window.addEventListener(GestureRecognizer.Events.TouchCancel, this, true);
        this.enterPossibleState();
    }

    touchesMoved(event)
    {
        // Implemented by subclasses.
    }

    touchesEnded(event)
    {
        // Implemented by subclasses.
    }

    touchesCancelled(event)
    {
        // Implemented by subclasses.
    }

    gestureBegan(event)
    {
        if (event.currentTarget !== this._target)
            return;

        this._target.addEventListener(GestureRecognizer.Events.GestureChange, this, true);
        this._target.addEventListener(GestureRecognizer.Events.GestureEnd, this, true);
        this.enterPossibleState();
    }

    gestureChanged(event)
    {
        // Implemented by subclasses.
    }

    gestureEnded(event)
    {
        // Implemented by subclasses.
    }

    enterPossibleState()
    {
        this.state = GestureRecognizer.States.Possible;
    }

    enterBeganState()
    {
        if (this.delegate && typeof this.delegate.gestureRecognizerShouldBegin === "function" && !this.delegate.gestureRecognizerShouldBegin(this)) {
            this.enterFailedState();
            return;
        }
        this.state = GestureRecognizer.States.Began;
    }

    enterEndedState()
    {
        this.state = GestureRecognizer.States.Ended;
        this._removeTrackingListeners();
        this.reset();
    }

    enterCancelledState()
    {
        this.state = GestureRecognizer.States.Cancelled;
        this._removeTrackingListeners();
        this.reset();
    }

    enterFailedState()
    {
        this.state = GestureRecognizer.States.Failed;
        this._removeTrackingListeners();
        this.reset();
    }

    enterChangedState()
    {
        this.state = GestureRecognizer.States.Changed;
    }

    enterRecognizedState()
    {
        this.state = GestureRecognizer.States.Recognized;
    }

    // Protected

    handleEvent(event)
    {
        this._updateTargetTouches(event);
        this._updateKeyboardModifiers(event);

        switch (event.type) {
        case GestureRecognizer.Events.TouchStart:
            this.touchesBegan(event);
            break;
        case GestureRecognizer.Events.TouchMove:
            this.touchesMoved(event);
            break;
        case GestureRecognizer.Events.TouchEnd:
            this.touchesEnded(event);
            break;
        case GestureRecognizer.Events.TouchCancel:
            this.touchesCancelled(event);
            break;
        case GestureRecognizer.Events.GestureStart:
            this.gestureBegan(event);
            break;
        case GestureRecognizer.Events.GestureChange:
            this.gestureChanged(event);
            break;
        case GestureRecognizer.Events.GestureEnd:
            this.gestureEnded(event);
            break;
        }
    }

    // Private

    _initRecognizer()
    {
        this.reset();
        this.state = GestureRecognizer.States.Possible;

        this._updateBaseListeners();
    }

    _updateBaseListeners()
    {
        if (!this._target)
            return;

        if (this._enabled) {
            this._target.addEventListener(GestureRecognizer.Events.TouchStart, this);
            if (GestureRecognizer.SupportsGestures)
                this._target.addEventListener(GestureRecognizer.Events.GestureStart, this);
        } else {
            this._target.removeEventListener(GestureRecognizer.Events.TouchStart, this);
            if (GestureRecognizer.SupportsGestures)
                this._target.removeEventListener(GestureRecognizer.Events.GestureStart, this);
        }
    }

    _removeTrackingListeners()
    {
        window.removeEventListener(GestureRecognizer.Events.TouchMove, this, true);
        window.removeEventListener(GestureRecognizer.Events.TouchEnd, this, true);
        this._target.removeEventListener(GestureRecognizer.Events.GestureChange, this, true);
        this._target.removeEventListener(GestureRecognizer.Events.GestureEnd, this, true);
    }

    _updateTargetTouches(event)
    {
        if (!GestureRecognizer.SupportsTouches) {
            if (event.type === GestureRecognizer.Events.TouchEnd)
                this._targetTouches = [];
            else
                this._targetTouches = [event];
            return;
        }

        if (!(event instanceof TouchEvent))
            return;

        // With a touchstart event, event.targetTouches is accurate so
        // we simply add all of those.
        if (event.type === GestureRecognizer.Events.TouchStart) {
            this._targetTouches = [];
            let touches = event.targetTouches;
            for (let i = 0, count = touches.length; i < count; ++i)
                this._targetTouches.push(touches[i]);
            return;
        }

        // With a touchmove event, the target is window so event.targetTouches is
        // inaccurate so we add all touches that we knew about previously.
        if (event.type === GestureRecognizer.Events.TouchMove) {
            let targetIdentifiers = this._targetTouches.map(function(touch) {
                return touch.identifier;
            });

            this._targetTouches = [];
            let touches = event.touches;
            for (let i = 0, count = touches.length; i < count; ++i) {
                let touch = touches[i];
                if (targetIdentifiers.indexOf(touch.identifier) !== -1)
                    this._targetTouches.push(touch);
            }
            return;
        }

        // With a touchend or touchcancel event, we only keep the existing touches
        // that are also found in event.touches.
        let allTouches = event.touches;
        let existingIdentifiers = [];
        for (let i = 0, count = allTouches.length; i < count; ++i)
            existingIdentifiers.push(allTouches[i].identifier);

        this._targetTouches = this._targetTouches.filter(function(touch) {
            return existingIdentifiers.indexOf(touch.identifier) !== -1;
        });
    }

    _updateKeyboardModifiers(event)
    {
        this.modifierKeys.alt = event.altKey;
        this.modifierKeys.ctrl = event.ctrlKey;
        this.modifierKeys.meta = event.metaKey;
        this.modifierKeys.shift = event.shiftKey;
    }

}

GestureRecognizer.SupportsTouches = "createTouch" in document;
GestureRecognizer.SupportsGestures = !!window.GestureEvent;

GestureRecognizer.States = {
    Possible   : "possible",
    Began      : "began",
    Changed    : "changed",
    Ended      : "ended",
    Cancelled  : "cancelled",
    Failed     : "failed",
    Recognized : "ended"
};

GestureRecognizer.Events = {
    TouchStart     : GestureRecognizer.SupportsTouches ? "touchstart" : "mousedown",
    TouchMove      : GestureRecognizer.SupportsTouches ? "touchmove" : "mousemove",
    TouchEnd       : GestureRecognizer.SupportsTouches ? "touchend" : "mouseup",
    TouchCancel    : "touchcancel",
    GestureStart   : "gesturestart",
    GestureChange  : "gesturechange",
    GestureEnd     : "gestureend"
};
