description("Test SVG path.getPathSegAtLength().");
/* 
   getPathSegAtLength() returns the index into pathSegList which is distance units along the path.
   Parameters
   in float distance        The distance along the path, relative to the start of the path, as a distance in the current user coordinate system.
   Return value
   unsigned long        The index of the path segment, where the first path segment is number 0.

   See specification for detail: http://www.w3.org/TR/SVG/paths.html#DistanceAlongAPath
*/

path = document.createElementNS("http://www.w3.org/2000/svg", "path");
path.setAttributeNS(null, "d", "M0 0 L0 5 L5 5 L 5 0");
shouldBe("path.getPathSegAtLength(0)", "0");
shouldBe("path.getPathSegAtLength(1)", "1");
shouldBe("path.getPathSegAtLength(5)", "1");
shouldBe("path.getPathSegAtLength(6)", "2");
shouldBe("path.getPathSegAtLength(10)", "2");
shouldBe("path.getPathSegAtLength(11)", "3");
// WebKit/Opera/FF all return the last path segment if the distance exceeds the actual path length:
shouldBe("path.getPathSegAtLength(16)", "3");
shouldBe("path.getPathSegAtLength(20)", "3");
shouldBe("path.getPathSegAtLength(24)", "3");
shouldBe("path.getPathSegAtLength(25)", "3");
shouldBe("path.getPathSegAtLength(100)", "3");
successfullyParsed = true;
