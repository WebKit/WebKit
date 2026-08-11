<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="html"/>
  <xsl:template match="/">
    <html>
      <body>
        <p>Transformed: <xsl:value-of select="/doc/example"/></p>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>
