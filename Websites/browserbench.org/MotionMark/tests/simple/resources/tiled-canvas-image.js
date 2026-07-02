(function() {

CanvasImageTile = Utilities.createClass(
    function(stage, source)
    {
        this._context = stage.context;
        this._size = stage.tileSize;
        this.source = source;
    }, {

    getImageData: function()
    {
        this._imagedata = this._context.getImageData(this.source.x, this.source.y, this._size.width, this._size.height);
    },

    putImageData: function(destination)
    {
        this._context.putImageData(this._imagedata, destination.x, destination.y);
    }
});

TiledCanvasImageStage = Utilities.createSubclass(Stage,
    function(element, options)
    {
        Stage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);
        this.context = this.element.getContext("2d");
        this._setupTiles();
    },

    _setupTiles: function()
    {
        const maxTilesPerRow = 50;
        const maxTilesPerCol = 50;

        this.tileSize = this.size.multiply(new Point(1 / maxTilesPerRow, 1 / maxTilesPerCol));

        this._tiles = new Array(maxTilesPerRow * maxTilesPerCol);

        var source = Point.zero;
        for (var index = 0; index < this._tiles.length; ++index) {
            this._tiles[index] = new CanvasImageTile(this, source);
            source = this._nextTilePosition(source);
        }

        this._ctiles = 0;
    },

    _nextTilePosition: function(destination)
    {
        var next = destination.add(this.tileSize);

        if (next.x >= this._size.width)
            return new Point(0, next.y >= this._size.height ? 0 : next.y);

        return new Point(next.x, destination.y);
    },

    tune: function(count)
    {
        this._ctiles += count;

        this._ctiles = Math.max(this._ctiles, 0);
        this._ctiles = Math.min(this._ctiles, this._tiles.length);
    },

    _drawBackground: function()
    {
        var size = this._benchmark._stage.size;
        var gradient = this.context.createLinearGradient(0, 0, size.width, 0);
        gradient.addColorStop(0, "red");
        gradient.addColorStop(1, "white");
        this.context.save();
            this.context.fillStyle = gradient;
            this.context.fillRect(0, 0, size.width, size.height);
        this.context.restore();
    },

    animate: function(timeDelta)
    {
        this._drawBackground();

        if (!this._ctiles)
            return;

        this._tiles.shuffle();

        var destinations = new Array(this._ctiles);
        for (var index = 0; index < this._ctiles; ++index) {
            this._tiles[index].getImageData();
            destinations[index] = this._tiles[index].source;
        }

        destinations.shuffle();

        for (var index = 0; index < this._ctiles; ++index)
            this._tiles[index].putImageData(destinations[index]);
    },

    complexity: function()
    {
        return this._ctiles;
    }
});

TiledCanvasImageBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new TiledCanvasImageStage(), options);
    }
);

window.benchmarkClass = TiledCanvasImageBenchmark;

})();
