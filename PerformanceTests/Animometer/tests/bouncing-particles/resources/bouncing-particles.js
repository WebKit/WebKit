function BouncingParticle(stage)
{
    this._stageSize = stage.size;
    this._size = stage.particleSize;
    
    this._position = stage.randomPosition(stage.size.subtract(stage.particleSize));
    this._angle = stage.randomAngle();
    this._velocity = stage.randomVelocity(stage.maxVelocity);
    this._rotater = stage.randomRotater();
}

BouncingParticle.prototype =
{
    get center()
    {
        return this._position.add(this._size.center);
    },
    
    animate: function(timeDelta)
    {
        this._position = this._position.move(this._angle, this._velocity, timeDelta);
        this._rotater.next(timeDelta);

        // If particle is going to move off right side
        if (this._position.x + this._size.x > this._stageSize.x) {
            // If direction is East-South, go West-South.
            if (this._angle >= 0 && this._angle < Math.PI / 2)
                this._angle = Math.PI - this._angle;
            // If angle is East-North, go West-North.
            else if (this._angle > Math.PI / 2 * 3)
                this._angle = this._angle - (this._angle - Math.PI / 2 * 3) * 2;
            // Make sure the particle does not go outside the stage boundaries.
            this._position.x = this._stageSize.x - this._size.x;
        }
        
        // If particle is going to move off left side
        if (this._position.x < 0) {
            // If angle is West-South, go East-South.
            if (this._angle > Math.PI / 2 && this._angle < Math.PI)
                this._angle = Math.PI - this._angle;
            // If angle is West-North, go East-North.
            else if (this._angle > Math.PI && this._angle < Math.PI / 2 * 3)
                this._angle = this._angle + (Math.PI / 2 * 3 - this._angle) * 2;
            // Make sure the particle does not go outside the stage boundaries.
            this._position.x = 0;
        }

        // If particle is going to move off bottom side
        if (this._position.y + this._size.y > this._stageSize.y) {
            // If direction is South, go North.
            if (this._angle > 0 && this._angle < Math.PI)
                this._angle = Math.PI * 2 - this._angle;
            // Make sure the particle does not go outside the stage boundaries.
            this._position.y = this._stageSize.y - this._size.y;
        }

        // If particle is going to move off top side
        if (this._position.y < 0) {
            // If direction is North, go South.
            if (this._angle > Math.PI && this._angle < Math.PI * 2)
                this._angle = this._angle - (this._angle - Math.PI) * 2;
            // Make sure the particle does not go outside the stage boundaries.
            this._position.y = 0;
        }
    }
}

function BouncingParticlesAnimator(benchmark, options)
{
    StageAnimator.call(this, benchmark, options);
};

BouncingParticlesAnimator.prototype = Object.create(StageAnimator.prototype);
BouncingParticlesAnimator.prototype.constructor = BouncingParticlesAnimator;

function BouncingParticlesStage(element, options)
{
    Stage.call(this, element, options);
    
    this.particleSize = new Point(parseInt(options["particleWidth"]) || 10, parseInt(options["particleHeight"]) || 10);
    this._particles = [];

    this.maxVelocity = parseInt(options["maxVelocity"]) || 500;
    this.maxVelocity = Math.max(this.maxVelocity, 100);
}

BouncingParticlesStage.prototype = Object.create(Stage.prototype);
BouncingParticlesStage.prototype.constructor = BouncingParticlesStage;

BouncingParticlesStage.prototype.parseShapeParamters = function(options)
{
    this.shape = options["shape"] || "circle";
    this.fill = options["fill"] || "solid";
    this.clip = options["clip"] || "";
}

BouncingParticlesStage.prototype.animate = function(timeDelta)
{
    this._particles.forEach(function(particle) {
        particle.animate(timeDelta);
    });
}
    
BouncingParticlesStage.prototype.tune = function(count)
{
    if (count == 0)
        return this._particles.length;

    if (count > 0) {
        console.assert(typeof(this.createParticle) == "function");
        for (var i = 0; i < count; ++i)
            this._particles.push(this.createParticle());
        return this._particles.length;
    }

    count = Math.min(-count, this._particles.length);
        
    if (typeof(this.particleWillBeRemoved) == "function") {
        for (var i = 0; i < count; ++i)
            this.particleWillBeRemoved(this._particles[this._particles.length - 1 - i]);
    }

    this._particles.splice(-count, count);
    return this._particles.length;
}

function BouncingParticlesBenchmark(suite, test, options, progressBar)
{
    StageBenchmark.call(this, suite, test, options, progressBar);
}

BouncingParticlesBenchmark.prototype = Object.create(StageBenchmark.prototype);
BouncingParticlesBenchmark.prototype.constructor = BouncingParticlesBenchmark;

BouncingParticlesBenchmark.prototype.createAnimator = function()
{
    return new BouncingParticlesAnimator(this, this._options);
}