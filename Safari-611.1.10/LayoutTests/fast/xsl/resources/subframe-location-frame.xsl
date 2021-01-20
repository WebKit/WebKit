<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

    <xsl:output method="html"/>

    <xsl:template match="/">

<html>
<head>
<title>Frame 2</title>
</head>
<body>
<script>
alert(/resources/.test(document.location) ? "SUCCESS" : ("FAILURE: " + document.location));
</script>
</body>
</html>

    </xsl:template>

</xsl:stylesheet>
