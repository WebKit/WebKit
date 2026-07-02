// These are helpers to be evaluated inside a worklet
// They are mostly from the wpt testharness.js.

/*
 * Convert a value to a nice, human-readable string
 */
function format_value(val, seen)
{
    if (!seen) {
        seen = [];
    }
    if (typeof val === "object" && val !== null) {
        if (seen.indexOf(val) >= 0) {
            return "[...]";
        }
        seen.push(val);
    }
    if (Array.isArray(val)) {
        return "[" + val.map(function(x) {return format_value(x, seen);}).join(", ") + "]";
    }

    switch (typeof val) {
    case "string":
        val = val.replace("\\", "\\\\");
        for (var p in replacements) {
            var replace = "\\" + replacements[p];
            val = val.replace(RegExp(String.fromCharCode(p), "g"), replace);
        }
        return '"' + val.replace(/"/g, '\\"') + '"';
    case "boolean":
    case "undefined":
        return String(val);
    case "number":
        // In JavaScript, -0 === 0 and String(-0) == "0", so we have to
        // special-case.
        if (val === -0 && 1/val === -Infinity) {
            return "-0";
        }
        return String(val);
    case "object":
        if (val === null) {
            return "null";
        }

        // Special-case Node objects, since those come up a lot in my tests.  I
        // ignore namespaces.
        if (is_node(val)) {
            switch (val.nodeType) {
            case Node.ELEMENT_NODE:
                var ret = "<" + val.localName;
                for (var i = 0; i < val.attributes.length; i++) {
                    ret += " " + val.attributes[i].name + '="' + val.attributes[i].value + '"';
                }
                ret += ">" + val.innerHTML + "</" + val.localName + ">";
                return "Element node " + truncate(ret, 60);
            case Node.TEXT_NODE:
                return 'Text node "' + truncate(val.data, 60) + '"';
            case Node.PROCESSING_INSTRUCTION_NODE:
                return "ProcessingInstruction node with target " + format_value(truncate(val.target, 60)) + " and data " + format_value(truncate(val.data, 60));
            case Node.COMMENT_NODE:
                return "Comment node <!--" + truncate(val.data, 60) + "-->";
            case Node.DOCUMENT_NODE:
                return "Document node with " + val.childNodes.length + (val.childNodes.length == 1 ? " child" : " children");
            case Node.DOCUMENT_TYPE_NODE:
                return "DocumentType node";
            case Node.DOCUMENT_FRAGMENT_NODE:
                return "DocumentFragment node with " + val.childNodes.length + (val.childNodes.length == 1 ? " child" : " children");
            default:
                return "Node object of unknown type";
            }
        }

    /* falls through */
    default:
        try {
            return typeof val + ' "' + truncate(String(val), 1000) + '"';
        } catch(e) {
            return ("[stringifying object threw " + String(e) +
                    " with type " + String(typeof e) + "]");
        }
    }
}

function is_single_node(template)
{
    return typeof template[0] === "string";
}

function substitute(template, substitutions)
{
    if (typeof template === "function") {
        var replacement = template(substitutions);
        if (!replacement) {
            return null;
        }

        return substitute(replacement, substitutions);
    }

    if (is_single_node(template)) {
        return substitute_single(template, substitutions);
    }

    return filter(map(template, function(x) {
                          return substitute(x, substitutions);
                      }), function(x) {return x !== null;});
}

function substitute_single(template, substitutions)
{
    var substitution_re = /\$\{([^ }]*)\}/g;

    function do_substitution(input) {
        var components = input.split(substitution_re);
        var rv = [];
        for (var i = 0; i < components.length; i += 2) {
            rv.push(components[i]);
            if (components[i + 1]) {
                rv.push(String(substitutions[components[i + 1]]));
            }
        }
        return rv;
    }

    function substitute_attrs(attrs, rv)
    {
        rv[1] = {};
        for (var name in template[1]) {
            if (attrs.hasOwnProperty(name)) {
                var new_name = do_substitution(name).join("");
                var new_value = do_substitution(attrs[name]).join("");
                rv[1][new_name] = new_value;
            }
        }
    }

    function substitute_children(children, rv)
    {
        for (var i = 0; i < children.length; i++) {
            if (children[i] instanceof Object) {
                var replacement = substitute(children[i], substitutions);
                if (replacement !== null) {
                    if (is_single_node(replacement)) {
                        rv.push(replacement);
                    } else {
                        extend(rv, replacement);
                    }
                }
            } else {
                extend(rv, do_substitution(String(children[i])));
            }
        }
        return rv;
    }

    var rv = [];
    rv.push(do_substitution(String(template[0])).join(""));

    if (template[0] === "{text}") {
        substitute_children(template.slice(1), rv);
    } else {
        substitute_attrs(template[1], rv);
        substitute_children(template.slice(2), rv);
    }

    return rv;
}

