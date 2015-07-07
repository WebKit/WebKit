describe("/admin/reprocess-report", function () {
    var simpleReport = [{
        "buildNumber": "1986",
        "buildTime": "2013-02-28T10:12:03",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
                "test": {
                    "metrics": {"FrameRate": { "current": [[1, 2, 3], [4, 5, 6]] }}
                },
            },
        }];

    function addBuilder(report, callback) {
        queryAndFetchAll('INSERT INTO builders (builder_name, builder_password_hash) values ($1, $2)',
            [report[0].builderName, sha256(report[0].builderPassword)], callback);
    }

    it("should add build", function () {
        addBuilder(simpleReport, function () {
            postJSON('/api/report/', simpleReport, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryAndFetchAll('SELECT * FROM builds', [], function (buildRows) {
                    assert.equal(buildRows.length, 1);
                    assert.equal(buildRows[0]['build_number'], 1986);
                    queryAndFetchAll('SELECT * FROM reports', [], function (reportRows) {
                        assert.equal(reportRows.length, 1);
                        assert.equal(reportRows[0]['report_build_number'], 1986);
                        queryAndFetchAll('DELETE FROM builds; SELECT * FROM builds', [], function (buildRows) {
                            assert.equal(buildRows.length, 0);
                            var reportId = reportRows[0]['report_id'];
                            httpGet('/admin/reprocess-report?report=' + reportId, function (response) {
                                assert.equal(response.statusCode, 200);
                                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                                queryAndFetchAll('SELECT * FROM builds', [], function (buildRows) {
                                    assert.equal(buildRows.length, 1);
                                    assert.equal(buildRows[0]['build_number'], 1986);
                                    notifyDone();
                                });
                            });
                        });
                    });
                });
            });
        });
    });

    it("should not duplicate the reprocessed report", function () {
        addBuilder(simpleReport, function () {
            postJSON('/api/report/', simpleReport, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryAndFetchAll('SELECT * FROM reports', [], function (originalReprotRows) {
                    assert.equal(originalReprotRows.length, 1);
                    queryAndFetchAll('DELETE FROM builds', [], function () {
                        httpGet('/admin/reprocess-report?report=' + originalReprotRows[0]['report_id'], function (response) {
                            assert.equal(response.statusCode, 200);
                            assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                            queryAndFetchAll('SELECT * FROM reports', [], function (reportRows) {
                                originalReprotRows[0]['report_committed_at'] = null;
                                reportRows[0]['report_committed_at'] = null;
                                assert.deepEqual(reportRows, originalReprotRows);
                                notifyDone();
                            });
                        });
                    });
                });
            });
        });
    });
});
