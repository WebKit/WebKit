
const scheduler = new class
{

    constructor()
    {
        this._frameID = -1;
        this._layoutCallbacks = new Set;
        this.debug = new Function;
    }

    // Public

    get hasScheduledLayoutCallbacks()
    {
        return this._frameID !== -1 || this._layoutCallbacks.size > 0;
    }

    scheduleLayout(callback)
    {
        this.debug("scheduleLayout() - start");
        if (typeof callback !== "function") {
            this.debug("scheduleLayout() - end - callback was not a function");
            return;
        }

        this._layoutCallbacks.add(callback);
        this._requestFrameIfNeeded();
        this.debug(`scheduleLayout() - _layoutCallbacks.size = ${this._layoutCallbacks.size}`);
        this.debug("scheduleLayout() - end");
    }

    unscheduleLayout(callback)
    {
        this.debug("unscheduleLayout() - start");
        if (typeof callback !== "function") {
            this.debug("unscheduleLayout() - end - callback was not a function");
            return;
        }

        this._layoutCallbacks.delete(callback);
        this.debug(`unscheduleLayout() - this._layoutCallbacks.size = ${this._layoutCallbacks.size}`);
        this.debug("unscheduleLayout() - end");
    }

    flushScheduledLayoutCallbacks()
    {
        this._frameDidFire();
    }

    // Private

    _requestFrameIfNeeded()
    {
        this.debug("_requestFrameIfNeeded()");
        if (this._frameID === -1 && this._layoutCallbacks.size > 0) {
            this._frameID = window.requestAnimationFrame(this._frameDidFire.bind(this));
            this.debug(`_requestFrameIfNeeded() - registered rAF, _frameID = ${this._frameID}`);
        } else
            this.debug(`_requestFrameIfNeeded() - failed to register rAF call, _frameID = ${this._frameID}, _layoutCallbacks.size = ${this._layoutCallbacks.size}`);
    }

    _frameDidFire()
    {
        this.debug("_frameDidFire() - start");
        if (typeof scheduler.frameWillFire === "function")
            scheduler.frameWillFire();

        this._layout();
        this._frameID = -1;
        this._requestFrameIfNeeded();

        if (typeof scheduler.frameDidFire === "function")
            scheduler.frameDidFire();
        this.debug("_frameDidFire() - end");
    }

    _layout()
    {
        this.debug("_layout() - start");
        // Layouts are not re-entrant.
        const layoutCallbacks = this._layoutCallbacks;
        this._layoutCallbacks = new Set;

        this.debug(`_layout() - layoutCallbacks.size = ${layoutCallbacks.size}`);

        for (let callback of layoutCallbacks)
            callback();
        this.debug("_layout() - end");
    }

};
