<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output method="html" />

<xsl:template match="image">
    <img src="{.}" />
</xsl:template>

<xsl:template match="notifyDone">
    <script>
    if (window.testRunner)
        testRunner.notifyDone();
    </script>
</xsl:template>

</xsl:stylesheet>
