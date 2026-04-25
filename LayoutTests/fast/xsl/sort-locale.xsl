<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="/">
  <html>
  <body>
    <script>
      if (window.testRunner)
        testRunner.dumpAsText();
    </script>
    <h2>en</h2>
    <table border="1">
      <xsl:for-each select="list/item">
          <xsl:sort select="." data-type="text" order="ascending" lang="en" />
          <tr>
            <td><xsl:value-of select="."/></td>
          </tr>
      </xsl:for-each>
    </table>
    <h2>fr</h2>
    <table border="1">
      <xsl:for-each select="list/item">
          <xsl:sort select="." data-type="text" order="ascending" lang="fr" />
          <tr>
            <td><xsl:value-of select="."/></td>
          </tr>
      </xsl:for-each>
    </table>
    <h2>fr-CA</h2>
    <table border="1">
      <xsl:for-each select="list/item">
          <xsl:sort select="." data-type="text" order="ascending" lang="fr-CA" />
          <tr>
            <td><xsl:value-of select="."/></td>
          </tr>
      </xsl:for-each>
    </table>
  </body>
  </html>
</xsl:template>
</xsl:stylesheet>
