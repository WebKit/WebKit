'use strict';

var assert = require('assert');
var fs = require('fs');

var Config = require('../tools/js/config.js');
var Database = require('../tools/js/database.js');

describe('config.json', function () {
    it('should be a valid file', function () {
        assert.doesNotThrow(function () {
            fs.readFileSync(Config.configFilePath())
        });
    });

    it('should be a valid JSON', function () {
        assert.doesNotThrow(function () {
            JSON.parse(fs.readFileSync(Config.configFilePath()));
        });
    });

    it('should define `siteTitle`', function () {
        assert.equal(typeof Config.value('siteTitle'), 'string');
    });

    it('should define `dataDirectory`', function () {
        assert.ok(Config.value('dataDirectory'));
        assert.ok(fs.existsSync(Config.path('dataDirectory')), 'dataDirectory should exist');
        assert.ok(fs.statSync(Config.path('dataDirectory')).isDirectory(), 'dataDirectory should be a dictionary');
    });

    it('should define `jsonCacheMaxAge`', function () {
        assert.equal(typeof Config.value('jsonCacheMaxAge'), 'number');
    });

    it('should define `jsonCacheMaxAge`', function () {
        assert.equal(typeof Config.value('jsonCacheMaxAge'), 'number');
    });

    it('should define `clusterStart`', function () {
        var clusterStart = Config.value('clusterStart');
        assert.ok(clusterStart instanceof Array);
        assert.equal(clusterStart.length, [2000, 1, 1, 0, 0].length,
            'Must specify year, month, date, hour, and minute');
        var maxYear = (new Date).getFullYear() + 1;
        assert.ok(clusterStart[0] >= 1970 && clusterStart[0] <= maxYear, `year must be between 1970 and ${maxYear}`);
        assert.ok(clusterStart[1] >= 1 && clusterStart[1] <= 12, 'month must be between 1 and 12');
        assert.ok(clusterStart[2] >= 1 && clusterStart[2] <= 31, 'date must be between 1 and 31');
        assert.ok(clusterStart[3] >= 0 && clusterStart[3] <= 60, 'minute must be between 0 and 60');
        assert.ok(clusterStart[4] >= 0 && clusterStart[4] <= 60, 'minute must be between 0 and 60');
    });

    it('should define `clusterSize`', function () {
        var clusterSize = Config.value('clusterSize');
        assert.ok(clusterSize instanceof Array);
        assert.equal(clusterSize.length, [0, 2, 0].length,
            'Must specify the number of years, months, and days');
        assert.equal(typeof clusterSize[0], 'number', 'the number of year must be a number');
        assert.equal(typeof clusterSize[1], 'number', 'the number of month must be a number');
        assert.equal(typeof clusterSize[2], 'number', 'the number of days must be a number');
    });

    describe('`dashboards`', function () {
        var dashboards = Config.value('dashboards');

        it('should exist for v2 and v3 UI', function () {
            assert.equal(typeof dashboards, 'object');
        });

        it('dashboard names that do not contain /', function () {
            for (var name in dashboards)
                assert.ok(name.indexOf('/') < 0, 'Dashboard name "${name}" should not contain "/"');
        });

        it('each dashboard must be an array', function () {
            for (var name in dashboards)
                assert.ok(dashboards[name] instanceof Array);
        });

        it('each row in a dashboard must be an array', function () {
            for (var name in dashboards) {
                for (var row of dashboards[name]) {
                    console.assert(row instanceof Array);
                }
            }
        });

        it('each cell in a dashboard must be an array or a string', function () {
            for (var name in dashboards) {
                for (var row of dashboards[name]) {
                    for (var cell of row) {
                        if (cell instanceof Array)
                            assert.ok(cell.length == 0 || cell.length == 2,
                                'Each cell must be empty or specify [platform, metric] pair');
                        else
                            assert.equal(typeof cell, 'string');
                    }
                }
            }
        });

    });

    describe('`database`', function () {
        it('should exist', function () {
            assert.ok(Config.value('database'));
        });

        it('should define `database.host`', function () {
            assert.equal(typeof Config.value('database.host'), 'string');
        });

        it('should define `database.port`', function () {
            assert.equal(typeof Config.value('database.port'), 'string');
        });

        it('should define `database.username`', function () {
            assert.equal(typeof Config.value('database.username'), 'string');
        });

        it('should define `database.password`', function () {
            assert.equal(typeof Config.value('database.password'), 'string');
        });

        it('should define `database.name`', function () {
            assert.equal(typeof Config.value('database.name'), 'string');
        });

        it('should be able to connect to the database', function (done) {
            let database = new Database;
            return database.connect().then(function () {
                database.disconnect();
                done();
            }, function (error) {
                database.disconnect();
                done(error);
            });
        });
    });

    describe('optional configurations', function () {
        function assertNullOrType(value, type) {
            if (value !== null)
                assert.equal(typeof value, type);
        }

        it('`debug` should be `null` or a boolean', function () {
            assertNullOrType(Config.value('debug'), 'boolean');
        });

        it('`maintenanceMode` should be `null` or a boolean', function () {
            assertNullOrType(Config.value('maintenanceMode'), 'boolean');
        });

        it('`maintenanceDirectory` should be `null` or a string', function () {
            assertNullOrType(Config.value('maintenanceDirectory'), 'string');
        });

        it('`maintenanceDirectory` should be a string if `maintenanceMode` is true', function () {
            if (Config.value('maintenanceMode'))
                assert.equal(Config.value('maintenanceDirectory'), 'string');
        });

        it('`universalSlavePassword` should be `null` or a string', function () {
            assertNullOrType(Config.value('universalSlavePassword'), 'string');
        });
    });
});