function assert(expected_true, function_name, description, error, substitutions)
{
    if (expected_true !== true) {
        var msg = make_message(function_name, description,
                               error, substitutions);
        throw new AssertionError(msg);
    }
}

function AssertionError(message)
{
    this.message = message;
    this.stack = this.get_stack();
}

AssertionError.prototype = Object.create(Error.prototype);

AssertionError.prototype.get_stack = function() {
    var stack = new Error().stack;
    // IE11 does not initialize 'Error.stack' until the object is thrown.
    if (!stack) {
        try {
            throw new Error();
        } catch (e) {
            stack = e.stack;
        }
    }

    // 'Error.stack' is not supported in all browsers/versions
    if (!stack) {
        return "(Stack trace unavailable)";
    }

    var lines = stack.split("\n");

    // Create a pattern to match stack frames originating within testharness.js.  These include the
    // script URL, followed by the line/col (e.g., '/resources/testharness.js:120:21').
    // Escape the URL per http://stackoverflow.com/questions/3561493/is-there-a-regexp-escape-function-in-javascript
    // in case it contains RegExp characters.
    var script_url = get_script_url();
    var re_text = script_url ? script_url.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&') : "\\btestharness.js";
    var re = new RegExp(re_text + ":\\d+:\\d+");

    // Some browsers include a preamble that specifies the type of the error object.  Skip this by
    // advancing until we find the first stack frame originating from testharness.js.
    var i = 0;
    while (!re.test(lines[i]) && i < lines.length) {
        i++;
    }

    // Then skip the top frames originating from testharness.js to begin the stack at the test code.
    while (re.test(lines[i]) && i < lines.length) {
        i++;
    }

    // Paranoid check that we didn't skip all frames.  If so, return the original stack unmodified.
    if (i >= lines.length) {
        return stack;
    }

    return lines.slice(i).join("\n");
}

function make_message(function_name, description, error, substitutions)
{
    for (var p in substitutions) {
        if (substitutions.hasOwnProperty(p)) {
            substitutions[p] = format_value(substitutions[p]);
        }
    }
    var node_form = substitute(["{text}", "${function_name}: ${description}" + error],
                               merge({function_name:function_name,
                                      description:(description?description + " ":"")},
                                      substitutions));
    return node_form.slice(1).join("");
}

function filter(array, callable, thisObj) {
    var rv = [];
    for (var i = 0; i < array.length; i++) {
        if (array.hasOwnProperty(i)) {
            var pass = callable.call(thisObj, array[i], i, array);
            if (pass) {
                rv.push(array[i]);
            }
        }
    }
    return rv;
}

function map(array, callable, thisObj)
{
    var rv = [];
    rv.length = array.length;
    for (var i = 0; i < array.length; i++) {
        if (array.hasOwnProperty(i)) {
            rv[i] = callable.call(thisObj, array[i], i, array);
        }
    }
    return rv;
}

function forEach(array, callback, thisObj)
{
    for (var i = 0; i < array.length; i++) {
        if (array.hasOwnProperty(i)) {
            callback.call(thisObj, array[i], i, array);
        }
    }
}

function merge(a,b)
{
    var rv = {};
    var p;
    for (p in a) {
        rv[p] = a[p];
    }
    for (p in b) {
        rv[p] = b[p];
    }
    return rv;
}

function same_value(x, y) {
    if (y !== y) {
        //NaN case
        return x !== x;
    }
    if (x === 0 && y === 0) {
        //Distinguish +0 and -0
        return 1/x === 1/y;
    }
    return x === y;
}

function assert_equals(actual, expected, description)
{
     /*
      * Test if two primitives are equal or two objects
      * are the same object
      */
    if (typeof actual != typeof expected) {
        assert(false, "assert_equals", description,
                      "expected (" + typeof expected + ") ${expected} but got (" + typeof actual + ") ${actual}",
                      {expected:expected, actual:actual});
        return;
    }
    assert(same_value(actual, expected), "assert_equals", description,
                                         "expected ${expected} but got ${actual}",
                                         {expected:expected, actual:actual});
}

