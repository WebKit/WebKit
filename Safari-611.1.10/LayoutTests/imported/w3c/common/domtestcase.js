/*
Copyright (c) 2001-2004 World Wide Web Consortium,
(Massachusetts Institute of Technology, Institut National de
Recherche en Informatique et en Automatique, Keio University). All
Rights Reserved. This program is distributed under the W3C's Software
Intellectual Property License. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.
See W3C License http://www.w3.org/Consortium/Legal/ for more details.
*/
  
//
// Log to console wrapper, use this so one doesn't create script errors on
// user agents that don't support console.log
function consoleLog(description)
{
    try
	{
	    console.log(description);
	}
	catch(e)
	{
	    alert(description);
	}
	
}

function assertEquals(description, expected, actual)
{
    var szError = description + " assertEquals failure: \r\n";

    if ( !(expected === actual))
    {
        szError = szError + "expected=" + expected + " \r\n";
        szError = szError + "actual=" + actual + " \r\n";

        throw szError;
    }
}