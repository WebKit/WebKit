/*
 * testharness-helpers contains various useful extensions to testharness.js to
 * allow them to be used across multiple tests before they have been
 * upstreamed. This file is intended to be usable from both document and worker
 * environments, so code should for example not rely on the DOM.
 */

// Returns a promise that fulfills after the provided |promise| is fulfilled.
// The |test| succeeds only if |promise| rejects with an exception matching
// |code|. Accepted values for |code| follow those accepted for assert_throws().
// The optional |description| describes the test being performed.
//
// E.g.:
//   assert_promise_rejects(
//       new Promise(...), // something that should throw an exception.
//       'NotFoundError',
//       'Should throw NotFoundError.');
//
//   assert_promise_rejects(
//       new Promise(...),
//       new TypeError(),
//       'Should throw TypeError');
function assert_promise_rejects(promise, code, description) {
  return promise.then(
    function() {
      throw 'assert_promise_rejects: ' + description + ' Promise did not reject.';
    },
    function(e) {
      if (code !== undefined) {
        assert_throws(code, function() { throw e; }, description);
      }
    });
}

// Asserts that |object| that is an instance of some interface has the attribute
// |attribute_name| following the conditions specified by WebIDL, but it's
// acceptable that the attribute |attribute_name| is an own property of the
// object because we're in the middle of moving the attribute to a prototype
// chain.  Once we complete the transition to prototype chains,
// assert_will_be_idl_attribute must be replaced with assert_idl_attribute
// defined in testharness.js.
//
// FIXME: Remove assert_will_be_idl_attribute once we complete the transition
// of moving the DOM attributes to prototype chains.  (http://crbug.com/43394)
function assert_will_be_idl_attribute(object, attribute_name, description) {
  assert_true(typeof object === "object", description);

  assert_true("hasOwnProperty" in object, description);

  // Do not test if |attribute_name| is not an own property because
  // |attribute_name| is in the middle of the transition to a prototype
  // chain.  (http://crbug.com/43394)

  assert_true(attribute_name in object, description);
}

// Stringifies a DOM object.  This function stringifies not only own properties
// but also DOM attributes which are on a prototype chain.  Note that
// JSON.stringify only stringifies own properties.
function stringifyDOMObject(object)
{
    function deepCopy(src) {
        if (typeof src != "object")
            return src;
        var dst = Array.isArray(src) ? [] : {};
        for (var property in src) {
            dst[property] = deepCopy(src[property]);
        }
        return dst;
    }
    return JSON.stringify(deepCopy(object));
}
