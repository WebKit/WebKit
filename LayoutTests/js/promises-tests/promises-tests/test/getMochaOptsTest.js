"use strict";

var assert = require("assert");
var getMochaOpts = require("../lib/getMochaOpts");

describe("getMochaOpts", function() {
    function test(argsString, expectedOpts) {
        var opts = getMochaOpts(argsString.split(" "));
        assert.deepEqual(opts, expectedOpts);
    }

    it("parses the provided options to an object", function () {
        test("--reporter spec --ui bdd", {
            reporter: "spec",
            ui: "bdd"
        });
    });

    it("sets the value for no-arg options to true", function () {
        test("--bail --ui bdd", {
          bail: true,
          ui: "bdd"
        });
    });

    it("supports no-arg options as the last option", function () {
        test("--reporter spec --bail", {
          reporter: "spec",
          bail: true
        });
    });
});
