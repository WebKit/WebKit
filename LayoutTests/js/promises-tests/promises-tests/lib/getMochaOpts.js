"use strict";

module.exports = function getMochaOpts(args) {
    var rawOpts = args;
    var opts = {};

    rawOpts.join(" ").split("--").forEach(function (opt) {
        var optSplit = opt.split(" ");

        var key = optSplit[0];
        var value = optSplit[1] || true;

        if (key) {
            opts[key] = value;
        }
    });

    return opts;
};
