var global = (typeof window !== 'undefined' && window) || (typeof self !== 'undefined' && self) || (typeof global !== 'undefined' && global) || {};

if (!global.FormData) {
  global.FormData = function () {
    return {
      append: function () { }
    };
  };
}

module.exports = global;