// Library for wraping JavaScript code for different evaluation contexts.

var libwrapjs = {
  transform_each_character: function(str, transform) {
    var result = new Array();
    for (var i=0; i < str.length; ++i)
      result.push(transform(str.charAt(i)));
    return result.join('');
  },

  escape_for_single_quote: function(str) {
    function transform(ch) {
      if (ch == "\\")
        return "\\\\";
      if (ch == "/")
        return "\\/";
      if (ch == "'")
        return "\\'";
      return ch;
    }
    return this.transform_each_character(str, transform);
  },

  escape_for_html: function(str) {
    function transform(ch) {
      if (ch == "<")
        return "&lt;";
      if (ch == ">")
        return "&gt;";
      if (ch == "&")
        return "&amp;";
      if (ch == "\n")
        return "<br />"
      if (ch == "\"")
        return "&quot;";
      return ch;
    }
    return this.transform_each_character(str, transform);
  },

  in_string: function(code) {
    return "'" + this.escape_for_single_quote(code) + "'";
  },

  in_script_tag: function(code) {
    return "<script>" + code + "</scr" + "ipt>";
  },

  in_document_write: function(code) {
    return "document.write(" + this.in_string(this.in_script_tag(code)) + ")";
  },

  in_javascript_url: function(code) {
    return this.in_string("javascript:void(" + code + ")");
  },

  in_javascript_document: function(code) {
    return this.in_string("javascript:" +
               this.in_string(this.in_script_tag(code)));
  }
};

