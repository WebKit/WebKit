<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output method="html" encoding="iso8859-1"/>
    <xsl:template match="TEST">
        <html>
        <head>
        <title>XSLT Encodings</title>
        </head>
        <body>
            <script>
               if (window.testRunner)
                   testRunner.dumpAsText();
            </script>
            <p>The letters in quotes should look similar (first is Roman, second is Cyrillic): "B", "В".</p>
        </body>
        </html>
    </xsl:template>
</xsl:stylesheet>
