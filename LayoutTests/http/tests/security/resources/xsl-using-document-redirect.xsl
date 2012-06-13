<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="*">
<html>
<body>
<script>
if (window.testRunner)
  testRunner.dumpAsText();
</script>
<div>This test includes content via a cross-origin document() command.  It
passes if the load fails and thus there is no text below this line.</div>
<xsl:value-of select="document('redir.php?url=http://localhost:8000/security/resources/target.xml')"/>
</body>
</html>
</xsl:template>
</xsl:stylesheet>

