function Particle(stage)
{
    this.stage = stage;
    this.rotater = this.stage.randomRotater();
    this.reset();
}

Particle.prototype =
{
    reset: function()
    {
        var randSize = Math.pow(Math.random(), 4) * 25 + 15;
        this.size = new Point(randSize, randSize);
        this.maxPosition = this.stage.size.subtract(this.size);
        this.position = new Point(this.stage.size.x / 2, this.stage.size.y / 4);

        var angle = this.stage.randomInt(0, this.stage.emitSteps) / this.stage.emitSteps * Math.PI * 2 + Date.now()/1000*this.stage.emissionSpin;
        this._velocity = new Point(Math.sin(angle), Math.cos(angle))
            .multiply(this.stage.random(.8, 1.2));
    },

    animate: function(timeDelta)
    {
        this.rotater.next(timeDelta);

        this.position = this.position.add(this._velocity.multiply(timeDelta));
        this._velocity.y += 0.03;

        // If particle is going to move off right side
        var maxX = this.stage.size.x - this.size.x;
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

    initialize: function(benchmark)
    {
        Stage.prototype.initialize.call(this, benchmark);
        this.emissionSpin = this.random(0, 3);
        this.emitSteps = this.randomInt(4, 6);
    },

    animate: function(timeDelta)
    {
        var offset = (Date.now() / 2000) % 1;
        this.element.style.background = [
            "linear-gradient(",
            offset * 360,
            "deg, hsl(",
            offset * 360,
            ", 50%, 80%), hsl(",
            (offset + .1) * 360,
            ", 50%, 80%))"
        ].join("");

        timeDelta /= 4;
        this.particles.forEach(function(particle) {
            particle.animate(timeDelta);
        });
    },

    tune: function(count)
    {
        if (count == 0)
            return this.particles.length;

        if (count > 0) {
            for (var i = 0; i < count; ++i)
                this.particles.push(this.createParticle());
            return this.particles.length;
        }

        count = Math.min(-count, this.particles.length);

        if (typeof(this.willRemoveParticle) == "function") {
            for (var i = 0; i < count; ++i)
                this.willRemoveParticle(this.particles[i]);
        }

        this.particles.splice(0, count);
        return this.particles.length;
    },

    complexity: function()
    {
        // We add one to represent the gradient background.
        return this.particles.length + 1;
    }
});
