<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output method="html" encoding="iso8859-1"/>
    <xsl:template match="TEST">
        <html>
        <head>
        <title>document.URL</title>
        </head>
        <body>
        <script>
        if (window.testRunner)
            testRunner.dumpAsText();

        if (document.URL.length > 0)
            document.write("<p>Success</p>");
        else
            document.write("<p>Failure, document.URL is empty.</p>");
        </script>
        </body>
        </html>
    </xsl:template>
</xsl:stylesheet>
