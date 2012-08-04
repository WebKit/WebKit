<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="html"/>
  <xsl:template match="test">
    <script>
      if (window.testRunner)
          testRunner.dumpAsText();
    </script>
    PASS
  </xsl:template>
</xsl:stylesheet> 
