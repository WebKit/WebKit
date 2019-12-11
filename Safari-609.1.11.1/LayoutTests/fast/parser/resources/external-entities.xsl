<?xml version="1.0" encoding="utf-8" ?>
<!DOCTYPE xsl:stylesheet SYSTEM "external-entities.dtd">
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:template match="/">
        <HTML>
        <SCRIPT type="text/javascript">
        if (window.testRunner)
            testRunner.dumpAsText();
        </SCRIPT>
            <BODY>
               &message;
            </BODY>
        </HTML>
    </xsl:template>
</xsl:stylesheet>