(function() {

DOMParticle = Utilities.createSubclass(Particle,
    function(stage)
    {
        this.element = document.createElement("div");
        stage.element.appendChild(this.element);

        Particle.call(this, stage);
    }, {

    reset: function()
    {
        Particle.prototype.reset.call(this);

        this.position = Stage.randomElementInArray(this.stage.emitLocation);

        var angle = Stage.randomInt(0, this.stage.emitSteps) / this.stage.emitSteps * Math.PI * 2 + Stage.dateCounterValue(100) * this.stage.emissionSpin;
        this.velocity = new Point(Math.sin(angle), Math.cos(angle))
            .multiply(Stage.random(.5, 2.5));

        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this.stage.colorOffset = (this.stage.colorOffset + 1) % 360;
        this.element.style.backgroundColor = "hsl(" + this.stage.colorOffset + ", 70%, 45%)";
    },

    move: function()
    {
        this.element.style.transform = "translate(" + this.position.x + "px, " + this.position.y + "px)" + this.rotater.rotateZ();
    }
});

DOMParticleStage = Utilities.createSubclass(ParticlesStage,
    function()
    {
        ParticlesStage.call(this);
    }, {

    initialize: function(benchmark)
    {
        ParticlesStage.prototype.initialize.call(this, benchmark);
        this.emissionSpin = Stage.random(0, 3);
        this.emitSteps = Stage.randomInt(4, 6);
        this.emitLocation = [
            new Point(this.size.x * .25, this.size.y * .333),
            new Point(this.size.x * .5, this.size.y * .25),
            new Point(this.size.x * .75, this.size.y * .333)
        ];
        this.colorOffset = Stage.randomInt(0, 359);
    },

    createParticle: function()
    {
        return new DOMParticle(this);
    },

    willRemoveParticle: function(particle)
    {
        particle.element.remove();
    }
});

DOMParticleBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new DOMParticleStage(), options);
    }
);

window.benchmarkClass = DOMParticleBenchmark;

})();
