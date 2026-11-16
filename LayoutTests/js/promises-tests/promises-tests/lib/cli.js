#!/usr/bin/env node
"use strict";

var path = require("path");
var getMochaOpts = require("./getMochaOpts");
var programmaticRunner = require("./programmaticRunner");

var filePath = getAdapterFilePath();
var adapter = adapterObjectFromFilePath(filePath);
var mochaOpts = getMochaOpts(process.argv.slice(3));
programmaticRunner(adapter, mochaOpts, function (err) {
    if (err) {
        process.exit(err.failures || -1);
    }
});

function getAdapterFilePath() {
    if (process.argv[2]) {
        return path.join(process.cwd(), process.argv[2]);
    } else {
        throw new Error("Specify your adapter file as an argument.");
    }
}

function adapterObjectFromFilePath(filePath) {
    try {
        return require(filePath);
    } catch (e) {
        var error = new Error("Error `require`ing adapter file " + filePath + "\n\n" + e);
        error.cause = e;

        throw error;
    }
}
