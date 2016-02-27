function Particle(stage)
{
    this.stage = stage;
    this.rotater = Stage.randomRotater();
    this.reset();
    this.move();
}

Particle.prototype =
{
    reset: function()
    {
        var randSize = Math.pow(Pseudo.random(), 4) * 10 + 40;
        this.size = new Point(randSize, randSize);
        this.maxPosition = this.stage.size.subtract(this.size);

        var emitLocation = this.stage.emitLocation[Stage.randomInt(0, this.stage.emitLocation.length - 1)];
        this.position = new Point(emitLocation.x, emitLocation.y);

        var angle = Stage.randomInt(0, this.stage.emitSteps) / this.stage.emitSteps * Math.PI * 2 + Stage.dateCounterValue(100) * this.stage.emissionSpin;
        this._velocity = new Point(Math.sin(angle), Math.cos(angle))
            .multiply(Stage.random(.5, 2.5));
    },

    animate: function(timeDelta)
    {
        this.rotater.next(timeDelta);

        this.position = this.position.add(this._velocity.multiply(timeDelta));
        this._velocity.y += 0.03;

        // If particle is going to move off right side
        if (this.position.x > this.maxPosition.x) {
            if (this._velocity.x > 0)
                this._velocity.x *= -1;
            this.position.x = this.maxPosition.x;
        } else if (this.position.x < 0) {
            // If particle is going to move off left side
            if (this._velocity.x < 0)
                this._velocity.x *= -1;
            this.position.x = 0;
        }

        // If particle is going to move off bottom side
        if (this.position.y > this.maxPosition.y) {
            // Adjust direction but maintain magnitude
            var magnitude = this._velocity.length();
            this._velocity.x *= 1.5 + .005 * this.size.x;
            this._velocity = this._velocity.normalize().multiply(magnitude);
            if (Math.abs(this._velocity.y) < 0.7)
                this.reset();
            else {
                if (this._velocity.y > 0)
                    this._velocity.y *= -0.999;
                this.position.y = this.maxPosition.y;
            }
        } else if (this.position.y < 0) {
            // If particle is going to move off top side
            var magnitude = this._velocity.length();
            this._velocity.x *= 1.5 + .005 * this.size.x;
            this._velocity = this._velocity.normalize().multiply(magnitude);
            if (this._velocity.y < 0)
                this._velocity.y *= -0.998;
            this.position.y = 0;
        }

        this.move();
    },

    move: function()
    {
    }
}

ParticlesStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
        this.particles = [];
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);
        this.emissionSpin = Stage.random(0, 3);
        this.emitSteps = Stage.randomInt(4, 6);
        this.emitLocation = [
            new Point(this.size.x * .25, this.size.y * .333),
            new Point(this.size.x * .5, this.size.y * .25),
            new Point(this.size.x * .75, this.size.y * .333)
        ];
    },

    animate: function(timeDelta)
    {
        var offset = Stage.dateFractionalValue();
        timeDelta /= 4;
        this.particles.forEach(function(particle) {
            particle.animate(timeDelta);
        });
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count > 0) {
            for (var i = 0; i < count; ++i)
                this.particles.push(this.createParticle());
            return;
        }

        count = Math.min(-count, this.particles.length);

        if (typeof(this.willRemoveParticle) == "function") {
            for (var i = 0; i < count; ++i)
                this.willRemoveParticle(this.particles[i]);
        }

        this.particles.splice(0, count);
    },

    complexity: function()
    {
        // We add one to represent the gradient background.
        return this.particles.length + 1;
    }
});
