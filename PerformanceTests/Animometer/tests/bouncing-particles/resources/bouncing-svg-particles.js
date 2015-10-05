function BouncingSvgParticle(stage)
{
    BouncingParticle.call(this, stage);
}

BouncingSvgParticle.prototype = Object.create(BouncingParticle.prototype);
BouncingSvgParticle.prototype.constructor = BouncingParticle;

BouncingSvgParticle.prototype._applyClipping = function(stage)
{
    if (stage.clip != "star")
        return;
        
    stage.ensureClipStarIsCreated();
    this.element.setAttribute("clip-path", "url(#star-clip)");
}

BouncingSvgParticle.prototype._move = function()
{
    var transform = "translate(" + this._position.x + ", " + this._position.y + ")";
    if (this._shape != "circle")
        transform += this._rotater.rotate(this._size.center);
    this.element.setAttribute("transform", transform);
}

BouncingSvgParticle.prototype.animate = function(timeDelta)
{
    BouncingParticle.prototype.animate.call(this, timeDelta);
    this._move();
}

function BouncingSvgParticlesStage(element, options)
{
    BouncingParticlesStage.call(this, element, options);
}

BouncingSvgParticlesStage.prototype = Object.create(BouncingParticlesStage.prototype);
BouncingSvgParticlesStage.prototype.constructor = BouncingSvgParticlesStage;

BouncingSvgParticlesStage.prototype._createDefs = function()
{
    return Utilities.createSvgElement("defs", {}, {}, this.element);
}
                                                               
BouncingSvgParticlesStage.prototype._ensureDefsIsCreated = function()
{
    return this.element.querySelector("defs") || this._createDefs();
}

BouncingSvgParticlesStage.prototype._createClipStar = function()
{
    var attrs = { id: "star-clip", clipPathUnits: "objectBoundingBox" };
    var clipPath  = Utilities.createSvgElement("clipPath", attrs, {}, this._ensureDefsIsCreated());

    attrs = { d: "M.50,0L.38,.38L0,.38L.30,.60L.18,1L.50,.75L.82,1L.70,.60L1,.38L.62,.38z" };
    Utilities.createSvgElement("path", attrs, {}, clipPath);
    return clipPath;
}

BouncingSvgParticlesStage.prototype.ensureClipStarIsCreated = function()
{
    return this.element.querySelector("#star-clip") || this._createClipStar();
}

BouncingSvgParticlesStage.prototype.particleWillBeRemoved = function(particle)
{
    particle.element.remove();
}
