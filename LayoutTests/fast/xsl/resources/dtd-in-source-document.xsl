<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
        <xsl:output method="html" encoding="UTF-8" />
        <xsl:template match="/TEST">
            
<html>
<head>
    <script>
        if (window.testRunner)
            testRunner.dumpAsText();
    </script>
</head>
<body>
<div>This tests that entities defined in an external DTD referenced to from the source document are replaced correctly. 
If this test is successful, the text SUCCESS should be shown below.</div>
<div><xsl:value-of select="."/></div>
</body>
</html>
</xsl:template>
</xsl:stylesheet>
