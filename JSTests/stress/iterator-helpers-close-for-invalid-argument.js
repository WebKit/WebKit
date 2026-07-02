function shouldThrow(errorType, func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType)) {
        print(error.message);
        throw new Error(`Expected ${errorType.name}! got ${error.name}`);
    }
}

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

{
    // Iterator.prototype.map
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };
    shouldThrow(TypeError, function() {
      closable.map();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(TypeError, function() {
      closable.map({});
    });
    shouldBe(closed, true);
}

{
    // Iterator.prototype.filter
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };
    shouldThrow(TypeError, function() {
      closable.filter();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(TypeError, function() {
      closable.filter({});
    });
    shouldBe(closed, true);
}

{
    // Iterator.prototype.take
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };

    shouldThrow(RangeError, function() {
      closable.take();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(RangeError, function() {
      closable.take(NaN);
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(RangeError, function() {
      closable.take(-1);
    });
    shouldBe(closed, true);

    closed = false;
    function OurError() {}
    shouldThrow(OurError, function() {
      closable.take({ get valueOf() { throw new OurError(); }});
    });
    shouldBe(closed, true);
}

{
    // Iterator.prototype.drop
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };

    shouldThrow(RangeError, function() {
      closable.drop();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(RangeError, function() {
      closable.drop(NaN);
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(RangeError, function() {
      closable.drop(-1);
    });
    shouldBe(closed, true);

    closed = false;
    function OurError() {}
    shouldThrow(OurError, function() {
      closable.drop({ get valueOf() { throw new OurError(); }});
    });
    shouldBe(closed, true);
}

{
    // Iterator.prototype.flatMap
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };
    shouldThrow(TypeError, function() {
      closable.flatMap();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(TypeError, function() {
      closable.flatMap({});
    });
    shouldBe(closed, true);
}

{
    // Iterator.prototype.some
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };
    shouldThrow(TypeError, function() {
      closable.some();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(TypeError, function() {
      closable.some({});
    });
    shouldBe(closed, true);
}

{
    // Iterator.prototype.every
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };
    shouldThrow(TypeError, function() {
      closable.every();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(TypeError, function() {
      closable.every({});
    });
    shouldBe(closed, true);
}

{
    // Iterator.prototype.find
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };
    shouldThrow(TypeError, function() {
      closable.find();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(TypeError, function() {
      closable.find({});
    });
    shouldBe(closed, true);
}

{
    // Iterator.prototype.reduce
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };
    shouldThrow(TypeError, function() {
      closable.reduce();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(TypeError, function() {
      closable.reduce({});
    });
    shouldBe(closed, true);
}

{
    // Iterator.prototype.forEach
    let closed = false;
    let closable = {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be read');
      },
      return() {
        closed = true;
        return {};
      },
    };
    shouldThrow(TypeError, function() {
      closable.forEach();
    });
    shouldBe(closed, true);

    closed = false;
    shouldThrow(TypeError, function() {
      closable.forEach({});
    });
    shouldBe(closed, true);
}

