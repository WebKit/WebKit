<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:template match="/">
    <html>
      <body>
        <script>if (window.testRunner) testRunner.dumpAsText();</script>
        <div id="mydiv">
           <p>Tests a crash resulting from a string literal in a nested XSL stylesheet. If you reached
           here without crashing, the test passed.  See https://bugs.webkit.org/show_bug.cgi?id=15715 .</p>
           <p>SUCCESS</p>
        </div>
      </body>
    </html>
  </xsl:template>

</xsl:stylesheet>
