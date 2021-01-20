
const scheduler = new class
{

    constructor()
    {
        this._frameID = -1;
        this._layoutCallbacks = new Set;
    }

    // Public

    get hasScheduledLayoutCallbacks()
    {
        return this._frameID !== -1 || this._layoutCallbacks.size > 0;
    }

    scheduleLayout(callback)
    {
        if (typeof callback !== "function")
            return;

        this._layoutCallbacks.add(callback);
        this._requestFrameIfNeeded();
    }

    unscheduleLayout(callback)
    {
        if (typeof callback !== "function")
            return;

        this._layoutCallbacks.delete(callback);
    }

    flushScheduledLayoutCallbacks()
    {
        this._frameDidFire();
    }

    // Private

    _requestFrameIfNeeded()
    {
        if (this._frameID === -1 && this._layoutCallbacks.size > 0)
            this._frameID = window.requestAnimationFrame(this._frameDidFire.bind(this));
    }

    _frameDidFire()
    {
        if (typeof scheduler.frameWillFire === "function")
            scheduler.frameWillFire();

        this._layout();
        this._frameID = -1;
        this._requestFrameIfNeeded();

        if (typeof scheduler.frameDidFire === "function")
            scheduler.frameDidFire();
    }

    _layout()
    {
        // Layouts are not re-entrant.
        const layoutCallbacks = this._layoutCallbacks;
        this._layoutCallbacks = new Set;

        for (let callback of layoutCallbacks)
            callback();
    }

};
