try {
    new (function () {
      /x/.constructor.input = /x/.constructor;
      this.constructor();
    });
} catch (e) {
    if (!(e instanceof RangeError))
        throw e;
}