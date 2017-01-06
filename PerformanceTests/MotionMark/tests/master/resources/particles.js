function Particle(stage)
{
    this.stage = stage;
    this.rotater = Stage.randomRotater();
    this.reset();
    this.move();
}

Particle.prototype =
{
    sizeMinimum: 40,
    sizeRange: 10,

    reset: function()
    {
        var randSize = Math.round(Math.pow(Pseudo.random(), 4) * this.sizeRange + this.sizeMinimum);
        this.size = new Point(randSize, randSize);
        this.minPosition = this.size.center;
        this.maxPosition = this.stage.size.subtract(this.minPosition);
    },

    animate: function(timeDelta)
    {
        this.rotater.next(timeDelta);

        this.position = this.position.add(this.velocity.multiply(timeDelta));
        this.velocity.y += 0.03;

        // If particle is going to move off right side
        if (this.position.x > this.maxPosition.x) {
            if (this.velocity.x > 0)
                this.velocity.x *= -1;
            this.position.x = this.maxPosition.x;
        } else if (this.position.x < this.minPosition.x) {
            // If particle is going to move off left side
            if (this.velocity.x < 0)
                this.velocity.x *= -1;
            this.position.x = this.minPosition.x;
        }

        // If particle is going to move off bottom side
        if (this.position.y > this.maxPosition.y) {
            // Adjust direction but maintain magnitude
            var magnitude = this.velocity.length();
            this.velocity.x *= 1.5 + .005 * this.size.x;
            this.velocity = this.velocity.normalize().multiply(magnitude);
            if (Math.abs(this.velocity.y) < 0.7)
                this.reset();
            else {
                if (this.velocity.y > 0)
                    this.velocity.y *= -0.999;
                this.position.y = this.maxPosition.y;
            }
        } else if (this.position.y < this.minPosition.y) {
            // If particle is going to move off top side
            var magnitude = this.velocity.length();
            this.velocity.x *= 1.5 + .005 * this.size.x;
            this.velocity = this.velocity.normalize().multiply(magnitude);
            if (this.velocity.y < 0)
                this.velocity.y *= -0.998;
            this.position.y = this.minPosition.y;
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

    animate: function(timeDelta)
    {
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
        return this.particles.length;
    }
});
