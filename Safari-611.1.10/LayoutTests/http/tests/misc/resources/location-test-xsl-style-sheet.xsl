<?xml version="1.0" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="/">

<html>
   <head>

      <script language="JavaScript">

		function _onLoad()
		{
            if (location.href == "about:blank")
                document.getElementById("fail").innerHTML = "<span>FAIL</span> - location.href should not be about:blank";
            else
                document.getElementById("pass").innerHTML = "<span>PASS</span>";

            if (window.testRunner)
                testRunner.dumpAsText();
        }

     </script>

   </head>

   <body onload="_onLoad();"><div id="fail" style="color: red;"></div><div id="pass" style="color: green;"></div></body>
 </html>

</xsl:template>

</xsl:stylesheet>