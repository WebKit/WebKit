if (this.document === undefined) {
  importScripts("/resources/testharness.js");
  importScripts("../resources/utils.js");
}

function checkFetchResponse(url, data, mime, size, desc) {
  promise_test(function(test) {
    size = size.toString();
    return fetch(url).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_equals(resp.type, "basic", "response type is basic");
      assert_equals(resp.headers.get("Content-Type"), mime, "Content-Type is " + resp.headers.get("Content-Type"));
      assert_equals(resp.headers.get("Content-Length"), size, "Content-Length is " + resp.headers.get("Content-Length"));
      return resp.text();
    }).then(function(bodyAsText) {
      assert_equals(bodyAsText, data, "Response's body is " + data);
    })
  }, desc);
}

var blob = new Blob(["Blob's data"], { "type" : "text/plain" });
checkFetchResponse(URL.createObjectURL(blob), "Blob's data", "text/plain",  blob.size, "Regular Blob loading");

function checkKoUrl(url, method, desc) {
  promise_test(function(test) {
    var promise = fetch(url, {"method": method});
    return promise_rejects(test, new TypeError(), promise);
  }, desc);
}

var blob2 = new Blob(["Blob's data"], { "type" : "text/plain" });
checkKoUrl("blob:http://{{domains[www]}}:{{ports[http][0]}}/", "GET", "Loading an erroneous blob scheme URL");
checkKoUrl(URL.createObjectURL(blob2), "POST", "Loading a blob URL using POST");

done();
