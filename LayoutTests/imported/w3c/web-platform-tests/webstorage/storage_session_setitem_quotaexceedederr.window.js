test(function() {
    sessionStorage.clear();

    var index = 0;
    var key = "name";
    var val = "x".repeat(1024);

    assert_throws_quotaexceedederror(function() {
        while (true) {
            index++;
            sessionStorage.setItem("" + key + index, "" + val + index);
        }
    }, null, null);

    sessionStorage.clear();
}, "Throws QuotaExceededError when the quota has been exceeded");
