<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:template match="/">
    <html>
      <title>Checks that an XSLT-generated HTML doc does not circumvent third-party cookie rules</title>
      <script>
if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.setAlwaysAcceptCookies(false);
}
      </script>
      <body>
        <iframe src="http://localhost:8000/security/cookies/resources/set-a-cookie.php"></iframe>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>
