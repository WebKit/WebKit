/*

var enc = encodeURI;
var dec = decodeURI;
//var enc = encodeURIComponent;
//var dec = decodeURIComponent;

function printstr(i,j)
{
  var str;
  if (j == undefined)
    str = "x"+String.fromCharCode(i)+"x";
  else
    str = "x"+String.fromCharCode(i)+String.fromCharCode(j)+"x";
  var encoded = "(exception)";
  var decoded = "(unknown)";
  try {
    encoded = enc(str);
    decoded = "(exception)";
    decoded = dec(encoded);
  }
  catch (e) {
  }

  var prefix = i;
  if (j != undefined)
    prefix += "/" + j;

  var match;
  if (encoded == "(exception)" || decoded == "(exception)")
    match = "";
  else if (str == decoded)
    match = " (match)";
  else
    match = " (nomatch)";

  print(prefix+": encoded "+encoded+" decoded "+decoded+" "+match);
}

for (var charcode = 0; charcode <= 0xFFFF; charcode++)
  printstr(charcode);

for (var charcode = 0xDC00; charcode <= 0xDFFF; charcode++)
  printstr(0xD800,charcode);

for (var charcode = 0xD800; charcode <= 0xDBFF; charcode++)
  printstr(charcode,0xD800);

*/

// --------------------------------------------------------------------------------

var resolution = 13; // set to 1 for 100% coverage

function checkEncodeException(encodeFunction,c1,c2)
{
  var str;
  if (c2 == undefined)
    str = "x"+String.fromCharCode(c1)+"x";
  else
    str = "x"+String.fromCharCode(c1)+String.fromCharCode(c2)+"x";

  try {
    encodeFunction(str);
//    print("checkEncodeException("+c1+","+c2+"): false");
    return false;
  }
  catch (e) {
//    print("checkEncodeException("+c1+","+c2+"): true");
    return true;
  }
}

function checkEncodeDecode(encodeFunction,decodeFunction,c1,c2)
{
  var str;
  if (c2 == undefined)
    str = "x"+String.fromCharCode(c1)+"x";
  else
    str = "x"+String.fromCharCode(c1)+String.fromCharCode(c2)+"x";

  try {
    var encoded = encodeFunction(str);
    var decoded = decodeFunction(encoded);

//    print("checkEncodeDecode("+c1+","+c2+"): "+(str == decoded));
    return (str == decoded);
  }
  catch (e) {
//    print("checkEncodeDecode("+c1+","+c2+"): false (exception)");
    return false;
  }
}



function checkWithFunctions(encodeFunction,decodeFunction)
{
  var passes = 0;
  var failures = 0;

  // 0-0xD800 and 0xDC00-0xFFF

  for (var charcode = 0; charcode < 0xD800; charcode += resolution) {
    if (checkEncodeDecode(encodeFunction,decodeFunction,charcode))
      passes++;
    else
      failures++;
  }

  for (var charcode = 0xE000; charcode <= 0xFFFF; charcode += resolution) {
    if (checkEncodeDecode(encodeFunction,decodeFunction,charcode))
      passes++;
    else
      failures++;
  }

  // 0xDC00-0xDFFF

  for (var charcode = 0xDC00; charcode <= 0xDFFF; charcode += resolution) {
    if (checkEncodeException(encodeFunction,charcode))
      passes++;
    else
      failures++;
  }

  // 0xD800-0xDBFF followed by 0xDC00-0xDFFF

  for (var charcode = 0xD800; charcode < 0xDC00; charcode += resolution) {
    if (checkEncodeDecode(encodeFunction,decodeFunction,charcode,0xDC00))
      passes++;
    else
      failures++;
  }

  for (var charcode = 0xDC00; charcode < 0xE000; charcode += resolution) {
    if (checkEncodeDecode(encodeFunction,decodeFunction,0xD800,charcode))
      passes++;
    else
      failures++;
  }

  // 0xD800-0xDBFF _not_ followed by 0xDC00-0xDFFF

  for (var charcode = 0; charcode < 0xDC00; charcode += resolution) {
    if (checkEncodeException(encodeFunction,0xD800,charcode))
      passes++;
    else
      failures++;
  }

  for (var charcode = 0xE000; charcode <= 0xFFFF; charcode += resolution) {
    if (checkEncodeException(encodeFunction,0xD800,charcode))
      passes++;
    else
      failures++;
  }

//  print("passes = "+passes);
//  print("failures = "+failures);

  return (failures == 0);
}






shouldBe("checkWithFunctions(encodeURI,decodeURI)","true");
shouldBe("checkWithFunctions(encodeURIComponent,decodeURIComponent)","true");
successfullyParsed = true
