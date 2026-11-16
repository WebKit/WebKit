<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
	<xsl:output method="text" encoding="UTF-8" />
    <xsl:param name="param">
    	FAILURE
	</xsl:param>
	<xsl:template match="TEST">
		<xsl:value-of select="$param" />
	</xsl:template>
</xsl:stylesheet>
