function parse_webkit_version(version) {
  var bits = version.split(".");
  var is_nightly = (version[version.length - 1] == "+");
  if (is_nightly) {
    var minor = "+";
  } else {
    var minor = parseInt(bits[1]);
    // If minor is Not a Number (NaN) return an empty string
    if (isNaN(minor)) {
      minor = "";
    }
  }
  return {major: parseInt(bits[0]), minor: minor, is_nightly: is_nightly};
}

function get_webkit_version() {
  var regex = new RegExp("\\(.*\\) AppleWebKit/(.*) \\((.*)");
  var matches = regex.exec(navigator.userAgent);
  if (matches) {
    var webkit_version = parse_webkit_version(matches[1]);    
  } 
  return {major: webkit_version['major'], minor: webkit_version['minor'], is_nightly: webkit_version['is_nightly']};
}  