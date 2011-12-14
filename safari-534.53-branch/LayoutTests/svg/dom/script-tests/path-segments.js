description("This tests the SVG path segment DOM by creating paths and inspecting their properties.");

var pathElement = document.createElementNS("http://www.w3.org/2000/svg", "path");

shouldBe("SVGPathSeg.PATHSEG_UNKNOWN", "0");
shouldBe("SVGPathSeg.PATHSEG_CLOSEPATH", "1");
shouldBe("SVGPathSeg.PATHSEG_MOVETO_ABS", "2");
shouldBe("SVGPathSeg.PATHSEG_MOVETO_REL", "3");
shouldBe("SVGPathSeg.PATHSEG_LINETO_ABS", "4");
shouldBe("SVGPathSeg.PATHSEG_LINETO_REL", "5");
shouldBe("SVGPathSeg.PATHSEG_CURVETO_CUBIC_ABS", "6");
shouldBe("SVGPathSeg.PATHSEG_CURVETO_CUBIC_REL", "7");
shouldBe("SVGPathSeg.PATHSEG_CURVETO_QUADRATIC_ABS", "8");
shouldBe("SVGPathSeg.PATHSEG_CURVETO_QUADRATIC_REL", "9");
shouldBe("SVGPathSeg.PATHSEG_ARC_ABS", "10");
shouldBe("SVGPathSeg.PATHSEG_ARC_REL", "11");
shouldBe("SVGPathSeg.PATHSEG_LINETO_HORIZONTAL_ABS", "12");
shouldBe("SVGPathSeg.PATHSEG_LINETO_HORIZONTAL_REL", "13");
shouldBe("SVGPathSeg.PATHSEG_LINETO_VERTICAL_ABS", "14");
shouldBe("SVGPathSeg.PATHSEG_LINETO_VERTICAL_REL", "15");
shouldBe("SVGPathSeg.PATHSEG_CURVETO_CUBIC_SMOOTH_ABS", "16");
shouldBe("SVGPathSeg.PATHSEG_CURVETO_CUBIC_SMOOTH_REL", "17");
shouldBe("SVGPathSeg.PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS", "18");
shouldBe("SVGPathSeg.PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL", "19");

shouldBe("pathElement.createSVGPathSegClosePath().pathSegType", "SVGPathSeg.PATHSEG_CLOSEPATH");
shouldBe("pathElement.createSVGPathSegClosePath().pathSegTypeAsLetter", "'Z'");

shouldBe("pathElement.createSVGPathSegMovetoAbs(1, 2).pathSegType", "SVGPathSeg.PATHSEG_MOVETO_ABS");
shouldBe("pathElement.createSVGPathSegMovetoAbs(1, 2).pathSegTypeAsLetter", "'M'");
shouldBe("pathElement.createSVGPathSegMovetoAbs(1, 2).x", "1");
shouldBe("pathElement.createSVGPathSegMovetoAbs(1, 2).y", "2");

shouldBe("pathElement.createSVGPathSegMovetoRel(1, 2).pathSegType", "SVGPathSeg.PATHSEG_MOVETO_REL");
shouldBe("pathElement.createSVGPathSegMovetoRel(1, 2).pathSegTypeAsLetter", "'m'");
shouldBe("pathElement.createSVGPathSegMovetoRel(1, 2).x", "1");
shouldBe("pathElement.createSVGPathSegMovetoRel(1, 2).y", "2");

shouldBe("pathElement.createSVGPathSegLinetoAbs(1, 2).pathSegType", "SVGPathSeg.PATHSEG_LINETO_ABS");
shouldBe("pathElement.createSVGPathSegLinetoAbs(1, 2).pathSegTypeAsLetter", "'L'");
shouldBe("pathElement.createSVGPathSegLinetoAbs(1, 2).x", "1");
shouldBe("pathElement.createSVGPathSegLinetoAbs(1, 2).y", "2");

