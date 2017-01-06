BouncingSvgParticle = Utilities.createSubclass(BouncingParticle,
    function(stage, shape)
    {
        BouncingParticle.call(this, stage);
        this._shape = shape;
    }, {

    _applyClipping: function(stage)
    {
        if (stage.clip != "star")
            return;

        stage.ensureClipStarIsCreated();
        this.element.setAttribute("clip-path", "url(#star-clip)");
    },

    _move: function()
    {
        var transform = "translate(" + this.position.x + ", " + this.position.y + ")";
        if (this._shape != "circle")
            transform += this.rotater.rotate(this.size.center);
        this.element.setAttribute("transform", transform);
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this._move();
    }
});

BouncingSvgParticlesStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    _createDefs: function()
    {
        return Utilities.createSVGElement("defs", {}, {}, this.element);
    },

    _ensureDefsIsCreated: function()
    {
        return this.element.querySelector("defs") || this._createDefs();
    },

    _createClipStar: function()
    {
        var attrs = { id: "star-clip", clipPathUnits: "objectBoundingBox" };
        var clipPath  = Utilities.createSVGElement("clipPath", attrs, {}, this._ensureDefsIsCreated());

        attrs = { d: "M.50,0L.38,.38L0,.38L.30,.60L.18,1L.50,.75L.82,1L.70,.60L1,.38L.62,.38z" };
        Utilities.createSVGElement("path", attrs, {}, clipPath);
        return clipPath;
    },

    ensureClipStarIsCreated: function()
    {
        return this.element.querySelector("#star-clip") || this._createClipStar();
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});
