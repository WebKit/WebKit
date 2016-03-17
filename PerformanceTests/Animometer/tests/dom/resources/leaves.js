Leaf = Utilities.createSubclass(Particle,
    function(stage)
    {
        this.element = document.createElement("img");
        this.element.setAttribute("src", stage.images[Stage.randomInt(0, stage.images.length - 1)].src);
        stage.element.appendChild(this.element);

        Particle.call(this, stage);
    }, {

    sizeMinimum: 20,
    sizeRange: 40,

    reset: function()
    {
        Particle.prototype.reset.call(this);
        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this._opacity = .01;
        this._opacityRate = 0.02 * Stage.random(1, 6);
        this._position = new Point(Stage.random(0, this.maxPosition.x), Stage.random(-this.size.height, this.maxPosition.y));
        this._velocity = new Point(Stage.random(-6, -2), .1 * this.size.y + Stage.random(-1, 1));
    },

    animate: function(timeDelta)
    {
        this.rotater.next(timeDelta);

        this._position.x += this._velocity.x + 8 * this.stage.focusX;
        this._position.y += this._velocity.y;
        this._opacity += this._opacityRate;
        if (this._opacity > 1) {
            this._opacity = 1;
            this._opacityRate *= -1;
        } else if (this._opacity < 0 || this._position.y > this.stage.size.y)
            this.reset();

        if (this._position.x < -this.size.width || this._position.x > this.maxPosition.x)
            this._position.x = (this._position.x + this.maxPosition.x) % this.maxPosition.x;
        this.move();
    },

    move: function()
    {
        this.element.style.transform = "translate(" + this._position.x + "px, " + this._position.y + "px)" + this.rotater.rotateZ();
        this.element.style.opacity = this._opacity;
    }
});
