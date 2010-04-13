function canonicalize(url)
{
  var a = document.createElement("a");
  a.href = url;
  return a.href;
}
