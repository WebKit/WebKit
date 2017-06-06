
const MOVE_TOLERANCE = GestureRecognizer.SupportsTouches ? 40 : 0;
const WAITING_FOR_NEXT_TAP_TO_START_TIMEOUT = 350;
const WAITING_FOR_TAP_COMPLETION_TIMEOUT = 750;

class TapGestureRecognizer extends GestureRecognizer
{

    constructor(target, delegate)
    {
        super(target, delegate);

        this.numberOfTapsRequired = 1;
        this.numberOfTouchesRequired = 1;
        this.allowsRightMouseButton = false;
    }

    // Protected

    touchesBegan(event)
    {
        if (event.currentTarget !== this.target)
            return;

        if (event.button === 2 && !this.allowsRightMouseButton)
            return;

        super.touchesBegan(event);

        if (this.numberOfTouches !== this.numberOfTouchesRequired) {
            this.enterFailedState();
            return;
        }

        this._startPoint = super.locationInElement();
        this._startClientPoint = super.locationInClient();

        this._rewindTimer(WAITING_FOR_TAP_COMPLETION_TIMEOUT);
    }

    touchesMoved(event)
    {
        const touchLocation = super.locationInElement();
        const distance = Math.sqrt(Math.pow(this._startPoint.x - touchLocation.x, 2) + Math.pow(this._startPoint.y - touchLocation.y, 2));
        if (distance > MOVE_TOLERANCE)
            this.enterFailedState();
    }

    touchesEnded(event)
    {
        this._taps++;

        if (this._taps === this.numberOfTapsRequired) {
            // We call prevent default here to override the potential double-tap-to-zoom
            // behavior of the browser.
            event.preventDefault();

            this.enterRecognizedState();
            this.reset();
        }

        this._rewindTimer(WAITING_FOR_NEXT_TAP_TO_START_TIMEOUT);
    }

    reset()
    {
        this._taps = 0;
        this._clearTimer();
    }

    locationInElement(element)
    {
        const p = this._startPoint || new DOMPoint;

        if (!element)
            return p;

        // FIXME: are WebKitPoint and DOMPoint interchangeable?
        const wkPoint = window.webkitConvertPointFromPageToNode(element, new WebKitPoint(p.x, p.y));
        return new DOMPoint(wkPoint.x, wkPoint.y);
    }

    locationInClient()
    {
        return this._startClientPoint || new DOMPoint;
    }

    // Private

    _clearTimer()
    {
        window.clearTimeout(this._timerId);
        delete this._timerId;
    }

    _rewindTimer(timeout)
    {
        this._clearTimer();
        this._timerId = window.setTimeout(this._timerFired.bind(this), timeout);
    }

    _timerFired()
    {
        this.enterFailedState();
    }

}
