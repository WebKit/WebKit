<xsl:stylesheet version="1.0" 
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		xmlns:exslt="http://exslt.org/common"
		exclude-result-prefixes="exslt">
  
<xsl:variable name="x">
  <y/>
</xsl:variable>

<xsl:template match="doc">
  <html>
    <body>
      <script>if (window.testRunner) testRunner.dumpAsText();</script>
      <p>Test that exslt:node-set() function is supported.</p>
      <xsl:apply-templates select="exslt:node-set($x)/*"/>
    </body>
  </html>
</xsl:template>

<xsl:template match="y">
  <p>SUCCESS</p>
</xsl:template>

</xsl:stylesheet>
