/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Template Test
// This test uses a simple JS templating system for injecting JSON data into
// an HTML page.  It is designed to use the kind of DOM manipulation common
// in templating systems.
//
// Code from http://code.google.com/p/google-jstemplate/source/browse/trunk/jstemplate.js

// This is the HTML code to use in the template test
var content ="\
<style type=\"text/css\"> \
body {\
  border-top: 10px solid #3B85E3;\
  color: #333;\
  font-family: Verdana,Arial,Helvetica,sans-serif;\
}\
body, td {\
  font-size: 11px;\
}\
a:link, a:visited {\
  color: #2C3EBA;\
  text-decoration: none;\
}\
a:hover {\
  color: red;\
  text-decoration: underline;\
}\
h1 {\
  border-left: 10px solid #FFF;\
  font-size: 16px;\
  font-weight: bold;\
  margin: 0;\
  padding: 0.2em;\
  color: #3B85E3;\
}\
h2 {\
  border-left: 10px solid #FFF;\
  font-size: 11px;\
  font-weight: normal;\
  margin: 0;\
  padding: 0 6em 0.2em 0.2em;\
}\
.details {\
  margin: 0.4em 1.9em 0 1.2em;\
  padding: 0 0.4em 0.3em 0;\
  white-space: nowrap;\
}\
.details .outer {\
  padding-right: 0;\
  vertical-align: top;\
}\
.details .top {\
  border-top: 2px solid #333;\
  font-weight: bold;\
  margin-top: 0.4em;\
}\
.details .header2 {\
  font-weight: bold;\
  padding-left: 0.9em;\
}\
.details .key {\
  padding-left: 1.1em;\
  vertical-align: top;\
}\
.details .value {\
  text-align: right;\
  color: #333;\
  font-weight: bold;\
}\
.details .zebra {\
  background: #EEE;\
}\
.lower {\
  text-transform: lowercase;\
}\
</style> \
  <h1 class=\"lower\">About Stats</h1> \
  <table class=\"details\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\"> \
    <tbody> \
      <tr> \
        <td class=\"outer\"> \
          <table cellspacing=\"0\" cellpadding=\"0\" border=\"0\"> \
            <tbody> \
              <tr> \
                <td class=\"top\" width=\"100\">Counters</td> \
                <td class=\"top value\" colspan=2></td> \
              </tr> \
              <tr> \
                <td class=\"header2 lower\" width=\"200\">name</td> \
                <td class=\"header2 lower\">value</td> \
                <td class=\"header2 lower\">delta</td> \
              </tr> \
              <tr jsselect=\"counters\" name=\"counter\"> \
                <td class=\"key\" width=\"200\" jscontent=\"name\"></td> \
                <td class=\"value\" jscontent=\"value\"></td> \
                <td class=\"value\" jscontent=\"delta\"></td> \
              </tr> \
            </tbody> \
          </table> \
        </td> \
        <td width=\"15\"/> \
        <td class=\"outer\"> \
          <table cellspacing=\"0\" cellpadding=\"0\" border=\"0\"> \
            <tbody> \
              <tr> \
                <td class=\"top\" width=\"100\">Timers</td> \
                <td class=\"top value\"></td> \
                <td class=\"top value\" colspan=3></td> \
              </tr> \
              <tr> \
                <td class=\"header2 lower\" width=\"200\">name</td> \
                <td class=\"header2 lower\">count</td> \
                <td class=\"header2 lower\">time (ms)</td> \
                <td class=\"header2 lower\">avg time (ms)</td> \
              </tr> \
              <tr jsselect=\"timers\" name=\"timer\"> \
                <td class=\"key\" width=\"200\" jscontent=\"name\"></td> \
                <td class=\"value\" jscontent=\"value\"></td> \
                <td class=\"value\" jscontent=\"time\"></td> \
                <td class=\"value\"></td> \
              </tr> \
            </tbody> \
          </table> \
        </td> \
      </tr> \
    </tbody> \
  </table><br/> \
</body> \
</html> \
";

// Generic Template Library

/**
 * @fileoverview This file contains miscellaneous basic functionality.
 *
 */

/**
 * Returns the document owner of the given element. In particular,
 * returns window.document if node is null or the browser does not
 * support ownerDocument.
 *
 * @param {Node} node  The node whose ownerDocument is required.
 * @returns {Document|Null}  The owner document or null if unsupported.
 */
function Template_ownerDocument(node) {
    return (node ? node.ownerDocument : null) || document;
}

/**
 * Wrapper function to create CSS units (pixels) string
 *
 * @param {Number} numPixels  Number of pixels, may be floating point.
 * @returns {String}  Corresponding CSS units string.
 */
function Template_px(numPixels) {
    return round(numPixels) + "px";
}

/**
 * Sets display to none. Doing this as a function saves a few bytes for
 * the 'style.display' property and the 'none' literal.
 *
 * @param {Element} node  The dom element to manipulate.
 */
function Template_displayNone(node) {
    node.style.display = 'none';
}

/**
 * Sets display to default.
 *
 * @param {Element} node  The dom element to manipulate.
 */
function Template_displayDefault(node) {
    node.style.display = '';
}

var DOM_ELEMENT_NODE = 1;
var DOM_ATTRIBUTE_NODE = 2;
var DOM_TEXT_NODE = 3;
var DOM_CDATA_SECTION_NODE = 4;
var DOM_ENTITY_REFERENCE_NODE = 5;
var DOM_ENTITY_NODE = 6;
var DOM_PROCESSING_INSTRUCTION_NODE = 7;
var DOM_COMMENT_NODE = 8;
var DOM_DOCUMENT_NODE = 9;
var DOM_DOCUMENT_TYPE_NODE = 10;
var DOM_DOCUMENT_FRAGMENT_NODE = 11;
var DOM_NOTATION_NODE = 12;

