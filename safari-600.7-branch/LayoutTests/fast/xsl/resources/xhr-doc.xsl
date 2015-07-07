<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns="http://www.w3.org/1999/xhtml">

<xsl:import href="xhr-doc-imported.xsl" />

<xsl:template match="/root">
    <xsl:call-template name="works" />
</xsl:template>

</xsl:stylesheet>
