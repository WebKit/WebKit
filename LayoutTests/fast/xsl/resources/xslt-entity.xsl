<?xml version="1.0" encoding="utf-8" ?>
<!DOCTYPE test-document [
<!ENTITY HELLO "Hello World, Success!">
]>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:template match="/">
		<P>&HELLO;</P>
    </xsl:template>
</xsl:stylesheet>

