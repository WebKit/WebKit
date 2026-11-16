<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="/">
<html><body><script>parent.postMessage("transformed", "*");</script></body></html>
</xsl:template>
</xsl:stylesheet>
