TestPage.registerInitializer(() => {
    WI.TestView = class TestView extends WI.View
    {
        constructor()
        {
            super();

            this._layoutCallbacks = [];
            this._initialLayoutCount = 0;
            this._layoutCount = 0;
        }

        // Public

        get initialLayoutCount() { return this._initialLayoutCount; }
        get layoutCount() { return this._layoutCount; }

        evaluateAfterLayout(callback)
        {
            this._layoutCallbacks.push(callback);
        }

        // Protected

        initialLayout()
        {
            this._initialLayoutCount++;
        }

        layout()
        {
            this._layoutCount++;
        }

        didLayoutSubtree()
        {
            let callbacks = this._layoutCallbacks;
            this._layoutCallbacks = [];
            for (let callback of callbacks)
                callback();
        }
    };
});