/**
 * Get an attribute from the DOM.  Simple redirect, exists to compress code.
 *
 * @param {Element} node  Element to interrogate.
 * @param {String} name  Name of parameter to extract.
 * @return {String}  Resulting attribute.
 */
function Template_domGetAttribute(node, name) {
    return node.getAttribute(name);
}

/**
 * Set an attribute in the DOM.  Simple redirect to compress code.
 *
 * @param {Element} node  Element to interrogate.
 * @param {String} name  Name of parameter to set.
 * @param {String} value  Set attribute to this value.
 */
function Template_domSetAttribute(node, name, value) {
    node.setAttribute(name, value);
}

/**
 * Remove an attribute from the DOM.  Simple redirect to compress code.
 *
 * @param {Element} node  Element to interrogate.
 * @param {String} name  Name of parameter to remove.
 */
function Template_domRemoveAttribute(node, name) {
    node.removeAttribute(name);
}

/**
 * Clone a node in the DOM.
 *
 * @param {Node} node  Node to clone.
 * @return {Node}  Cloned node.
 */
function Template_domCloneNode(node) {
    return node.cloneNode(true);
}

/**
 * Return a safe string for the className of a node.
 * If className is not a string, returns "".
 *
 * @param {Element} node  DOM element to query.
 * @return {String}
 */
function Template_domClassName(node) {
    return node.className ? "" + node.className : "";
}

/**
 * Inserts a new child before a given sibling.
 *
 * @param {Node} newChild  Node to insert.
 * @param {Node} oldChild  Sibling node.
 * @return {Node}  Reference to new child.
 */
function Template_domInsertBefore(newChild, oldChild) {
    return oldChild.parentNode.insertBefore(newChild, oldChild);
}

/**
 * Appends a new child to the specified (parent) node.
 *
 * @param {Element} node  Parent element.
 * @param {Node} child  Child node to append.
 * @return {Node}  Newly appended node.
 */
function Template_domAppendChild(node, child) {
    return node.appendChild(child);
}

/**
 * Remove a new child from the specified (parent) node.
 *
 * @param {Element} node  Parent element.
 * @param {Node} child  Child node to remove.
 * @return {Node}  Removed node.
 */
function Template_domRemoveChild(node, child) {
    return node.removeChild(child);
}

/**
 * Replaces an old child node with a new child node.
 *
 * @param {Node} newChild  New child to append.
 * @param {Node} oldChild  Old child to remove.
 * @return {Node}  Replaced node.
 */
function Template_domReplaceChild(newChild, oldChild) {
    return oldChild.parentNode.replaceChild(newChild, oldChild);
}

/**
 * Removes a node from the DOM.
 *
 * @param {Node} node  The node to remove.
 * @return {Node}  The removed node.
 */
function Template_domRemoveNode(node) {
    return Template_domRemoveChild(node.parentNode, node);
}

/**
 * Creates a new text node in the given document.
 *
 * @param {Document} doc  Target document.
 * @param {String} text  Text composing new text node.
 * @return {Text}  Newly constructed text node.
 */
function Template_domCreateTextNode(doc, text) {
    return doc.createTextNode(text);
}

/**
 * Redirect to document.getElementById
 *
 * @param {Document} doc  Target document.
 * @param {String} id  Id of requested node.
 * @return {Element|Null}  Resulting element.
 */
function Template_domGetElementById(doc, id) {
    return doc.getElementById(id);
}

/**
 * @fileoverview This file contains javascript utility functions that
 * do not depend on anything defined elsewhere.
 *
 */

/**
 * Returns the value of the length property of the given object. Used
 * to reduce compiled code size.
 *
 * @param {Array | String} a  The string or array to interrogate.
 * @return {Number}  The value of the length property.
 */
function Template_jsLength(a) {
    return a.length;
}

var min = Math.min;
var max = Math.max;
var ceil = Math.ceil;
var floor = Math.floor;
var round = Math.round;
var abs = Math.abs;

/**
 * Copies all properties from second object to the first.  Modifies to.
 *
 * @param {Object} to  The target object.
 * @param {Object} from  The source object.
 */
function Template_copyProperties(to, from) {
    foreachin(from, function(p) { to[p] = from[p]; });
}

/**
 * Iterates over the array, calling the given function for each
 * element.
 *
 * @param {Array} array
 * @param {Function} fn
 */
function foreach(array, fn) {
    var I = Template_jsLength(array);
    for (var i = 0; i < I; ++i)
        fn(array[i], i);
}

/**
 * Safely iterates over all properties of the given object, calling
 * the given function for each property. If opt_all isn't true, uses
 * hasOwnProperty() to assure the property is on the object, not on
 * its prototype.
 *
 * @param {Object} object
 * @param {Function} fn
 * @param {Boolean} opt_all  If true, also iterates over inherited properties.
 */
function foreachin(object, fn, opt_all) {
    for (var i in object) {
        if (opt_all || !object.hasOwnProperty || object.hasOwnProperty(i))
            fn(i, object[i]);
    }
}

/**
 * Trim whitespace from begin and end of string.
 *
 * @see testStringTrim();
 *
 * @param {String} str  Input string.
 * @return {String}  Trimmed string.
 */
function Template_stringTrim(str) {
    return Template_stringTrimRight(stringTrimLeft(str));
}

/**
 * Trim whitespace from beginning of string.
 *
 * @see testStringTrimLeft();
 *
 * @param {String} str  Input string.
 * @return {String}  Trimmed string.
 */
function Template_stringTrimLeft(str) {
    return str.replace(/^\s+/, "");
}

