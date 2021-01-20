// Start the bidding at 42 for no particular reason.
const FILE_PROTOCOL = "file:";

var lastID = 42;

var g_shouldEllipsizeFileURLPaths = false;

function setShouldEllipsizeFileURLPaths(shouldEllipsizeFileURLPaths)
{
  g_shouldEllipsizeFileURLPaths = true;
}

// Simplifies file URL comparisons by ellipsizing everything except the basename of the file,
// and normalizing to a Unix-style path. Note that this algorithm does not preserve path
// hierarchy (e.g. file:///a => file:///.../a), but it is good enough for our purposes.
function canonicalizedPathname(pathname)
{
  const NOT_FOUND = 0;
  let positionAfterLastSlash = pathname.lastIndexOf("\\") + 1; // Windows path separator
  if (positionAfterLastSlash === NOT_FOUND)
    positionAfterLastSlash = pathname.lastIndexOf("/") + 1;
  return "/.../" + (positionAfterLastSlash === NOT_FOUND ? pathname : pathname.substr(positionAfterLastSlash));
}

function canonicalize(url)
{
  // It would be more elegant to use the DOM here, but we use document.write()
  // so the tests run correctly in Firefox.
  var id = ++lastID;
  document.write("<a id='" + id + "' href='" + url + "'></a>");
  let result = document.getElementById(id).href;
  if (url !== "." && g_shouldEllipsizeFileURLPaths && result.startsWith(FILE_PROTOCOL))
    return FILE_PROTOCOL + "//" + canonicalizedPathname(result.substr(FILE_PROTOCOL.length));
  return result;
}

function setBaseURL(url)
{
    // It would be more elegant to use the DOM here, but we chose document.write()
    // so the tests ran correctly in Firefox at the time we originally wrote them.

    // Remove any existing base elements.
    var existingBase = document.getElementsByTagName('base');
    while (existingBase.length) {
        var element = existingBase[0];
        element.parentNode.removeChild(element);
    }

    // Add a new base element.
    document.write('<base href="' + url + '">');
}

function segments(url)
{
  // It would be more elegant to use the DOM here, but we use document.write()
  // so the tests run correctly in Firefox.
  var id = ++lastID;
  document.write("<a id='" + id + "' href='" + url + "'></a>");
  let link = document.getElementById(id);
  let pathname = g_shouldEllipsizeFileURLPaths && link.protocol === FILE_PROTOCOL ? canonicalizedPathname(link.pathname) : link.pathname;
  return JSON.stringify([link.protocol, link.hostname, link.port, pathname, link.search, link.hash]);
}
