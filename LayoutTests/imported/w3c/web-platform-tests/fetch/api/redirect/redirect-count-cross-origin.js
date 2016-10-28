if (this.document === undefined) {
  importScripts("/resources/testharness.js");
  importScripts("../resources/utils.js");
  importScripts("/common/utils.js");
  importScripts("/common/get-host-info.sub.js");
}

function redirectCount(desc, redirectUrl, redirectLocation, redirectStatus, maxCount1, maxCount2, requirePreflight)
{
  var allow_headers = "";
  var requestInit = {"redirect": "follow"};
  if (requirePreflight) {
      requestInit.headers = [["x-test", "test"]];
      allow_headers = "&allow_headers=x-test";
  }

  var uuid_token = token();

  var urlParameters = "?token=" + uuid_token + "&max_age=0";
  urlParameters += "&redirect_status=" + redirectStatus;
  urlParameters += "&max_count=" + maxCount1;
  urlParameters += allow_headers;
  urlParameters += "&location=" + encodeURIComponent(redirectLocation + "?token=" + uuid_token + "&max_age=0&max_count=" + (maxCount1 + maxCount2) + allow_headers);

  var maxCount = maxCount1 + maxCount2;
  var url = redirectUrl;
  promise_test(function(test) {
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(resp) {
      assert_equals(resp.status, 200, "Clean stash response's status is 200");

      if (maxCount > 20)
        return promise_rejects(test, new TypeError(), fetch(url + urlParameters, requestInit));

      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, 200, "Response's status is 200");
        return resp.text();
      }).then(function(body) {
        assert_equals(body, maxCount.toString(), "Redirected " + maxCount + " times");
      });
    });
  }, desc);
}

var redirUrl = "/fetch/api/resources/redirect-count.py";
var crossOriginRedirUrl = get_host_info().HTTP_ORIGIN_WITH_DIFFERENT_PORT + redirUrl;

redirectCount("Redirect 20 times, last redirection to cross-origin", redirUrl, crossOriginRedirUrl, 301, 19, 1);
redirectCount("Redirect 21 times, last redirection to cross-origin", redirUrl, crossOriginRedirUrl, 301, 20, 1);

redirectCount("Redirect 20 times, going to cross-origin after 10", redirUrl, crossOriginRedirUrl, 302, 10, 10);
redirectCount("Redirect 21 times, going to cross-origin after 10", redirUrl, crossOriginRedirUrl, 302, 10, 11);

redirectCount("Redirect 20 times, last redirection to cross-origin with preflight", redirUrl, crossOriginRedirUrl, 301, 19, 1, true);
redirectCount("Redirect 21 times, last redirection to cross-origin with preflight", redirUrl, crossOriginRedirUrl, 301, 20, 1, true);

redirectCount("Redirect 20 times, going to cross-origin after 10 with preflight", redirUrl, crossOriginRedirUrl, 303, 10, 10, true);
redirectCount("Redirect 21 times, going to cross-origin after 10 with preflight", redirUrl, crossOriginRedirUrl, 303, 10, 11, true);

done();