/**
 * Trim whitespace from end of string.
 *
 * @see testStringTrimRight();
 *
 * @param {String} str  Input string.
 * @return {String}  Trimmed string.
 */
function Template_stringTrimRight(str) {
    return str.replace(/\s+$/, "");
}

/**
 * Jscompiler wrapper for parseInt() with base 10.
 *
 * @param {String} s String repersentation of a number.
 *
 * @return {Number} The integer contained in s, converted on base 10.
 */
function Template_parseInt10(s) {
    return parseInt(s, 10);
}
/**
 * @fileoverview A simple formatter to project JavaScript data into
 * HTML templates. The template is edited in place. I.e. in order to
 * instantiate a template, clone it from the DOM first, and then
 * process the cloned template. This allows for updating of templates:
 * If the templates is processed again, changed values are merely
 * updated.
 *
 * NOTE: IE DOM doesn't have importNode().
 *
 * NOTE: The property name "length" must not be used in input
 * data, see comment in jstSelect_().
 */

/**
 * Names of jstemplate attributes. These attributes are attached to
 * normal HTML elements and bind expression context data to the HTML
 * fragment that is used as template.
 */
var ATT_select = 'jsselect';
var ATT_instance = 'jsinstance';
var ATT_display = 'jsdisplay';
var ATT_values = 'jsvalues';
var ATT_eval = 'jseval';
var ATT_transclude = 'transclude';
var ATT_content = 'jscontent';

/**
 * Names of special variables defined by the jstemplate evaluation
 * context. These can be used in js expression in jstemplate
 * attributes.
 */
var VAR_index = '$index';
var VAR_this = '$this';

/**
 * Context for processing a jstemplate. The context contains a context
 * object, whose properties can be referred to in jstemplate
 * expressions, and it holds the locally defined variables.
 *
 * @param {Object} opt_data The context object. Null if no context.
 *
 * @param {Object} opt_parent The parent context, from which local
 * variables are inherited. Normally the context object of the parent
 * context is the object whose property the parent object is. Null for the
 * context of the root object.
 *
 * @constructor
 */
function JsExprContext(opt_data, opt_parent) {
    var me = this;

    /**
     * The local context of the input data in which the jstemplate
     * expressions are evaluated. Notice that this is usually an Object,
     * but it can also be a scalar value (and then still the expression
     * $this can be used to refer to it). Notice this can be a scalar
     * value, including undefined.
     *
     * @type {Object}
     */
    me.data_ = opt_data;

    /**
     * The context for variable definitions in which the jstemplate
     * expressions are evaluated. Other than for the local context,
     * which replaces the parent context, variable definitions of the
     * parent are inherited. The special variable $this points to data_.
     *
     * @type {Object}
     */
    me.vars_ = {};
    if (opt_parent)
        Template_copyProperties(me.vars_, opt_parent.vars_);
    this.vars_[VAR_this] = me.data_;
}

/**
 * Evaluates the given expression in the context of the current
 * context object and the current local variables.
 *
 * @param {String} expr A javascript expression.
 *
 * @param {Element} template DOM node of the template.
 *
 * @return The value of that expression.
 */
JsExprContext.prototype.jseval = function(expr, template) {
    with (this.vars_) {
        with (this.data_) {
            try {
                return (function() { return eval('[' + expr + '][0]'); }).call(template);
            } catch (e) {
                return null;
            }
        }
    }
};

/**
 * Clones the current context for a new context object. The cloned
 * context has the data object as its context object and the current
 * context as its parent context. It also sets the $index variable to
 * the given value. This value usually is the position of the data
 * object in a list for which a template is instantiated multiply.
 *
 * @param {Object} data The new context object.
 *
 * @param {Number} index Position of the new context when multiply
 * instantiated. (See implementation of jstSelect().)
 *
 * @return {JsExprContext}
 */
JsExprContext.prototype.clone = function(data, index) {
    var ret = new JsExprContext(data, this);
    ret.setVariable(VAR_index, index);
    if (this.resolver_)
        ret.setSubTemplateResolver(this.resolver_);
    return ret;
};

/**
 * Binds a local variable to the given value. If set from jstemplate
 * jsvalue expressions, variable names must start with $, but in the
 * API they only have to be valid javascript identifier.
 *
 * @param {String} name
 *
 * @param {Object} value
 */
JsExprContext.prototype.setVariable = function(name, value) {
    this.vars_[name] = value;
};

/**
 * Sets the function used to resolve the values of the transclude
 * attribute into DOM nodes. By default, this is jstGetTemplate(). The
 * value set here is inherited by clones of this context.
 *
 * @param {Function} resolver The function used to resolve transclude
 * ids into a DOM node of a subtemplate. The DOM node returned by this
 * function will be inserted into the template instance being
 * processed. Thus, the resolver function must instantiate the
 * subtemplate as necessary.
 */
JsExprContext.prototype.setSubTemplateResolver = function(resolver) {
    this.resolver_ = resolver;
};

/**
 * Resolves a sub template from an id. Used to process the transclude
 * attribute. If a resolver function was set using
 * setSubTemplateResolver(), it will be used, otherwise
 * jstGetTemplate().
 *
 * @param {String} id The id of the sub template.
 *
 * @return {Node} The root DOM node of the sub template, for direct
 * insertion into the currently processed template instance.
 */
JsExprContext.prototype.getSubTemplate = function(id) {
    return (this.resolver_ || jstGetTemplate).call(this, id);
};

/**
 * HTML template processor. Data values are bound to HTML templates
 * using the attributes transclude, jsselect, jsdisplay, jscontent,
 * jsvalues. The template is modifed in place. The values of those
 * attributes are JavaScript expressions that are evaluated in the
 * context of the data object fragment.
 *
 * @param {JsExprContext} context Context created from the input data
 * object.
 *
 * @param {Element} template DOM node of the template. This will be
 * processed in place. After processing, it will still be a valid
 * template that, if processed again with the same data, will remain
 * unchanged.
 */
