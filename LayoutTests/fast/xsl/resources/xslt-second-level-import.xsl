<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE xsl:stylesheet SYSTEM "resources/xslt-second-level-import.xsl.dtd">
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:include href="xslt-second-level-import2.xsl" />
  <xsl:template match="/">
    <script>
        if (window.layoutTestController)
            layoutTestController.dumpAsText();
    </script>
    <p>&success;</p>
  </xsl:template>
</xsl:stylesheet>
