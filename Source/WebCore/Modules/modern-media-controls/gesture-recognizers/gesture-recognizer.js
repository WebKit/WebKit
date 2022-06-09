class GestureRecognizer
{

    constructor(target = null, delegate = null)
    {
        this._targetPointers = new Map;

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
        return this._targetPointers.size;
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
        const count = this._targetPointers.size;
        if (!count)
            return p;
        this._targetPointers.forEach(function (pointer) {
            p.x += pointer.pageX;
            p.y += pointer.pageY;
        });
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
        const count = this._targetPointers.size;
        if (!count)
            return p;
        this._targetPointers.forEach(function (pointer) {
            p.x += pointer.clientX;
            p.y += pointer.clientY;
        });
        p.x /= count;
        p.y /= count;

        return p;
    }

    touchesBegan(event)
    {
        if (event.currentTarget !== this._target)
            return;

        window.addEventListener(GestureRecognizer.Events.PointerMove, this, true);
        window.addEventListener(GestureRecognizer.Events.PointerUp, this, true);
        window.addEventListener(GestureRecognizer.Events.PointerCancel, this, true);
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
        case GestureRecognizer.Events.PointerDown:
            this.touchesBegan(event);
            break;
        case GestureRecognizer.Events.PointerMove:
            this.touchesMoved(event);
            break;
        case GestureRecognizer.Events.PointerUp:
            this.touchesEnded(event);
            break;
        case GestureRecognizer.Events.PointerCancel:
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
            this._target.addEventListener(GestureRecognizer.Events.PointerDown, this);
            if (GestureRecognizer.SupportsGestures)
                this._target.addEventListener(GestureRecognizer.Events.GestureStart, this);
        } else {
            this._target.removeEventListener(GestureRecognizer.Events.PointerDown, this);
            if (GestureRecognizer.SupportsGestures)
                this._target.removeEventListener(GestureRecognizer.Events.GestureStart, this);
        }
    }

    _removeTrackingListeners()
    {
        window.removeEventListener(GestureRecognizer.Events.PointerMove, this, true);
        window.removeEventListener(GestureRecognizer.Events.PointerUp, this, true);
        window.removeEventListener(GestureRecognizer.Events.PointerCancel, this, true);
        this._target.removeEventListener(GestureRecognizer.Events.GestureChange, this, true);
        this._target.removeEventListener(GestureRecognizer.Events.GestureEnd, this, true);

        this._targetPointers = new Map;
    }

    _updateTargetTouches(event)
    {
        if (!(event instanceof PointerEvent))
            return;

        if (event.type === GestureRecognizer.Events.PointerDown) {
            this._targetPointers.set(event.pointerId, event);
            return;
        }

        if (event.type === GestureRecognizer.Events.PointerMove) {
            this._targetPointers.set(event.pointerId, event);
            return;
        }

        this._targetPointers.delete(event.pointerId);
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
    PointerDown    : "pointerdown",
    PointerMove    : "pointermove",
    PointerUp      : "pointerup",
    PointerCancel  : "pointercancel",
    GestureStart   : "gesturestart",
    GestureChange  : "gesturechange",
    GestureEnd     : "gestureend"
};