function assert_not_equals(actual, expected, description)
{
     /*
      * Test if two primitives are unequal or two objects
      * are different objects
      */
    assert(!same_value(actual, expected), "assert_not_equals", description,
                                          "got disallowed value ${actual}",
                                          {actual:actual});
}

function assert_in_array(actual, expected, description)
{
    assert(expected.indexOf(actual) != -1, "assert_in_array", description,
                                           "value ${actual} not in array ${expected}",
                                           {actual:actual, expected:expected});
}

function assert_object_equals(actual, expected, description)
{
     //This needs to be improved a great deal
     function check_equal(actual, expected, stack)
     {
         stack.push(actual);

         var p;
         for (p in actual) {
             assert(expected.hasOwnProperty(p), "assert_object_equals", description,
                                                "unexpected property ${p}", {p:p});

             if (typeof actual[p] === "object" && actual[p] !== null) {
                 if (stack.indexOf(actual[p]) === -1) {
                     check_equal(actual[p], expected[p], stack);
                 }
             } else {
                 assert(same_value(actual[p], expected[p]), "assert_object_equals", description,
                                                   "property ${p} expected ${expected} got ${actual}",
                                                   {p:p, expected:expected, actual:actual});
             }
         }
         for (p in expected) {
             assert(actual.hasOwnProperty(p),
                    "assert_object_equals", description,
                    "expected property ${p} missing", {p:p});
         }
         stack.pop();
     }
     check_equal(actual, expected, []);
}

function assert_array_equals(actual, expected, description)
{
    assert(actual.length === expected.length,
           "assert_array_equals", description,
           "lengths differ, expected ${expected} got ${actual}",
           {expected:expected.length, actual:actual.length});

    for (var i = 0; i < actual.length; i++) {
        assert(actual.hasOwnProperty(i) === expected.hasOwnProperty(i),
               "assert_array_equals", description,
               "property ${i}, property expected to be ${expected} but was ${actual}",
               {i:i, expected:expected.hasOwnProperty(i) ? "present" : "missing",
               actual:actual.hasOwnProperty(i) ? "present" : "missing"});
        assert(same_value(expected[i], actual[i]),
               "assert_array_equals", description,
               "property ${i}, expected ${expected} but got ${actual}",
               {i:i, expected:expected[i], actual:actual[i]});
    }
}

function assert_approx_equals(actual, expected, epsilon, description)
{
    /*
     * Test if two primitive numbers are equal withing +/- epsilon
     */
    assert(typeof actual === "number",
           "assert_approx_equals", description,
           "expected a number but got a ${type_actual}",
           {type_actual:typeof actual});

    assert(Math.abs(actual - expected) <= epsilon,
           "assert_approx_equals", description,
           "expected ${expected} +/- ${epsilon} but got ${actual}",
           {expected:expected, actual:actual, epsilon:epsilon});
}

function assert_less_than(actual, expected, description)
{
    /*
     * Test if a primitive number is less than another
     */
    assert(typeof actual === "number",
           "assert_less_than", description,
           "expected a number but got a ${type_actual}",
           {type_actual:typeof actual});

    assert(actual < expected,
           "assert_less_than", description,
           "expected a number less than ${expected} but got ${actual}",
           {expected:expected, actual:actual});
}

function assert_greater_than(actual, expected, description)
{
    /*
     * Test if a primitive number is greater than another
     */
    assert(typeof actual === "number",
           "assert_greater_than", description,
           "expected a number but got a ${type_actual}",
           {type_actual:typeof actual});

    assert(actual > expected,
           "assert_greater_than", description,
           "expected a number greater than ${expected} but got ${actual}",
           {expected:expected, actual:actual});
}

function assert_between_exclusive(actual, lower, upper, description)
{
    /*
     * Test if a primitive number is between two others
     */
    assert(typeof actual === "number",
           "assert_between_exclusive", description,
           "expected a number but got a ${type_actual}",
           {type_actual:typeof actual});

    assert(actual > lower && actual < upper,
           "assert_between_exclusive", description,
           "expected a number greater than ${lower} " +
           "and less than ${upper} but got ${actual}",
           {lower:lower, upper:upper, actual:actual});
}