function jstProcess(context, template) {
    var processor = new JstProcessor();
    processor.run_([ processor, processor.jstProcess_, context, template ]);
}

/**
 * Internal class used by jstemplates to maintain context.
 * NOTE: This is necessary to process deep templates in Safari
 * which has a relatively shallow stack.
 * @class
 */
function JstProcessor() {
}

/**
 * Runs the state machine, beginning with function "start".
 *
 * @param {Array} start The first function to run, in the form
 * [object, method, args ...]
 */
JstProcessor.prototype.run_ = function(start) {
    var me = this;

    me.queue_ = [ start ];
    while (Template_jsLength(me.queue_)) {
        var f = me.queue_.shift();
        f[1].apply(f[0], f.slice(2));
    }
};

/**
 * Appends a function to be called later.
 * Analogous to calling that function on a subsequent line, or a subsequent
 * iteration of a loop.
 *
 * @param {Array} f  A function in the form [object, method, args ...]
 */
JstProcessor.prototype.enqueue_ = function(f) {
    this.queue_.push(f);
};

/**
 * Implements internals of jstProcess.
 *
 * @param {JsExprContext} context
 *
 * @param {Element} template
 */
JstProcessor.prototype.jstProcess_ = function(context, template) {
    var me = this;

    var transclude = Template_domGetAttribute(template, ATT_transclude);
    if (transclude) {
        var tr = context.getSubTemplate(transclude);
        if (tr) {
            Template_domReplaceChild(tr, template);
            me.enqueue_([ me, me.jstProcess_, context, tr ]);
        } else
            Template_domRemoveNode(template);
        return;
    }

    var select = Template_domGetAttribute(template, ATT_select);
    if (select) {
        me.jstSelect_(context, template, select);
        return;
    }

    var display = Template_domGetAttribute(template, ATT_display);
    if (display) {
        if (!context.jseval(display, template)) {
            Template_displayNone(template);
            return;
        }
        Template_displayDefault(template);
    }

    var values = Template_domGetAttribute(template, ATT_values);
    if (values)
        me.jstValues_(context, template, values);

    var expressions = Template_domGetAttribute(template, ATT_eval);
    if (expressions) {
        foreach(expressions.split(/\s*;\s*/), function(expression) {
            expression = Template_stringTrim(expression);
            if (Template_jsLength(expression))
                context.jseval(expression, template);
        });
    }

    var content = Template_domGetAttribute(template, ATT_content);
    if (content)
        me.jstContent_(context, template, content);
    else {
        var childnodes = [];
        for (var i = 0; i < Template_jsLength(template.childNodes); ++i) {
            if (template.childNodes[i].nodeType == DOM_ELEMENT_NODE)
                me.enqueue_([me, me.jstProcess_, context, template.childNodes[i]]);
        }
    }
};

/**
 * Implements the jsselect attribute: evalutes the value of the
 * jsselect attribute in the current context, with the current
 * variable bindings (see JsExprContext.jseval()). If the value is an
 * array, the current template node is multiplied once for every
 * element in the array, with the array element being the context
 * object. If the array is empty, or the value is undefined, then the
 * current template node is dropped. If the value is not an array,
 * then it is just made the context object.
 *
 * @param {JsExprContext} context The current evaluation context.
 *
 * @param {Element} template The currently processed node of the template.
 *
 * @param {String} select The javascript expression to evaluate.
 *
 * @param {Function} process The function to continue processing with.
 */
JstProcessor.prototype.jstSelect_ = function(context, template, select) {
    var me = this;

    var value = context.jseval(select, template);
    Template_domRemoveAttribute(template, ATT_select);

    var instance = Template_domGetAttribute(template, ATT_instance);
    var instance_last = false;
    if (instance) {
        if (instance.charAt(0) == '*') {
            instance = Template_parseInt10(instance.substr(1));
            instance_last = true;
        } else
            instance = Template_parseInt10(instance);
    }

    var multiple = (value !== null && typeof value == 'object' && typeof value.length == 'number');
    var multiple_empty = (multiple && value.length == 0);

    if (multiple) {
        if (multiple_empty) {
            if (!instance) {
                Template_domSetAttribute(template, ATT_select, select);
                Template_domSetAttribute(template, ATT_instance, '*0');
                Template_displayNone(template);
            } else
                Template_domRemoveNode(template);
        } else {
            Template_displayDefault(template);
            if (instance === null || instance === "" || instance === undefined ||
                (instance_last && instance < Template_jsLength(value) - 1)) {
                var templatenodes = [];
                var instances_start = instance || 0;
                for (var i = instances_start + 1; i < Template_jsLength(value); ++i) {
                    var node = Template_domCloneNode(template);
                    templatenodes.push(node);
                    Template_domInsertBefore(node, template);
                }
                templatenodes.push(template);

                for (var i = 0; i < Template_jsLength(templatenodes); ++i) {
                    var ii = i + instances_start;
                    var v = value[ii];
                    var t = templatenodes[i];

                    me.enqueue_([me, me.jstProcess_, context.clone(v, ii), t]);
                    var instanceStr = (ii == Template_jsLength(value) - 1 ? '*' : '') + ii;
                    me.enqueue_([null, postProcessMultiple_, t, select, instanceStr]);
                }
            } else if (instance < Template_jsLength(value)) {
                var v = value[instance];

                me.enqueue_([me, me.jstProcess_, context.clone(v, instance), template]);
                var instanceStr = (instance == Template_jsLength(value) - 1 ? '*' : '') + instance;
                me.enqueue_([null, postProcessMultiple_, template, select, instanceStr]);
            } else
                Template_domRemoveNode(template);
        }
    } else {
        if (value == null) {
            Template_domSetAttribute(template, ATT_select, select);
            Template_displayNone(template);
        } else {
            me.enqueue_([me, me.jstProcess_, context.clone(value, 0), template]);
            me.enqueue_([null, postProcessSingle_, template, select]);
        }
    }
};

