// META: script=/common/get-host-info.sub.js

promise_test(() => fetch("resources/access-control-expose-headers.json").then(res => res.json()).then(runTests), "Loading JSON…");

function runTests(allTestData) {
  allTestData.forEach(testData => {
    const encodedInput = encodeURIComponent(testData.input);
    promise_test(() => {
      const relativeURL = "cors/resources/expose-headers.py?expose=" + encodedInput,
            url = new URL(relativeURL, get_host_info().HTTP_REMOTE_ORIGIN);
      return fetch(url).then(res => {
        assert_equals(res.headers.get("content-language"), "mkay");
        assert_equals(res.headers.get("bb-8"), (testData.exposed ? "hey" : null));
      });
    }, "Parsing: " + encodedInput);
  })
}
