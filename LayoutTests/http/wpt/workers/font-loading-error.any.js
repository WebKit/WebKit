promise_test(async () => {
    return new FontFace("ABC", "url(data:font/woff2,abcd) format('woff2')").load().catch(e => {
        assert_equals(e.name, "NetworkError");
    });
}, "Data URL should not trigger CORS errors and if failing should reject the promise.");