/**
 * Sets ATT_select and ATT_instance following recursion to jstProcess.
 *
 * @param {Element} template  The template
 *
 * @param {String} select  The jsselect string
 *
 * @param {String} instanceStr  The new value for the jsinstance attribute
 */
function postProcessMultiple_(template, select, instanceStr) {
    Template_domSetAttribute(template, ATT_select, select);
    Template_domSetAttribute(template, ATT_instance, instanceStr);
}

/**
 * Sets ATT_select and makes the element visible following recursion to
 * jstProcess.
 *
 * @param {Element} template  The template
 *
 * @param {String} select  The jsselect string
 */
function postProcessSingle_(template, select) {
    Template_domSetAttribute(template, ATT_select, select);
    Template_displayDefault(template);
}

/**
 * Implements the jsvalues attribute: evaluates each of the values and
 * assigns them to variables in the current context (if the name
 * starts with '$', javascript properties of the current template node
 * (if the name starts with '.'), or DOM attributes of the current
 * template node (otherwise). Since DOM attribute values are always
 * strings, the value is coerced to string in the latter case,
 * otherwise it's the uncoerced javascript value.
 *
 * @param {JsExprContext} context Current evaluation context.
 *
 * @param {Element} template Currently processed template node.
 *
 * @param {String} valuesStr Value of the jsvalues attribute to be
 * processed.
 */
JstProcessor.prototype.jstValues_ = function(context, template, valuesStr) {
    var values = valuesStr.split(/\s*;\s*/);
    for (var i = 0; i < Template_jsLength(values); ++i) {
        var colon = values[i].indexOf(':');
        if (colon < 0)
            continue;
        var label = Template_stringTrim(values[i].substr(0, colon));
        var value = context.jseval(values[i].substr(colon + 1), template);

        if (label.charAt(0) == '$')
            context.setVariable(label, value);
        else if (label.charAt(0) == '.')
            template[label.substr(1)] = value;
        else if (label) {
            if (typeof value == 'boolean') {
                if (value)
                    Template_domSetAttribute(template, label, label);
                else
                    Template_domRemoveAttribute(template, label);
            } else
                Template_domSetAttribute(template, label, '' + value);
        }
    }
};

/**
 * Implements the jscontent attribute. Evalutes the expression in
 * jscontent in the current context and with the current variables,
 * and assigns its string value to the content of the current template
 * node.
 *
 * @param {JsExprContext} context Current evaluation context.
 *
 * @param {Element} template Currently processed template node.
 *
 * @param {String} content Value of the jscontent attribute to be
 * processed.
 */
JstProcessor.prototype.jstContent_ = function(context, template, content) {
    var value = '' + context.jseval(content, template);
    if (template.innerHTML == value)
        return;
    while (template.firstChild)
        Template_domRemoveNode(template.firstChild);
    var t = Template_domCreateTextNode(Template_ownerDocument(template), value);
    Template_domAppendChild(template, t);
};

/**
 * Helps to implement the transclude attribute, and is the initial
 * call to get hold of a template from its ID.
 *
 * @param {String} name The ID of the HTML element used as template.
 *
 * @returns {Element} The DOM node of the template. (Only element
 * nodes can be found by ID, hence it's a Element.)
 */
function jstGetTemplate(name) {
    var section = Template_domGetElementById(document, name);
    if (section) {
        var ret = Template_domCloneNode(section);
        Template_domRemoveAttribute(ret, 'id');
        return ret;
    } else
        return null;
}

window['jstGetTemplate'] = jstGetTemplate;
window['jstProcess'] = jstProcess;
window['JsExprContext'] = JsExprContext;

