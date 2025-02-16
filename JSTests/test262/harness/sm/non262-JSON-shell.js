/*---
defines: [testJSON]
allow_unused: True
---*/
function testJSON(str, expectSyntaxError)
{
  // Leading and trailing whitespace never affect parsing, so test the string
  // multiple times with and without whitespace around it as it's easy and can
  // potentially detect bugs.

  // Try the provided string
  try
  {
    JSON.parse(str);
    reportCompare(false, expectSyntaxError,
                  "string <" + str + "> " +
                  "should" + (expectSyntaxError ? "n't" : "") + " " +
                  "have parsed as JSON");
  }
  catch (e)
  {
    if (!(e instanceof SyntaxError))
    {
      reportCompare(true, false,
                    "parsing string <" + str + "> threw a non-SyntaxError " +
                    "exception: " + e);
    }
    else
    {
      reportCompare(true, expectSyntaxError,
                    "string <" + str + "> " +
                    "should" + (expectSyntaxError ? "n't" : "") + " " +
                    "have parsed as JSON, exception: " + e);
    }
  }

  // Now try the provided string with trailing whitespace
  try
  {
    JSON.parse(str + " ");
    reportCompare(false, expectSyntaxError,
                  "string <" + str + " > " +
                  "should" + (expectSyntaxError ? "n't" : "") + " " +
                  "have parsed as JSON");
  }
  catch (e)
  {
    if (!(e instanceof SyntaxError))
    {
      reportCompare(true, false,
                    "parsing string <" + str + " > threw a non-SyntaxError " +
                    "exception: " + e);
    }
    else
    {
      reportCompare(true, expectSyntaxError,
                    "string <" + str + " > " +
                    "should" + (expectSyntaxError ? "n't" : "") + " " +
                    "have parsed as JSON, exception: " + e);
    }
  }

  // Now try the provided string with leading whitespace
  try
  {
    JSON.parse(" " + str);
    reportCompare(false, expectSyntaxError,
                  "string < " + str + "> " +
                  "should" + (expectSyntaxError ? "n't" : "") + " " +
                  "have parsed as JSON");
  }
  catch (e)
  {
    if (!(e instanceof SyntaxError))
    {
      reportCompare(true, false,
                    "parsing string < " + str + "> threw a non-SyntaxError " +
                    "exception: " + e);
    }
    else
    {
      reportCompare(true, expectSyntaxError,
                    "string < " + str + "> " +
                    "should" + (expectSyntaxError ? "n't" : "") + " " +
                    "have parsed as JSON, exception: " + e);
    }
  }

  // Now try the provided string with whitespace surrounding it
  try
  {
    JSON.parse(" " + str + " ");
    reportCompare(false, expectSyntaxError,
                  "string < " + str + " > " +
                  "should" + (expectSyntaxError ? "n't" : "") + " " +
                  "have parsed as JSON");
  }
  catch (e)
  {
    if (!(e instanceof SyntaxError))
    {
      reportCompare(true, false,
                    "parsing string < " + str + " > threw a non-SyntaxError " +
                    "exception: " + e);
    }
    else
    {
      reportCompare(true, expectSyntaxError,
                    "string < " + str + " > " +
                    "should" + (expectSyntaxError ? "n't" : "") + " " +
                    "have parsed as JSON, exception: " + e);
    }
  }
}
