if (this.document === undefined) {
    importScripts("/js-test-resources/testharness.js");
    importScripts("resources/sri-utilities.js");
}

var main_host = '127.0.0.1';
var remote_host = 'localhost';
var port_string = "8000";
var main_host_and_port = main_host + ':' + port_string;
var remote_host_and_port = remote_host + ':' + port_string;

var resource = "resources/resource.txt";
var empty_resource = "resources/empty-resource.txt";
var crossorigin_anon_resource = location.protocol + '//' + remote_host_and_port + '/subresource-integrity/resources/crossorigin-anon-resource.txt';
var crossorigin_creds_resource = location.protocol + '//' + remote_host_and_port + '/subresource-integrity/resources/crossorigin-creds-resource.txt';
var crossorigin_ineligible_resource = location.protocol + '//' + remote_host_and_port + '/subresource-integrity/resources/crossorigin-ineligible-resource.txt';

function integrity(desc, url, options, expectedError) {
    if (expectedError === undefined) {
        promise_test(function(test) {
            return fetch(url, options).then(function(resp) {
                assert_equals(resp.status, 200, "Response's status is 200");
            });
        }, desc);
    } else {
        promise_test(function(test) {
            return promise_rejects(test, expectedError, fetch(url, options));
        }, desc);
    }
}

var topSha256 = "sha256-KHIDZcXnR2oBHk9DrAA+5fFiR6JjudYjqoXtMR1zvzk=";
var topSha384 = "sha384-MgZYnnAzPM/MjhqfOIMfQK5qcFvGZsGLzx4Phd7/A8fHTqqLqXqKo8cNzY3xEPTL";
var topSha512 = "sha512-D6yns0qxG0E7+TwkevZ4Jt5t7Iy3ugmAajG/dlf6Pado1JqTyneKXICDiqFIkLMRExgtvg8PlxbKTkYfRejSOg==";
var invalidSha256 = "sha256-dKUcPOn/AlUjWIwcHeHNqYXPlvyGiq+2dWOdFcE+24I=";
var invalidSha512 = "sha512-oUceBRNxPxnY60g/VtPCj2syT4wo4EZh2CgYdWy9veW8+OsReTXoh7dizMGZafvx9+QhMS39L/gIkxnPIn41Zg==";
var unknownAlgorithm = "foo666-dKUcPOn/AlUjWIwcHeHNqYXPlvyGiq+2dWOdFcE+24I=";

integrity("Empty string integrity", resource, { 'integrity': "" });
integrity("SHA-256 integrity", resource, { 'integrity': topSha256 });
integrity("SHA-384 integrity", resource, { 'integrity': topSha384 });
integrity("SHA-512 integrity", resource, { 'integrity': topSha512 });
integrity("Invalid integrity", resource, { 'integrity': invalidSha256 }, new TypeError());
integrity("Unknown integrity", resource, { 'integrity': unknownAlgorithm });
integrity("Multiple integrities: valid stronger than invalid", resource, { 'integrity': invalidSha256 + " " + topSha384 });
integrity("Multiple integrities: invalid stronger than valid", resource, { 'integrity': invalidSha512 + " " + topSha384 }, new TypeError());
integrity("Multiple integrities: invalid as strong as valid", resource, { 'integrity': invalidSha512 + " " + topSha512 });
integrity("Multiple integrities: both are valid", resource,  { 'integrity': topSha384 + " " + topSha512 });
integrity("Multiple integrities: both are invalid", resource, { 'integrity': invalidSha256 + " " + invalidSha512 }, new TypeError());
integrity("Anonymous CORS empty integrity", crossorigin_anon_resource, { 'integrity': "" });
integrity("Anonymous CORS SHA-512 integrity", crossorigin_anon_resource, { 'integrity': topSha512 });
integrity("Anonymous CORS invalid integrity", crossorigin_anon_resource, { 'integrity': invalidSha512 }, new TypeError());

// FIXME: Upstream these additional tests to the official web-platform-tests repository.

integrity("Credential CORS empty integrity", crossorigin_creds_resource, { 'integrity': "", 'credentials': 'include' });
integrity("Credential CORS SHA-512 integrity", crossorigin_creds_resource, { 'integrity': topSha512, 'credentials': 'include' });
integrity("Credential CORS invalid integrity", crossorigin_creds_resource, { 'integrity': invalidSha512, 'credentials': 'include' }, new TypeError());
integrity("Ineligible CORS empty integrity", crossorigin_ineligible_resource, { 'integrity': "" }, new TypeError());
integrity("Ineligible CORS SHA-512 integrity", crossorigin_ineligible_resource, { 'integrity': topSha512 }, new TypeError());
integrity("Ineligible CORS invalid integrity", crossorigin_ineligible_resource, { 'integrity': invalidSha512 }, new TypeError());
integrity("SHA-256 integrity with 'no-cors' mode", resource, { 'integrity': topSha256, 'mode': 'no-cors' });
integrity("Resource with zero length body", empty_resource, { 'integrity': "47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=" });

done();
