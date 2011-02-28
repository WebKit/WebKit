<?xml version='1.0' encoding='utf-8'?>
<!--
   - Copyright (C) 2011 Google Inc. All rights reserved.
   -
   - Redistribution and use in source and binary forms, with or without
   - modification, are permitted provided that the following conditions are
   - met:
   -
   - 1. Redistributions of source code must retain the above copyright
   - notice, this list of conditions and the following disclaimer.
   -
   - 2. Redistributions in binary form must reproduce the above
   - copyright notice, this list of conditions and the following disclaimer
   - in the documentation and/or other materials provided with the
   - distribution.
   -
   - THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
   - “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   - LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   - A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
   - OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   - SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   - LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   - DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   - THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   - (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   - OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   -->

<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>
    <xsl:output method="html"/>
    <xsl:param name="xml_has_no_style_message"/>
    <xsl:param name="downArrowBase64"/>
    <xsl:param name="rightArrowBase64"/>

    <xsl:template match="/">
        <xsl:call-template name="main"/>
    </xsl:template>

    <!-- Empty element -->
    <xsl:template match="*">
        <div class="line">
            <span class="webkit-html-tag">
                <xsl:text>&lt;</xsl:text>
                <xsl:value-of select="name()"/>
                <xsl:apply-templates select="@*"/>
                <xsl:text>/&gt;</xsl:text>
            </span>
        </div>
    </xsl:template>

    <!-- Element with short text only -->
    <xsl:template match="*[node()]">
        <div class="line">
            <span class="webkit-html-tag">
                <xsl:text>&lt;</xsl:text>
                <xsl:value-of select="name()"/>
                <xsl:apply-templates select="@*"/>
                <xsl:text>&gt;</xsl:text>
            </span>

            <span class="text"><xsl:value-of select="."/></span>

            <span class="webkit-html-tag">
                <xsl:text>&lt;/</xsl:text>
                <xsl:value-of select="name()"/>
                <xsl:text>&gt;</xsl:text>
            </span>
        </div>
    </xsl:template>

    <!-- Collapsable element -->
    <xsl:template match="*[* or processing-instruction() or comment() or string-length() &gt; 50]">
        <div class="collapsable">
            <div class="expanded">
                <div class="line">
                    <xsl:call-template name="collapse-button"/>
                    <span class="webkit-html-tag">
                        <xsl:text>&lt;</xsl:text>
                        <xsl:value-of select="name()"/>
                        <xsl:apply-templates select="@*"/>
                        <xsl:text>&gt;</xsl:text>
                    </span>
                </div>

                <div class="collapsable-content">
                    <xsl:apply-templates/>
                </div>

                <div class="line">
                    <span class="webkit-html-tag">
                        <xsl:text>&lt;/</xsl:text>
                        <xsl:value-of select="name()"/>
                        <xsl:text>&gt;</xsl:text>
                    </span>
                </div>
            </div>

            <div class="collapsed hidden">
                <div class="line">
                    <xsl:call-template name="expand-button"/>
                    <span class="webkit-html-tag">
                        <xsl:text>&lt;</xsl:text>
                        <xsl:value-of select="name()"/>
                        <xsl:apply-templates select="@*"/>
                        <xsl:text>&gt;</xsl:text>
                        <xsl:text>...</xsl:text>
                        <xsl:text>&lt;/</xsl:text>
                        <xsl:value-of select="name()"/>
                        <xsl:text>&gt;</xsl:text>
                    </span>
                </div>
            </div>
        </div>
    </xsl:template>

    <!-- Any attribute -->
    <xsl:template match="@*">
        <xsl:text> </xsl:text>
        <span class="webkit-html-attribute-name"><xsl:value-of select="name()"/></span>
        <xsl:text>=&quot;</xsl:text>
        <span class="webkit-html-attribute-value"><xsl:value-of select="."/></span>
        <xsl:text>&quot;</xsl:text>
    </xsl:template>

    <!-- Short comment -->
    <xsl:template match="comment()">
        <div class="line">
            <span class="webkit-html-comment">
                <xsl:text>&lt;!--</xsl:text>
                <xsl:value-of select="."/>
                <xsl:text>--&gt;</xsl:text>
            </span>
        </div>
    </xsl:template>

    <!-- Long comment -->
    <xsl:template match="comment()[string-length() &gt; 50]">
        <div class="collapsable">
            <div class="expanded">
                <div class="line">
                    <xsl:call-template name="collapse-button"/>
                    <span class="webkit-html-comment">
                        <xsl:text>&lt;!--</xsl:text>
                    </span>
                </div>

                <div class="collapsable-content comment">
                    <span class="webkit-html-comment"><xsl:value-of select="."/></span>
                </div>

                <div class="line">
                    <span class="webkit-html-comment">
                        <xsl:text>--&gt;</xsl:text>
                    </span>
                </div>
            </div>

            <div class="collapsed hidden">
                <div class="line">
                    <xsl:call-template name="expand-button"/>
                    <span class="webkit-html-comment">
                        <xsl:text>&lt;!--</xsl:text>
                    </span>

                    <span class="webkit-html-comment"><xsl:text>...</xsl:text></span>

                    <span class="webkit-html-comment">
                        <xsl:text>--&gt;</xsl:text>
                    </span>
                </div>
            </div>
        </div>
    </xsl:template>

    <!-- Short processing instruction -->
    <xsl:template match="processing-instruction()[name() != 'xml-stylesheet']">
        <div class="line">
            <span class="webkit-html-comment">
                <xsl:text>&lt;?</xsl:text>
                <xsl:value-of select="name()"/>
                <xsl:text> </xsl:text>
                <xsl:value-of select="."/>
                <xsl:text>?&gt;</xsl:text>
            </span>
        </div>
    </xsl:template>

    <!-- Long processing instruction -->
    <xsl:template match="processing-instruction()[(string-length() &gt; 50) and (name() != 'xml-stylesheet')]">
        <div class="collapsable">
            <div class="expanded">
                <div class="line">
                    <xsl:call-template name="collapse-button"/>
                    <span class="webkit-html-comment">
                        <xsl:text>&lt;?</xsl:text>
                        <xsl:value-of select="name()"/>
                        <xsl:text> </xsl:text>
                    </span>
                </div>

                <div class="collapsable-content">
                    <span class="webkit-html-comment"><xsl:value-of select="."/></span>
                </div>

                <div class="line">
                    <span class="webkit-html-comment">
                        <xsl:text>?&gt;</xsl:text>
                    </span>
                </div>
            </div>
            <div class="collapsed hidden">
                <div class="line">
                  <xsl:call-template name="expand-button"/>
                    <span class="webkit-html-comment">
                        <xsl:text>&lt;?</xsl:text>
                        <xsl:value-of select="name()"/>
                    </span>

                    <span class="webkit-html-comment"><xsl:text>...</xsl:text></span>

                    <span class="webkit-html-comment">
                        <xsl:text>?&gt;</xsl:text>
                    </span>
                </div>
            </div>
        </div>
    </xsl:template>

    <!-- Text node -->
    <xsl:template match="text()">
        <xsl:value-of select="."/>
    </xsl:template>

    <xsl:template name="collapse-button">
        <span class="button collapse-button">
        </span>
    </xsl:template>

    <xsl:template name="expand-button">
        <span class="button expand-button">
        </span>
    </xsl:template>

    <xsl:template name="main">
        <html>
            <head>
                <xsl:call-template name="style"/>
                <xsl:call-template name="script"/>
            </head>
            <body onload="onload()">
                <div class="header">
                    <span> <xsl:value-of select="$xml_has_no_style_message"/> </span> 
                    <br/>
                </div>

                <div class="pretty-print">
                    <xsl:apply-templates/>
                </div>
                <div> </div>
            </body>
        </html>
    </xsl:template>

    <xsl:template name="script">
        <script type="text/javascript">
            <xsl:text>
                function onload()
                {
                    drawArrows();
                    initButtons();
                }

                function drawArrows()
                {
                    var ctx = document.getCSSCanvasContext("2d", "arrowRight", 10, 11);

                    ctx.fillStyle = "rgb(90,90,90)";
                    ctx.beginPath();
                    ctx.moveTo(0, 0);
                    ctx.lineTo(0, 8);
                    ctx.lineTo(7, 4);
                    ctx.lineTo(0, 0);
                    ctx.fill();
                    ctx.closePath();

                    var ctx = document.getCSSCanvasContext("2d", "arrowDown", 10, 10);

                    ctx.fillStyle = "rgb(90,90,90)";
                    ctx.beginPath();
                    ctx.moveTo(0, 0);
                    ctx.lineTo(8, 0);
                    ctx.lineTo(4, 7);
                    ctx.lineTo(0, 0);
                    ctx.fill();
                    ctx.closePath();
                }

                function expandFunction(sectionId)
                {
                    return function()
                    {
                        document.querySelector('#' + sectionId + ' > .expanded').className = 'expanded';
                        document.querySelector('#' + sectionId + ' > .collapsed').className = 'collapsed hidden';
                    };
                }

                function collapseFunction(sectionId)
                {
                    return function()
                    {
                        document.querySelector('#' + sectionId + ' > .expanded').className = 'expanded hidden';
                        document.querySelector('#' + sectionId + ' > .collapsed').className = 'collapsed';
                    };
                }

                function initButtons()
                {
                    var sections = document.querySelectorAll('.collapsable');
                    for (var i = 0; i &lt; sections.length; i++) {
                        var sectionId = 'collapsable' + i;
                        sections[i].id = sectionId;

                        var expandedPart = sections[i].querySelector('#' + sectionId + ' > .expanded');
                        var collapseButton = expandedPart.querySelector('.collapse-button');
                        collapseButton.onclick = collapseFunction(sectionId);
                        collapseButton.onmousedown = handleButtonMouseDown;

                        var collapsedPart = sections[i].querySelector('#' + sectionId + ' > .collapsed');
                        var expandButton = collapsedPart.querySelector('.expand-button');
                        expandButton.onclick = expandFunction(sectionId);
                        expandButton.onmousedown = handleButtonMouseDown;
                    }

                }

                function handleButtonMouseDown(e)
                {
                   // To prevent selection on double click
                   e.preventDefault();
                }

            </xsl:text>
        </script>
    </xsl:template>

    <xsl:template name="style">
        <style type="text/css">
            div.header {
                border-bottom: 2px solid black;
                padding-bottom: 5px;
                margin: 10px;
            }

            div.collapsable > div.hidden {
                display:none;
            }

            .pretty-print {
                margin-top: 1em;
                margin-left: 20px;
                font-family: monospace;
                font-size: 13px;
            }

            .collapsable-content {
                margin-left: 1em;
            }
            .comment {
                whitespace: pre;
            }

            .button {
                -webkit-user-select: none;
                cursor: pointer;
                display: inline-block;
                margin-left: -10px;
                width: 10px;
                background-repeat: no-repeat;
                background-position: left top;
                vertical-align: bottom;
            }

            .collapse-button {
                background-image: -webkit-canvas(arrowDown);
                height: 10px;
            }

            .expand-button {
                background-image: -webkit-canvas(arrowRight);
                height: 11px;
            }
        </style>
    </xsl:template>
</xsl:stylesheet>