function assert_less_than_equal(actual, expected, description)
{
    /*
     * Test if a primitive number is less than or equal to another
     */
    assert(typeof actual === "number",
           "assert_less_than_equal", description,
           "expected a number but got a ${type_actual}",
           {type_actual:typeof actual});

    assert(actual <= expected,
           "assert_less_than_equal", description,
           "expected a number less than or equal to ${expected} but got ${actual}",
           {expected:expected, actual:actual});
}

function assert_greater_than_equal(actual, expected, description)
{
    /*
     * Test if a primitive number is greater than or equal to another
     */
    assert(typeof actual === "number",
           "assert_greater_than_equal", description,
           "expected a number but got a ${type_actual}",
           {type_actual:typeof actual});

    assert(actual >= expected,
           "assert_greater_than_equal", description,
           "expected a number greater than or equal to ${expected} but got ${actual}",
           {expected:expected, actual:actual});
}

function assert_between_inclusive(actual, lower, upper, description)
{
    /*
     * Test if a primitive number is between to two others or equal to either of them
     */
    assert(typeof actual === "number",
           "assert_between_inclusive", description,
           "expected a number but got a ${type_actual}",
           {type_actual:typeof actual});

    assert(actual >= lower && actual <= upper,
           "assert_between_inclusive", description,
           "expected a number greater than or equal to ${lower} " +
           "and less than or equal to ${upper} but got ${actual}",
           {lower:lower, upper:upper, actual:actual});
}

function assert_throws(code, func, description)
{
    try {
        func.call(this);
        assert(false, "assert_throws", description,
               "${func} did not throw", {func:func});
    } catch (e) {
        if (e instanceof AssertionError) {
            throw e;
        }
        if (code === null) {
            return;
        }
        if (typeof code === "object") {
            assert(typeof e == "object" && "name" in e && e.name == code.name,
                   "assert_throws", description,
                   "${func} threw ${actual} (${actual_name}) expected ${expected} (${expected_name})",
                                {func:func, actual:e, actual_name:e.name,
                                 expected:code,
                                 expected_name:code.name});
            return;
        }

        var code_name_map = {
            INDEX_SIZE_ERR: 'IndexSizeError',
            HIERARCHY_REQUEST_ERR: 'HierarchyRequestError',
            WRONG_DOCUMENT_ERR: 'WrongDocumentError',
            INVALID_CHARACTER_ERR: 'InvalidCharacterError',
            NO_MODIFICATION_ALLOWED_ERR: 'NoModificationAllowedError',
            NOT_FOUND_ERR: 'NotFoundError',
            NOT_SUPPORTED_ERR: 'NotSupportedError',
            INUSE_ATTRIBUTE_ERR: 'InUseAttributeError',
            INVALID_STATE_ERR: 'InvalidStateError',
            SYNTAX_ERR: 'SyntaxError',
            INVALID_MODIFICATION_ERR: 'InvalidModificationError',
            NAMESPACE_ERR: 'NamespaceError',
            INVALID_ACCESS_ERR: 'InvalidAccessError',
            TYPE_MISMATCH_ERR: 'TypeMismatchError',
            SECURITY_ERR: 'SecurityError',
            NETWORK_ERR: 'NetworkError',
            ABORT_ERR: 'AbortError',
            URL_MISMATCH_ERR: 'URLMismatchError',
            QUOTA_EXCEEDED_ERR: 'QuotaExceededError',
            TIMEOUT_ERR: 'TimeoutError',
            INVALID_NODE_TYPE_ERR: 'InvalidNodeTypeError',
            DATA_CLONE_ERR: 'DataCloneError'
        };

        var name = code in code_name_map ? code_name_map[code] : code;

        var name_code_map = {
            IndexSizeError: 1,
            HierarchyRequestError: 3,
            WrongDocumentError: 4,
            InvalidCharacterError: 5,
            NoModificationAllowedError: 7,
            NotFoundError: 8,
            NotSupportedError: 9,
            InUseAttributeError: 10,
            InvalidStateError: 11,
            SyntaxError: 12,
            InvalidModificationError: 13,
            NamespaceError: 14,
            InvalidAccessError: 15,
            TypeMismatchError: 17,
            SecurityError: 18,
            NetworkError: 19,
            AbortError: 20,
            URLMismatchError: 21,
            QuotaExceededError: 22,
            TimeoutError: 23,
            InvalidNodeTypeError: 24,
            DataCloneError: 25,

            EncodingError: 0,
            NotReadableError: 0,
            UnknownError: 0,
            ConstraintError: 0,
            DataError: 0,
            TransactionInactiveError: 0,
            ReadOnlyError: 0,
            VersionError: 0,
            OperationError: 0,
            NotAllowedError: 0
        };

        if (!(name in name_code_map)) {
            throw new AssertionError('Test bug: unrecognized DOMException code "' + code + '" passed to assert_throws()');
        }

        var required_props = { code: name_code_map[name] };

        if (required_props.code === 0 ||
           (typeof e == "object" &&
            "name" in e &&
            e.name !== e.name.toUpperCase() &&
            e.name !== "DOMException")) {
            // New style exception: also test the name property.
            required_props.name = name;
        }

        //We'd like to test that e instanceof the appropriate interface,
        //but we can't, because we don't know what window it was created
        //in.  It might be an instanceof the appropriate interface on some
        //unknown other window.  TODO: Work around this somehow?

        assert(typeof e == "object",
               "assert_throws", description,
               "${func} threw ${e} with type ${type}, not an object",
               {func:func, e:e, type:typeof e});

        for (var prop in required_props) {
            assert(typeof e == "object" && prop in e && e[prop] == required_props[prop],
                   "assert_throws", description,
                   "${func} threw ${e} that is not a DOMException " + code + ": property ${prop} is equal to ${actual}, expected ${expected}",
                   {func:func, e:e, prop:prop, actual:e[prop], expected:required_props[prop]});
        }
    }
}

