<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="/">
  <html>
    <xsl:apply-templates/>
	<xsl:apply-templates select="document('resources/document-function-source.xml')/doc"/>
  </html>
</xsl:template>

<xsl:template match="para">
<p><xsl:value-of select="."/></p>
</xsl:template>
</xsl:stylesheet>
