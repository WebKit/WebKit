<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output method="xml" encoding="UTF-8"/>
    <xsl:template match="TEST">
        <html xmlns="http://www.w3.org/1999/xhtml">
            <body>
          		<p>You should see error text above this</p>
            </body>
		</html>
		<p>this, and nothing below it should be rendered</p>
		furtermore, this should be invalid.
  </xsl:template>
</xsl:stylesheet>