function TemplateTest() {
    // Find the location to insert the content
    var tp = document.getElementById('benchmark_content');

    // Inject the content
    tp.innerHTML = content;

    // Run the template
    var cx = new JsExprContext(
    {"counters": [
        {"name":"Chrome:Init","time":5},
        {"delta":0,"name":"Shutdown:window_close:time","time":111,"value":1},
        {"delta":0,"name":" Shutdown:window_close:timeMA","value":111},
        {"delta":0,"name":"Shutdown:window_close:time_pe","time":111,"value":1},
        {"delta":0,"name":" Shutdown:window_close:time_p","value":111},
        {"delta":0,"name":"Shutdown:renderers:total","time":1,"value":1},
        {"delta":0,"name":" Shutdown:renderers:totalMAX","value":1},
        {"delta":0,"name":"Shutdown:renderers:slow","time":0,"value":1},
        {"delta":0,"name":" Shutdown:renderers:slowMAX","value":10},
        {"delta":0,"name":"DNS:PrefetchQueue","time":2,"value":6},
        {"delta":0,"name":" DNS:PrefetchQueueMAX","value":1},
        {"delta":0,"name":"DNS:PrefetchFoundNameL","time":1048,"value":1003},
        {"delta":0,"name":" DNS:PrefetchFoundNameLMAX","value":46},
        {"delta":0,"name":"SB:QueueDepth","time":0,"value":1},
        {"delta":0,"name":" SB:QueueDepthMAX","value":0},
        {"delta":102,"name":"IPC:SendMsgCount","value":1016},
        {"delta":98,"name":"Chrome:ProcMsgL UI","time":2777,"value":1378},
        {"delta":2381,"name":" Chrome:ProcMsgL UIMAX","value":2409},
        {"delta":98,"name":"Chrome:TotalMsgL UI","time":5715,"value":1378},
        {"delta":1518,"name":" Chrome:TotalMsgL UIMAX","value":2409},
        {"name":"Chrome:RendererInit","time":9},
        {"delta":0,"name":"WebFrameActiveCount","value":2},
        {"delta":0,"name":"Gears:LoadTime","time":1,"value":1},
        {"delta":0,"name":" Gears:LoadTimeMAX","value":1},
        {"delta":1,"name":"URLRequestCount","value":41},
        {"delta":1,"name":"mime_sniffer:ShouldSniffMimeT","time":27,"value":27},
        {"delta":0,"name":" mime_sniffer:ShouldSniffMime","value":1},
        {"delta":3,"name":"ResourceLoadServer","time":1065,"value":73},
        {"delta":0,"name":" ResourceLoadServerMAX","value":51},
        {"delta":11,"name":"WebFramePaintTime","time":232,"value":42},
        {"delta":9,"name":" WebFramePaintTimeMAX","value":41},
        {"delta":11,"name":"MPArch:RWH_OnMsgPaintRect","time":136,"value":42},
        {"delta":0,"name":" MPArch:RWH_OnMsgPaintRectMAX","value":9},
        {"delta":0,"name":"NPObjects","value":2},
        {"delta":6008832,"name":"V8:OsMemoryAllocated","value":28422144},
        {"delta":7905,"name":"V8:GlobalHandles","value":16832},
        {"delta":0,"name":"V8:PcreMallocCount","value":0},
        {"delta":1,"name":"V8:ObjectPropertiesToDictiona","value":16},
        {"delta":0,"name":"V8:ObjectElementsToDictionary","value":0},
        {"delta":1128652,"name":"V8:AliveAfterLastGC","value":4467596},
        {"delta":0,"name":"V8:ObjsSinceLastYoung","value":0},
        {"delta":0,"name":"V8:ObjsSinceLastFull","value":0},
        {"delta":2048,"name":"V8:SymbolTableCapacity","value":12288},
        {"delta":1493,"name":"V8:NumberOfSymbols","value":6865},
        {"delta":100442,"name":"V8:TotalExternalStringMemory","value":359184},
        {"delta":0,"name":"V8:ScriptWrappers","value":0},
        {"delta":3,"name":"V8:CallInitializeStubs","value":20},
        {"delta":0,"name":"V8:CallPreMonomorphicStubs","value":4},
        {"delta":0,"name":"V8:CallNormalStubs","value":0},
        {"delta":6,"name":"V8:CallMegamorphicStubs","value":44},
        {"delta":0,"name":"V8:ArgumentsAdaptors","value":0},
        {"delta":647,"name":"V8:CompilationCacheHits","value":1269},
        {"delta":9,"name":"V8:CompilationCacheMisses","value":57},
        {"delta":0,"name":"V8:RegExpCacheHits","value":2},
        {"delta":0,"name":"V8:RegExpCacheMisses","value":6},
        {"delta":6260,"name":"V8:TotalEvalSize","value":12621},
        {"delta":50221,"name":"V8:TotalLoadSize","value":217362},
        {"delta":63734,"name":"V8:TotalParseSize","value":299135},
        {"delta":15174,"name":"V8:TotalPreparseSkipped","value":61824},
        {"delta":69932,"name":"V8:TotalCompileSize","value":313048},
        {"delta":22,"name":"V8:CodeStubs","value":117},
        {"delta":1185,"name":"V8:TotalStubsCodeSize","value":6456},
        {"delta":45987,"name":"V8:TotalCompiledCodeSize","value":169546},
        {"delta":0,"name":"V8:GCCompactorCausedByRequest","value":0},
        {"delta":0,"name":"V8:GCCompactorCausedByPromote","value":0},
        {"delta":0,"name":"V8:GCCompactorCausedByOldspac","value":0},
        {"delta":0,"name":"V8:GCCompactorCausedByWeakHan","value":0},
        {"delta":0,"name":"V8:GCLastResortFromJS","value":0},
        {"delta":0,"name":"V8:GCLastResortFromHandles","value":0},
        {"delta":0,"name":"V8:KeyedLoadGenericSmi","value":0},
        {"delta":0,"name":"V8:KeyedLoadGenericSymbol","value":0},
        {"delta":0,"name":"V8:KeyedLoadGenericSlow","value":0},
        {"delta":0,"name":"V8:KeyedLoadFunctionPrototype","value":0},
        {"delta":0,"name":"V8:KeyedLoadStringLength","value":0},
        {"delta":0,"name":"V8:KeyedLoadArrayLength","value":0},
        {"delta":0,"name":"V8:KeyedLoadConstantFunction","value":0},
        {"delta":0,"name":"V8:KeyedLoadField","value":0},
        {"delta":0,"name":"V8:KeyedLoadCallback","value":0},
        {"delta":0,"name":"V8:KeyedLoadInterceptor","value":0},
        {"delta":0,"name":"V8:KeyedStoreField","value":0},
        {"delta":0,"name":"V8:ForIn","value":0},
        {"delta":2,"name":"V8:EnumCacheHits","value":9},
        {"delta":4,"name":"V8:EnumCacheMisses","value":23},
        {"delta":3724,"name":"V8:RelocInfoCount","value":18374},
        {"delta":6080,"name":"V8:RelocInfoSize","value":30287},
        {"delta":0,"name":"History:InitTime","time":12,"value":1},
        {"delta":0,"name":" History:InitTimeMAX","value":12},
        {"delta":1,"name":"History:GetFavIconForURL","time":0,"value":22},
        {"delta":0,"name":" History:GetFavIconForURLMAX","value":0},
        {"delta":2,"name":"V8:PreParse","time":9,"value":11},
        {"delta":9,"name":"V8:Parse","time":9,"value":57},
        {"delta":3,"name":"V8:Compile","time":3,"value":22},
        {"delta":49,"name":"V8:ParseLazy","time":17,"value":231},
        {"delta":47,"name":"V8:CompileLazy","time":3,"value":221},
        {"delta":12,"name":"V8:GCScavenger","time":13,"value":28},
        {"delta":0,"name":"NewTabPage:SearchURLs:Total","time":0,"value":1},
        {"delta":0,"name":" NewTabPage:SearchURLs:TotalM","value":0},
        {"delta":6,"name":"V8:CompileEval","time":1,"value":35},
        {"delta":0,"name":"Memory:CachedFontAndDC","time":3,"value":3},
        {"delta":0,"name":" Memory:CachedFontAndDCMAX","value":2},
        {"delta":0,"name":"ResourceLoaderWait","time":1296,"value":48},
        {"delta":0,"name":" ResourceLoaderWaitMAX","value":55},
        {"delta":0,"name":"History:GetPageThumbnail","time":15,"value":9},
        {"delta":0,"name":" History:GetPageThumbnailMAX","value":10},
        {"delta":9,"name":"MPArch:RWH_InputEventDelta","time":327,"value":170},
        {"delta":0,"name":" MPArch:RWH_InputEventDeltaMA","value":154},
        {"delta":0,"name":"Omnibox:QueryBookmarksTime","time":2,"value":44},
        {"delta":0,"name":" Omnibox:QueryBookmarksTimeMA","value":1},
        {"delta":0,"name":"Chrome:DelayMsgUI","value":3},
        {"delta":0,"name":"Autocomplete:HistoryAsyncQuer","time":351,"value":86},
        {"delta":0,"name":" Autocomplete:HistoryAsyncQue","value":10},
        {"delta":0,"name":"History:QueryHistory","time":1018,"value":44},
        {"delta":0,"name":" History:QueryHistoryMAX","value":233},
        {"delta":0,"name":"DiskCache:GetFileForNewBlock","time":0,"value":34},
        {"delta":0,"name":" DiskCache:GetFileForNewBlock","value":0},
        {"delta":0,"name":"DiskCache:CreateBlock","time":0,"value":34},
        {"delta":0,"name":" DiskCache:CreateBlockMAX","value":0},
        {"delta":0,"name":"DiskCache:CreateTime","time":0,"value":10},
        {"delta":0,"name":" DiskCache:CreateTimeMAX","value":0},
        {"delta":0,"name":"DNS:PrefetchPositiveHitL","time":1048,"value":2},
        {"delta":0,"name":" DNS:PrefetchPositiveHitLMAX","value":1002},
        {"delta":0,"name":"DiskCache:GetRankings","time":0,"value":27},
        {"delta":0,"name":" DiskCache:GetRankingsMAX","value":0},
        {"delta":0,"name":"DiskCache:DeleteHeader","time":0,"value":3},
        {"delta":0,"name":" DiskCache:DeleteHeaderMAX","value":0},
        {"delta":0,"name":"DiskCache:DeleteData","time":0,"value":3},
        {"delta":0,"name":" DiskCache:DeleteDataMAX","value":0},
        {"delta":0,"name":"DiskCache:DeleteBlock","time":0,"value":6},
        {"delta":0,"name":" DiskCache:DeleteBlockMAX","value":0},
        {"delta":0,"name":"SessionRestore:last_session_f","time":0,"value":1},
        {"delta":0,"name":" SessionRestore:last_session_","value":0},
        {"delta":3,"name":"SessionRestore:command_size","time":2940,"value":36},
        {"delta":0,"name":" SessionRestore:command_sizeM","value":277},
        {"delta":0,"name":"DNS:IndependentNavigation","time":2,"value":4},
        {"delta":0,"name":" DNS:IndependentNavigationMAX","value":1},
        {"delta":0,"name":"DiskCache:UpdateRank","time":1,"value":25},
        {"delta":0,"name":" DiskCache:UpdateRankMAX","value":1},
        {"delta":0,"name":"DiskCache:WriteTime","time":1,"value":21},
        {"delta":0,"name":" DiskCache:WriteTimeMAX","value":1},
        {"delta":0,"name":"Net:Transaction_Latency","time":183,"value":7},
        {"delta":0,"name":" Net:Transaction_LatencyMAX","value":37},
        {"delta":0,"name":"Net:Transaction_Bandwidth","time":40,"value":7},
        {"delta":0,"name":" Net:Transaction_BandwidthMAX","value":8},
        {"delta":0,"name":"NewTabUI load","time":564,"value":1},
        {"delta":0,"name":" NewTabUI loadMAX","value":564},
        {"delta":0,"name":"DiskCache:OpenTime","time":0,"value":2},
        {"delta":0,"name":" DiskCache:OpenTimeMAX","value":0},
        {"delta":0,"name":"DiskCache:ReadTime","time":0,"value":4},
        {"delta":0,"name":" DiskCache:ReadTimeMAX","value":0},
        {"delta":0,"name":"MPArch:RWHH_WhiteoutDuration_","time":27,"value":1},
        {"delta":0,"name":" MPArch:RWHH_WhiteoutDuration","value":27},
        {"delta":1,"name":"AsyncIO:IPCChannelClose","time":0,"value":4},
        {"delta":0,"name":" AsyncIO:IPCChannelCloseMAX","value":0},
        {"name":"GetHistoryTimer","time":0},
        {"delta":0,"name":"DiskCache:Entries","time":7,"value":1},
        {"delta":0,"name":" DiskCache:EntriesMAX","value":7},
        {"delta":0,"name":"DiskCache:Size","time":0,"value":1},
        {"delta":0,"name":" DiskCache:SizeMAX","value":0},
        {"delta":0,"name":"DiskCache:MaxSize","time":80,"value":1},
        {"delta":0,"name":" DiskCache:MaxSizeMAX","value":80},
        {"delta":0,"name":"History:AddFTSData","time":1,"value":1},
        {"delta":0,"name":" History:AddFTSDataMAX","value":1},
        {"delta":0,"name":"Chrome:SlowMsgUI","value":1}
    ],
    "timers":[
        {"name":"Chrome:Init","time":5},
        {"delta":0,"name":"Shutdown:window_close:time","time":111,"value":1},
        {"delta":0,"name":"Shutdown:window_close:time_pe","time":111,"value":1},
        {"delta":0,"name":"Shutdown:renderers:total","time":1,"value":1},
        {"delta":0,"name":"Shutdown:renderers:slow","time":0,"value":1},
        {"delta":0,"name":"DNS:PrefetchQueue","time":2,"value":6},
        {"delta":0,"name":"DNS:PrefetchFoundNameL","time":1048,"value":1003},
        {"delta":0,"name":"SB:QueueDepth","time":0,"value":1},
        {"delta":98,"name":"Chrome:ProcMsgL UI","time":2777,"value":1378},
        {"delta":98,"name":"Chrome:TotalMsgL UI","time":5715,"value":1378},
        {"name":"Chrome:RendererInit","time":9},
        {"delta":0,"name":"Gears:LoadTime","time":1,"value":1},
        {"delta":1,"name":"mime_sniffer:ShouldSniffMimeT","time":27,"value":27},
        {"delta":3,"name":"ResourceLoadServer","time":1065,"value":73},
        {"delta":11,"name":"WebFramePaintTime","time":232,"value":42},
        {"delta":11,"name":"MPArch:RWH_OnMsgPaintRect","time":136,"value":42},
        {"delta":0,"name":"History:InitTime","time":12,"value":1},
        {"delta":1,"name":"History:GetFavIconForURL","time":0,"value":22},
        {"delta":2,"name":"V8:PreParse","time":9,"value":11},
        {"delta":9,"name":"V8:Parse","time":9,"value":57},
        {"delta":3,"name":"V8:Compile","time":3,"value":22},
        {"delta":49,"name":"V8:ParseLazy","time":17,"value":231},
        {"delta":47,"name":"V8:CompileLazy","time":3,"value":221},
        {"delta":12,"name":"V8:GCScavenger","time":13,"value":28},
        {"delta":0,"name":"NewTabPage:SearchURLs:Total","time":0,"value":1},
        {"delta":6,"name":"V8:CompileEval","time":1,"value":35},
        {"delta":0,"name":"Memory:CachedFontAndDC","time":3,"value":3},
        {"delta":0,"name":"ResourceLoaderWait","time":1296,"value":48},
        {"delta":0,"name":"History:GetPageThumbnail","time":15,"value":9},
        {"delta":9,"name":"MPArch:RWH_InputEventDelta","time":327,"value":170},
        {"delta":0,"name":"Omnibox:QueryBookmarksTime","time":2,"value":44},
        {"delta":0,"name":"Autocomplete:HistoryAsyncQuer","time":351,"value":86},
        {"delta":0,"name":"History:QueryHistory","time":1018,"value":44},
        {"delta":0,"name":"DiskCache:GetFileForNewBlock","time":0,"value":34},
        {"delta":0,"name":"DiskCache:CreateBlock","time":0,"value":34},
        {"delta":0,"name":"DiskCache:CreateTime","time":0,"value":10},
        {"delta":0,"name":"DNS:PrefetchPositiveHitL","time":1048,"value":2},
        {"delta":0,"name":"DiskCache:GetRankings","time":0,"value":27},
        {"delta":0,"name":"DiskCache:DeleteHeader","time":0,"value":3},
        {"delta":0,"name":"DiskCache:DeleteData","time":0,"value":3},
        {"delta":0,"name":"DiskCache:DeleteBlock","time":0,"value":6},
        {"delta":0,"name":"SessionRestore:last_session_f","time":0,"value":1},
        {"delta":3,"name":"SessionRestore:command_size","time":2940,"value":36},
        {"delta":0,"name":"DNS:IndependentNavigation","time":2,"value":4},
        {"delta":0,"name":"DiskCache:UpdateRank","time":1,"value":25},
        {"delta":0,"name":"DiskCache:WriteTime","time":1,"value":21},
        {"delta":0,"name":"Net:Transaction_Latency","time":183,"value":7},
        {"delta":0,"name":"Net:Transaction_Bandwidth","time":40,"value":7},
        {"delta":0,"name":"NewTabUI load","time":564,"value":1},
        {"delta":0,"name":"DiskCache:OpenTime","time":0,"value":2},
        {"delta":0,"name":"DiskCache:ReadTime","time":0,"value":4},
        {"delta":0,"name":"MPArch:RWHH_WhiteoutDuration_","time":27,"value":1},
        {"delta":1,"name":"AsyncIO:IPCChannelClose","time":0,"value":4},
        {"name":"GetHistoryTimer","time":0},
        {"delta":0,"name":"DiskCache:Entries","time":7,"value":1},
        {"delta":0,"name":"DiskCache:Size","time":0,"value":1},
        {"delta":0,"name":"DiskCache:MaxSize","time":80,"value":1},
        {"delta":0,"name":"History:AddFTSData","time":1,"value":1}
    ]});
    jstProcess(cx, tp);
}

var TemplateTest = new BenchmarkSuite('Template', [new Benchmark("Template",TemplateTest)]);
