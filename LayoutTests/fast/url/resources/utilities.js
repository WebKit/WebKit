// Start the bidding at 42 for no particular reason.
var lastID = 42;

function canonicalize(url)
{
  // It would be more elegant to use the DOM here, but we use document.write()
  // so the tests run correctly in Firefox.
  var id = ++lastID;
  document.write("<a id='" + id + "' href='" + url + "'></a>");
  return document.getElementById(id).href;
}

function setBaseURL(url)
{
  // According to the HTML5 spec, we're only supposed to honor <base> elements
  // in the <head>, but we use document.write() here to make the test run in
  // Firefox.
  document.write('<base href="' + url + '">');
}

function segments(url)
{
  // It would be more elegant to use the DOM here, but we use document.write()
  // so the tests run correctly in Firefox.
  var id = ++lastID;
  document.write("<a id='" + id + "' href='" + url + "'></a>");
  var elmt = document.getElementById(id);
  return JSON.stringify([
    elmt.protocol,
    elmt.hostname,
    elmt.port,
    elmt.pathname,
    elmt.search,
    elmt.hash
  ]);
}