shouldBe("pathElement.createSVGPathSegLinetoRel(1, 2).pathSegType", "SVGPathSeg.PATHSEG_LINETO_REL");
shouldBe("pathElement.createSVGPathSegLinetoRel(1, 2).pathSegTypeAsLetter", "'l'");
shouldBe("pathElement.createSVGPathSegLinetoRel(1, 2).x", "1");
shouldBe("pathElement.createSVGPathSegLinetoRel(1, 2).y", "2");

shouldBe("pathElement.createSVGPathSegCurvetoCubicAbs(1, 2, 3, 4, 5, 6).pathSegType", "SVGPathSeg.PATHSEG_CURVETO_CUBIC_ABS");
shouldBe("pathElement.createSVGPathSegCurvetoCubicAbs(1, 2, 3, 4, 5, 6).pathSegTypeAsLetter", "'C'");
shouldBe("pathElement.createSVGPathSegCurvetoCubicAbs(1, 2, 3, 4, 5, 6).x", "1");
shouldBe("pathElement.createSVGPathSegCurvetoCubicAbs(1, 2, 3, 4, 5, 6).y", "2");
shouldBe("pathElement.createSVGPathSegCurvetoCubicAbs(1, 2, 3, 4, 5, 6).x1", "3");
shouldBe("pathElement.createSVGPathSegCurvetoCubicAbs(1, 2, 3, 4, 5, 6).y1", "4");
shouldBe("pathElement.createSVGPathSegCurvetoCubicAbs(1, 2, 3, 4, 5, 6).x2", "5");
shouldBe("pathElement.createSVGPathSegCurvetoCubicAbs(1, 2, 3, 4, 5, 6).y2", "6");

shouldBe("pathElement.createSVGPathSegCurvetoCubicRel(1, 2, 3, 4, 5, 6).pathSegType", "SVGPathSeg.PATHSEG_CURVETO_CUBIC_REL");
shouldBe("pathElement.createSVGPathSegCurvetoCubicRel(1, 2, 3, 4, 5, 6).pathSegTypeAsLetter", "'c'");
shouldBe("pathElement.createSVGPathSegCurvetoCubicRel(1, 2, 3, 4, 5, 6).x", "1");
shouldBe("pathElement.createSVGPathSegCurvetoCubicRel(1, 2, 3, 4, 5, 6).y", "2");
shouldBe("pathElement.createSVGPathSegCurvetoCubicRel(1, 2, 3, 4, 5, 6).x1", "3");
shouldBe("pathElement.createSVGPathSegCurvetoCubicRel(1, 2, 3, 4, 5, 6).y1", "4");
shouldBe("pathElement.createSVGPathSegCurvetoCubicRel(1, 2, 3, 4, 5, 6).x2", "5");
shouldBe("pathElement.createSVGPathSegCurvetoCubicRel(1, 2, 3, 4, 5, 6).y2", "6");

shouldBe("pathElement.createSVGPathSegCurvetoQuadraticAbs(1, 2, 3, 4).pathSegType", "SVGPathSeg.PATHSEG_CURVETO_QUADRATIC_ABS");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticAbs(1, 2, 3, 4).pathSegTypeAsLetter", "'Q'");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticAbs(1, 2, 3, 4).x", "1");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticAbs(1, 2, 3, 4).y", "2");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticAbs(1, 2, 3, 4).x1", "3");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticAbs(1, 2, 3, 4).y1", "4");

shouldBe("pathElement.createSVGPathSegCurvetoQuadraticRel(1, 2, 3, 4).pathSegType", "SVGPathSeg.PATHSEG_CURVETO_QUADRATIC_REL");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticRel(1, 2, 3, 4).pathSegTypeAsLetter", "'q'");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticRel(1, 2, 3, 4).x", "1");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticRel(1, 2, 3, 4).y", "2");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticRel(1, 2, 3, 4).x1", "3");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticRel(1, 2, 3, 4).y1", "4");

shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, false).pathSegType", "SVGPathSeg.PATHSEG_ARC_ABS");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, false).pathSegTypeAsLetter", "'A'");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, false).x", "1");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, false).y", "2");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, false).r1", "3");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, false).r2", "4");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, false).angle", "5");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, false).largeArcFlag", "false");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, true, false).largeArcFlag", "true");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, false).sweepFlag", "false");
shouldBe("pathElement.createSVGPathSegArcAbs(1, 2, 3, 4, 5, false, true).sweepFlag", "true");

shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, false).pathSegType", "SVGPathSeg.PATHSEG_ARC_REL");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, false).pathSegTypeAsLetter", "'a'");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, false).x", "1");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, false).y", "2");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, false).r1", "3");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, false).r2", "4");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, false).angle", "5");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, false).largeArcFlag", "false");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, true, false).largeArcFlag", "true");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, false).sweepFlag", "false");
shouldBe("pathElement.createSVGPathSegArcRel(1, 2, 3, 4, 5, false, true).sweepFlag", "true");

shouldBe("pathElement.createSVGPathSegLinetoHorizontalAbs(1).pathSegType", "SVGPathSeg.PATHSEG_LINETO_HORIZONTAL_ABS");
shouldBe("pathElement.createSVGPathSegLinetoHorizontalAbs(1).pathSegTypeAsLetter", "'H'");
shouldBe("pathElement.createSVGPathSegLinetoHorizontalAbs(1).x", "1");

shouldBe("pathElement.createSVGPathSegLinetoHorizontalRel(1).pathSegType", "SVGPathSeg.PATHSEG_LINETO_HORIZONTAL_REL");
shouldBe("pathElement.createSVGPathSegLinetoHorizontalRel(1).pathSegTypeAsLetter", "'h'");
shouldBe("pathElement.createSVGPathSegLinetoHorizontalRel(1).x", "1");

shouldBe("pathElement.createSVGPathSegLinetoVerticalAbs(1).pathSegType", "SVGPathSeg.PATHSEG_LINETO_VERTICAL_ABS");
shouldBe("pathElement.createSVGPathSegLinetoVerticalAbs(1).pathSegTypeAsLetter", "'V'");
shouldBe("pathElement.createSVGPathSegLinetoVerticalAbs(1).y", "1");

shouldBe("pathElement.createSVGPathSegLinetoVerticalRel(1).pathSegType", "SVGPathSeg.PATHSEG_LINETO_VERTICAL_REL");
shouldBe("pathElement.createSVGPathSegLinetoVerticalRel(1).pathSegTypeAsLetter", "'v'");
shouldBe("pathElement.createSVGPathSegLinetoVerticalRel(1).y", "1");

shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothAbs(1, 2, 3, 4).pathSegType", "SVGPathSeg.PATHSEG_CURVETO_CUBIC_SMOOTH_ABS");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothAbs(1, 2, 3, 4).pathSegTypeAsLetter", "'S'");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothAbs(1, 2, 3, 4).x", "1");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothAbs(1, 2, 3, 4).y", "2");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothAbs(1, 2, 3, 4).x2", "3");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothAbs(1, 2, 3, 4).y2", "4");

shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothRel(1, 2, 3, 4).pathSegType", "SVGPathSeg.PATHSEG_CURVETO_CUBIC_SMOOTH_REL");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothRel(1, 2, 3, 4).pathSegTypeAsLetter", "'s'");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothRel(1, 2, 3, 4).x", "1");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothRel(1, 2, 3, 4).y", "2");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothRel(1, 2, 3, 4).x2", "3");
shouldBe("pathElement.createSVGPathSegCurvetoCubicSmoothRel(1, 2, 3, 4).y2", "4");

shouldBe("pathElement.createSVGPathSegCurvetoQuadraticSmoothAbs(1, 2).pathSegType", "SVGPathSeg.PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticSmoothAbs(1, 2).pathSegTypeAsLetter", "'T'");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticSmoothAbs(1, 2).x", "1");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticSmoothAbs(1, 2).y", "2");

shouldBe("pathElement.createSVGPathSegCurvetoQuadraticSmoothRel(1, 2).pathSegType", "SVGPathSeg.PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticSmoothRel(1, 2).pathSegTypeAsLetter", "'t'");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticSmoothRel(1, 2).x", "1");
shouldBe("pathElement.createSVGPathSegCurvetoQuadraticSmoothRel(1, 2).y", "2");

successfullyParsed = true;
