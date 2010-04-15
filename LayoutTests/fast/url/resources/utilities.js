function canonicalize(url)
{
  var a = document.createElement("a");
  a.href = url;
  return a.href;
}

function setBaseURL(url)
{
  var base = document.createElement("base");
  base.href = url;
  document.documentElement.appendChild(base);
}
