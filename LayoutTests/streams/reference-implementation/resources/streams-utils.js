var standardTimeout = 100;

function RandomPushSource(toPush) {
    this.pushed = 0;
    this.toPush = toPush;
    this.started = false;
    this.paused = false;
    this.closed = false;

    this._intervalHandle = null;
}

RandomPushSource.prototype = {

    readStart: function() {
        if (this.closed) {
            return;
        }

        if (!this.started) {
            this._intervalHandle = setInterval(writeChunk, 23);
            this.started = true;
        }

        if (this.paused) {
            this._intervalHandle = setInterval(writeChunk, 23);
            this.paused = false;
        }

        var stream = this;
        function writeChunk() {
            if (stream.paused) {
                return;
            }

            stream.pushed++;

            if (stream.toPush > 0 && stream.pushed > stream.toPush) {
                if (stream._intervalHandle) {
                    clearInterval(stream._intervalHandle);
                    stream._intervalHandle = undefined;
                }
                stream.closed = true;
                stream.onend();
            }
            else {
                stream.ondata(randomChunk(128));
            }
        }
    },

    readStop: function() {
        if (this.paused) {
            return;
        }

        if (this.started) {
            this.paused = true;
            clearInterval(this._intervalHandle);
            this._intervalHandle = undefined;
        } else {
            throw new Error('Can\'t pause reading an unstarted source.');
        }
    },
}

function randomChunk(size) {
    var chunk = '';

    for (var i = 0; i < size; i++) {
        // Add a random character from the basic printable ASCII set.
        chunk += String.fromCharCode(Math.round(Math.random() * 84) + 32)
    }

    return chunk;
}

function readableStreamToArray(readable, reader) {
    var chunks = [];

    if (reader == undefined) {
        reader = readable.getReader();
    }

    return pump();

    function pump() {
        return reader.read().then(function({ value, done }) {
            if (done) {
                return chunks;
            }

            chunks.push(value);
            return pump();
        });
    }
}

function SequentialPullSource(limit, async) {
    this.current = 0;
    this.limit = limit;
    this.opened = false;
    this.closed = false;

    this._exec = function(f) {
        f();
    };
    if (async)
        this._exec = function(f) {
            setTimeout(f, 0);
        };
}

SequentialPullSource.prototype = {

    open: function(cb) {
        var myFunction = function() {
            this.opened = true
            cb();
        };
        this._exec(myFunction.bind(this));
    },

    read: function(cb) {
        var myFunction = function() {
            if (++this.current <= this.limit) {
                cb(null, false, this.current);
            } else {
                cb(null, true, null);
            }
        };
        this._exec(myFunction.bind(this));
    },

    close: function(cb) {
        var myFunction = function() {
            this.closed = true;
            cb();
        };
        this._exec(myFunction.bind(this));
    },
}

function sequentialReadableStream(limit, options) {
    var sequentialSource = new SequentialPullSource(limit, options);

    var stream = new ReadableStream({
        start: function() {
            return new Promise(function(resolve, reject) {
                sequentialSource.open(function(err) {
                    if (err) {
                        reject(err);
                    }
                    resolve();
                });
            });
        },

        pull: function(enqueue, finish, error) {
            sequentialSource.read(function(err, done, chunk) {
                if (err) {
                    error(err);
                } else if (done) {
                    sequentialSource.close(function(err) {
                        if (err) {
                            error(err);
                        }
                        finish();
                    });
                } else {
                    enqueue(chunk);
                }
            });
        },
    });

    stream.source = sequentialSource;

    return stream;
};
