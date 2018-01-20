if (this.document === undefined) {
  importScripts("/resources/testharness.js");
  importScripts("/common/get-host-info.sub.js")
}

function redirectMode(desc, redirectUrl, redirectLocation, redirectStatus, redirectMode, corsMode) {
  var url = redirectUrl;
  var urlParameters = "?redirect_status=" + redirectStatus;
  urlParameters += "&location=" + encodeURIComponent(redirectLocation);

  var requestInit = {"redirect": redirectMode, mode: corsMode};

  promise_test(function(test) {
    if (redirectMode === "error" || (corsMode === "no-cors" && redirectMode !== "follow"))
      return promise_rejects(test, new TypeError(), fetch(url + urlParameters, requestInit));
    if (redirectMode === "manual")
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, 0, "Response's status is 0");
        assert_equals(resp.type, "opaqueredirect", "Response's type is opaqueredirect");
        assert_equals(resp.statusText, "", "Response's statusText is \"\"");
        assert_equals(resp.url, url + urlParameters, "Response URL should be the original one");
      });
    if (redirectMode === "follow")
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_true(new URL(resp.url).pathname.endsWith(locationUrl), "Response's url should be the redirected one");
        assert_equals(resp.status, 200, "Response's status is 200");
      });
    assert_unreached(redirectMode + " is no a valid redirect mode");
  }, desc);
}

var redirUrl = get_host_info().HTTP_ORIGIN + "/fetch/api/resources/redirect.py";
var locationUrl = "top.txt";

for (var statusCode of [301, 302, 303, 307, 308]) {
  redirectMode("Redirect " + statusCode + " in \"error\" redirect, cors mode", redirUrl, locationUrl, statusCode, "error", "cors");
  redirectMode("Redirect " + statusCode + " in \"follow\" redirect, cors mode", redirUrl, locationUrl, statusCode, "follow", "cors");
  redirectMode("Redirect " + statusCode + " in \"manual\" redirect, cors mode", redirUrl, locationUrl, statusCode, "manual", "cors");
}
redirectMode("Redirect in \"error\" redirect, no cors mode", redirUrl, locationUrl, 301, "error", "no-cors");
redirectMode("Redirect in \"manual\" redirect, no cors mode", redirUrl, locationUrl, 301, "manual", "no-cors");

done();