/*
 * Return true if object is probably a Node object.
 */
function is_node(object)
{
    // I use duck-typing instead of instanceof, because
    // instanceof doesn't work if the node is from another window (like an
    // iframe's contentWindow):
    // http://www.w3.org/Bugs/Public/show_bug.cgi?id=12295
    try {
        var has_node_properties = ("nodeType" in object &&
                                   "nodeName" in object &&
                                   "nodeValue" in object &&
                                   "childNodes" in object);
    } catch (e) {
        // We're probably cross-origin, which means we aren't a node
        return false;
    }

    if (has_node_properties) {
        try {
            object.nodeType;
        } catch (e) {
            // The object is probably Node.prototype or another prototype
            // object that inherits from it, and not a Node instance.
            return false;
        }
        return true;
    }
    return false;
}

var replacements = {
    "0": "0",
    "1": "x01",
    "2": "x02",
    "3": "x03",
    "4": "x04",
    "5": "x05",
    "6": "x06",
    "7": "x07",
    "8": "b",
    "9": "t",
    "10": "n",
    "11": "v",
    "12": "f",
    "13": "r",
    "14": "x0e",
    "15": "x0f",
    "16": "x10",
    "17": "x11",
    "18": "x12",
    "19": "x13",
    "20": "x14",
    "21": "x15",
    "22": "x16",
    "23": "x17",
    "24": "x18",
    "25": "x19",
    "26": "x1a",
    "27": "x1b",
    "28": "x1c",
    "29": "x1d",
    "30": "x1e",
    "31": "x1f",
    "0xfffd": "ufffd",
    "0xfffe": "ufffe",
    "0xffff": "uffff",
};

function extend(array, items)
{
    Array.prototype.push.apply(array, items);
}

function get_script_url()
{
    return "/";
}

/*
 * Return a string truncated to the given length, with ... added at the end
 * if it was longer.
 */
function truncate(s, len)
{
    if (s.length > len) {
        return s.substring(0, len - 3) + "...";
    }
    return s;
}

function assert_true(actual, description)
{
    assert(actual === true, "assert_true", description,
                            "expected true got ${actual}", {actual:actual});
}

function assert_false(actual, description)
{
    assert(actual === false, "assert_false", description,
                             "expected false got ${actual}", {actual:actual});
}

function test(tst, name) {
  try {
    tst();
  } catch (e) {
    if (!PaintWorkletGlobalScope.errors)
      PaintWorkletGlobalScope.errors = [];
      PaintWorkletGlobalScope.errors.push({ name: name, error: e.message });
  }
  PaintWorkletGlobalScope.hasRunATest = true;
}
