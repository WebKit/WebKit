var testContents = [ "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider 3d-cube</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>3d-cube</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"3d-cube failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// 3D Cube Rotation\n\
// http://www.speich.net/computer/moztesting/3d.htm\n\
// Created by Simon Speich\n\
\n\
var Q = new Array();\n\
var MTrans = new Array();  // transformation matrix\n\
var MQube = new Array();  // position information of qube\n\
var I = new Array();      // entity matrix\n\
var Origin = new Object();\n\
var Testing = new Object();\n\
var LoopTimer;\n\
\n\
var validation = {\n\
 20: 2889.0000000000045,\n\
 40: 2889.0000000000055,\n\
 80: 2889.000000000005,\n\
 160: 2889.0000000000055\n\
};\n\
\n\
var DisplArea = new Object();\n\
DisplArea.Width = 300;\n\
DisplArea.Height = 300;\n\
\n\
function DrawLine(From, To) {\n\
  var x1 = From.V[0];\n\
  var x2 = To.V[0];\n\
  var y1 = From.V[1];\n\
  var y2 = To.V[1];\n\
  var dx = Math.abs(x2 - x1);\n\
  var dy = Math.abs(y2 - y1);\n\
  var x = x1;\n\
  var y = y1;\n\
  var IncX1, IncY1;\n\
  var IncX2, IncY2;  \n\
  var Den;\n\
  var Num;\n\
  var NumAdd;\n\
  var NumPix;\n\
\n\
  if (x2 >= x1) {  IncX1 = 1; IncX2 = 1;  }\n\
  else { IncX1 = -1; IncX2 = -1; }\n\
  if (y2 >= y1)  {  IncY1 = 1; IncY2 = 1; }\n\
  else { IncY1 = -1; IncY2 = -1; }\n\
  if (dx >= dy) {\n\
    IncX1 = 0;\n\
    IncY2 = 0;\n\
    Den = dx;\n\
    Num = dx / 2;\n\
    NumAdd = dy;\n\
    NumPix = dx;\n\
  }\n\
  else {\n\
    IncX2 = 0;\n\
    IncY1 = 0;\n\
    Den = dy;\n\
    Num = dy / 2;\n\
    NumAdd = dx;\n\
    NumPix = dy;\n\
  }\n\
\n\
  NumPix = Math.round(Q.LastPx + NumPix);\n\
\n\
  var i = Q.LastPx;\n\
  for (; i < NumPix; i++) {\n\
    Num += NumAdd;\n\
    if (Num >= Den) {\n\
      Num -= Den;\n\
      x += IncX1;\n\
      y += IncY1;\n\
    }\n\
    x += IncX2;\n\
    y += IncY2;\n\
  }\n\
  Q.LastPx = NumPix;\n\
}\n\
\n\
function CalcCross(V0, V1) {\n\
  var Cross = new Array();\n\
  Cross[0] = V0[1]*V1[2] - V0[2]*V1[1];\n\
  Cross[1] = V0[2]*V1[0] - V0[0]*V1[2];\n\
  Cross[2] = V0[0]*V1[1] - V0[1]*V1[0];\n\
  return Cross;\n\
}\n\
\n\
function CalcNormal(V0, V1, V2) {\n\
  var A = new Array();   var B = new Array(); \n\
  for (var i = 0; i < 3; i++) {\n\
    A[i] = V0[i] - V1[i];\n\
    B[i] = V2[i] - V1[i];\n\
  }\n\
  A = CalcCross(A, B);\n\
  var Length = Math.sqrt(A[0]*A[0] + A[1]*A[1] + A[2]*A[2]); \n\
  for (var i = 0; i < 3; i++) A[i] = A[i] / Length;\n\
  A[3] = 1;\n\
  return A;\n\
}\n\
\n\
function CreateP(X,Y,Z) {\n\
  this.V = [X,Y,Z,1];\n\
}\n\
\n\
// multiplies two matrices\n\
function MMulti(M1, M2) {\n\
  var M = [[],[],[],[]];\n\
  var i = 0;\n\
  var j = 0;\n\
  for (; i < 4; i++) {\n\
    j = 0;\n\
    for (; j < 4; j++) M[i][j] = M1[i][0] * M2[0][j] + M1[i][1] * M2[1][j] + M1[i][2] * M2[2][j] + M1[i][3] * M2[3][j];\n\
  }\n\
  return M;\n\
}\n\
\n\
//multiplies matrix with vector\n\
function VMulti(M, V) {\n\
  var Vect = new Array();\n\
  var i = 0;\n\
  for (;i < 4; i++) Vect[i] = M[i][0] * V[0] + M[i][1] * V[1] + M[i][2] * V[2] + M[i][3] * V[3];\n\
  return Vect;\n\
}\n\
\n\
function VMulti2(M, V) {\n\
  var Vect = new Array();\n\
  var i = 0;\n\
  for (;i < 3; i++) Vect[i] = M[i][0] * V[0] + M[i][1] * V[1] + M[i][2] * V[2];\n\
  return Vect;\n\
}\n\
\n\
// add to matrices\n\
function MAdd(M1, M2) {\n\
  var M = [[],[],[],[]];\n\
  var i = 0;\n\
  var j = 0;\n\
  for (; i < 4; i++) {\n\
    j = 0;\n\
    for (; j < 4; j++) M[i][j] = M1[i][j] + M2[i][j];\n\
  }\n\
  return M;\n\
}\n\
\n\
function Translate(M, Dx, Dy, Dz) {\n\
  var T = [\n\
  [1,0,0,Dx],\n\
  [0,1,0,Dy],\n\
  [0,0,1,Dz],\n\
  [0,0,0,1]\n\
  ];\n\
  return MMulti(T, M);\n\
}\n\
\n\
function RotateX(M, Phi) {\n\
  var a = Phi;\n\
  a *= Math.PI / 180;\n\
  var Cos = Math.cos(a);\n\
  var Sin = Math.sin(a);\n\
  var R = [\n\
  [1,0,0,0],\n\
  [0,Cos,-Sin,0],\n\
  [0,Sin,Cos,0],\n\
  [0,0,0,1]\n\
  ];\n\
  return MMulti(R, M);\n\
}\n\
\n\
function RotateY(M, Phi) {\n\
  var a = Phi;\n\
  a *= Math.PI / 180;\n\
  var Cos = Math.cos(a);\n\
  var Sin = Math.sin(a);\n\
  var R = [\n\
  [Cos,0,Sin,0],\n\
  [0,1,0,0],\n\
  [-Sin,0,Cos,0],\n\
  [0,0,0,1]\n\
  ];\n\
  return MMulti(R, M);\n\
}\n\
\n\
function RotateZ(M, Phi) {\n\
  var a = Phi;\n\
  a *= Math.PI / 180;\n\
  var Cos = Math.cos(a);\n\
  var Sin = Math.sin(a);\n\
  var R = [\n\
  [Cos,-Sin,0,0],\n\
  [Sin,Cos,0,0],\n\
  [0,0,1,0],   \n\
  [0,0,0,1]\n\
  ];\n\
  return MMulti(R, M);\n\
}\n\
\n\
function DrawQube() {\n\
  // calc current normals\n\
  var CurN = new Array();\n\
  var i = 5;\n\
  Q.LastPx = 0;\n\
  for (; i > -1; i--) CurN[i] = VMulti2(MQube, Q.Normal[i]);\n\
  if (CurN[0][2] < 0) {\n\
    if (!Q.Line[0]) { DrawLine(Q[0], Q[1]); Q.Line[0] = true; };\n\
    if (!Q.Line[1]) { DrawLine(Q[1], Q[2]); Q.Line[1] = true; };\n\
    if (!Q.Line[2]) { DrawLine(Q[2], Q[3]); Q.Line[2] = true; };\n\
    if (!Q.Line[3]) { DrawLine(Q[3], Q[0]); Q.Line[3] = true; };\n\
  }\n\
  if (CurN[1][2] < 0) {\n\
    if (!Q.Line[2]) { DrawLine(Q[3], Q[2]); Q.Line[2] = true; };\n\
    if (!Q.Line[9]) { DrawLine(Q[2], Q[6]); Q.Line[9] = true; };\n\
    if (!Q.Line[6]) { DrawLine(Q[6], Q[7]); Q.Line[6] = true; };\n\
    if (!Q.Line[10]) { DrawLine(Q[7], Q[3]); Q.Line[10] = true; };\n\
  }\n\
  if (CurN[2][2] < 0) {\n\
    if (!Q.Line[4]) { DrawLine(Q[4], Q[5]); Q.Line[4] = true; };\n\
    if (!Q.Line[5]) { DrawLine(Q[5], Q[6]); Q.Line[5] = true; };\n\
    if (!Q.Line[6]) { DrawLine(Q[6], Q[7]); Q.Line[6] = true; };\n\
    if (!Q.Line[7]) { DrawLine(Q[7], Q[4]); Q.Line[7] = true; };\n\
  }\n\
  if (CurN[3][2] < 0) {\n\
    if (!Q.Line[4]) { DrawLine(Q[4], Q[5]); Q.Line[4] = true; };\n\
    if (!Q.Line[8]) { DrawLine(Q[5], Q[1]); Q.Line[8] = true; };\n\
    if (!Q.Line[0]) { DrawLine(Q[1], Q[0]); Q.Line[0] = true; };\n\
    if (!Q.Line[11]) { DrawLine(Q[0], Q[4]); Q.Line[11] = true; };\n\
  }\n\
  if (CurN[4][2] < 0) {\n\
    if (!Q.Line[11]) { DrawLine(Q[4], Q[0]); Q.Line[11] = true; };\n\
    if (!Q.Line[3]) { DrawLine(Q[0], Q[3]); Q.Line[3] = true; };\n\
    if (!Q.Line[10]) { DrawLine(Q[3], Q[7]); Q.Line[10] = true; };\n\
    if (!Q.Line[7]) { DrawLine(Q[7], Q[4]); Q.Line[7] = true; };\n\
  }\n\
  if (CurN[5][2] < 0) {\n\
    if (!Q.Line[8]) { DrawLine(Q[1], Q[5]); Q.Line[8] = true; };\n\
    if (!Q.Line[5]) { DrawLine(Q[5], Q[6]); Q.Line[5] = true; };\n\
    if (!Q.Line[9]) { DrawLine(Q[6], Q[2]); Q.Line[9] = true; };\n\
    if (!Q.Line[1]) { DrawLine(Q[2], Q[1]); Q.Line[1] = true; };\n\
  }\n\
  Q.Line = [false,false,false,false,false,false,false,false,false,false,false,false];\n\
  Q.LastPx = 0;\n\
}\n\
\n\
function Loop() {\n\
  if (Testing.LoopCount > Testing.LoopMax) return;\n\
  var TestingStr = String(Testing.LoopCount);\n\
  while (TestingStr.length < 3) TestingStr = \"0\" + TestingStr;\n\
  MTrans = Translate(I, -Q[8].V[0], -Q[8].V[1], -Q[8].V[2]);\n\
  MTrans = RotateX(MTrans, 1);\n\
  MTrans = RotateY(MTrans, 3);\n\
  MTrans = RotateZ(MTrans, 5);\n\
  MTrans = Translate(MTrans, Q[8].V[0], Q[8].V[1], Q[8].V[2]);\n\
  MQube = MMulti(MTrans, MQube);\n\
  var i = 8;\n\
  for (; i > -1; i--) {\n\
    Q[i].V = VMulti(MTrans, Q[i].V);\n\
  }\n\
  DrawQube();\n\
  Testing.LoopCount++;\n\
  Loop();\n\
}\n\
\n\
function Init(CubeSize) {\n\
  // init/reset vars\n\
  Origin.V = [150,150,20,1];\n\
  Testing.LoopCount = 0;\n\
  Testing.LoopMax = 50;\n\
  Testing.TimeMax = 0;\n\
  Testing.TimeAvg = 0;\n\
  Testing.TimeMin = 0;\n\
  Testing.TimeTemp = 0;\n\
  Testing.TimeTotal = 0;\n\
  Testing.Init = false;\n\
\n\
  // transformation matrix\n\
  MTrans = [\n\
  [1,0,0,0],\n\
  [0,1,0,0],\n\
  [0,0,1,0],\n\
  [0,0,0,1]\n\
  ];\n\
  \n\
  // position information of qube\n\
  MQube = [\n\
  [1,0,0,0],\n\
  [0,1,0,0],\n\
  [0,0,1,0],\n\
  [0,0,0,1]\n\
  ];\n\
  \n\
  // entity matrix\n\
  I = [\n\
  [1,0,0,0],\n\
  [0,1,0,0],\n\
  [0,0,1,0],\n\
  [0,0,0,1]\n\
  ];\n\
  \n\
  // create qube\n\
  Q[0] = new CreateP(-CubeSize,-CubeSize, CubeSize);\n\
  Q[1] = new CreateP(-CubeSize, CubeSize, CubeSize);\n\
  Q[2] = new CreateP( CubeSize, CubeSize, CubeSize);\n\
  Q[3] = new CreateP( CubeSize,-CubeSize, CubeSize);\n\
  Q[4] = new CreateP(-CubeSize,-CubeSize,-CubeSize);\n\
  Q[5] = new CreateP(-CubeSize, CubeSize,-CubeSize);\n\
  Q[6] = new CreateP( CubeSize, CubeSize,-CubeSize);\n\
  Q[7] = new CreateP( CubeSize,-CubeSize,-CubeSize);\n\
  \n\
  // center of gravity\n\
  Q[8] = new CreateP(0, 0, 0);\n\
  \n\
  // anti-clockwise edge check\n\
  Q.Edge = [[0,1,2],[3,2,6],[7,6,5],[4,5,1],[4,0,3],[1,5,6]];\n\
  \n\
  // calculate squad normals\n\
  Q.Normal = new Array();\n\
  for (var i = 0; i < Q.Edge.length; i++) Q.Normal[i] = CalcNormal(Q[Q.Edge[i][0]].V, Q[Q.Edge[i][1]].V, Q[Q.Edge[i][2]].V);\n\
  \n\
  // line drawn ?\n\
  Q.Line = [false,false,false,false,false,false,false,false,false,false,false,false];\n\
  \n\
  // create line pixels\n\
  Q.NumPx = 9 * 2 * CubeSize;\n\
  for (var i = 0; i < Q.NumPx; i++) CreateP(0,0,0);\n\
  \n\
  MTrans = Translate(MTrans, Origin.V[0], Origin.V[1], Origin.V[2]);\n\
  MQube = MMulti(MTrans, MQube);\n\
\n\
  var i = 0;\n\
  for (; i < 9; i++) {\n\
    Q[i].V = VMulti(MTrans, Q[i].V);\n\
  }\n\
  DrawQube();\n\
  Testing.Init = true;\n\
  Loop();\n\
  \n\
  // Perform a simple sum-based verification.\n\
  var sum = 0;\n\
  for (var i = 0; i < Q.length; ++i) {\n\
    var vector = Q[i].V;\n\
    for (var j = 0; j < vector.length; ++j)\n\
      sum += vector[j];\n\
  }\n\
  if (sum != validation[CubeSize])\n\
    throw \"Error: bad vector sum for CubeSize = \" + CubeSize + \"; expected \" + validation[CubeSize] + \" but got \" + sum;\n\
}\n\
\n\
for ( var i = 20; i <= 160; i *= 2 ) {\n\
  Init(i);\n\
}\n\
\n\
Q = null;\n\
MTrans = null;\n\
MQube = null;\n\
I = null;\n\
Origin = null;\n\
Testing = null;\n\
LoopTime = null;\n\
DisplArea = null;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider 3d-morph</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>3d-morph</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"3d-morph failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/*\n\
 * Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
 *\n\
 * Redistribution and use in source and binary forms, with or without\n\
 * modification, are permitted provided that the following conditions\n\
 * are met:\n\
 * 1. Redistributions of source code must retain the above copyright\n\
 *    notice, this list of conditions and the following disclaimer.\n\
 * 2. Redistributions in binary form must reproduce the above copyright\n\
 *    notice, this list of conditions and the following disclaimer in the\n\
 *    documentation and/or other materials provided with the distribution.\n\
 *\n\
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
 */\n\
\n\
var loops = 15\n\
var nx = 120\n\
var nz = 120\n\
\n\
function morph(a, f) {\n\
    var PI2nx = Math.PI * 8/nx\n\
    var sin = Math.sin\n\
    var f30 = -(50 * sin(f*Math.PI*2))\n\
    \n\
    for (var i = 0; i < nz; ++i) {\n\
        for (var j = 0; j < nx; ++j) {\n\
            a[3*(i*nx+j)+1]    = sin((j-1) * PI2nx ) * -f30\n\
        }\n\
    }\n\
}\n\
\n\
    \n\
var a = Array()\n\
for (var i=0; i < nx*nz*3; ++i) \n\
    a[i] = 0\n\
\n\
for (var i = 0; i < loops; ++i) {\n\
    morph(a, i/loops)\n\
}\n\
\n\
testOutput = 0;\n\
for (var i = 0; i < nx; i++)\n\
    testOutput += a[3*(i*nx+i)+1];\n\
a = null;\n\
\n\
// This has to be an approximate test since ECMAscript doesn't formally specify\n\
// what sin() returns. Even if it did specify something like for example what Java 7\n\
// says - that sin() has to return a value within 1 ulp of exact - then we still\n\
// would not be able to do an exact test here since that would allow for just enough\n\
// low-bit slop to create possibly big errors due to testOutput being a sum.\n\
var expected = 6;\n\
if ((\"\" + testOutput)[0] != expected)\n\
    throw \"Error: bad test output: expected leading digit to be \" + expected + \" but got \" + testOutput;\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider 3d-raytrace</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>3d-raytrace</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"3d-raytrace failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/*\n\
 * Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
 *\n\
 * Redistribution and use in source and binary forms, with or without\n\
 * modification, are permitted provided that the following conditions\n\
 * are met:\n\
 * 1. Redistributions of source code must retain the above copyright\n\
 *    notice, this list of conditions and the following disclaimer.\n\
 * 2. Redistributions in binary form must reproduce the above copyright\n\
 *    notice, this list of conditions and the following disclaimer in the\n\
 *    documentation and/or other materials provided with the distribution.\n\
 *\n\
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
 */\n\
\n\
function createVector(x,y,z) {\n\
    return new Array(x,y,z);\n\
}\n\
\n\
function sqrLengthVector(self) {\n\
    return self[0] * self[0] + self[1] * self[1] + self[2] * self[2];\n\
}\n\
\n\
function lengthVector(self) {\n\
    return Math.sqrt(self[0] * self[0] + self[1] * self[1] + self[2] * self[2]);\n\
}\n\
\n\
function addVector(self, v) {\n\
    self[0] += v[0];\n\
    self[1] += v[1];\n\
    self[2] += v[2];\n\
    return self;\n\
}\n\
\n\
function subVector(self, v) {\n\
    self[0] -= v[0];\n\
    self[1] -= v[1];\n\
    self[2] -= v[2];\n\
    return self;\n\
}\n\
\n\
function scaleVector(self, scale) {\n\
    self[0] *= scale;\n\
    self[1] *= scale;\n\
    self[2] *= scale;\n\
    return self;\n\
}\n\
\n\
function normaliseVector(self) {\n\
    var len = Math.sqrt(self[0] * self[0] + self[1] * self[1] + self[2] * self[2]);\n\
    self[0] /= len;\n\
    self[1] /= len;\n\
    self[2] /= len;\n\
    return self;\n\
}\n\
\n\
function add(v1, v2) {\n\
    return new Array(v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2]);\n\
}\n\
\n\
function sub(v1, v2) {\n\
    return new Array(v1[0] - v2[0], v1[1] - v2[1], v1[2] - v2[2]);\n\
}\n\
\n\
function scalev(v1, v2) {\n\
    return new Array(v1[0] * v2[0], v1[1] * v2[1], v1[2] * v2[2]);\n\
}\n\
\n\
function dot(v1, v2) {\n\
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];\n\
}\n\
\n\
function scale(v, scale) {\n\
    return [v[0] * scale, v[1] * scale, v[2] * scale];\n\
}\n\
\n\
function cross(v1, v2) {\n\
    return [v1[1] * v2[2] - v1[2] * v2[1], \n\
            v1[2] * v2[0] - v1[0] * v2[2],\n\
            v1[0] * v2[1] - v1[1] * v2[0]];\n\
\n\
}\n\
\n\
function normalise(v) {\n\
    var len = lengthVector(v);\n\
    return [v[0] / len, v[1] / len, v[2] / len];\n\
}\n\
\n\
function transformMatrix(self, v) {\n\
    var vals = self;\n\
    var x  = vals[0] * v[0] + vals[1] * v[1] + vals[2] * v[2] + vals[3];\n\
    var y  = vals[4] * v[0] + vals[5] * v[1] + vals[6] * v[2] + vals[7];\n\
    var z  = vals[8] * v[0] + vals[9] * v[1] + vals[10] * v[2] + vals[11];\n\
    return [x, y, z];\n\
}\n\
\n\
function invertMatrix(self) {\n\
    var temp = new Array(16);\n\
    var tx = -self[3];\n\
    var ty = -self[7];\n\
    var tz = -self[11];\n\
    for (h = 0; h < 3; h++) \n\
        for (v = 0; v < 3; v++) \n\
            temp[h + v * 4] = self[v + h * 4];\n\
    for (i = 0; i < 11; i++)\n\
        self[i] = temp[i];\n\
    self[3] = tx * self[0] + ty * self[1] + tz * self[2];\n\
    self[7] = tx * self[4] + ty * self[5] + tz * self[6];\n\
    self[11] = tx * self[8] + ty * self[9] + tz * self[10];\n\
    return self;\n\
}\n\
\n\
\n\
// Triangle intersection using barycentric coord method\n\
function Triangle(p1, p2, p3) {\n\
    var edge1 = sub(p3, p1);\n\
    var edge2 = sub(p2, p1);\n\
    var normal = cross(edge1, edge2);\n\
    if (Math.abs(normal[0]) > Math.abs(normal[1]))\n\
        if (Math.abs(normal[0]) > Math.abs(normal[2]))\n\
            this.axis = 0; \n\
        else \n\
            this.axis = 2;\n\
    else\n\
        if (Math.abs(normal[1]) > Math.abs(normal[2])) \n\
            this.axis = 1;\n\
        else \n\
            this.axis = 2;\n\
    var u = (this.axis + 1) % 3;\n\
    var v = (this.axis + 2) % 3;\n\
    var u1 = edge1[u];\n\
    var v1 = edge1[v];\n\
    \n\
    var u2 = edge2[u];\n\
    var v2 = edge2[v];\n\
    this.normal = normalise(normal);\n\
    this.nu = normal[u] / normal[this.axis];\n\
    this.nv = normal[v] / normal[this.axis];\n\
    this.nd = dot(normal, p1) / normal[this.axis];\n\
    var det = u1 * v2 - v1 * u2;\n\
    this.eu = p1[u];\n\
    this.ev = p1[v]; \n\
    this.nu1 = u1 / det;\n\
    this.nv1 = -v1 / det;\n\
    this.nu2 = v2 / det;\n\
    this.nv2 = -u2 / det; \n\
    this.material = [0.7, 0.7, 0.7];\n\
}\n\
\n\
Triangle.prototype.intersect = function(orig, dir, near, far) {\n\
    var u = (this.axis + 1) % 3;\n\
    var v = (this.axis + 2) % 3;\n\
    var d = dir[this.axis] + this.nu * dir[u] + this.nv * dir[v];\n\
    var t = (this.nd - orig[this.axis] - this.nu * orig[u] - this.nv * orig[v]) / d;\n\
    if (t < near || t > far)\n\
        return null;\n\
    var Pu = orig[u] + t * dir[u] - this.eu;\n\
    var Pv = orig[v] + t * dir[v] - this.ev;\n\
    var a2 = Pv * this.nu1 + Pu * this.nv1;\n\
    if (a2 < 0) \n\
        return null;\n\
    var a3 = Pu * this.nu2 + Pv * this.nv2;\n\
    if (a3 < 0) \n\
        return null;\n\
\n\
    if ((a2 + a3) > 1) \n\
        return null;\n\
    return t;\n\
}\n\
\n\
function Scene(a_triangles) {\n\
    this.triangles = a_triangles;\n\
    this.lights = [];\n\
    this.ambient = [0,0,0];\n\
    this.background = [0.8,0.8,1];\n\
}\n\
var zero = new Array(0,0,0);\n\
\n\
Scene.prototype.intersect = function(origin, dir, near, far) {\n\
    var closest = null;\n\
    for (i = 0; i < this.triangles.length; i++) {\n\
        var triangle = this.triangles[i];   \n\
        var d = triangle.intersect(origin, dir, near, far);\n\
        if (d == null || d > far || d < near)\n\
            continue;\n\
        far = d;\n\
        closest = triangle;\n\
    }\n\
    \n\
    if (!closest)\n\
        return [this.background[0],this.background[1],this.background[2]];\n\
        \n\
    var normal = closest.normal;\n\
    var hit = add(origin, scale(dir, far)); \n\
    if (dot(dir, normal) > 0)\n\
        normal = [-normal[0], -normal[1], -normal[2]];\n\
    \n\
    var colour = null;\n\
    if (closest.shader) {\n\
        colour = closest.shader(closest, hit, dir);\n\
    } else {\n\
        colour = closest.material;\n\
    }\n\
    \n\
    // do reflection\n\
    var reflected = null;\n\
    if (colour.reflection > 0.001) {\n\
        var reflection = addVector(scale(normal, -2*dot(dir, normal)), dir);\n\
        reflected = this.intersect(hit, reflection, 0.0001, 1000000);\n\
        if (colour.reflection >= 0.999999)\n\
            return reflected;\n\
    }\n\
    \n\
    var l = [this.ambient[0], this.ambient[1], this.ambient[2]];\n\
    for (var i = 0; i < this.lights.length; i++) {\n\
        var light = this.lights[i];\n\
        var toLight = sub(light, hit);\n\
        var distance = lengthVector(toLight);\n\
        scaleVector(toLight, 1.0/distance);\n\
        distance -= 0.0001;\n\
        if (this.blocked(hit, toLight, distance))\n\
            continue;\n\
        var nl = dot(normal, toLight);\n\
        if (nl > 0)\n\
            addVector(l, scale(light.colour, nl));\n\
    }\n\
    l = scalev(l, colour);\n\
    if (reflected) {\n\
        l = addVector(scaleVector(l, 1 - colour.reflection), scaleVector(reflected, colour.reflection));\n\
    }\n\
    return l;\n\
}\n\
\n\
Scene.prototype.blocked = function(O, D, far) {\n\
    var near = 0.0001;\n\
    var closest = null;\n\
    for (i = 0; i < this.triangles.length; i++) {\n\
        var triangle = this.triangles[i];   \n\
        var d = triangle.intersect(O, D, near, far);\n\
        if (d == null || d > far || d < near)\n\
            continue;\n\
        return true;\n\
    }\n\
    \n\
    return false;\n\
}\n\
\n\
\n\
// this camera code is from notes i made ages ago, it is from *somewhere* -- i cannot remember where\n\
// that somewhere is\n\
function Camera(origin, lookat, up) {\n\
    var zaxis = normaliseVector(subVector(lookat, origin));\n\
    var xaxis = normaliseVector(cross(up, zaxis));\n\
    var yaxis = normaliseVector(cross(xaxis, subVector([0,0,0], zaxis)));\n\
    var m = new Array(16);\n\
    m[0] = xaxis[0]; m[1] = xaxis[1]; m[2] = xaxis[2];\n\
    m[4] = yaxis[0]; m[5] = yaxis[1]; m[6] = yaxis[2];\n\
    m[8] = zaxis[0]; m[9] = zaxis[1]; m[10] = zaxis[2];\n\
    invertMatrix(m);\n\
    m[3] = 0; m[7] = 0; m[11] = 0;\n\
    this.origin = origin;\n\
    this.directions = new Array(4);\n\
    this.directions[0] = normalise([-0.7,  0.7, 1]);\n\
    this.directions[1] = normalise([ 0.7,  0.7, 1]);\n\
    this.directions[2] = normalise([ 0.7, -0.7, 1]);\n\
    this.directions[3] = normalise([-0.7, -0.7, 1]);\n\
    this.directions[0] = transformMatrix(m, this.directions[0]);\n\
    this.directions[1] = transformMatrix(m, this.directions[1]);\n\
    this.directions[2] = transformMatrix(m, this.directions[2]);\n\
    this.directions[3] = transformMatrix(m, this.directions[3]);\n\
}\n\
\n\
Camera.prototype.generateRayPair = function(y) {\n\
    rays = new Array(new Object(), new Object());\n\
    rays[0].origin = this.origin;\n\
    rays[1].origin = this.origin;\n\
    rays[0].dir = addVector(scale(this.directions[0], y), scale(this.directions[3], 1 - y));\n\
    rays[1].dir = addVector(scale(this.directions[1], y), scale(this.directions[2], 1 - y));\n\
    return rays;\n\
}\n\
\n\
function renderRows(camera, scene, pixels, width, height, starty, stopy) {\n\
    for (var y = starty; y < stopy; y++) {\n\
        var rays = camera.generateRayPair(y / height);\n\
        for (var x = 0; x < width; x++) {\n\
            var xp = x / width;\n\
            var origin = addVector(scale(rays[0].origin, xp), scale(rays[1].origin, 1 - xp));\n\
            var dir = normaliseVector(addVector(scale(rays[0].dir, xp), scale(rays[1].dir, 1 - xp)));\n\
            var l = scene.intersect(origin, dir);\n\
            pixels[y][x] = l;\n\
        }\n\
    }\n\
}\n\
\n\
Camera.prototype.render = function(scene, pixels, width, height) {\n\
    var cam = this;\n\
    var row = 0;\n\
    renderRows(cam, scene, pixels, width, height, 0, height);\n\
}\n\
\n\
\n\
\n\
function raytraceScene()\n\
{\n\
    var startDate = new Date().getTime();\n\
    var numTriangles = 2 * 6;\n\
    var triangles = new Array();//numTriangles);\n\
    var tfl = createVector(-10,  10, -10);\n\
    var tfr = createVector( 10,  10, -10);\n\
    var tbl = createVector(-10,  10,  10);\n\
    var tbr = createVector( 10,  10,  10);\n\
    var bfl = createVector(-10, -10, -10);\n\
    var bfr = createVector( 10, -10, -10);\n\
    var bbl = createVector(-10, -10,  10);\n\
    var bbr = createVector( 10, -10,  10);\n\
    \n\
    // cube!!!\n\
    // front\n\
    var i = 0;\n\
    \n\
    triangles[i++] = new Triangle(tfl, tfr, bfr);\n\
    triangles[i++] = new Triangle(tfl, bfr, bfl);\n\
    // back\n\
    triangles[i++] = new Triangle(tbl, tbr, bbr);\n\
    triangles[i++] = new Triangle(tbl, bbr, bbl);\n\
    //        triangles[i-1].material = [0.7,0.2,0.2];\n\
    //            triangles[i-1].material.reflection = 0.8;\n\
    // left\n\
    triangles[i++] = new Triangle(tbl, tfl, bbl);\n\
    //            triangles[i-1].reflection = 0.6;\n\
    triangles[i++] = new Triangle(tfl, bfl, bbl);\n\
    //            triangles[i-1].reflection = 0.6;\n\
    // right\n\
    triangles[i++] = new Triangle(tbr, tfr, bbr);\n\
    triangles[i++] = new Triangle(tfr, bfr, bbr);\n\
    // top\n\
    triangles[i++] = new Triangle(tbl, tbr, tfr);\n\
    triangles[i++] = new Triangle(tbl, tfr, tfl);\n\
    // bottom\n\
    triangles[i++] = new Triangle(bbl, bbr, bfr);\n\
    triangles[i++] = new Triangle(bbl, bfr, bfl);\n\
    \n\
    //Floor!!!!\n\
    var green = createVector(0.0, 0.4, 0.0);\n\
    var grey = createVector(0.4, 0.4, 0.4);\n\
    grey.reflection = 1.0;\n\
    var floorShader = function(tri, pos, view) {\n\
        var x = ((pos[0]/32) % 2 + 2) % 2;\n\
        var z = ((pos[2]/32 + 0.3) % 2 + 2) % 2;\n\
        if (x < 1 != z < 1) {\n\
            //in the real world we use the fresnel term...\n\
            //    var angle = 1-dot(view, tri.normal);\n\
            //   angle *= angle;\n\
            //  angle *= angle;\n\
            // angle *= angle;\n\
            //grey.reflection = angle;\n\
            return grey;\n\
        } else \n\
            return green;\n\
    }\n\
    var ffl = createVector(-1000, -30, -1000);\n\
    var ffr = createVector( 1000, -30, -1000);\n\
    var fbl = createVector(-1000, -30,  1000);\n\
    var fbr = createVector( 1000, -30,  1000);\n\
    triangles[i++] = new Triangle(fbl, fbr, ffr);\n\
    triangles[i-1].shader = floorShader;\n\
    triangles[i++] = new Triangle(fbl, ffr, ffl);\n\
    triangles[i-1].shader = floorShader;\n\
    \n\
    var _scene = new Scene(triangles);\n\
    _scene.lights[0] = createVector(20, 38, -22);\n\
    _scene.lights[0].colour = createVector(0.7, 0.3, 0.3);\n\
    _scene.lights[1] = createVector(-23, 40, 17);\n\
    _scene.lights[1].colour = createVector(0.7, 0.3, 0.3);\n\
    _scene.lights[2] = createVector(23, 20, 17);\n\
    _scene.lights[2].colour = createVector(0.7, 0.7, 0.7);\n\
    _scene.ambient = createVector(0.1, 0.1, 0.1);\n\
    //  _scene.background = createVector(0.7, 0.7, 1.0);\n\
    \n\
    var size = 30;\n\
    var pixels = new Array();\n\
    for (var y = 0; y < size; y++) {\n\
        pixels[y] = new Array();\n\
        for (var x = 0; x < size; x++) {\n\
            pixels[y][x] = 0;\n\
        }\n\
    }\n\
\n\
    var _camera = new Camera(createVector(-40, 40, 40), createVector(0, 0, 0), createVector(0, 1, 0));\n\
    _camera.render(_scene, pixels, size, size);\n\
\n\
    return pixels;\n\
}\n\
\n\
function arrayToCanvasCommands(pixels)\n\
{\n\
    var s = '<canvas id=\"renderCanvas\" width=\"30px\" height=\"30px\"></canvas><scr' + 'ipt>\\nvar pixels = [';\n\
    var size = 30;\n\
    for (var y = 0; y < size; y++) {\n\
        s += \"[\";\n\
        for (var x = 0; x < size; x++) {\n\
            s += \"[\" + pixels[y][x] + \"],\";\n\
        }\n\
        s+= \"],\";\n\
    }\n\
    s += '];\\n    var canvas = document.getElementById(\"renderCanvas\").getContext(\"2d\");\\n\\\n\
\\n\\\n\
\\n\\\n\
    var size = 30;\\n\\\n\
    canvas.fillStyle = \"red\";\\n\\\n\
    canvas.fillRect(0, 0, size, size);\\n\\\n\
    canvas.scale(1, -1);\\n\\\n\
    canvas.translate(0, -size);\\n\\\n\
\\n\\\n\
    if (!canvas.setFillColor)\\n\\\n\
        canvas.setFillColor = function(r, g, b, a) {\\n\\\n\
            this.fillStyle = \"rgb(\"+[Math.floor(r * 255), Math.floor(g * 255), Math.floor(b * 255)]+\")\";\\n\\\n\
    }\\n\\\n\
\\n\\\n\
for (var y = 0; y < size; y++) {\\n\\\n\
  for (var x = 0; x < size; x++) {\\n\\\n\
    var l = pixels[y][x];\\n\\\n\
    canvas.setFillColor(l[0], l[1], l[2], 1);\\n\\\n\
    canvas.fillRect(x, y, 1, 1);\\n\\\n\
  }\\n\\\n\
}</scr' + 'ipt>';\n\
\n\
    return s;\n\
}\n\
\n\
testOutput = arrayToCanvasCommands(raytraceScene());\n\
\n\
var expectedLength = 20970;\n\
\n\
if (testOutput.length != expectedLength)\n\
    throw \"Error: bad result: expected length \" + expectedLength + \" but got \" + testOutput.length;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider access-binary-trees</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>access-binary-trees</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"access-binary-trees failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/* The Great Computer Language Shootout\n\
   http://shootout.alioth.debian.org/\n\
   contributed by Isaac Gouy */\n\
\n\
function TreeNode(left,right,item){\n\
   this.left = left;\n\
   this.right = right;\n\
   this.item = item;\n\
}\n\
\n\
TreeNode.prototype.itemCheck = function(){\n\
   if (this.left==null) return this.item;\n\
   else return this.item + this.left.itemCheck() - this.right.itemCheck();\n\
}\n\
\n\
function bottomUpTree(item,depth){\n\
   if (depth>0){\n\
      return new TreeNode(\n\
          bottomUpTree(2*item-1, depth-1)\n\
         ,bottomUpTree(2*item, depth-1)\n\
         ,item\n\
      );\n\
   }\n\
   else {\n\
      return new TreeNode(null,null,item);\n\
   }\n\
}\n\
\n\
var ret = 0;\n\
\n\
for ( var n = 4; n <= 7; n += 1 ) {\n\
    var minDepth = 4;\n\
    var maxDepth = Math.max(minDepth + 2, n);\n\
    var stretchDepth = maxDepth + 1;\n\
    \n\
    var check = bottomUpTree(0,stretchDepth).itemCheck();\n\
    \n\
    var longLivedTree = bottomUpTree(0,maxDepth);\n\
    for (var depth=minDepth; depth<=maxDepth; depth+=2){\n\
        var iterations = 1 << (maxDepth - depth + minDepth);\n\
\n\
        check = 0;\n\
        for (var i=1; i<=iterations; i++){\n\
            check += bottomUpTree(i,depth).itemCheck();\n\
            check += bottomUpTree(-i,depth).itemCheck();\n\
        }\n\
    }\n\
\n\
    ret += longLivedTree.itemCheck();\n\
}\n\
\n\
var expected = -4;\n\
if (ret != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + ret;\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider access-fannkuch</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>access-fannkuch</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"access-fannkuch failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/* The Great Computer Language Shootout\n\
   http://shootout.alioth.debian.org/\n\
   contributed by Isaac Gouy */\n\
\n\
function fannkuch(n) {\n\
   var check = 0;\n\
   var perm = Array(n);\n\
   var perm1 = Array(n);\n\
   var count = Array(n);\n\
   var maxPerm = Array(n);\n\
   var maxFlipsCount = 0;\n\
   var m = n - 1;\n\
\n\
   for (var i = 0; i < n; i++) perm1[i] = i;\n\
   var r = n;\n\
\n\
   while (true) {\n\
      // write-out the first 30 permutations\n\
      if (check < 30){\n\
         var s = \"\";\n\
         for(var i=0; i<n; i++) s += (perm1[i]+1).toString();\n\
         check++;\n\
      }\n\
\n\
      while (r != 1) { count[r - 1] = r; r--; }\n\
      if (!(perm1[0] == 0 || perm1[m] == m)) {\n\
         for (var i = 0; i < n; i++) perm[i] = perm1[i];\n\
\n\
         var flipsCount = 0;\n\
         var k;\n\
\n\
         while (!((k = perm[0]) == 0)) {\n\
            var k2 = (k + 1) >> 1;\n\
            for (var i = 0; i < k2; i++) {\n\
               var temp = perm[i]; perm[i] = perm[k - i]; perm[k - i] = temp;\n\
            }\n\
            flipsCount++;\n\
         }\n\
\n\
         if (flipsCount > maxFlipsCount) {\n\
            maxFlipsCount = flipsCount;\n\
            for (var i = 0; i < n; i++) maxPerm[i] = perm1[i];\n\
         }\n\
      }\n\
\n\
      while (true) {\n\
         if (r == n) return maxFlipsCount;\n\
         var perm0 = perm1[0];\n\
         var i = 0;\n\
         while (i < r) {\n\
            var j = i + 1;\n\
            perm1[i] = perm1[j];\n\
            i = j;\n\
         }\n\
         perm1[r] = perm0;\n\
\n\
         count[r] = count[r] - 1;\n\
         if (count[r] > 0) break;\n\
         r++;\n\
      }\n\
   }\n\
}\n\
\n\
var n = 8;\n\
var ret = fannkuch(n);\n\
\n\
var expected = 22;\n\
if (ret != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + ret;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider access-nbody</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>access-nbody</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"access-nbody failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/* The Great Computer Language Shootout\n\
   http://shootout.alioth.debian.org/\n\
   contributed by Isaac Gouy */\n\
\n\
var PI = 3.141592653589793;\n\
var SOLAR_MASS = 4 * PI * PI;\n\
var DAYS_PER_YEAR = 365.24;\n\
\n\
function Body(x,y,z,vx,vy,vz,mass){\n\
   this.x = x;\n\
   this.y = y;\n\
   this.z = z;\n\
   this.vx = vx;\n\
   this.vy = vy;\n\
   this.vz = vz;\n\
   this.mass = mass;\n\
}\n\
\n\
Body.prototype.offsetMomentum = function(px,py,pz) {\n\
   this.vx = -px / SOLAR_MASS;\n\
   this.vy = -py / SOLAR_MASS;\n\
   this.vz = -pz / SOLAR_MASS;\n\
   return this;\n\
}\n\
\n\
function Jupiter(){\n\
   return new Body(\n\
      4.84143144246472090e+00,\n\
      -1.16032004402742839e+00,\n\
      -1.03622044471123109e-01,\n\
      1.66007664274403694e-03 * DAYS_PER_YEAR,\n\
      7.69901118419740425e-03 * DAYS_PER_YEAR,\n\
      -6.90460016972063023e-05 * DAYS_PER_YEAR,\n\
      9.54791938424326609e-04 * SOLAR_MASS\n\
   );\n\
}\n\
\n\
function Saturn(){\n\
   return new Body(\n\
      8.34336671824457987e+00,\n\
      4.12479856412430479e+00,\n\
      -4.03523417114321381e-01,\n\
      -2.76742510726862411e-03 * DAYS_PER_YEAR,\n\
      4.99852801234917238e-03 * DAYS_PER_YEAR,\n\
      2.30417297573763929e-05 * DAYS_PER_YEAR,\n\
      2.85885980666130812e-04 * SOLAR_MASS\n\
   );\n\
}\n\
\n\
function Uranus(){\n\
   return new Body(\n\
      1.28943695621391310e+01,\n\
      -1.51111514016986312e+01,\n\
      -2.23307578892655734e-01,\n\
      2.96460137564761618e-03 * DAYS_PER_YEAR,\n\
      2.37847173959480950e-03 * DAYS_PER_YEAR,\n\
      -2.96589568540237556e-05 * DAYS_PER_YEAR,\n\
      4.36624404335156298e-05 * SOLAR_MASS\n\
   );\n\
}\n\
\n\
function Neptune(){\n\
   return new Body(\n\
      1.53796971148509165e+01,\n\
      -2.59193146099879641e+01,\n\
      1.79258772950371181e-01,\n\
      2.68067772490389322e-03 * DAYS_PER_YEAR,\n\
      1.62824170038242295e-03 * DAYS_PER_YEAR,\n\
      -9.51592254519715870e-05 * DAYS_PER_YEAR,\n\
      5.15138902046611451e-05 * SOLAR_MASS\n\
   );\n\
}\n\
\n\
function Sun(){\n\
   return new Body(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, SOLAR_MASS);\n\
}\n\
\n\
\n\
function NBodySystem(bodies){\n\
   this.bodies = bodies;\n\
   var px = 0.0;\n\
   var py = 0.0;\n\
   var pz = 0.0;\n\
   var size = this.bodies.length;\n\
   for (var i=0; i<size; i++){\n\
      var b = this.bodies[i];\n\
      var m = b.mass;\n\
      px += b.vx * m;\n\
      py += b.vy * m;\n\
      pz += b.vz * m;\n\
   }\n\
   this.bodies[0].offsetMomentum(px,py,pz);\n\
}\n\
\n\
NBodySystem.prototype.advance = function(dt){\n\
   var dx, dy, dz, distance, mag;\n\
   var size = this.bodies.length;\n\
\n\
   for (var i=0; i<size; i++) {\n\
      var bodyi = this.bodies[i];\n\
      for (var j=i+1; j<size; j++) {\n\
         var bodyj = this.bodies[j];\n\
         dx = bodyi.x - bodyj.x;\n\
         dy = bodyi.y - bodyj.y;\n\
         dz = bodyi.z - bodyj.z;\n\
\n\
         distance = Math.sqrt(dx*dx + dy*dy + dz*dz);\n\
         mag = dt / (distance * distance * distance);\n\
\n\
         bodyi.vx -= dx * bodyj.mass * mag;\n\
         bodyi.vy -= dy * bodyj.mass * mag;\n\
         bodyi.vz -= dz * bodyj.mass * mag;\n\
\n\
         bodyj.vx += dx * bodyi.mass * mag;\n\
         bodyj.vy += dy * bodyi.mass * mag;\n\
         bodyj.vz += dz * bodyi.mass * mag;\n\
      }\n\
   }\n\
\n\
   for (var i=0; i<size; i++) {\n\
      var body = this.bodies[i];\n\
      body.x += dt * body.vx;\n\
      body.y += dt * body.vy;\n\
      body.z += dt * body.vz;\n\
   }\n\
}\n\
\n\
NBodySystem.prototype.energy = function(){\n\
   var dx, dy, dz, distance;\n\
   var e = 0.0;\n\
   var size = this.bodies.length;\n\
\n\
   for (var i=0; i<size; i++) {\n\
      var bodyi = this.bodies[i];\n\
\n\
      e += 0.5 * bodyi.mass *\n\
         ( bodyi.vx * bodyi.vx\n\
         + bodyi.vy * bodyi.vy\n\
         + bodyi.vz * bodyi.vz );\n\
\n\
      for (var j=i+1; j<size; j++) {\n\
         var bodyj = this.bodies[j];\n\
         dx = bodyi.x - bodyj.x;\n\
         dy = bodyi.y - bodyj.y;\n\
         dz = bodyi.z - bodyj.z;\n\
\n\
         distance = Math.sqrt(dx*dx + dy*dy + dz*dz);\n\
         e -= (bodyi.mass * bodyj.mass) / distance;\n\
      }\n\
   }\n\
   return e;\n\
}\n\
\n\
var ret = 0;\n\
\n\
for ( var n = 3; n <= 24; n *= 2 ) {\n\
    (function(){\n\
        var bodies = new NBodySystem( Array(\n\
           Sun(),Jupiter(),Saturn(),Uranus(),Neptune()\n\
        ));\n\
        var max = n * 100;\n\
        \n\
        ret += bodies.energy();\n\
        for (var i=0; i<max; i++){\n\
            bodies.advance(0.01);\n\
        }\n\
        ret += bodies.energy();\n\
    })();\n\
}\n\
\n\
var expected = -1.3524862408537381;\n\
if (ret != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + ret;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider access-nsieve</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>access-nsieve</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"access-nsieve failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// The Great Computer Language Shootout\n\
// http://shootout.alioth.debian.org/\n\
//\n\
// modified by Isaac Gouy\n\
\n\
function pad(number,width){\n\
   var s = number.toString();\n\
   var prefixWidth = width - s.length;\n\
   if (prefixWidth>0){\n\
      for (var i=1; i<=prefixWidth; i++) s = \" \" + s;\n\
   }\n\
   return s;\n\
}\n\
\n\
function nsieve(m, isPrime){\n\
   var i, k, count;\n\
\n\
   for (i=2; i<=m; i++) { isPrime[i] = true; }\n\
   count = 0;\n\
\n\
   for (i=2; i<=m; i++){\n\
      if (isPrime[i]) {\n\
         for (k=i+i; k<=m; k+=i) isPrime[k] = false;\n\
         count++;\n\
      }\n\
   }\n\
   return count;\n\
}\n\
\n\
function sieve() {\n\
    var sum = 0;\n\
    for (var i = 1; i <= 3; i++ ) {\n\
        var m = (1<<i)*10000;\n\
        var flags = Array(m+1);\n\
        sum += nsieve(m, flags);\n\
    }\n\
    return sum;\n\
}\n\
\n\
var result = sieve();\n\
\n\
var expected = 14302;\n\
if (result != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + result;\n\
\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider bitops-3bit-bits-in-byte</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>bitops-3bit-bits-in-byte</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"bitops-3bit-bits-in-byte failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// Copyright (c) 2004 by Arthur Langereis (arthur_ext at domain xfinitegames, tld com\n\
\n\
var result = 0;\n\
\n\
// 1 op = 6 ANDs, 3 SHRs, 3 SHLs, 4 assigns, 2 ADDs\n\
// O(1)\n\
function fast3bitlookup(b) {\n\
var c, bi3b = 0xE994; // 0b1110 1001 1001 0100; // 3 2 2 1  2 1 1 0\n\
c  = 3 & (bi3b >> ((b << 1) & 14));\n\
c += 3 & (bi3b >> ((b >> 2) & 14));\n\
c += 3 & (bi3b >> ((b >> 5) & 6));\n\
return c;\n\
\n\
/*\n\
lir4,0xE994; 9 instructions, no memory access, minimal register dependence, 6 shifts, 2 adds, 1 inline assign\n\
rlwinmr5,r3,1,28,30\n\
rlwinmr6,r3,30,28,30\n\
rlwinmr7,r3,27,29,30\n\
rlwnmr8,r4,r5,30,31\n\
rlwnmr9,r4,r6,30,31\n\
rlwnmr10,r4,r7,30,31\n\
addr3,r8,r9\n\
addr3,r3,r10\n\
*/\n\
}\n\
\n\
\n\
function TimeFunc(func) {\n\
var x, y, t;\n\
var sum = 0;\n\
for(var x=0; x<500; x++)\n\
for(var y=0; y<256; y++) sum += func(y);\n\
return sum;\n\
}\n\
\n\
sum = TimeFunc(fast3bitlookup);\n\
\n\
var expected = 512000;\n\
if (sum != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + sum;\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider bitops-bits-in-byte</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>bitops-bits-in-byte</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"bitops-bits-in-byte failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// Copyright (c) 2004 by Arthur Langereis (arthur_ext at domain xfinitegames, tld com)\n\
\n\
\n\
var result = 0;\n\
\n\
// 1 op = 2 assigns, 16 compare/branches, 8 ANDs, (0-8) ADDs, 8 SHLs\n\
// O(n)\n\
function bitsinbyte(b) {\n\
var m = 1, c = 0;\n\
while(m<0x100) {\n\
if(b & m) c++;\n\
m <<= 1;\n\
}\n\
return c;\n\
}\n\
\n\
function TimeFunc(func) {\n\
var x, y, t;\n\
var sum = 0;\n\
for(var x=0; x<350; x++)\n\
for(var y=0; y<256; y++) sum += func(y);\n\
return sum;\n\
}\n\
\n\
result = TimeFunc(bitsinbyte);\n\
\n\
var expected = 358400;\n\
if (result != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + result;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider bitops-bitwise-and</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>bitops-bitwise-and</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"bitops-bitwise-and failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/*\n\
 * Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
 *\n\
 * Redistribution and use in source and binary forms, with or without\n\
 * modification, are permitted provided that the following conditions\n\
 * are met:\n\
 * 1. Redistributions of source code must retain the above copyright\n\
 *    notice, this list of conditions and the following disclaimer.\n\
 * 2. Redistributions in binary form must reproduce the above copyright\n\
 *    notice, this list of conditions and the following disclaimer in the\n\
 *    documentation and/or other materials provided with the distribution.\n\
 *\n\
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
 */\n\
\n\
bitwiseAndValue = 4294967296;\n\
for (var i = 0; i < 600000; i++)\n\
    bitwiseAndValue = bitwiseAndValue & i;\n\
\n\
var result = bitwiseAndValue;\n\
\n\
var expected = 0;\n\
if (result != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + result;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider bitops-nsieve-bits</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>bitops-nsieve-bits</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"bitops-nsieve-bits failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// The Great Computer Language Shootout\n\
//  http://shootout.alioth.debian.org\n\
//\n\
//  Contributed by Ian Osgood\n\
\n\
function pad(n,width) {\n\
  var s = n.toString();\n\
  while (s.length < width) s = ' ' + s;\n\
  return s;\n\
}\n\
\n\
function primes(isPrime, n) {\n\
  var i, count = 0, m = 10000<<n, size = m+31>>5;\n\
\n\
  for (i=0; i<size; i++) isPrime[i] = 0xffffffff;\n\
\n\
  for (i=2; i<m; i++)\n\
    if (isPrime[i>>5] & 1<<(i&31)) {\n\
      for (var j=i+i; j<m; j+=i)\n\
        isPrime[j>>5] &= ~(1<<(j&31));\n\
      count++;\n\
    }\n\
}\n\
\n\
function sieve() {\n\
    for (var i = 4; i <= 4; i++) {\n\
        var isPrime = new Array((10000<<i)+31>>5);\n\
        primes(isPrime, i);\n\
    }\n\
    return isPrime;\n\
}\n\
\n\
var result = sieve();\n\
\n\
var sum = 0;\n\
for (var i = 0; i < result.length; ++i)\n\
    sum += result[i];\n\
\n\
var expected = -1286749544853;\n\
if (sum != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + sum;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider controlflow-recursive</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>controlflow-recursive</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"controlflow-recursive failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// The Computer Language Shootout\n\
// http://shootout.alioth.debian.org/\n\
// contributed by Isaac Gouy\n\
\n\
function ack(m,n){\n\
   if (m==0) { return n+1; }\n\
   if (n==0) { return ack(m-1,1); }\n\
   return ack(m-1, ack(m,n-1) );\n\
}\n\
\n\
function fib(n) {\n\
    if (n < 2){ return 1; }\n\
    return fib(n-2) + fib(n-1);\n\
}\n\
\n\
function tak(x,y,z) {\n\
    if (y >= x) return z;\n\
    return tak(tak(x-1,y,z), tak(y-1,z,x), tak(z-1,x,y));\n\
}\n\
\n\
var result = 0;\n\
\n\
for ( var i = 3; i <= 5; i++ ) {\n\
    result += ack(3,i);\n\
    result += fib(17.0+i);\n\
    result += tak(3*i+3,2*i+2,i+1);\n\
}\n\
\n\
var expected = 57775;\n\
if (result != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + result;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider crypto-aes</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>crypto-aes</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"crypto-aes failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */\n\
\n\
/*\n\
 * AES Cipher function: encrypt 'input' with Rijndael algorithm\n\
 *\n\
 *   takes   byte-array 'input' (16 bytes)\n\
 *           2D byte-array key schedule 'w' (Nr+1 x Nb bytes)\n\
 *\n\
 *   applies Nr rounds (10/12/14) using key schedule w for 'add round key' stage\n\
 *\n\
 *   returns byte-array encrypted value (16 bytes)\n\
 */\n\
function Cipher(input, w) {    // main Cipher function [§5.1]\n\
  var Nb = 4;               // block size (in words): no of columns in state (fixed at 4 for AES)\n\
  var Nr = w.length/Nb - 1; // no of rounds: 10/12/14 for 128/192/256-bit keys\n\
\n\
  var state = [[],[],[],[]];  // initialise 4xNb byte-array 'state' with input [§3.4]\n\
  for (var i=0; i<4*Nb; i++) state[i%4][Math.floor(i/4)] = input[i];\n\
\n\
  state = AddRoundKey(state, w, 0, Nb);\n\
\n\
  for (var round=1; round<Nr; round++) {\n\
    state = SubBytes(state, Nb);\n\
    state = ShiftRows(state, Nb);\n\
    state = MixColumns(state, Nb);\n\
    state = AddRoundKey(state, w, round, Nb);\n\
  }\n\
\n\
  state = SubBytes(state, Nb);\n\
  state = ShiftRows(state, Nb);\n\
  state = AddRoundKey(state, w, Nr, Nb);\n\
\n\
  var output = new Array(4*Nb);  // convert state to 1-d array before returning [§3.4]\n\
  for (var i=0; i<4*Nb; i++) output[i] = state[i%4][Math.floor(i/4)];\n\
  return output;\n\
}\n\
\n\
\n\
function SubBytes(s, Nb) {    // apply SBox to state S [§5.1.1]\n\
  for (var r=0; r<4; r++) {\n\
    for (var c=0; c<Nb; c++) s[r][c] = Sbox[s[r][c]];\n\
  }\n\
  return s;\n\
}\n\
\n\
\n\
function ShiftRows(s, Nb) {    // shift row r of state S left by r bytes [§5.1.2]\n\
  var t = new Array(4);\n\
  for (var r=1; r<4; r++) {\n\
    for (var c=0; c<4; c++) t[c] = s[r][(c+r)%Nb];  // shift into temp copy\n\
    for (var c=0; c<4; c++) s[r][c] = t[c];         // and copy back\n\
  }          // note that this will work for Nb=4,5,6, but not 7,8 (always 4 for AES):\n\
  return s;  // see fp.gladman.plus.com/cryptography_technology/rijndael/aes.spec.311.pdf \n\
}\n\
\n\
\n\
function MixColumns(s, Nb) {   // combine bytes of each col of state S [§5.1.3]\n\
  for (var c=0; c<4; c++) {\n\
    var a = new Array(4);  // 'a' is a copy of the current column from 's'\n\
    var b = new Array(4);  // 'b' is a•{02} in GF(2^8)\n\
    for (var i=0; i<4; i++) {\n\
      a[i] = s[i][c];\n\
      b[i] = s[i][c]&0x80 ? s[i][c]<<1 ^ 0x011b : s[i][c]<<1;\n\
    }\n\
    // a[n] ^ b[n] is a•{03} in GF(2^8)\n\
    s[0][c] = b[0] ^ a[1] ^ b[1] ^ a[2] ^ a[3]; // 2*a0 + 3*a1 + a2 + a3\n\
    s[1][c] = a[0] ^ b[1] ^ a[2] ^ b[2] ^ a[3]; // a0 * 2*a1 + 3*a2 + a3\n\
    s[2][c] = a[0] ^ a[1] ^ b[2] ^ a[3] ^ b[3]; // a0 + a1 + 2*a2 + 3*a3\n\
    s[3][c] = a[0] ^ b[0] ^ a[1] ^ a[2] ^ b[3]; // 3*a0 + a1 + a2 + 2*a3\n\
  }\n\
  return s;\n\
}\n\
\n\
\n\
function AddRoundKey(state, w, rnd, Nb) {  // xor Round Key into state S [§5.1.4]\n\
  for (var r=0; r<4; r++) {\n\
    for (var c=0; c<Nb; c++) state[r][c] ^= w[rnd*4+c][r];\n\
  }\n\
  return state;\n\
}\n\
\n\
\n\
function KeyExpansion(key) {  // generate Key Schedule (byte-array Nr+1 x Nb) from Key [§5.2]\n\
  var Nb = 4;            // block size (in words): no of columns in state (fixed at 4 for AES)\n\
  var Nk = key.length/4  // key length (in words): 4/6/8 for 128/192/256-bit keys\n\
  var Nr = Nk + 6;       // no of rounds: 10/12/14 for 128/192/256-bit keys\n\
\n\
  var w = new Array(Nb*(Nr+1));\n\
  var temp = new Array(4);\n\
\n\
  for (var i=0; i<Nk; i++) {\n\
    var r = [key[4*i], key[4*i+1], key[4*i+2], key[4*i+3]];\n\
    w[i] = r;\n\
  }\n\
\n\
  for (var i=Nk; i<(Nb*(Nr+1)); i++) {\n\
    w[i] = new Array(4);\n\
    for (var t=0; t<4; t++) temp[t] = w[i-1][t];\n\
    if (i % Nk == 0) {\n\
      temp = SubWord(RotWord(temp));\n\
      for (var t=0; t<4; t++) temp[t] ^= Rcon[i/Nk][t];\n\
    } else if (Nk > 6 && i%Nk == 4) {\n\
      temp = SubWord(temp);\n\
    }\n\
    for (var t=0; t<4; t++) w[i][t] = w[i-Nk][t] ^ temp[t];\n\
  }\n\
\n\
  return w;\n\
}\n\
\n\
function SubWord(w) {    // apply SBox to 4-byte word w\n\
  for (var i=0; i<4; i++) w[i] = Sbox[w[i]];\n\
  return w;\n\
}\n\
\n\
function RotWord(w) {    // rotate 4-byte word w left by one byte\n\
  w[4] = w[0];\n\
  for (var i=0; i<4; i++) w[i] = w[i+1];\n\
  return w;\n\
}\n\
\n\
\n\
// Sbox is pre-computed multiplicative inverse in GF(2^8) used in SubBytes and KeyExpansion [§5.1.1]\n\
var Sbox =  [0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,\n\
             0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,\n\
             0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,\n\
             0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,\n\
             0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,\n\
             0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,\n\
             0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,\n\
             0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,\n\
             0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,\n\
             0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,\n\
             0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,\n\
             0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,\n\
             0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,\n\
             0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,\n\
             0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,\n\
             0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16];\n\
\n\
// Rcon is Round Constant used for the Key Expansion [1st col is 2^(r-1) in GF(2^8)] [§5.2]\n\
var Rcon = [ [0x00, 0x00, 0x00, 0x00],\n\
             [0x01, 0x00, 0x00, 0x00],\n\
             [0x02, 0x00, 0x00, 0x00],\n\
             [0x04, 0x00, 0x00, 0x00],\n\
             [0x08, 0x00, 0x00, 0x00],\n\
             [0x10, 0x00, 0x00, 0x00],\n\
             [0x20, 0x00, 0x00, 0x00],\n\
             [0x40, 0x00, 0x00, 0x00],\n\
             [0x80, 0x00, 0x00, 0x00],\n\
             [0x1b, 0x00, 0x00, 0x00],\n\
             [0x36, 0x00, 0x00, 0x00] ]; \n\
\n\
\n\
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */\n\
\n\
/* \n\
 * Use AES to encrypt 'plaintext' with 'password' using 'nBits' key, in 'Counter' mode of operation\n\
 *                           - see http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf\n\
 *   for each block\n\
 *   - outputblock = cipher(counter, key)\n\
 *   - cipherblock = plaintext xor outputblock\n\
 */\n\
function AESEncryptCtr(plaintext, password, nBits) {\n\
  if (!(nBits==128 || nBits==192 || nBits==256)) return '';  // standard allows 128/192/256 bit keys\n\
\n\
  // for this example script, generate the key by applying Cipher to 1st 16/24/32 chars of password; \n\
  // for real-world applications, a more secure approach would be to hash the password e.g. with SHA-1\n\
  var nBytes = nBits/8;  // no bytes in key\n\
  var pwBytes = new Array(nBytes);\n\
  for (var i=0; i<nBytes; i++) pwBytes[i] = password.charCodeAt(i) & 0xff;\n\
  var key = Cipher(pwBytes, KeyExpansion(pwBytes));\n\
  key = key.concat(key.slice(0, nBytes-16));  // key is now 16/24/32 bytes long\n\
\n\
  // initialise counter block (NIST SP800-38A §B.2): millisecond time-stamp for nonce in 1st 8 bytes,\n\
  // block counter in 2nd 8 bytes\n\
  var blockSize = 16;  // block size fixed at 16 bytes / 128 bits (Nb=4) for AES\n\
  var counterBlock = new Array(blockSize);  // block size fixed at 16 bytes / 128 bits (Nb=4) for AES\n\
  var nonce = (new Date()).getTime();  // milliseconds since 1-Jan-1970\n\
\n\
  // encode nonce in two stages to cater for JavaScript 32-bit limit on bitwise ops\n\
  for (var i=0; i<4; i++) counterBlock[i] = (nonce >>> i*8) & 0xff;\n\
  for (var i=0; i<4; i++) counterBlock[i+4] = (nonce/0x100000000 >>> i*8) & 0xff; \n\
\n\
  // generate key schedule - an expansion of the key into distinct Key Rounds for each round\n\
  var keySchedule = KeyExpansion(key);\n\
\n\
  var blockCount = Math.ceil(plaintext.length/blockSize);\n\
  var ciphertext = new Array(blockCount);  // ciphertext as array of strings\n\
  \n\
  for (var b=0; b<blockCount; b++) {\n\
    // set counter (block #) in last 8 bytes of counter block (leaving nonce in 1st 8 bytes)\n\
    // again done in two stages for 32-bit ops\n\
    for (var c=0; c<4; c++) counterBlock[15-c] = (b >>> c*8) & 0xff;\n\
    for (var c=0; c<4; c++) counterBlock[15-c-4] = (b/0x100000000 >>> c*8)\n\
\n\
    var cipherCntr = Cipher(counterBlock, keySchedule);  // -- encrypt counter block --\n\
    \n\
    // calculate length of final block:\n\
    var blockLength = b<blockCount-1 ? blockSize : (plaintext.length-1)%blockSize+1;\n\
\n\
    var ct = '';\n\
    for (var i=0; i<blockLength; i++) {  // -- xor plaintext with ciphered counter byte-by-byte --\n\
      var plaintextByte = plaintext.charCodeAt(b*blockSize+i);\n\
      var cipherByte = plaintextByte ^ cipherCntr[i];\n\
      ct += String.fromCharCode(cipherByte);\n\
    }\n\
    // ct is now ciphertext for this block\n\
\n\
    ciphertext[b] = escCtrlChars(ct);  // escape troublesome characters in ciphertext\n\
  }\n\
\n\
  // convert the nonce to a string to go on the front of the ciphertext\n\
  var ctrTxt = '';\n\
  for (var i=0; i<8; i++) ctrTxt += String.fromCharCode(counterBlock[i]);\n\
  ctrTxt = escCtrlChars(ctrTxt);\n\
\n\
  // use '-' to separate blocks, use Array.join to concatenate arrays of strings for efficiency\n\
  return ctrTxt + '-' + ciphertext.join('-');\n\
}\n\
\n\
\n\
/* \n\
 * Use AES to decrypt 'ciphertext' with 'password' using 'nBits' key, in Counter mode of operation\n\
 *\n\
 *   for each block\n\
 *   - outputblock = cipher(counter, key)\n\
 *   - cipherblock = plaintext xor outputblock\n\
 */\n\
function AESDecryptCtr(ciphertext, password, nBits) {\n\
  if (!(nBits==128 || nBits==192 || nBits==256)) return '';  // standard allows 128/192/256 bit keys\n\
\n\
  var nBytes = nBits/8;  // no bytes in key\n\
  var pwBytes = new Array(nBytes);\n\
  for (var i=0; i<nBytes; i++) pwBytes[i] = password.charCodeAt(i) & 0xff;\n\
  var pwKeySchedule = KeyExpansion(pwBytes);\n\
  var key = Cipher(pwBytes, pwKeySchedule);\n\
  key = key.concat(key.slice(0, nBytes-16));  // key is now 16/24/32 bytes long\n\
\n\
  var keySchedule = KeyExpansion(key);\n\
\n\
  ciphertext = ciphertext.split('-');  // split ciphertext into array of block-length strings \n\
\n\
  // recover nonce from 1st element of ciphertext\n\
  var blockSize = 16;  // block size fixed at 16 bytes / 128 bits (Nb=4) for AES\n\
  var counterBlock = new Array(blockSize);\n\
  var ctrTxt = unescCtrlChars(ciphertext[0]);\n\
  for (var i=0; i<8; i++) counterBlock[i] = ctrTxt.charCodeAt(i);\n\
\n\
  var plaintext = new Array(ciphertext.length-1);\n\
\n\
  for (var b=1; b<ciphertext.length; b++) {\n\
    // set counter (block #) in last 8 bytes of counter block (leaving nonce in 1st 8 bytes)\n\
    for (var c=0; c<4; c++) counterBlock[15-c] = ((b-1) >>> c*8) & 0xff;\n\
    for (var c=0; c<4; c++) counterBlock[15-c-4] = ((b/0x100000000-1) >>> c*8) & 0xff;\n\
\n\
    var cipherCntr = Cipher(counterBlock, keySchedule);  // encrypt counter block\n\
\n\
    ciphertext[b] = unescCtrlChars(ciphertext[b]);\n\
\n\
    var pt = '';\n\
    for (var i=0; i<ciphertext[b].length; i++) {\n\
      // -- xor plaintext with ciphered counter byte-by-byte --\n\
      var ciphertextByte = ciphertext[b].charCodeAt(i);\n\
      var plaintextByte = ciphertextByte ^ cipherCntr[i];\n\
      pt += String.fromCharCode(plaintextByte);\n\
    }\n\
    // pt is now plaintext for this block\n\
\n\
    plaintext[b-1] = pt;  // b-1 'cos no initial nonce block in plaintext\n\
  }\n\
\n\
  return plaintext.join('');\n\
}\n\
\n\
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */\n\
\n\
function escCtrlChars(str) {  // escape control chars which might cause problems handling ciphertext\n\
  return str.replace(/[\\0\\t\\n\\v\\f\\r\\xa0'\"!-]/g, function(c) { return '!' + c.charCodeAt(0) + '!'; });\n\
}  // \\xa0 to cater for bug in Firefox; include '-' to leave it free for use as a block marker\n\
\n\
function unescCtrlChars(str) {  // unescape potentially problematic control characters\n\
  return str.replace(/!\\d\\d?\\d?!/g, function(c) { return String.fromCharCode(c.slice(1,-1)); });\n\
}\n\
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */\n\
\n\
/*\n\
 * if escCtrlChars()/unescCtrlChars() still gives problems, use encodeBase64()/decodeBase64() instead\n\
 */\n\
var b64 = \"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\";\n\
\n\
function encodeBase64(str) {  // http://tools.ietf.org/html/rfc4648\n\
   var o1, o2, o3, h1, h2, h3, h4, bits, i=0, enc='';\n\
   \n\
   str = encodeUTF8(str);  // encode multi-byte chars into UTF-8 for byte-array\n\
\n\
   do {  // pack three octets into four hexets\n\
      o1 = str.charCodeAt(i++);\n\
      o2 = str.charCodeAt(i++);\n\
      o3 = str.charCodeAt(i++);\n\
      \n\
      bits = o1<<16 | o2<<8 | o3;\n\
      \n\
      h1 = bits>>18 & 0x3f;\n\
      h2 = bits>>12 & 0x3f;\n\
      h3 = bits>>6 & 0x3f;\n\
      h4 = bits & 0x3f;\n\
      \n\
      // end of string? index to '=' in b64\n\
      if (isNaN(o3)) h4 = 64;\n\
      if (isNaN(o2)) h3 = 64;\n\
      \n\
      // use hexets to index into b64, and append result to encoded string\n\
      enc += b64.charAt(h1) + b64.charAt(h2) + b64.charAt(h3) + b64.charAt(h4);\n\
   } while (i < str.length);\n\
   \n\
   return enc;\n\
}\n\
\n\
function decodeBase64(str) {\n\
   var o1, o2, o3, h1, h2, h3, h4, bits, i=0, enc='';\n\
\n\
   do {  // unpack four hexets into three octets using index points in b64\n\
      h1 = b64.indexOf(str.charAt(i++));\n\
      h2 = b64.indexOf(str.charAt(i++));\n\
      h3 = b64.indexOf(str.charAt(i++));\n\
      h4 = b64.indexOf(str.charAt(i++));\n\
      \n\
      bits = h1<<18 | h2<<12 | h3<<6 | h4;\n\
      \n\
      o1 = bits>>16 & 0xff;\n\
      o2 = bits>>8 & 0xff;\n\
      o3 = bits & 0xff;\n\
      \n\
      if (h3 == 64)      enc += String.fromCharCode(o1);\n\
      else if (h4 == 64) enc += String.fromCharCode(o1, o2);\n\
      else               enc += String.fromCharCode(o1, o2, o3);\n\
   } while (i < str.length);\n\
\n\
   return decodeUTF8(enc);  // decode UTF-8 byte-array back to Unicode\n\
}\n\
\n\
function encodeUTF8(str) {  // encode multi-byte string into utf-8 multiple single-byte characters \n\
  str = str.replace(\n\
      /[\\u0080-\\u07ff]/g,  // U+0080 - U+07FF = 2-byte chars\n\
      function(c) { \n\
        var cc = c.charCodeAt(0);\n\
        return String.fromCharCode(0xc0 | cc>>6, 0x80 | cc&0x3f); }\n\
    );\n\
  str = str.replace(\n\
      /[\\u0800-\\uffff]/g,  // U+0800 - U+FFFF = 3-byte chars\n\
      function(c) { \n\
        var cc = c.charCodeAt(0); \n\
        return String.fromCharCode(0xe0 | cc>>12, 0x80 | cc>>6&0x3F, 0x80 | cc&0x3f); }\n\
    );\n\
  return str;\n\
}\n\
\n\
function decodeUTF8(str) {  // decode utf-8 encoded string back into multi-byte characters\n\
  str = str.replace(\n\
      /[\\u00c0-\\u00df][\\u0080-\\u00bf]/g,                 // 2-byte chars\n\
      function(c) { \n\
        var cc = (c.charCodeAt(0)&0x1f)<<6 | c.charCodeAt(1)&0x3f;\n\
        return String.fromCharCode(cc); }\n\
    );\n\
  str = str.replace(\n\
      /[\\u00e0-\\u00ef][\\u0080-\\u00bf][\\u0080-\\u00bf]/g,  // 3-byte chars\n\
      function(c) { \n\
        var cc = (c.charCodeAt(0)&0x0f)<<12 | (c.charCodeAt(1)&0x3f<<6) | c.charCodeAt(2)&0x3f; \n\
        return String.fromCharCode(cc); }\n\
    );\n\
  return str;\n\
}\n\
\n\
\n\
function byteArrayToHexStr(b) {  // convert byte array to hex string for displaying test vectors\n\
  var s = '';\n\
  for (var i=0; i<b.length; i++) s += b[i].toString(16) + ' ';\n\
  return s;\n\
}\n\
\n\
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */\n\
\n\
\n\
var plainText = \"ROMEO: But, soft! what light through yonder window breaks?\\n\\\n\
It is the east, and Juliet is the sun.\\n\\\n\
Arise, fair sun, and kill the envious moon,\\n\\\n\
Who is already sick and pale with grief,\\n\\\n\
That thou her maid art far more fair than she:\\n\\\n\
Be not her maid, since she is envious;\\n\\\n\
Her vestal livery is but sick and green\\n\\\n\
And none but fools do wear it; cast it off.\\n\\\n\
It is my lady, O, it is my love!\\n\\\n\
O, that she knew she were!\\n\\\n\
She speaks yet she says nothing: what of that?\\n\\\n\
Her eye discourses; I will answer it.\\n\\\n\
I am too bold, 'tis not to me she speaks:\\n\\\n\
Two of the fairest stars in all the heaven,\\n\\\n\
Having some business, do entreat her eyes\\n\\\n\
To twinkle in their spheres till they return.\\n\\\n\
What if her eyes were there, they in her head?\\n\\\n\
The brightness of her cheek would shame those stars,\\n\\\n\
As daylight doth a lamp; her eyes in heaven\\n\\\n\
Would through the airy region stream so bright\\n\\\n\
That birds would sing and think it were not night.\\n\\\n\
See, how she leans her cheek upon her hand!\\n\\\n\
O, that I were a glove upon that hand,\\n\\\n\
That I might touch that cheek!\\n\\\n\
JULIET: Ay me!\\n\\\n\
ROMEO: She speaks:\\n\\\n\
O, speak again, bright angel! for thou art\\n\\\n\
As glorious to this night, being o'er my head\\n\\\n\
As is a winged messenger of heaven\\n\\\n\
Unto the white-upturned wondering eyes\\n\\\n\
Of mortals that fall back to gaze on him\\n\\\n\
When he bestrides the lazy-pacing clouds\\n\\\n\
And sails upon the bosom of the air.\";\n\
\n\
var password = \"O Romeo, Romeo! wherefore art thou Romeo?\";\n\
\n\
var cipherText = AESEncryptCtr(plainText, password, 256);\n\
var decryptedText = AESDecryptCtr(cipherText, password, 256);\n\
\n\
if (decryptedText != plainText)\n\
    throw \"ERROR: bad result: expected \" + plainText + \" but got \" + decryptedText;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider crypto-md5</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>crypto-md5</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"crypto-md5 failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/*\n\
 * A JavaScript implementation of the RSA Data Security, Inc. MD5 Message\n\
 * Digest Algorithm, as defined in RFC 1321.\n\
 * Version 2.1 Copyright (C) Paul Johnston 1999 - 2002.\n\
 * Other contributors: Greg Holt, Andrew Kepert, Ydnar, Lostinet\n\
 * Distributed under the BSD License\n\
 * See http://pajhome.org.uk/crypt/md5 for more info.\n\
 */\n\
\n\
/*\n\
 * Configurable variables. You may need to tweak these to be compatible with\n\
 * the server-side, but the defaults work in most cases.\n\
 */\n\
var hexcase = 0;  /* hex output format. 0 - lowercase; 1 - uppercase        */\n\
var b64pad  = \"\"; /* base-64 pad character. \"=\" for strict RFC compliance   */\n\
var chrsz   = 8;  /* bits per input character. 8 - ASCII; 16 - Unicode      */\n\
\n\
/*\n\
 * These are the functions you'll usually want to call\n\
 * They take string arguments and return either hex or base-64 encoded strings\n\
 */\n\
function hex_md5(s){ return binl2hex(core_md5(str2binl(s), s.length * chrsz));}\n\
function b64_md5(s){ return binl2b64(core_md5(str2binl(s), s.length * chrsz));}\n\
function str_md5(s){ return binl2str(core_md5(str2binl(s), s.length * chrsz));}\n\
function hex_hmac_md5(key, data) { return binl2hex(core_hmac_md5(key, data)); }\n\
function b64_hmac_md5(key, data) { return binl2b64(core_hmac_md5(key, data)); }\n\
function str_hmac_md5(key, data) { return binl2str(core_hmac_md5(key, data)); }\n\
\n\
/*\n\
 * Perform a simple self-test to see if the VM is working\n\
 */\n\
function md5_vm_test()\n\
{\n\
  return hex_md5(\"abc\") == \"900150983cd24fb0d6963f7d28e17f72\";\n\
}\n\
\n\
/*\n\
 * Calculate the MD5 of an array of little-endian words, and a bit length\n\
 */\n\
function core_md5(x, len)\n\
{\n\
  /* append padding */\n\
  x[len >> 5] |= 0x80 << ((len) % 32);\n\
  x[(((len + 64) >>> 9) << 4) + 14] = len;\n\
\n\
  var a =  1732584193;\n\
  var b = -271733879;\n\
  var c = -1732584194;\n\
  var d =  271733878;\n\
\n\
  for(var i = 0; i < x.length; i += 16)\n\
  {\n\
    var olda = a;\n\
    var oldb = b;\n\
    var oldc = c;\n\
    var oldd = d;\n\
\n\
    a = md5_ff(a, b, c, d, x[i+ 0], 7 , -680876936);\n\
    d = md5_ff(d, a, b, c, x[i+ 1], 12, -389564586);\n\
    c = md5_ff(c, d, a, b, x[i+ 2], 17,  606105819);\n\
    b = md5_ff(b, c, d, a, x[i+ 3], 22, -1044525330);\n\
    a = md5_ff(a, b, c, d, x[i+ 4], 7 , -176418897);\n\
    d = md5_ff(d, a, b, c, x[i+ 5], 12,  1200080426);\n\
    c = md5_ff(c, d, a, b, x[i+ 6], 17, -1473231341);\n\
    b = md5_ff(b, c, d, a, x[i+ 7], 22, -45705983);\n\
    a = md5_ff(a, b, c, d, x[i+ 8], 7 ,  1770035416);\n\
    d = md5_ff(d, a, b, c, x[i+ 9], 12, -1958414417);\n\
    c = md5_ff(c, d, a, b, x[i+10], 17, -42063);\n\
    b = md5_ff(b, c, d, a, x[i+11], 22, -1990404162);\n\
    a = md5_ff(a, b, c, d, x[i+12], 7 ,  1804603682);\n\
    d = md5_ff(d, a, b, c, x[i+13], 12, -40341101);\n\
    c = md5_ff(c, d, a, b, x[i+14], 17, -1502002290);\n\
    b = md5_ff(b, c, d, a, x[i+15], 22,  1236535329);\n\
\n\
    a = md5_gg(a, b, c, d, x[i+ 1], 5 , -165796510);\n\
    d = md5_gg(d, a, b, c, x[i+ 6], 9 , -1069501632);\n\
    c = md5_gg(c, d, a, b, x[i+11], 14,  643717713);\n\
    b = md5_gg(b, c, d, a, x[i+ 0], 20, -373897302);\n\
    a = md5_gg(a, b, c, d, x[i+ 5], 5 , -701558691);\n\
    d = md5_gg(d, a, b, c, x[i+10], 9 ,  38016083);\n\
    c = md5_gg(c, d, a, b, x[i+15], 14, -660478335);\n\
    b = md5_gg(b, c, d, a, x[i+ 4], 20, -405537848);\n\
    a = md5_gg(a, b, c, d, x[i+ 9], 5 ,  568446438);\n\
    d = md5_gg(d, a, b, c, x[i+14], 9 , -1019803690);\n\
    c = md5_gg(c, d, a, b, x[i+ 3], 14, -187363961);\n\
    b = md5_gg(b, c, d, a, x[i+ 8], 20,  1163531501);\n\
    a = md5_gg(a, b, c, d, x[i+13], 5 , -1444681467);\n\
    d = md5_gg(d, a, b, c, x[i+ 2], 9 , -51403784);\n\
    c = md5_gg(c, d, a, b, x[i+ 7], 14,  1735328473);\n\
    b = md5_gg(b, c, d, a, x[i+12], 20, -1926607734);\n\
\n\
    a = md5_hh(a, b, c, d, x[i+ 5], 4 , -378558);\n\
    d = md5_hh(d, a, b, c, x[i+ 8], 11, -2022574463);\n\
    c = md5_hh(c, d, a, b, x[i+11], 16,  1839030562);\n\
    b = md5_hh(b, c, d, a, x[i+14], 23, -35309556);\n\
    a = md5_hh(a, b, c, d, x[i+ 1], 4 , -1530992060);\n\
    d = md5_hh(d, a, b, c, x[i+ 4], 11,  1272893353);\n\
    c = md5_hh(c, d, a, b, x[i+ 7], 16, -155497632);\n\
    b = md5_hh(b, c, d, a, x[i+10], 23, -1094730640);\n\
    a = md5_hh(a, b, c, d, x[i+13], 4 ,  681279174);\n\
    d = md5_hh(d, a, b, c, x[i+ 0], 11, -358537222);\n\
    c = md5_hh(c, d, a, b, x[i+ 3], 16, -722521979);\n\
    b = md5_hh(b, c, d, a, x[i+ 6], 23,  76029189);\n\
    a = md5_hh(a, b, c, d, x[i+ 9], 4 , -640364487);\n\
    d = md5_hh(d, a, b, c, x[i+12], 11, -421815835);\n\
    c = md5_hh(c, d, a, b, x[i+15], 16,  530742520);\n\
    b = md5_hh(b, c, d, a, x[i+ 2], 23, -995338651);\n\
\n\
    a = md5_ii(a, b, c, d, x[i+ 0], 6 , -198630844);\n\
    d = md5_ii(d, a, b, c, x[i+ 7], 10,  1126891415);\n\
    c = md5_ii(c, d, a, b, x[i+14], 15, -1416354905);\n\
    b = md5_ii(b, c, d, a, x[i+ 5], 21, -57434055);\n\
    a = md5_ii(a, b, c, d, x[i+12], 6 ,  1700485571);\n\
    d = md5_ii(d, a, b, c, x[i+ 3], 10, -1894986606);\n\
    c = md5_ii(c, d, a, b, x[i+10], 15, -1051523);\n\
    b = md5_ii(b, c, d, a, x[i+ 1], 21, -2054922799);\n\
    a = md5_ii(a, b, c, d, x[i+ 8], 6 ,  1873313359);\n\
    d = md5_ii(d, a, b, c, x[i+15], 10, -30611744);\n\
    c = md5_ii(c, d, a, b, x[i+ 6], 15, -1560198380);\n\
    b = md5_ii(b, c, d, a, x[i+13], 21,  1309151649);\n\
    a = md5_ii(a, b, c, d, x[i+ 4], 6 , -145523070);\n\
    d = md5_ii(d, a, b, c, x[i+11], 10, -1120210379);\n\
    c = md5_ii(c, d, a, b, x[i+ 2], 15,  718787259);\n\
    b = md5_ii(b, c, d, a, x[i+ 9], 21, -343485551);\n\
\n\
    a = safe_add(a, olda);\n\
    b = safe_add(b, oldb);\n\
    c = safe_add(c, oldc);\n\
    d = safe_add(d, oldd);\n\
  }\n\
  return Array(a, b, c, d);\n\
\n\
}\n\
\n\
/*\n\
 * These functions implement the four basic operations the algorithm uses.\n\
 */\n\
function md5_cmn(q, a, b, x, s, t)\n\
{\n\
  return safe_add(bit_rol(safe_add(safe_add(a, q), safe_add(x, t)), s),b);\n\
}\n\
function md5_ff(a, b, c, d, x, s, t)\n\
{\n\
  return md5_cmn((b & c) | ((~b) & d), a, b, x, s, t);\n\
}\n\
function md5_gg(a, b, c, d, x, s, t)\n\
{\n\
  return md5_cmn((b & d) | (c & (~d)), a, b, x, s, t);\n\
}\n\
function md5_hh(a, b, c, d, x, s, t)\n\
{\n\
  return md5_cmn(b ^ c ^ d, a, b, x, s, t);\n\
}\n\
function md5_ii(a, b, c, d, x, s, t)\n\
{\n\
  return md5_cmn(c ^ (b | (~d)), a, b, x, s, t);\n\
}\n\
\n\
/*\n\
 * Calculate the HMAC-MD5, of a key and some data\n\
 */\n\
function core_hmac_md5(key, data)\n\
{\n\
  var bkey = str2binl(key);\n\
  if(bkey.length > 16) bkey = core_md5(bkey, key.length * chrsz);\n\
\n\
  var ipad = Array(16), opad = Array(16);\n\
  for(var i = 0; i < 16; i++)\n\
  {\n\
    ipad[i] = bkey[i] ^ 0x36363636;\n\
    opad[i] = bkey[i] ^ 0x5C5C5C5C;\n\
  }\n\
\n\
  var hash = core_md5(ipad.concat(str2binl(data)), 512 + data.length * chrsz);\n\
  return core_md5(opad.concat(hash), 512 + 128);\n\
}\n\
\n\
/*\n\
 * Add integers, wrapping at 2^32. This uses 16-bit operations internally\n\
 * to work around bugs in some JS interpreters.\n\
 */\n\
function safe_add(x, y)\n\
{\n\
  var lsw = (x & 0xFFFF) + (y & 0xFFFF);\n\
  var msw = (x >> 16) + (y >> 16) + (lsw >> 16);\n\
  return (msw << 16) | (lsw & 0xFFFF);\n\
}\n\
\n\
/*\n\
 * Bitwise rotate a 32-bit number to the left.\n\
 */\n\
function bit_rol(num, cnt)\n\
{\n\
  return (num << cnt) | (num >>> (32 - cnt));\n\
}\n\
\n\
/*\n\
 * Convert a string to an array of little-endian words\n\
 * If chrsz is ASCII, characters >255 have their hi-byte silently ignored.\n\
 */\n\
function str2binl(str)\n\
{\n\
  var bin = Array();\n\
  var mask = (1 << chrsz) - 1;\n\
  for(var i = 0; i < str.length * chrsz; i += chrsz)\n\
    bin[i>>5] |= (str.charCodeAt(i / chrsz) & mask) << (i%32);\n\
  return bin;\n\
}\n\
\n\
/*\n\
 * Convert an array of little-endian words to a string\n\
 */\n\
function binl2str(bin)\n\
{\n\
  var str = \"\";\n\
  var mask = (1 << chrsz) - 1;\n\
  for(var i = 0; i < bin.length * 32; i += chrsz)\n\
    str += String.fromCharCode((bin[i>>5] >>> (i % 32)) & mask);\n\
  return str;\n\
}\n\
\n\
/*\n\
 * Convert an array of little-endian words to a hex string.\n\
 */\n\
function binl2hex(binarray)\n\
{\n\
  var hex_tab = hexcase ? \"0123456789ABCDEF\" : \"0123456789abcdef\";\n\
  var str = \"\";\n\
  for(var i = 0; i < binarray.length * 4; i++)\n\
  {\n\
    str += hex_tab.charAt((binarray[i>>2] >> ((i%4)*8+4)) & 0xF) +\n\
           hex_tab.charAt((binarray[i>>2] >> ((i%4)*8  )) & 0xF);\n\
  }\n\
  return str;\n\
}\n\
\n\
/*\n\
 * Convert an array of little-endian words to a base-64 string\n\
 */\n\
function binl2b64(binarray)\n\
{\n\
  var tab = \"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\";\n\
  var str = \"\";\n\
  for(var i = 0; i < binarray.length * 4; i += 3)\n\
  {\n\
    var triplet = (((binarray[i   >> 2] >> 8 * ( i   %4)) & 0xFF) << 16)\n\
                | (((binarray[i+1 >> 2] >> 8 * ((i+1)%4)) & 0xFF) << 8 )\n\
                |  ((binarray[i+2 >> 2] >> 8 * ((i+2)%4)) & 0xFF);\n\
    for(var j = 0; j < 4; j++)\n\
    {\n\
      if(i * 8 + j * 6 > binarray.length * 32) str += b64pad;\n\
      else str += tab.charAt((triplet >> 6*(3-j)) & 0x3F);\n\
    }\n\
  }\n\
  return str;\n\
}\n\
\n\
var plainText = \"Rebellious subjects, enemies to peace,\\n\\\n\
Profaners of this neighbour-stained steel,--\\n\\\n\
Will they not hear? What, ho! you men, you beasts,\\n\\\n\
That quench the fire of your pernicious rage\\n\\\n\
With purple fountains issuing from your veins,\\n\\\n\
On pain of torture, from those bloody hands\\n\\\n\
Throw your mistemper'd weapons to the ground,\\n\\\n\
And hear the sentence of your moved prince.\\n\\\n\
Three civil brawls, bred of an airy word,\\n\\\n\
By thee, old Capulet, and Montague,\\n\\\n\
Have thrice disturb'd the quiet of our streets,\\n\\\n\
And made Verona's ancient citizens\\n\\\n\
Cast by their grave beseeming ornaments,\\n\\\n\
To wield old partisans, in hands as old,\\n\\\n\
Canker'd with peace, to part your canker'd hate:\\n\\\n\
If ever you disturb our streets again,\\n\\\n\
Your lives shall pay the forfeit of the peace.\\n\\\n\
For this time, all the rest depart away:\\n\\\n\
You Capulet; shall go along with me:\\n\\\n\
And, Montague, come you this afternoon,\\n\\\n\
To know our further pleasure in this case,\\n\\\n\
To old Free-town, our common judgment-place.\\n\\\n\
Once more, on pain of death, all men depart.\"\n\
\n\
for (var i = 0; i <4; i++) {\n\
    plainText += plainText;\n\
}\n\
\n\
var md5Output = hex_md5(plainText);\n\
\n\
var expected = \"a831e91e0f70eddcb70dc61c6f82f6cd\";\n\
\n\
if (md5Output != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + md5Output;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider crypto-sha1</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>crypto-sha1</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"crypto-sha1 failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/*\n\
 * A JavaScript implementation of the Secure Hash Algorithm, SHA-1, as defined\n\
 * in FIPS PUB 180-1\n\
 * Version 2.1a Copyright Paul Johnston 2000 - 2002.\n\
 * Other contributors: Greg Holt, Andrew Kepert, Ydnar, Lostinet\n\
 * Distributed under the BSD License\n\
 * See http://pajhome.org.uk/crypt/md5 for details.\n\
 */\n\
\n\
/*\n\
 * Configurable variables. You may need to tweak these to be compatible with\n\
 * the server-side, but the defaults work in most cases.\n\
 */\n\
var hexcase = 0;  /* hex output format. 0 - lowercase; 1 - uppercase        */\n\
var b64pad  = \"\"; /* base-64 pad character. \"=\" for strict RFC compliance   */\n\
var chrsz   = 8;  /* bits per input character. 8 - ASCII; 16 - Unicode      */\n\
\n\
/*\n\
 * These are the functions you'll usually want to call\n\
 * They take string arguments and return either hex or base-64 encoded strings\n\
 */\n\
function hex_sha1(s){return binb2hex(core_sha1(str2binb(s),s.length * chrsz));}\n\
function b64_sha1(s){return binb2b64(core_sha1(str2binb(s),s.length * chrsz));}\n\
function str_sha1(s){return binb2str(core_sha1(str2binb(s),s.length * chrsz));}\n\
function hex_hmac_sha1(key, data){ return binb2hex(core_hmac_sha1(key, data));}\n\
function b64_hmac_sha1(key, data){ return binb2b64(core_hmac_sha1(key, data));}\n\
function str_hmac_sha1(key, data){ return binb2str(core_hmac_sha1(key, data));}\n\
\n\
/*\n\
 * Perform a simple self-test to see if the VM is working\n\
 */\n\
function sha1_vm_test()\n\
{\n\
  return hex_sha1(\"abc\") == \"a9993e364706816aba3e25717850c26c9cd0d89d\";\n\
}\n\
\n\
/*\n\
 * Calculate the SHA-1 of an array of big-endian words, and a bit length\n\
 */\n\
function core_sha1(x, len)\n\
{\n\
  /* append padding */\n\
  x[len >> 5] |= 0x80 << (24 - len % 32);\n\
  x[((len + 64 >> 9) << 4) + 15] = len;\n\
\n\
  var w = Array(80);\n\
  var a =  1732584193;\n\
  var b = -271733879;\n\
  var c = -1732584194;\n\
  var d =  271733878;\n\
  var e = -1009589776;\n\
\n\
  for(var i = 0; i < x.length; i += 16)\n\
  {\n\
    var olda = a;\n\
    var oldb = b;\n\
    var oldc = c;\n\
    var oldd = d;\n\
    var olde = e;\n\
\n\
    for(var j = 0; j < 80; j++)\n\
    {\n\
      if(j < 16) w[j] = x[i + j];\n\
      else w[j] = rol(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);\n\
      var t = safe_add(safe_add(rol(a, 5), sha1_ft(j, b, c, d)),\n\
                       safe_add(safe_add(e, w[j]), sha1_kt(j)));\n\
      e = d;\n\
      d = c;\n\
      c = rol(b, 30);\n\
      b = a;\n\
      a = t;\n\
    }\n\
\n\
    a = safe_add(a, olda);\n\
    b = safe_add(b, oldb);\n\
    c = safe_add(c, oldc);\n\
    d = safe_add(d, oldd);\n\
    e = safe_add(e, olde);\n\
  }\n\
  return Array(a, b, c, d, e);\n\
\n\
}\n\
\n\
/*\n\
 * Perform the appropriate triplet combination function for the current\n\
 * iteration\n\
 */\n\
function sha1_ft(t, b, c, d)\n\
{\n\
  if(t < 20) return (b & c) | ((~b) & d);\n\
  if(t < 40) return b ^ c ^ d;\n\
  if(t < 60) return (b & c) | (b & d) | (c & d);\n\
  return b ^ c ^ d;\n\
}\n\
\n\
/*\n\
 * Determine the appropriate additive constant for the current iteration\n\
 */\n\
function sha1_kt(t)\n\
{\n\
  return (t < 20) ?  1518500249 : (t < 40) ?  1859775393 :\n\
         (t < 60) ? -1894007588 : -899497514;\n\
}\n\
\n\
/*\n\
 * Calculate the HMAC-SHA1 of a key and some data\n\
 */\n\
function core_hmac_sha1(key, data)\n\
{\n\
  var bkey = str2binb(key);\n\
  if(bkey.length > 16) bkey = core_sha1(bkey, key.length * chrsz);\n\
\n\
  var ipad = Array(16), opad = Array(16);\n\
  for(var i = 0; i < 16; i++)\n\
  {\n\
    ipad[i] = bkey[i] ^ 0x36363636;\n\
    opad[i] = bkey[i] ^ 0x5C5C5C5C;\n\
  }\n\
\n\
  var hash = core_sha1(ipad.concat(str2binb(data)), 512 + data.length * chrsz);\n\
  return core_sha1(opad.concat(hash), 512 + 160);\n\
}\n\
\n\
/*\n\
 * Add integers, wrapping at 2^32. This uses 16-bit operations internally\n\
 * to work around bugs in some JS interpreters.\n\
 */\n\
function safe_add(x, y)\n\
{\n\
  var lsw = (x & 0xFFFF) + (y & 0xFFFF);\n\
  var msw = (x >> 16) + (y >> 16) + (lsw >> 16);\n\
  return (msw << 16) | (lsw & 0xFFFF);\n\
}\n\
\n\
/*\n\
 * Bitwise rotate a 32-bit number to the left.\n\
 */\n\
function rol(num, cnt)\n\
{\n\
  return (num << cnt) | (num >>> (32 - cnt));\n\
}\n\
\n\
/*\n\
 * Convert an 8-bit or 16-bit string to an array of big-endian words\n\
 * In 8-bit function, characters >255 have their hi-byte silently ignored.\n\
 */\n\
function str2binb(str)\n\
{\n\
  var bin = Array();\n\
  var mask = (1 << chrsz) - 1;\n\
  for(var i = 0; i < str.length * chrsz; i += chrsz)\n\
    bin[i>>5] |= (str.charCodeAt(i / chrsz) & mask) << (32 - chrsz - i%32);\n\
  return bin;\n\
}\n\
\n\
/*\n\
 * Convert an array of big-endian words to a string\n\
 */\n\
function binb2str(bin)\n\
{\n\
  var str = \"\";\n\
  var mask = (1 << chrsz) - 1;\n\
  for(var i = 0; i < bin.length * 32; i += chrsz)\n\
    str += String.fromCharCode((bin[i>>5] >>> (32 - chrsz - i%32)) & mask);\n\
  return str;\n\
}\n\
\n\
/*\n\
 * Convert an array of big-endian words to a hex string.\n\
 */\n\
function binb2hex(binarray)\n\
{\n\
  var hex_tab = hexcase ? \"0123456789ABCDEF\" : \"0123456789abcdef\";\n\
  var str = \"\";\n\
  for(var i = 0; i < binarray.length * 4; i++)\n\
  {\n\
    str += hex_tab.charAt((binarray[i>>2] >> ((3 - i%4)*8+4)) & 0xF) +\n\
           hex_tab.charAt((binarray[i>>2] >> ((3 - i%4)*8  )) & 0xF);\n\
  }\n\
  return str;\n\
}\n\
\n\
/*\n\
 * Convert an array of big-endian words to a base-64 string\n\
 */\n\
function binb2b64(binarray)\n\
{\n\
  var tab = \"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\";\n\
  var str = \"\";\n\
  for(var i = 0; i < binarray.length * 4; i += 3)\n\
  {\n\
    var triplet = (((binarray[i   >> 2] >> 8 * (3 -  i   %4)) & 0xFF) << 16)\n\
                | (((binarray[i+1 >> 2] >> 8 * (3 - (i+1)%4)) & 0xFF) << 8 )\n\
                |  ((binarray[i+2 >> 2] >> 8 * (3 - (i+2)%4)) & 0xFF);\n\
    for(var j = 0; j < 4; j++)\n\
    {\n\
      if(i * 8 + j * 6 > binarray.length * 32) str += b64pad;\n\
      else str += tab.charAt((triplet >> 6*(3-j)) & 0x3F);\n\
    }\n\
  }\n\
  return str;\n\
}\n\
\n\
\n\
var plainText = \"Two households, both alike in dignity,\\n\\\n\
In fair Verona, where we lay our scene,\\n\\\n\
From ancient grudge break to new mutiny,\\n\\\n\
Where civil blood makes civil hands unclean.\\n\\\n\
From forth the fatal loins of these two foes\\n\\\n\
A pair of star-cross'd lovers take their life;\\n\\\n\
Whole misadventured piteous overthrows\\n\\\n\
Do with their death bury their parents' strife.\\n\\\n\
The fearful passage of their death-mark'd love,\\n\\\n\
And the continuance of their parents' rage,\\n\\\n\
Which, but their children's end, nought could remove,\\n\\\n\
Is now the two hours' traffic of our stage;\\n\\\n\
The which if you with patient ears attend,\\n\\\n\
What here shall miss, our toil shall strive to mend.\";\n\
\n\
for (var i = 0; i <4; i++) {\n\
    plainText += plainText;\n\
}\n\
\n\
var sha1Output = hex_sha1(plainText);\n\
\n\
var expected = \"2524d264def74cce2498bf112bedf00e6c0b796d\";\n\
if (sha1Output != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + sha1Output;\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider date-format-tofte</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>date-format-tofte</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"date-format-tofte failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
function arrayExists(array, x) {\n\
    for (var i = 0; i < array.length; i++) {\n\
        if (array[i] == x) return true;\n\
    }\n\
    return false;\n\
}\n\
\n\
Date.prototype.formatDate = function (input,time) {\n\
    // formatDate :\n\
    // a PHP date like function, for formatting date strings\n\
    // See: http://www.php.net/date\n\
    //\n\
    // input : format string\n\
    // time : epoch time (seconds, and optional)\n\
    //\n\
    // if time is not passed, formatting is based on \n\
    // the current \"this\" date object's set time.\n\
    //\n\
    // supported:\n\
    // a, A, B, d, D, F, g, G, h, H, i, j, l (lowercase L), L, \n\
    // m, M, n, O, r, s, S, t, U, w, W, y, Y, z\n\
    //\n\
    // unsupported:\n\
    // I (capital i), T, Z    \n\
\n\
    var switches =    [\"a\", \"A\", \"B\", \"d\", \"D\", \"F\", \"g\", \"G\", \"h\", \"H\", \n\
                       \"i\", \"j\", \"l\", \"L\", \"m\", \"M\", \"n\", \"O\", \"r\", \"s\", \n\
                       \"S\", \"t\", \"U\", \"w\", \"W\", \"y\", \"Y\", \"z\"];\n\
    var daysLong =    [\"Sunday\", \"Monday\", \"Tuesday\", \"Wednesday\", \n\
                       \"Thursday\", \"Friday\", \"Saturday\"];\n\
    var daysShort =   [\"Sun\", \"Mon\", \"Tue\", \"Wed\", \n\
                       \"Thu\", \"Fri\", \"Sat\"];\n\
    var monthsShort = [\"Jan\", \"Feb\", \"Mar\", \"Apr\",\n\
                       \"May\", \"Jun\", \"Jul\", \"Aug\", \"Sep\",\n\
                       \"Oct\", \"Nov\", \"Dec\"];\n\
    var monthsLong =  [\"January\", \"February\", \"March\", \"April\",\n\
                       \"May\", \"June\", \"July\", \"August\", \"September\",\n\
                       \"October\", \"November\", \"December\"];\n\
    var daysSuffix = [\"st\", \"nd\", \"rd\", \"th\", \"th\", \"th\", \"th\", // 1st - 7th\n\
                      \"th\", \"th\", \"th\", \"th\", \"th\", \"th\", \"th\", // 8th - 14th\n\
                      \"th\", \"th\", \"th\", \"th\", \"th\", \"th\", \"st\", // 15th - 21st\n\
                      \"nd\", \"rd\", \"th\", \"th\", \"th\", \"th\", \"th\", // 22nd - 28th\n\
                      \"th\", \"th\", \"st\"];                        // 29th - 31st\n\
\n\
    function a() {\n\
        // Lowercase Ante meridiem and Post meridiem\n\
        return self.getHours() > 11? \"pm\" : \"am\";\n\
    }\n\
    function A() {\n\
        // Uppercase Ante meridiem and Post meridiem\n\
        return self.getHours() > 11? \"PM\" : \"AM\";\n\
    }\n\
\n\
    function B(){\n\
        // Swatch internet time. code simply grabbed from ppk,\n\
        // since I was feeling lazy:\n\
        // http://www.xs4all.nl/~ppk/js/beat.html\n\
        var off = (self.getTimezoneOffset() + 60)*60;\n\
        var theSeconds = (self.getHours() * 3600) + \n\
                         (self.getMinutes() * 60) + \n\
                          self.getSeconds() + off;\n\
        var beat = Math.floor(theSeconds/86.4);\n\
        if (beat > 1000) beat -= 1000;\n\
        if (beat < 0) beat += 1000;\n\
        if ((\"\"+beat).length == 1) beat = \"00\"+beat;\n\
        if ((\"\"+beat).length == 2) beat = \"0\"+beat;\n\
        return beat;\n\
    }\n\
    \n\
    function d() {\n\
        // Day of the month, 2 digits with leading zeros\n\
        return new String(self.getDate()).length == 1?\n\
        \"0\"+self.getDate() : self.getDate();\n\
    }\n\
    function D() {\n\
        // A textual representation of a day, three letters\n\
        return daysShort[self.getDay()];\n\
    }\n\
    function F() {\n\
        // A full textual representation of a month\n\
        return monthsLong[self.getMonth()];\n\
    }\n\
    function g() {\n\
        // 12-hour format of an hour without leading zeros\n\
        return self.getHours() > 12? self.getHours()-12 : self.getHours();\n\
    }\n\
    function G() {\n\
        // 24-hour format of an hour without leading zeros\n\
        return self.getHours();\n\
    }\n\
    function h() {\n\
        // 12-hour format of an hour with leading zeros\n\
        if (self.getHours() > 12) {\n\
          var s = new String(self.getHours()-12);\n\
          return s.length == 1?\n\
          \"0\"+ (self.getHours()-12) : self.getHours()-12;\n\
        } else { \n\
          var s = new String(self.getHours());\n\
          return s.length == 1?\n\
          \"0\"+self.getHours() : self.getHours();\n\
        }  \n\
    }\n\
    function H() {\n\
        // 24-hour format of an hour with leading zeros\n\
        return new String(self.getHours()).length == 1?\n\
        \"0\"+self.getHours() : self.getHours();\n\
    }\n\
    function i() {\n\
        // Minutes with leading zeros\n\
        return new String(self.getMinutes()).length == 1? \n\
        \"0\"+self.getMinutes() : self.getMinutes(); \n\
    }\n\
    function j() {\n\
        // Day of the month without leading zeros\n\
        return self.getDate();\n\
    }    \n\
    function l() {\n\
        // A full textual representation of the day of the week\n\
        return daysLong[self.getDay()];\n\
    }\n\
    function L() {\n\
        // leap year or not. 1 if leap year, 0 if not.\n\
        // the logic should match iso's 8601 standard.\n\
        var y_ = Y();\n\
        if (         \n\
            (y_ % 4 == 0 && y_ % 100 != 0) ||\n\
            (y_ % 4 == 0 && y_ % 100 == 0 && y_ % 400 == 0)\n\
            ) {\n\
            return 1;\n\
        } else {\n\
            return 0;\n\
        }\n\
    }\n\
    function m() {\n\
        // Numeric representation of a month, with leading zeros\n\
        return self.getMonth() < 9?\n\
        \"0\"+(self.getMonth()+1) : \n\
        self.getMonth()+1;\n\
    }\n\
    function M() {\n\
        // A short textual representation of a month, three letters\n\
        return monthsShort[self.getMonth()];\n\
    }\n\
    function n() {\n\
        // Numeric representation of a month, without leading zeros\n\
        return self.getMonth()+1;\n\
    }\n\
    function O() {\n\
        // Difference to Greenwich time (GMT) in hours\n\
        var os = Math.abs(self.getTimezoneOffset());\n\
        var h = \"\"+Math.floor(os/60);\n\
        var m = \"\"+(os%60);\n\
        h.length == 1? h = \"0\"+h:1;\n\
        m.length == 1? m = \"0\"+m:1;\n\
        return self.getTimezoneOffset() < 0 ? \"+\"+h+m : \"-\"+h+m;\n\
    }\n\
    function r() {\n\
        // RFC 822 formatted date\n\
        var r; // result\n\
        //  Thu    ,     21          Dec         2000\n\
        r = D() + \", \" + j() + \" \" + M() + \" \" + Y() +\n\
        //        16     :    01     :    07          +0200\n\
            \" \" + H() + \":\" + i() + \":\" + s() + \" \" + O();\n\
        return r;\n\
    }\n\
    function S() {\n\
        // English ordinal suffix for the day of the month, 2 characters\n\
        return daysSuffix[self.getDate()-1];\n\
    }\n\
    function s() {\n\
        // Seconds, with leading zeros\n\
        return new String(self.getSeconds()).length == 1?\n\
        \"0\"+self.getSeconds() : self.getSeconds();\n\
    }\n\
    function t() {\n\
\n\
        // thanks to Matt Bannon for some much needed code-fixes here!\n\
        var daysinmonths = [null,31,28,31,30,31,30,31,31,30,31,30,31];\n\
        if (L()==1 && n()==2) return 29; // leap day\n\
        return daysinmonths[n()];\n\
    }\n\
    function U() {\n\
        // Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)\n\
        return Math.round(self.getTime()/1000);\n\
    }\n\
    function W() {\n\
        // Weeknumber, as per ISO specification:\n\
        // http://www.cl.cam.ac.uk/~mgk25/iso-time.html\n\
        \n\
        // if the day is three days before newyears eve,\n\
        // there's a chance it's \"week 1\" of next year.\n\
        // here we check for that.\n\
        var beforeNY = 364+L() - z();\n\
        var afterNY  = z();\n\
        var weekday = w()!=0?w()-1:6; // makes sunday (0), into 6.\n\
        if (beforeNY <= 2 && weekday <= 2-beforeNY) {\n\
            return 1;\n\
        }\n\
        // similarly, if the day is within threedays of newyears\n\
        // there's a chance it belongs in the old year.\n\
        var ny = new Date(\"January 1 \" + Y() + \" 00:00:00\");\n\
        var nyDay = ny.getDay()!=0?ny.getDay()-1:6;\n\
        if (\n\
            (afterNY <= 2) && \n\
            (nyDay >=4)  && \n\
            (afterNY >= (6-nyDay))\n\
            ) {\n\
            // Since I'm not sure we can just always return 53,\n\
            // i call the function here again, using the last day\n\
            // of the previous year, as the date, and then just\n\
            // return that week.\n\
            var prevNY = new Date(\"December 31 \" + (Y()-1) + \" 00:00:00\");\n\
            return prevNY.formatDate(\"W\");\n\
        }\n\
        \n\
        // week 1, is the week that has the first thursday in it.\n\
        // note that this value is not zero index.\n\
        if (nyDay <= 3) {\n\
            // first day of the year fell on a thursday, or earlier.\n\
            return 1 + Math.floor( ( z() + nyDay ) / 7 );\n\
        } else {\n\
            // first day of the year fell on a friday, or later.\n\
            return 1 + Math.floor( ( z() - ( 7 - nyDay ) ) / 7 );\n\
        }\n\
    }\n\
    function w() {\n\
        // Numeric representation of the day of the week\n\
        return self.getDay();\n\
    }\n\
    \n\
    function Y() {\n\
        // A full numeric representation of a year, 4 digits\n\
\n\
        // we first check, if getFullYear is supported. if it\n\
        // is, we just use that. ppks code is nice, but wont\n\
        // work with dates outside 1900-2038, or something like that\n\
        if (self.getFullYear) {\n\
            var newDate = new Date(\"January 1 2001 00:00:00 +0000\");\n\
            var x = newDate .getFullYear();\n\
            if (x == 2001) {              \n\
                // i trust the method now\n\
                return self.getFullYear();\n\
            }\n\
        }\n\
        // else, do this:\n\
        // codes thanks to ppk:\n\
        // http://www.xs4all.nl/~ppk/js/introdate.html\n\
        var x = self.getYear();\n\
        var y = x % 100;\n\
        y += (y < 38) ? 2000 : 1900;\n\
        return y;\n\
    }\n\
    function y() {\n\
        // A two-digit representation of a year\n\
        var y = Y()+\"\";\n\
        return y.substring(y.length-2,y.length);\n\
    }\n\
    function z() {\n\
        // The day of the year, zero indexed! 0 through 366\n\
        var t = new Date(\"January 1 \" + Y() + \" 00:00:00\");\n\
        var diff = self.getTime() - t.getTime();\n\
        return Math.floor(diff/1000/60/60/24);\n\
    }\n\
        \n\
    var self = this;\n\
    if (time) {\n\
        // save time\n\
        var prevTime = self.getTime();\n\
        self.setTime(time);\n\
    }\n\
    \n\
    var ia = input.split(\"\");\n\
    var ij = 0;\n\
    while (ia[ij]) {\n\
        if (ia[ij] == \"\\\\\") {\n\
            // this is our way of allowing users to escape stuff\n\
            ia.splice(ij,1);\n\
        } else {\n\
            if (arrayExists(switches,ia[ij])) {\n\
                ia[ij] = eval(ia[ij] + \"()\");\n\
            }\n\
        }\n\
        ij++;\n\
    }\n\
    // reset time, back to what it was\n\
    if (prevTime) {\n\
        self.setTime(prevTime);\n\
    }\n\
    return ia.join(\"\");\n\
}\n\
\n\
var date = new Date(\"1/1/2007 1:11:11\");\n\
\n\
for (i = 0; i < 500; ++i) {\n\
    var shortFormat = date.formatDate(\"Y-m-d\");\n\
    var longFormat = date.formatDate(\"l, F d, Y g:i:s A\");\n\
    date.setTime(date.getTime() + 84266956);\n\
}\n\
\n\
// FIXME: Find a way to validate this test.\n\
// https://bugs.webkit.org/show_bug.cgi?id=114849\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider date-format-xparb</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>date-format-xparb</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"date-format-xparb failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/*\n\
 * Copyright (C) 2004 Baron Schwartz <baron at sequent dot org>\n\
 *\n\
 * This program is free software; you can redistribute it and/or modify it\n\
 * under the terms of the GNU Lesser General Public License as published by the\n\
 * Free Software Foundation, version 2.1.\n\
 *\n\
 * This program is distributed in the hope that it will be useful, but WITHOUT\n\
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS\n\
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more\n\
 * details.\n\
 */\n\
\n\
Date.parseFunctions = {count:0};\n\
Date.parseRegexes = [];\n\
Date.formatFunctions = {count:0};\n\
\n\
Date.prototype.dateFormat = function(format) {\n\
    if (Date.formatFunctions[format] == null) {\n\
        Date.createNewFormat(format);\n\
    }\n\
    var func = Date.formatFunctions[format];\n\
    return this[func]();\n\
}\n\
\n\
Date.createNewFormat = function(format) {\n\
    var funcName = \"format\" + Date.formatFunctions.count++;\n\
    Date.formatFunctions[format] = funcName;\n\
    var code = \"Date.prototype.\" + funcName + \" = function(){return \";\n\
    var special = false;\n\
    var ch = '';\n\
    for (var i = 0; i < format.length; ++i) {\n\
        ch = format.charAt(i);\n\
        if (!special && ch == \"\\\\\") {\n\
            special = true;\n\
        }\n\
        else if (special) {\n\
            special = false;\n\
            code += \"'\" + String.escape(ch) + \"' + \";\n\
        }\n\
        else {\n\
            code += Date.getFormatCode(ch);\n\
        }\n\
    }\n\
    eval(code.substring(0, code.length - 3) + \";}\");\n\
}\n\
\n\
Date.getFormatCode = function(character) {\n\
    switch (character) {\n\
    case \"d\":\n\
        return \"String.leftPad(this.getDate(), 2, '0') + \";\n\
    case \"D\":\n\
        return \"Date.dayNames[this.getDay()].substring(0, 3) + \";\n\
    case \"j\":\n\
        return \"this.getDate() + \";\n\
    case \"l\":\n\
        return \"Date.dayNames[this.getDay()] + \";\n\
    case \"S\":\n\
        return \"this.getSuffix() + \";\n\
    case \"w\":\n\
        return \"this.getDay() + \";\n\
    case \"z\":\n\
        return \"this.getDayOfYear() + \";\n\
    case \"W\":\n\
        return \"this.getWeekOfYear() + \";\n\
    case \"F\":\n\
        return \"Date.monthNames[this.getMonth()] + \";\n\
    case \"m\":\n\
        return \"String.leftPad(this.getMonth() + 1, 2, '0') + \";\n\
    case \"M\":\n\
        return \"Date.monthNames[this.getMonth()].substring(0, 3) + \";\n\
    case \"n\":\n\
        return \"(this.getMonth() + 1) + \";\n\
    case \"t\":\n\
        return \"this.getDaysInMonth() + \";\n\
    case \"L\":\n\
        return \"(this.isLeapYear() ? 1 : 0) + \";\n\
    case \"Y\":\n\
        return \"this.getFullYear() + \";\n\
    case \"y\":\n\
        return \"('' + this.getFullYear()).substring(2, 4) + \";\n\
    case \"a\":\n\
        return \"(this.getHours() < 12 ? 'am' : 'pm') + \";\n\
    case \"A\":\n\
        return \"(this.getHours() < 12 ? 'AM' : 'PM') + \";\n\
    case \"g\":\n\
        return \"((this.getHours() %12) ? this.getHours() % 12 : 12) + \";\n\
    case \"G\":\n\
        return \"this.getHours() + \";\n\
    case \"h\":\n\
        return \"String.leftPad((this.getHours() %12) ? this.getHours() % 12 : 12, 2, '0') + \";\n\
    case \"H\":\n\
        return \"String.leftPad(this.getHours(), 2, '0') + \";\n\
    case \"i\":\n\
        return \"String.leftPad(this.getMinutes(), 2, '0') + \";\n\
    case \"s\":\n\
        return \"String.leftPad(this.getSeconds(), 2, '0') + \";\n\
    case \"O\":\n\
        return \"this.getGMTOffset() + \";\n\
    case \"T\":\n\
        return \"this.getTimezone() + \";\n\
    case \"Z\":\n\
        return \"(this.getTimezoneOffset() * -60) + \";\n\
    default:\n\
        return \"'\" + String.escape(character) + \"' + \";\n\
    }\n\
}\n\
\n\
Date.parseDate = function(input, format) {\n\
    if (Date.parseFunctions[format] == null) {\n\
        Date.createParser(format);\n\
    }\n\
    var func = Date.parseFunctions[format];\n\
    return Date[func](input);\n\
}\n\
\n\
Date.createParser = function(format) {\n\
    var funcName = \"parse\" + Date.parseFunctions.count++;\n\
    var regexNum = Date.parseRegexes.length;\n\
    var currentGroup = 1;\n\
    Date.parseFunctions[format] = funcName;\n\
\n\
    var code = \"Date.\" + funcName + \" = function(input){\\n\"\n\
        + \"var y = -1, m = -1, d = -1, h = -1, i = -1, s = -1;\\n\"\n\
        + \"var d = new Date();\\n\"\n\
        + \"y = d.getFullYear();\\n\"\n\
        + \"m = d.getMonth();\\n\"\n\
        + \"d = d.getDate();\\n\"\n\
        + \"var results = input.match(Date.parseRegexes[\" + regexNum + \"]);\\n\"\n\
        + \"if (results && results.length > 0) {\"\n\
    var regex = \"\";\n\
\n\
    var special = false;\n\
    var ch = '';\n\
    for (var i = 0; i < format.length; ++i) {\n\
        ch = format.charAt(i);\n\
        if (!special && ch == \"\\\\\") {\n\
            special = true;\n\
        }\n\
        else if (special) {\n\
            special = false;\n\
            regex += String.escape(ch);\n\
        }\n\
        else {\n\
            obj = Date.formatCodeToRegex(ch, currentGroup);\n\
            currentGroup += obj.g;\n\
            regex += obj.s;\n\
            if (obj.g && obj.c) {\n\
                code += obj.c;\n\
            }\n\
        }\n\
    }\n\
\n\
    code += \"if (y > 0 && m >= 0 && d > 0 && h >= 0 && i >= 0 && s >= 0)\\n\"\n\
        + \"{return new Date(y, m, d, h, i, s);}\\n\"\n\
        + \"else if (y > 0 && m >= 0 && d > 0 && h >= 0 && i >= 0)\\n\"\n\
        + \"{return new Date(y, m, d, h, i);}\\n\"\n\
        + \"else if (y > 0 && m >= 0 && d > 0 && h >= 0)\\n\"\n\
        + \"{return new Date(y, m, d, h);}\\n\"\n\
        + \"else if (y > 0 && m >= 0 && d > 0)\\n\"\n\
        + \"{return new Date(y, m, d);}\\n\"\n\
        + \"else if (y > 0 && m >= 0)\\n\"\n\
        + \"{return new Date(y, m);}\\n\"\n\
        + \"else if (y > 0)\\n\"\n\
        + \"{return new Date(y);}\\n\"\n\
        + \"}return null;}\";\n\
\n\
    Date.parseRegexes[regexNum] = new RegExp(\"^\" + regex + \"$\");\n\
    eval(code);\n\
}\n\
\n\
Date.formatCodeToRegex = function(character, currentGroup) {\n\
    switch (character) {\n\
    case \"D\":\n\
        return {g:0,\n\
        c:null,\n\
        s:\"(?:Sun|Mon|Tue|Wed|Thu|Fri|Sat)\"};\n\
    case \"j\":\n\
    case \"d\":\n\
        return {g:1,\n\
            c:\"d = parseInt(results[\" + currentGroup + \"], 10);\\n\",\n\
            s:\"(\\\\d{1,2})\"};\n\
    case \"l\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"(?:\" + Date.dayNames.join(\"|\") + \")\"};\n\
    case \"S\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"(?:st|nd|rd|th)\"};\n\
    case \"w\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"\\\\d\"};\n\
    case \"z\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"(?:\\\\d{1,3})\"};\n\
    case \"W\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"(?:\\\\d{2})\"};\n\
    case \"F\":\n\
        return {g:1,\n\
            c:\"m = parseInt(Date.monthNumbers[results[\" + currentGroup + \"].substring(0, 3)], 10);\\n\",\n\
            s:\"(\" + Date.monthNames.join(\"|\") + \")\"};\n\
    case \"M\":\n\
        return {g:1,\n\
            c:\"m = parseInt(Date.monthNumbers[results[\" + currentGroup + \"]], 10);\\n\",\n\
            s:\"(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\"};\n\
    case \"n\":\n\
    case \"m\":\n\
        return {g:1,\n\
            c:\"m = parseInt(results[\" + currentGroup + \"], 10) - 1;\\n\",\n\
            s:\"(\\\\d{1,2})\"};\n\
    case \"t\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"\\\\d{1,2}\"};\n\
    case \"L\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"(?:1|0)\"};\n\
    case \"Y\":\n\
        return {g:1,\n\
            c:\"y = parseInt(results[\" + currentGroup + \"], 10);\\n\",\n\
            s:\"(\\\\d{4})\"};\n\
    case \"y\":\n\
        return {g:1,\n\
            c:\"var ty = parseInt(results[\" + currentGroup + \"], 10);\\n\"\n\
                + \"y = ty > Date.y2kYear ? 1900 + ty : 2000 + ty;\\n\",\n\
            s:\"(\\\\d{1,2})\"};\n\
    case \"a\":\n\
        return {g:1,\n\
            c:\"if (results[\" + currentGroup + \"] == 'am') {\\n\"\n\
                + \"if (h == 12) { h = 0; }\\n\"\n\
                + \"} else { if (h < 12) { h += 12; }}\",\n\
            s:\"(am|pm)\"};\n\
    case \"A\":\n\
        return {g:1,\n\
            c:\"if (results[\" + currentGroup + \"] == 'AM') {\\n\"\n\
                + \"if (h == 12) { h = 0; }\\n\"\n\
                + \"} else { if (h < 12) { h += 12; }}\",\n\
            s:\"(AM|PM)\"};\n\
    case \"g\":\n\
    case \"G\":\n\
    case \"h\":\n\
    case \"H\":\n\
        return {g:1,\n\
            c:\"h = parseInt(results[\" + currentGroup + \"], 10);\\n\",\n\
            s:\"(\\\\d{1,2})\"};\n\
    case \"i\":\n\
        return {g:1,\n\
            c:\"i = parseInt(results[\" + currentGroup + \"], 10);\\n\",\n\
            s:\"(\\\\d{2})\"};\n\
    case \"s\":\n\
        return {g:1,\n\
            c:\"s = parseInt(results[\" + currentGroup + \"], 10);\\n\",\n\
            s:\"(\\\\d{2})\"};\n\
    case \"O\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"[+-]\\\\d{4}\"};\n\
    case \"T\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"[A-Z]{3}\"};\n\
    case \"Z\":\n\
        return {g:0,\n\
            c:null,\n\
            s:\"[+-]\\\\d{1,5}\"};\n\
    default:\n\
        return {g:0,\n\
            c:null,\n\
            s:String.escape(character)};\n\
    }\n\
}\n\
\n\
Date.prototype.getTimezone = function() {\n\
    return this.toString().replace(\n\
        /^.*? ([A-Z]{3}) [0-9]{4}.*$/, \"$1\").replace(\n\
        /^.*?\\(([A-Z])[a-z]+ ([A-Z])[a-z]+ ([A-Z])[a-z]+\\)$/, \"$1$2$3\");\n\
}\n\
\n\
Date.prototype.getGMTOffset = function() {\n\
    return (this.getTimezoneOffset() > 0 ? \"-\" : \"+\")\n\
        + String.leftPad(Math.floor(this.getTimezoneOffset() / 60), 2, \"0\")\n\
        + String.leftPad(this.getTimezoneOffset() % 60, 2, \"0\");\n\
}\n\
\n\
Date.prototype.getDayOfYear = function() {\n\
    var num = 0;\n\
    Date.daysInMonth[1] = this.isLeapYear() ? 29 : 28;\n\
    for (var i = 0; i < this.getMonth(); ++i) {\n\
        num += Date.daysInMonth[i];\n\
    }\n\
    return num + this.getDate() - 1;\n\
}\n\
\n\
Date.prototype.getWeekOfYear = function() {\n\
    // Skip to Thursday of this week\n\
    var now = this.getDayOfYear() + (4 - this.getDay());\n\
    // Find the first Thursday of the year\n\
    var jan1 = new Date(this.getFullYear(), 0, 1);\n\
    var then = (7 - jan1.getDay() + 4);\n\
    document.write(then);\n\
    return String.leftPad(((now - then) / 7) + 1, 2, \"0\");\n\
}\n\
\n\
Date.prototype.isLeapYear = function() {\n\
    var year = this.getFullYear();\n\
    return ((year & 3) == 0 && (year % 100 || (year % 400 == 0 && year)));\n\
}\n\
\n\
Date.prototype.getFirstDayOfMonth = function() {\n\
    var day = (this.getDay() - (this.getDate() - 1)) % 7;\n\
    return (day < 0) ? (day + 7) : day;\n\
}\n\
\n\
Date.prototype.getLastDayOfMonth = function() {\n\
    var day = (this.getDay() + (Date.daysInMonth[this.getMonth()] - this.getDate())) % 7;\n\
    return (day < 0) ? (day + 7) : day;\n\
}\n\
\n\
Date.prototype.getDaysInMonth = function() {\n\
    Date.daysInMonth[1] = this.isLeapYear() ? 29 : 28;\n\
    return Date.daysInMonth[this.getMonth()];\n\
}\n\
\n\
Date.prototype.getSuffix = function() {\n\
    switch (this.getDate()) {\n\
        case 1:\n\
        case 21:\n\
        case 31:\n\
            return \"st\";\n\
        case 2:\n\
        case 22:\n\
            return \"nd\";\n\
        case 3:\n\
        case 23:\n\
            return \"rd\";\n\
        default:\n\
            return \"th\";\n\
    }\n\
}\n\
\n\
String.escape = function(string) {\n\
    return string.replace(/('|\\\\)/g, \"\\\\$1\");\n\
}\n\
\n\
String.leftPad = function (val, size, ch) {\n\
    var result = new String(val);\n\
    if (ch == null) {\n\
        ch = \" \";\n\
    }\n\
    while (result.length < size) {\n\
        result = ch + result;\n\
    }\n\
    return result;\n\
}\n\
\n\
Date.daysInMonth = [31,28,31,30,31,30,31,31,30,31,30,31];\n\
Date.monthNames =\n\
   [\"January\",\n\
    \"February\",\n\
    \"March\",\n\
    \"April\",\n\
    \"May\",\n\
    \"June\",\n\
    \"July\",\n\
    \"August\",\n\
    \"September\",\n\
    \"October\",\n\
    \"November\",\n\
    \"December\"];\n\
Date.dayNames =\n\
   [\"Sunday\",\n\
    \"Monday\",\n\
    \"Tuesday\",\n\
    \"Wednesday\",\n\
    \"Thursday\",\n\
    \"Friday\",\n\
    \"Saturday\"];\n\
Date.y2kYear = 50;\n\
Date.monthNumbers = {\n\
    Jan:0,\n\
    Feb:1,\n\
    Mar:2,\n\
    Apr:3,\n\
    May:4,\n\
    Jun:5,\n\
    Jul:6,\n\
    Aug:7,\n\
    Sep:8,\n\
    Oct:9,\n\
    Nov:10,\n\
    Dec:11};\n\
Date.patterns = {\n\
    ISO8601LongPattern:\"Y-m-d H:i:s\",\n\
    ISO8601ShortPattern:\"Y-m-d\",\n\
    ShortDatePattern: \"n/j/Y\",\n\
    LongDatePattern: \"l, F d, Y\",\n\
    FullDateTimePattern: \"l, F d, Y g:i:s A\",\n\
    MonthDayPattern: \"F d\",\n\
    ShortTimePattern: \"g:i A\",\n\
    LongTimePattern: \"g:i:s A\",\n\
    SortableDateTimePattern: \"Y-m-d\\\\TH:i:s\",\n\
    UniversalSortableDateTimePattern: \"Y-m-d H:i:sO\",\n\
    YearMonthPattern: \"F, Y\"};\n\
\n\
var date = new Date(\"1/1/2007 1:11:11\");\n\
\n\
for (i = 0; i < 4000; ++i) {\n\
    var shortFormat = date.dateFormat(\"Y-m-d\");\n\
    var longFormat = date.dateFormat(\"l, F d, Y g:i:s A\");\n\
    date.setTime(date.getTime() + 84266956);\n\
}\n\
\n\
// FIXME: Find a way to validate this test.\n\
// https://bugs.webkit.org/show_bug.cgi?id=114849\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider math-cordic</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>math-cordic</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"math-cordic failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/*\n\
 * Copyright (C) Rich Moore.  All rights reserved.\n\
 *\n\
 * Redistribution and use in source and binary forms, with or without\n\
 * modification, are permitted provided that the following conditions\n\
 * are met:\n\
 * 1. Redistributions of source code must retain the above copyright\n\
 *    notice, this list of conditions and the following disclaimer.\n\
 * 2. Redistributions in binary form must reproduce the above copyright\n\
 *    notice, this list of conditions and the following disclaimer in the\n\
 *    documentation and/or other materials provided with the distribution.\n\
 *\n\
 * THIS SOFTWARE IS PROVIDED BY CONTRIBUTORS ``AS IS'' AND ANY\n\
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
 */\n\
\n\
/////. Start CORDIC\n\
\n\
var AG_CONST = 0.6072529350;\n\
\n\
function FIXED(X)\n\
{\n\
  return X * 65536.0;\n\
}\n\
\n\
function FLOAT(X)\n\
{\n\
  return X / 65536.0;\n\
}\n\
\n\
function DEG2RAD(X)\n\
{\n\
  return 0.017453 * (X);\n\
}\n\
\n\
var Angles = [\n\
  FIXED(45.0), FIXED(26.565), FIXED(14.0362), FIXED(7.12502),\n\
  FIXED(3.57633), FIXED(1.78991), FIXED(0.895174), FIXED(0.447614),\n\
  FIXED(0.223811), FIXED(0.111906), FIXED(0.055953),\n\
  FIXED(0.027977) \n\
              ];\n\
\n\
var Target = 28.027;\n\
\n\
function cordicsincos(Target) {\n\
    var X;\n\
    var Y;\n\
    var TargetAngle;\n\
    var CurrAngle;\n\
    var Step;\n\
 \n\
    X = FIXED(AG_CONST);         /* AG_CONST * cos(0) */\n\
    Y = 0;                       /* AG_CONST * sin(0) */\n\
\n\
    TargetAngle = FIXED(Target);\n\
    CurrAngle = 0;\n\
    for (Step = 0; Step < 12; Step++) {\n\
        var NewX;\n\
        if (TargetAngle > CurrAngle) {\n\
            NewX = X - (Y >> Step);\n\
            Y = (X >> Step) + Y;\n\
            X = NewX;\n\
            CurrAngle += Angles[Step];\n\
        } else {\n\
            NewX = X + (Y >> Step);\n\
            Y = -(X >> Step) + Y;\n\
            X = NewX;\n\
            CurrAngle -= Angles[Step];\n\
        }\n\
    }\n\
\n\
    return FLOAT(X) * FLOAT(Y);\n\
}\n\
\n\
///// End CORDIC\n\
\n\
var total = 0;\n\
\n\
function cordic( runs ) {\n\
  var start = new Date();\n\
\n\
  for ( var i = 0 ; i < runs ; i++ ) {\n\
      total += cordicsincos(Target);\n\
  }\n\
\n\
  var end = new Date();\n\
\n\
  return end.getTime() - start.getTime();\n\
}\n\
\n\
cordic(25000);\n\
\n\
var expected = 10362.570468755888;\n\
\n\
if (total != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + total;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider math-partial-sums</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>math-partial-sums</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"math-partial-sums failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// The Computer Language Shootout\n\
// http://shootout.alioth.debian.org/\n\
// contributed by Isaac Gouy\n\
\n\
function partial(n){\n\
    var a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = a9 = 0.0;\n\
    var twothirds = 2.0/3.0;\n\
    var alt = -1.0;\n\
    var k2 = k3 = sk = ck = 0.0;\n\
    \n\
    for (var k = 1; k <= n; k++){\n\
        k2 = k*k;\n\
        k3 = k2*k;\n\
        sk = Math.sin(k);\n\
        ck = Math.cos(k);\n\
        alt = -alt;\n\
        \n\
        a1 += Math.pow(twothirds,k-1);\n\
        a2 += Math.pow(k,-0.5);\n\
        a3 += 1.0/(k*(k+1.0));\n\
        a4 += 1.0/(k3 * sk*sk);\n\
        a5 += 1.0/(k3 * ck*ck);\n\
        a6 += 1.0/k;\n\
        a7 += 1.0/k2;\n\
        a8 += alt/k;\n\
        a9 += alt/(2*k -1);\n\
    }\n\
    \n\
    // NOTE: We don't try to validate anything from pow(),  sin() or cos() because those aren't\n\
    // well-specified in ECMAScript.\n\
    return a6 + a7 + a8 + a9;\n\
}\n\
\n\
var total = 0;\n\
\n\
for (var i = 1024; i <= 16384; i *= 2) {\n\
    total += partial(i);\n\
}\n\
\n\
var expected = 60.08994194659945;\n\
\n\
if (total != expected) {\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + total;\n\
}\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider math-spectral-norm</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>math-spectral-norm</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"math-spectral-norm failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// The Great Computer Language Shootout\n\
// http://shootout.alioth.debian.org/\n\
//\n\
// contributed by Ian Osgood\n\
\n\
function A(i,j) {\n\
  return 1/((i+j)*(i+j+1)/2+i+1);\n\
}\n\
\n\
function Au(u,v) {\n\
  for (var i=0; i<u.length; ++i) {\n\
    var t = 0;\n\
    for (var j=0; j<u.length; ++j)\n\
      t += A(i,j) * u[j];\n\
    v[i] = t;\n\
  }\n\
}\n\
\n\
function Atu(u,v) {\n\
  for (var i=0; i<u.length; ++i) {\n\
    var t = 0;\n\
    for (var j=0; j<u.length; ++j)\n\
      t += A(j,i) * u[j];\n\
    v[i] = t;\n\
  }\n\
}\n\
\n\
function AtAu(u,v,w) {\n\
  Au(u,w);\n\
  Atu(w,v);\n\
}\n\
\n\
function spectralnorm(n) {\n\
  var i, u=[], v=[], w=[], vv=0, vBv=0;\n\
  for (i=0; i<n; ++i) {\n\
    u[i] = 1; v[i] = w[i] = 0;\n\
  }\n\
  for (i=0; i<10; ++i) {\n\
    AtAu(u,v,w);\n\
    AtAu(v,u,w);\n\
  }\n\
  for (i=0; i<n; ++i) {\n\
    vBv += u[i]*v[i];\n\
    vv  += v[i]*v[i];\n\
  }\n\
  return Math.sqrt(vBv/vv);\n\
}\n\
\n\
var total = 0;\n\
\n\
for (var i = 6; i <= 48; i *= 2) {\n\
    total += spectralnorm(i);\n\
}\n\
\n\
var expected = 5.086694231303284;\n\
\n\
if (total != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + total;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider regexp-dna</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>regexp-dna</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"regexp-dna failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// The Computer Language Shootout\n\
// http://shootout.alioth.debian.org/\n\
//\n\
// contributed by Jesse Millikan\n\
// Base on the Ruby version by jose fco. gonzalez\n\
\n\
var l;\n\
var dnaInput = \">ONE Homo sapiens alu\\n\\\n\
GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGA\\n\\\n\
TCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACT\\n\\\n\
AAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAG\\n\\\n\
GCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCG\\n\\\n\
CCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGT\\n\\\n\
GGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCA\\n\\\n\
GGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAA\\n\\\n\
TTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAG\\n\\\n\
AATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCA\\n\\\n\
GCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGT\\n\\\n\
AATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACC\\n\\\n\
AGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTG\\n\\\n\
GTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACC\\n\\\n\
CGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAG\\n\\\n\
AGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTT\\n\\\n\
TGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACA\\n\\\n\
TGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCT\\n\\\n\
GTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGG\\n\\\n\
TTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGT\\n\\\n\
CTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGG\\n\\\n\
CGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCG\\n\\\n\
TCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTA\\n\\\n\
CTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCG\\n\\\n\
AGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCG\\n\\\n\
GGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACC\\n\\\n\
TGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAA\\n\\\n\
TACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGA\\n\\\n\
GGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACT\\n\\\n\
GCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTC\\n\\\n\
ACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGT\\n\\\n\
TCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGC\\n\\\n\
CGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCG\\n\\\n\
CTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTG\\n\\\n\
GGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCC\\n\\\n\
CAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCT\\n\\\n\
GGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGC\\n\\\n\
GCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGA\\n\\\n\
GGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGA\\n\\\n\
GACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGA\\n\\\n\
GGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTG\\n\\\n\
AAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAAT\\n\\\n\
CCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCA\\n\\\n\
GTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAA\\n\\\n\
AAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGC\\n\\\n\
GGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCT\\n\\\n\
ACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGG\\n\\\n\
GAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATC\\n\\\n\
GCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGC\\n\\\n\
GGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGG\\n\\\n\
TCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAA\\n\\\n\
AAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAG\\n\\\n\
GAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACT\\n\\\n\
CCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCC\\n\\\n\
TGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAG\\n\\\n\
ACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGC\\n\\\n\
GTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGA\\n\\\n\
ACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGA\\n\\\n\
CAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCA\\n\\\n\
CTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCA\\n\\\n\
ACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCG\\n\\\n\
CCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG\\n\\\n\
AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTC\\n\\\n\
CGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCG\\n\\\n\
AGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACC\\n\\\n\
CCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAG\\n\\\n\
CTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAG\\n\\\n\
CCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGG\\n\\\n\
CCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATC\\n\\\n\
ACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAA\\n\\\n\
AAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGC\\n\\\n\
TGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCC\\n\\\n\
ACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGG\\n\\\n\
CTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGG\\n\\\n\
AGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATT\\n\\\n\
AGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAA\\n\\\n\
TCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGC\\n\\\n\
CTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAA\\n\\\n\
TCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAG\\n\\\n\
CCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGT\\n\\\n\
GGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCG\\n\\\n\
GGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAG\\n\\\n\
CGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTG\\n\\\n\
GGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATG\\n\\\n\
GTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGT\\n\\\n\
AATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTT\\n\\\n\
GCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCT\\n\\\n\
CAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCG\\n\\\n\
GGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTC\\n\\\n\
TCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACT\\n\\\n\
CGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAG\\n\\\n\
ATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGG\\n\\\n\
CGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTG\\n\\\n\
AGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATA\\n\\\n\
CAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGG\\n\\\n\
CAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGC\\n\\\n\
ACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCAC\\n\\\n\
GCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTC\\n\\\n\
GAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCG\\n\\\n\
GGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCT\\n\\\n\
TGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGG\\n\\\n\
CGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCA\\n\\\n\
GCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGG\\n\\\n\
CCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGC\\n\\\n\
GCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGG\\n\\\n\
CGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGA\\n\\\n\
CTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGG\\n\\\n\
CCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAA\\n\\\n\
ACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCC\\n\\\n\
CAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGT\\n\\\n\
GAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAA\\n\\\n\
AGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGG\\n\\\n\
ATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTAC\\n\\\n\
TAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGA\\n\\\n\
GGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGC\\n\\\n\
GCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGG\\n\\\n\
TGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC\\n\\\n\
AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAA\\n\\\n\
ATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGA\\n\\\n\
GAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC\\n\\\n\
AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTG\\n\\\n\
TAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGAC\\n\\\n\
CAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGT\\n\\\n\
GGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAAC\\n\\\n\
CCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACA\\n\\\n\
GAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACT\\n\\\n\
TTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAAC\\n\\\n\
ATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCC\\n\\\n\
TGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAG\\n\\\n\
GTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCG\\n\\\n\
TCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAG\\n\\\n\
GCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCC\\n\\\n\
GTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCT\\n\\\n\
ACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCC\\n\\\n\
GAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCC\\n\\\n\
GGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCAC\\n\\\n\
CTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAA\\n\\\n\
ATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTG\\n\\\n\
AGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCAC\\n\\\n\
TGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCT\\n\\\n\
CACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAG\\n\\\n\
TTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAG\\n\\\n\
CCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATC\\n\\\n\
GCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCT\\n\\\n\
GGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATC\\n\\\n\
CCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCC\\n\\\n\
TGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGG\\n\\\n\
CGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGG\\n\\\n\
AGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCG\\n\\\n\
AGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGG\\n\\\n\
AGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGT\\n\\\n\
GAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAA\\n\\\n\
TCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGC\\n\\\n\
AGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCA\\n\\\n\
AAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGG\\n\\\n\
CGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTC\\n\\\n\
TACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCG\\n\\\n\
GGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGAT\\n\\\n\
CGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCG\\n\\\n\
CGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAG\\n\\\n\
GTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACA\\n\\\n\
AAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCA\\n\\\n\
GGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCAC\\n\\\n\
TCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGC\\n\\\n\
CTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGA\\n\\\n\
GACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGG\\n\\\n\
CGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTG\\n\\\n\
AACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCG\\n\\\n\
ACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGC\\n\\\n\
ACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCC\\n\\\n\
AACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGC\\n\\\n\
GCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCG\\n\\\n\
GAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACT\\n\\\n\
CCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCC\\n\\\n\
GAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAAC\\n\\\n\
CCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA\\n\\\n\
GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGA\\n\\\n\
GCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAG\\n\\\n\
GCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGAT\\n\\\n\
CACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTA\\n\\\n\
AAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGG\\n\\\n\
CTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGC\\n\\\n\
CACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTG\\n\\\n\
GCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAG\\n\\\n\
GAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAAT\\n\\\n\
TAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGA\\n\\\n\
ATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAG\\n\\\n\
CCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTA\\n\\\n\
ATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCA\\n\\\n\
GCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGG\\n\\\n\
TGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCC\\n\\\n\
GGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGA\\n\\\n\
GCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTT\\n\\\n\
GGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACAT\\n\\\n\
GGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTG\\n\\\n\
TAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGT\\n\\\n\
TGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTC\\n\\\n\
TCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGC\\n\\\n\
GGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGT\\n\\\n\
CTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTAC\\n\\\n\
TCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGA\\n\\\n\
GATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGG\\n\\\n\
GCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCT\\n\\\n\
GAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAAT\\n\\\n\
ACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAG\\n\\\n\
GCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTG\\n\\\n\
CACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCA\\n\\\n\
CGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTT\\n\\\n\
CGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCC\\n\\\n\
GGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGC\\n\\\n\
TTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGG\\n\\\n\
GCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCC\\n\\\n\
AGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTG\\n\\\n\
GCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCG\\n\\\n\
CGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAG\\n\\\n\
GCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAG\\n\\\n\
ACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAG\\n\\\n\
GCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGA\\n\\\n\
AACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATC\\n\\\n\
CCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAG\\n\\\n\
TGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAA\\n\\\n\
AAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCG\\n\\\n\
GATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTA\\n\\\n\
CTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGG\\n\\\n\
AGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCG\\n\\\n\
CGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCG\\n\\\n\
GTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGT\\n\\\n\
CAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAA\\n\\\n\
AATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGG\\n\\\n\
AGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTC\\n\\\n\
CAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCT\\n\\\n\
GTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA\\n\\\n\
CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG\\n\\\n\
TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAA\\n\\\n\
CCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGAC\\n\\\n\
AGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCAC\\n\\\n\
TTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAA\\n\\\n\
CATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGC\\n\\\n\
CTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGA\\n\\\n\
GGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCC\\n\\\n\
GTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGA\\n\\\n\
GGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCC\\n\\\n\
CGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGC\\n\\\n\
TACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGC\\n\\\n\
CGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGC\\n\\\n\
CGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCA\\n\\\n\
CCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAA\\n\\\n\
AATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCT\\n\\\n\
GAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCA\\n\\\n\
CTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGC\\n\\\n\
TCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGA\\n\\\n\
GTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTA\\n\\\n\
GCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAAT\\n\\\n\
CGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCC\\n\\\n\
TGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAAT\\n\\\n\
CCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGC\\n\\\n\
CTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTG\\n\\\n\
GCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGG\\n\\\n\
GAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGC\\n\\\n\
GAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG\\n\\\n\
GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGG\\n\\\n\
TGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTA\\n\\\n\
ATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTG\\n\\\n\
CAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTC\\n\\\n\
AAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGG\\n\\\n\
GCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCT\\n\\\n\
CTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTC\\n\\\n\
GGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGA\\n\\\n\
TCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGC\\n\\\n\
GCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGA\\n\\\n\
GGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATAC\\n\\\n\
AAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGC\\n\\\n\
AGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCA\\n\\\n\
CTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACG\\n\\\n\
CCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCG\\n\\\n\
AGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGG\\n\\\n\
GCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTT\\n\\\n\
GAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGC\\n\\\n\
GACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAG\\n\\\n\
CACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGC\\n\\\n\
CAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCG\\n\\\n\
CGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGC\\n\\\n\
GGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGAC\\n\\\n\
TCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGC\\n\\\n\
CGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAA\\n\\\n\
CCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCC\\n\\\n\
AGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTG\\n\\\n\
AGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA\\n\\\n\
GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGA\\n\\\n\
TCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACT\\n\\\n\
AAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAG\\n\\\n\
GCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCG\\n\\\n\
CCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGT\\n\\\n\
GGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCA\\n\\\n\
GGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAA\\n\\\n\
TTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAG\\n\\\n\
AATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCA\\n\\\n\
GCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGT\\n\\\n\
AATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACC\\n\\\n\
AGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTG\\n\\\n\
GTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACC\\n\\\n\
CGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAG\\n\\\n\
AGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTT\\n\\\n\
TGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACA\\n\\\n\
TGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCT\\n\\\n\
GTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGG\\n\\\n\
TTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGT\\n\\\n\
CTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGG\\n\\\n\
CGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCG\\n\\\n\
TCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTA\\n\\\n\
CTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCG\\n\\\n\
AGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCG\\n\\\n\
GGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACC\\n\\\n\
TGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAA\\n\\\n\
TACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGA\\n\\\n\
GGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACT\\n\\\n\
GCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTC\\n\\\n\
ACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGT\\n\\\n\
TCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGC\\n\\\n\
CGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCG\\n\\\n\
CTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTG\\n\\\n\
GGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCC\\n\\\n\
CAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCT\\n\\\n\
GGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGC\\n\\\n\
GCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGA\\n\\\n\
GGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGA\\n\\\n\
GACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGA\\n\\\n\
GGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTG\\n\\\n\
AAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAAT\\n\\\n\
CCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCA\\n\\\n\
GTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAA\\n\\\n\
AAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGC\\n\\\n\
GGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCT\\n\\\n\
ACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGG\\n\\\n\
GAGGCTGAGGCAGGAGAATC\\n\\\n\
>TWO IUB ambiguity codes\\n\\\n\
cttBtatcatatgctaKggNcataaaSatgtaaaDcDRtBggDtctttataattcBgtcg\\n\\\n\
tactDtDagcctatttSVHtHttKtgtHMaSattgWaHKHttttagacatWatgtRgaaa\\n\\\n\
NtactMcSMtYtcMgRtacttctWBacgaaatatagScDtttgaagacacatagtVgYgt\\n\\\n\
cattHWtMMWcStgttaggKtSgaYaaccWStcgBttgcgaMttBYatcWtgacaYcaga\\n\\\n\
gtaBDtRacttttcWatMttDBcatWtatcttactaBgaYtcttgttttttttYaaScYa\\n\\\n\
HgtgttNtSatcMtcVaaaStccRcctDaataataStcYtRDSaMtDttgttSagtRRca\\n\\\n\
tttHatSttMtWgtcgtatSSagactYaaattcaMtWatttaSgYttaRgKaRtccactt\\n\\\n\
tattRggaMcDaWaWagttttgacatgttctacaaaRaatataataaMttcgDacgaSSt\\n\\\n\
acaStYRctVaNMtMgtaggcKatcttttattaaaaagVWaHKYagtttttatttaacct\\n\\\n\
tacgtVtcVaattVMBcttaMtttaStgacttagattWWacVtgWYagWVRctDattBYt\\n\\\n\
gtttaagaagattattgacVatMaacattVctgtBSgaVtgWWggaKHaatKWcBScSWa\\n\\\n\
accRVacacaaactaccScattRatatKVtactatatttHttaagtttSKtRtacaaagt\\n\\\n\
RDttcaaaaWgcacatWaDgtDKacgaacaattacaRNWaatHtttStgttattaaMtgt\\n\\\n\
tgDcgtMgcatBtgcttcgcgaDWgagctgcgaggggVtaaScNatttacttaatgacag\\n\\\n\
cccccacatYScaMgtaggtYaNgttctgaMaacNaMRaacaaacaKctacatagYWctg\\n\\\n\
ttWaaataaaataRattagHacacaagcgKatacBttRttaagtatttccgatctHSaat\\n\\\n\
actcNttMaagtattMtgRtgaMgcataatHcMtaBSaRattagttgatHtMttaaKagg\\n\\\n\
YtaaBataSaVatactWtataVWgKgttaaaacagtgcgRatatacatVtHRtVYataSa\\n\\\n\
KtWaStVcNKHKttactatccctcatgWHatWaRcttactaggatctataDtDHBttata\\n\\\n\
aaaHgtacVtagaYttYaKcctattcttcttaataNDaaggaaaDYgcggctaaWSctBa\\n\\\n\
aNtgctggMBaKctaMVKagBaactaWaDaMaccYVtNtaHtVWtKgRtcaaNtYaNacg\\n\\\n\
gtttNattgVtttctgtBaWgtaattcaagtcaVWtactNggattctttaYtaaagccgc\\n\\\n\
tcttagHVggaYtgtNcDaVagctctctKgacgtatagYcctRYHDtgBattDaaDgccK\\n\\\n\
tcHaaStttMcctagtattgcRgWBaVatHaaaataYtgtttagMDMRtaataaggatMt\\n\\\n\
ttctWgtNtgtgaaaaMaatatRtttMtDgHHtgtcattttcWattRSHcVagaagtacg\\n\\\n\
ggtaKVattKYagactNaatgtttgKMMgYNtcccgSKttctaStatatNVataYHgtNa\\n\\\n\
BKRgNacaactgatttcctttaNcgatttctctataScaHtataRagtcRVttacDSDtt\\n\\\n\
aRtSatacHgtSKacYagttMHtWataggatgactNtatSaNctataVtttRNKtgRacc\\n\\\n\
tttYtatgttactttttcctttaaacatacaHactMacacggtWataMtBVacRaSaatc\\n\\\n\
cgtaBVttccagccBcttaRKtgtgcctttttRtgtcagcRttKtaaacKtaaatctcac\\n\\\n\
aattgcaNtSBaaccgggttattaaBcKatDagttactcttcattVtttHaaggctKKga\\n\\\n\
tacatcBggScagtVcacattttgaHaDSgHatRMaHWggtatatRgccDttcgtatcga\\n\\\n\
aacaHtaagttaRatgaVacttagattVKtaaYttaaatcaNatccRttRRaMScNaaaD\\n\\\n\
gttVHWgtcHaaHgacVaWtgttScactaagSgttatcttagggDtaccagWattWtRtg\\n\\\n\
ttHWHacgattBtgVcaYatcggttgagKcWtKKcaVtgaYgWctgYggVctgtHgaNcV\\n\\\n\
taBtWaaYatcDRaaRtSctgaHaYRttagatMatgcatttNattaDttaattgttctaa\\n\\\n\
ccctcccctagaWBtttHtBccttagaVaatMcBHagaVcWcagBVttcBtaYMccagat\\n\\\n\
gaaaaHctctaacgttagNWRtcggattNatcRaNHttcagtKttttgWatWttcSaNgg\\n\\\n\
gaWtactKKMaacatKatacNattgctWtatctaVgagctatgtRaHtYcWcttagccaa\\n\\\n\
tYttWttaWSSttaHcaaaaagVacVgtaVaRMgattaVcDactttcHHggHRtgNcctt\\n\\\n\
tYatcatKgctcctctatVcaaaaKaaaagtatatctgMtWtaaaacaStttMtcgactt\\n\\\n\
taSatcgDataaactaaacaagtaaVctaggaSccaatMVtaaSKNVattttgHccatca\\n\\\n\
cBVctgcaVatVttRtactgtVcaattHgtaaattaaattttYtatattaaRSgYtgBag\\n\\\n\
aHSBDgtagcacRHtYcBgtcacttacactaYcgctWtattgSHtSatcataaatataHt\\n\\\n\
cgtYaaMNgBaatttaRgaMaatatttBtttaaaHHKaatctgatWatYaacttMctctt\\n\\\n\
ttVctagctDaaagtaVaKaKRtaacBgtatccaaccactHHaagaagaaggaNaaatBW\\n\\\n\
attccgStaMSaMatBttgcatgRSacgttVVtaaDMtcSgVatWcaSatcttttVatag\\n\\\n\
ttactttacgatcaccNtaDVgSRcgVcgtgaacgaNtaNatatagtHtMgtHcMtagaa\\n\\\n\
attBgtataRaaaacaYKgtRccYtatgaagtaataKgtaaMttgaaRVatgcagaKStc\\n\\\n\
tHNaaatctBBtcttaYaBWHgtVtgacagcaRcataWctcaBcYacYgatDgtDHccta\\n\\\n\
aagacYRcaggattHaYgtKtaatgcVcaataMYacccatatcacgWDBtgaatcBaata\\n\\\n\
cKcttRaRtgatgaBDacggtaattaaYtataStgVHDtDctgactcaaatKtacaatgc\\n\\\n\
gYatBtRaDatHaactgtttatatDttttaaaKVccYcaaccNcBcgHaaVcattHctcg\\n\\\n\
attaaatBtatgcaaaaatYMctSactHatacgaWacattacMBgHttcgaatVaaaaca\\n\\\n\
BatatVtctgaaaaWtctRacgBMaatSgRgtgtcgactatcRtattaScctaStagKga\\n\\\n\
DcWgtYtDDWKRgRtHatRtggtcgaHgggcgtattaMgtcagccaBggWVcWctVaaat\\n\\\n\
tcgNaatcKWagcNaHtgaaaSaaagctcYctttRVtaaaatNtataaccKtaRgtttaM\\n\\\n\
tgtKaBtRtNaggaSattHatatWactcagtgtactaKctatttgRYYatKatgtccgtR\\n\\\n\
tttttatttaatatVgKtttgtatgtNtataRatWYNgtRtHggtaaKaYtKSDcatcKg\\n\\\n\
taaYatcSRctaVtSMWtVtRWHatttagataDtVggacagVcgKWagBgatBtaaagNc\\n\\\n\
aRtagcataBggactaacacRctKgttaatcctHgDgttKHHagttgttaatgHBtatHc\\n\\\n\
DaagtVaBaRccctVgtgDtacRHSctaagagcggWYaBtSaKtHBtaaactYacgNKBa\\n\\\n\
VYgtaacttagtVttcttaatgtBtatMtMtttaattaatBWccatRtttcatagVgMMt\\n\\\n\
agctStKctaMactacDNYgKYHgaWcgaHgagattacVgtttgtRaSttaWaVgataat\\n\\\n\
gtgtYtaStattattMtNgWtgttKaccaatagNYttattcgtatHcWtctaaaNVYKKt\\n\\\n\
tWtggcDtcgaagtNcagatacgcattaagaccWctgcagcttggNSgaNcHggatgtVt\\n\\\n\
catNtRaaBNcHVagagaaBtaaSggDaatWaatRccaVgggStctDaacataKttKatt\\n\\\n\
tggacYtattcSatcttagcaatgaVBMcttDattctYaaRgatgcattttNgVHtKcYR\\n\\\n\
aatRKctgtaaacRatVSagctgtWacBtKVatctgttttKcgtctaaDcaagtatcSat\\n\\\n\
aWVgcKKataWaYttcccSaatgaaaacccWgcRctWatNcWtBRttYaattataaNgac\\n\\\n\
acaatagtttVNtataNaYtaatRaVWKtBatKagtaatataDaNaaaaataMtaagaaS\\n\\\n\
tccBcaatNgaataWtHaNactgtcDtRcYaaVaaaaaDgtttRatctatgHtgttKtga\\n\\\n\
aNSgatactttcgagWaaatctKaaDaRttgtggKKagcDgataaattgSaacWaVtaNM\\n\\\n\
acKtcaDaaatttctRaaVcagNacaScRBatatctRatcctaNatWgRtcDcSaWSgtt\\n\\\n\
RtKaRtMtKaatgttBHcYaaBtgatSgaSWaScMgatNtctcctatttctYtatMatMt\\n\\\n\
RRtSaattaMtagaaaaStcgVgRttSVaScagtgDtttatcatcatacRcatatDctta\\n\\\n\
tcatVRtttataaHtattcYtcaaaatactttgVctagtaaYttagatagtSYacKaaac\\n\\\n\
gaaKtaaatagataatSatatgaaatSgKtaatVtttatcctgKHaatHattagaaccgt\\n\\\n\
YaaHactRcggSBNgtgctaaBagBttgtRttaaattYtVRaaaattgtaatVatttctc\\n\\\n\
ttcatgBcVgtgKgaHaaatattYatagWacNctgaaMcgaattStagWaSgtaaKagtt\\n\\\n\
ttaagaDgatKcctgtaHtcatggKttVDatcaaggtYcgccagNgtgcVttttagagat\\n\\\n\
gctaccacggggtNttttaSHaNtatNcctcatSaaVgtactgBHtagcaYggYVKNgta\\n\\\n\
KBcRttgaWatgaatVtagtcgattYgatgtaatttacDacSctgctaaaStttaWMagD\\n\\\n\
aaatcaVYctccgggcgaVtaaWtStaKMgDtttcaaMtVgBaatccagNaaatcYRMBg\\n\\\n\
gttWtaaScKttMWtYataRaDBMaDataatHBcacDaaKDactaMgagttDattaHatH\\n\\\n\
taYatDtattDcRNStgaatattSDttggtattaaNSYacttcDMgYgBatWtaMagact\\n\\\n\
VWttctttgYMaYaacRgHWaattgRtaagcattctMKVStatactacHVtatgatcBtV\\n\\\n\
NataaBttYtSttacKgggWgYDtgaVtYgatDaacattYgatggtRDaVDttNactaSa\\n\\\n\
MtgNttaacaaSaBStcDctaccacagacgcaHatMataWKYtaYattMcaMtgSttDag\\n\\\n\
cHacgatcaHttYaKHggagttccgatYcaatgatRaVRcaagatcagtatggScctata\\n\\\n\
ttaNtagcgacgtgKaaWaactSgagtMYtcttccaKtStaacggMtaagNttattatcg\\n\\\n\
tctaRcactctctDtaacWYtgaYaSaagaWtNtatttRacatgNaatgttattgWDDcN\\n\\\n\
aHcctgaaHacSgaataaRaataMHttatMtgaSDSKatatHHaNtacagtccaYatWtc\\n\\\n\
actaactatKDacSaStcggataHgYatagKtaatKagStaNgtatactatggRHacttg\\n\\\n\
tattatgtDVagDVaRctacMYattDgtttYgtctatggtKaRSttRccRtaaccttaga\\n\\\n\
gRatagSaaMaacgcaNtatgaaatcaRaagataatagatactcHaaYKBctccaagaRa\\n\\\n\
BaStNagataggcgaatgaMtagaatgtcaKttaaatgtaWcaBttaatRcggtgNcaca\\n\\\n\
aKtttScRtWtgcatagtttWYaagBttDKgcctttatMggNttattBtctagVtacata\\n\\\n\
aaYttacacaaRttcYtWttgHcaYYtaMgBaBatctNgcDtNttacgacDcgataaSat\\n\\\n\
YaSttWtcctatKaatgcagHaVaacgctgcatDtgttaSataaaaYSNttatagtaNYt\\n\\\n\
aDaaaNtggggacttaBggcHgcgtNtaaMcctggtVtaKcgNacNtatVaSWctWtgaW\\n\\\n\
cggNaBagctctgaYataMgaagatBSttctatacttgtgtKtaattttRagtDtacata\\n\\\n\
tatatgatNHVgBMtKtaKaNttDHaagatactHaccHtcatttaaagttVaMcNgHata\\n\\\n\
tKtaNtgYMccttatcaaNagctggacStttcNtggcaVtattactHaSttatgNMVatt\\n\\\n\
MMDtMactattattgWMSgtHBttStStgatatRaDaagattttctatMtaaaaaggtac\\n\\\n\
taaVttaSacNaatactgMttgacHaHRttgMacaaaatagttaatatWKRgacDgaRta\\n\\\n\
tatttattatcYttaWtgtBRtWatgHaaattHataagtVaDtWaVaWtgStcgtMSgaS\\n\\\n\
RgMKtaaataVacataatgtaSaatttagtcgaaHtaKaatgcacatcggRaggSKctDc\\n\\\n\
agtcSttcccStYtccRtctctYtcaaKcgagtaMttttcRaYDttgttatctaatcata\\n\\\n\
NctctgctatcaMatactataggDaHaaSttMtaDtcNatataattctMcStaaBYtaNa\\n\\\n\
gatgtaatHagagSttgWHVcttatKaYgDctcttggtgttMcRaVgSgggtagacaata\\n\\\n\
aDtaattSaDaNaHaBctattgNtaccaaRgaVtKNtaaYggHtaKKgHcatctWtctDt\\n\\\n\
ttctttggSDtNtaStagttataaacaattgcaBaBWggHgcaaaBtYgctaatgaaatW\\n\\\n\
cDcttHtcMtWWattBHatcatcaaatctKMagtDNatttWaBtHaaaNgMttaaStagt\\n\\\n\
tctctaatDtcRVaYttgttMtRtgtcaSaaYVgSWDRtaatagctcagDgcWWaaaBaa\\n\\\n\
RaBctgVgggNgDWStNaNBKcBctaaKtttDcttBaaggBttgaccatgaaaNgttttt\\n\\\n\
tttatctatgttataccaaDRaaSagtaVtDtcaWatBtacattaWacttaSgtattggD\\n\\\n\
gKaaatScaattacgWcagKHaaccaYcRcaRttaDttRtttHgaHVggcttBaRgtccc\\n\\\n\
tDatKaVtKtcRgYtaKttacgtatBtStaagcaattaagaRgBagSaattccSWYttta\\n\\\n\
ttVaataNctgHgttaaNBgcVYgtRtcccagWNaaaacaDNaBcaaaaRVtcWMgBagM\\n\\\n\
tttattacgDacttBtactatcattggaaatVccggttRttcatagttVYcatYaSHaHc\\n\\\n\
ttaaagcNWaHataaaRWtctVtRYtagHtaaaYMataHYtNBctNtKaatattStgaMc\\n\\\n\
BtRgctaKtgcScSttDgYatcVtggaaKtaagatWccHccgKYctaNNctacaWctttt\\n\\\n\
gcRtgtVcgaKttcMRHgctaHtVaataaDtatgKDcttatBtDttggNtacttttMtga\\n\\\n\
acRattaaNagaactcaaaBBVtcDtcgaStaDctgaaaSgttMaDtcgttcaccaaaag\\n\\\n\
gWtcKcgSMtcDtatgtttStaaBtatagDcatYatWtaaaBacaKgcaDatgRggaaYc\\n\\\n\
taRtccagattDaWtttggacBaVcHtHtaacDacYgtaatataMagaatgHMatcttat\\n\\\n\
acgtatttttatattacHactgttataMgStYaattYaccaattgagtcaaattaYtgta\\n\\\n\
tcatgMcaDcgggtcttDtKgcatgWRtataatatRacacNRBttcHtBgcRttgtgcgt\\n\\\n\
catacMtttBctatctBaatcattMttMYgattaaVYatgDaatVagtattDacaacDMa\\n\\\n\
tcMtHcccataagatgBggaccattVWtRtSacatgctcaaggggYtttDtaaNgNtaaB\\n\\\n\
atggaatgtctRtaBgBtcNYatatNRtagaacMgagSaSDDSaDcctRagtVWSHtVSR\\n\\\n\
ggaacaBVaccgtttaStagaacaMtactccagtttVctaaRaaHttNcttagcaattta\\n\\\n\
ttaatRtaaaatctaacDaBttggSagagctacHtaaRWgattcaaBtctRtSHaNtgta\\n\\\n\
cattVcaHaNaagtataccacaWtaRtaaVKgMYaWgttaKggKMtKcgWatcaDatYtK\\n\\\n\
SttgtacgaccNctSaattcDcatcttcaaaDKttacHtggttHggRRaRcaWacaMtBW\\n\\\n\
VHSHgaaMcKattgtaRWttScNattBBatYtaNRgcggaagacHSaattRtttcYgacc\\n\\\n\
BRccMacccKgatgaacttcgDgHcaaaaaRtatatDtatYVtttttHgSHaSaatagct\\n\\\n\
NYtaHYaVYttattNtttgaaaYtaKttWtctaNtgagaaaNctNDctaaHgttagDcRt\\n\\\n\
tatagccBaacgcaRBtRctRtggtaMYYttWtgataatcgaataattattataVaaaaa\\n\\\n\
ttacNRVYcaaMacNatRttcKatMctgaagactaattataaYgcKcaSYaatMNctcaa\\n\\\n\
cgtgatttttBacNtgatDccaattattKWWcattttatatatgatBcDtaaaagttgaa\\n\\\n\
VtaHtaHHtBtataRBgtgDtaataMttRtDgDcttattNtggtctatctaaBcatctaR\\n\\\n\
atgNacWtaatgaagtcMNaacNgHttatactaWgcNtaStaRgttaaHacccgaYStac\\n\\\n\
aaaatWggaYaWgaattattcMaactcBKaaaRVNcaNRDcYcgaBctKaacaaaaaSgc\\n\\\n\
tccYBBHYaVagaatagaaaacagYtctVccaMtcgtttVatcaatttDRtgWctagtac\\n\\\n\
RttMctgtDctttcKtWttttataaatgVttgBKtgtKWDaWagMtaaagaaattDVtag\\n\\\n\
gttacatcatttatgtcgMHaVcttaBtVRtcgtaYgBRHatttHgaBcKaYWaatcNSc\\n\\\n\
tagtaaaaatttacaatcactSWacgtaatgKttWattagttttNaggtctcaagtcact\\n\\\n\
attcttctaagKggaataMgtttcataagataaaaatagattatDgcBVHWgaBKttDgc\\n\\\n\
atRHaagcaYcRaattattatgtMatatattgHDtcaDtcaaaHctStattaatHaccga\\n\\\n\
cNattgatatattttgtgtDtRatagSacaMtcRtcattcccgacacSattgttKaWatt\\n\\\n\
NHcaacttccgtttSRtgtctgDcgctcaaMagVtBctBMcMcWtgtaacgactctcttR\\n\\\n\
ggRKSttgYtYatDccagttDgaKccacgVatWcataVaaagaataMgtgataaKYaaat\\n\\\n\
cHDaacgataYctRtcYatcgcaMgtNttaBttttgatttaRtStgcaacaaaataccVg\\n\\\n\
aaDgtVgDcStctatatttattaaaaRKDatagaaagaKaaYYcaYSgKStctccSttac\\n\\\n\
agtcNactttDVttagaaagMHttRaNcSaRaMgBttattggtttaRMggatggcKDgWR\\n\\\n\
tNaataataWKKacttcKWaaagNaBttaBatMHtccattaacttccccYtcBcYRtaga\\n\\\n\
ttaagctaaYBDttaNtgaaaccHcaRMtKtaaHMcNBttaNaNcVcgVttWNtDaBatg\\n\\\n\
ataaVtcWKcttRggWatcattgaRagHgaattNtatttctctattaattaatgaDaaMa\\n\\\n\
tacgttgggcHaYVaaNaDDttHtcaaHtcVVDgBVagcMacgtgttaaBRNtatRtcag\\n\\\n\
taagaggtttaagacaVaaggttaWatctccgtVtaDtcDatttccVatgtacNtttccg\\n\\\n\
tHttatKgScBatgtVgHtYcWagcaKtaMYaaHgtaattaSaHcgcagtWNaatNccNN\\n\\\n\
YcacgVaagaRacttctcattcccRtgtgtaattagcSttaaStWaMtctNNcSMacatt\\n\\\n\
ataaactaDgtatWgtagtttaagaaaattgtagtNagtcaataaatttgatMMYactaa\\n\\\n\
tatcggBWDtVcYttcDHtVttatacYaRgaMaacaStaatcRttttVtagaDtcacWat\\n\\\n\
ttWtgaaaagaaagNRacDtttStVatBaDNtaactatatcBSMcccaSttccggaMatg\\n\\\n\
attaaWatKMaBaBatttgataNctgttKtVaagtcagScgaaaDggaWgtgttttKtWt\\n\\\n\
atttHaatgtagttcactaaKMagttSYBtKtaYgaactcagagRtatagtVtatcaaaW\\n\\\n\
YagcgNtaDagtacNSaaYDgatBgtcgataacYDtaaactacagWDcYKaagtttatta\\n\\\n\
gcatcgagttKcatDaattgattatDtcagRtWSKtcgNtMaaaaacaMttKcaWcaaSV\\n\\\n\
MaaaccagMVtaMaDtMaHaBgaacataBBVtaatVYaNSWcSgNtDNaaKacacBttta\\n\\\n\
tKtgtttcaaHaMctcagtaacgtcgYtactDcgcctaNgagagcYgatattttaaattt\\n\\\n\
ccattttacatttDaaRctattttWctttacgtDatYtttcagacgcaaVttagtaaKaa\\n\\\n\
aRtgVtccataBggacttatttgtttaWNtgttVWtaWNVDaattgtatttBaagcBtaa\\n\\\n\
BttaaVatcHcaVgacattccNggtcgacKttaaaRtagRtctWagaYggtgMtataatM\\n\\\n\
tgaaRttattttgWcttNtDRRgMDKacagaaaaggaaaRStcccagtYccVattaNaaK\\n\\\n\
StNWtgacaVtagaagcttSaaDtcacaacgDYacWDYtgtttKatcVtgcMaDaSKStV\\n\\\n\
cgtagaaWaKaagtttcHaHgMgMtctataagBtKaaaKKcactggagRRttaagaBaaN\\n\\\n\
atVVcgRcKSttDaactagtSttSattgttgaaRYatggttVttaataaHttccaagDtg\\n\\\n\
atNWtaagHtgcYtaactRgcaatgMgtgtRaatRaNaacHKtagactactggaatttcg\\n\\\n\
ccataacgMctRgatgttaccctaHgtgWaYcactcacYaattcttaBtgacttaaacct\\n\\\n\
gYgaWatgBttcttVttcgttWttMcNYgtaaaatctYgMgaaattacNgaHgaacDVVM\\n\\\n\
tttggtHtctaaRgtacagacgHtVtaBMNBgattagcttaRcttacaHcRctgttcaaD\\n\\\n\
BggttKaacatgKtttYataVaNattccgMcgcgtagtRaVVaattaKaatggttRgaMc\\n\\\n\
agtatcWBttNtHagctaatctagaaNaaacaYBctatcgcVctBtgcaaagDgttVtga\\n\\\n\
HtactSNYtaaNccatgtgDacgaVtDcgKaRtacDcttgctaagggcagMDagggtBWR\\n\\\n\
tttSgccttttttaacgtcHctaVtVDtagatcaNMaVtcVacatHctDWNaataRgcgt\\n\\\n\
aVHaggtaaaaSgtttMtattDgBtctgatSgtRagagYtctSaKWaataMgattRKtaa\\n\\\n\
catttYcgtaacacattRWtBtcggtaaatMtaaacBatttctKagtcDtttgcBtKYYB\\n\\\n\
aKttctVttgttaDtgattttcttccacttgSaaacggaaaNDaattcYNNaWcgaaYat\\n\\\n\
tttMgcBtcatRtgtaaagatgaWtgaccaYBHgaatagataVVtHtttVgYBtMctaMt\\n\\\n\
cctgaDcYttgtccaaaRNtacagcMctKaaaggatttacatgtttaaWSaYaKttBtag\\n\\\n\
DacactagctMtttNaKtctttcNcSattNacttggaacaatDagtattRtgSHaataat\\n\\\n\
gccVgacccgatactatccctgtRctttgagaSgatcatatcgDcagWaaHSgctYYWta\\n\\\n\
tHttggttctttatVattatcgactaagtgtagcatVgtgHMtttgtttcgttaKattcM\\n\\\n\
atttgtttWcaaStNatgtHcaaaDtaagBaKBtRgaBgDtSagtatMtaacYaatYtVc\\n\\\n\
KatgtgcaacVaaaatactKcRgtaYtgtNgBBNcKtcttaccttKgaRaYcaNKtactt\\n\\\n\
tgagSBtgtRagaNgcaaaNcacagtVtttHWatgttaNatBgtttaatNgVtctgaata\\n\\\n\
tcaRtattcttttttttRaaKcRStctcggDgKagattaMaaaKtcaHacttaataataK\\n\\\n\
taRgDtKVBttttcgtKaggHHcatgttagHggttNctcgtatKKagVagRaaaggaaBt\\n\\\n\
NatttVKcRttaHctaHtcaaatgtaggHccaBataNaNaggttgcWaatctgatYcaaa\\n\\\n\
HaatWtaVgaaBttagtaagaKKtaaaKtRHatMaDBtBctagcatWtatttgWttVaaa\\n\\\n\
ScMNattRactttgtYtttaaaagtaagtMtaMaSttMBtatgaBtttaKtgaatgagYg\\n\\\n\
tNNacMtcNRacMMHcttWtgtRtctttaacaacattattcYaMagBaacYttMatcttK\\n\\\n\
cRMtgMNccattaRttNatHaHNaSaaHMacacaVaatacaKaSttHatattMtVatWga\\n\\\n\
ttttttaYctttKttHgScWaacgHtttcaVaaMgaacagNatcgttaacaaaaagtaca\\n\\\n\
HBNaattgttKtcttVttaaBtctgctacgBgcWtttcaggacacatMgacatcccagcg\\n\\\n\
gMgaVKaBattgacttaatgacacacaaaaaatRKaaBctacgtRaDcgtagcVBaacDS\\n\\\n\
BHaaaaSacatatacagacRNatcttNaaVtaaaataHattagtaaaaSWccgtatWatg\\n\\\n\
gDttaactattgcccatcttHaSgYataBttBaactattBtcHtgatcaataSttaBtat\\n\\\n\
KSHYttWggtcYtttBttaataccRgVatStaHaKagaatNtagRMNgtcttYaaSaact\\n\\\n\
cagDSgagaaYtMttDtMRVgWKWtgMaKtKaDttttgactatacataatcNtatNaHat\\n\\\n\
tVagacgYgatatatttttgtStWaaatctWaMgagaRttRatacgStgattcttaagaD\\n\\\n\
taWccaaatRcagcagaaNKagtaaDggcgccBtYtagSBMtactaaataMataBSacRM\\n\\\n\
gDgattMMgtcHtcaYDtRaDaacggttDaggcMtttatgttaNctaattaVacgaaMMt\\n\\\n\
aatDccSgtattgaRtWWaccaccgagtactMcgVNgctDctaMScatagcgtcaactat\\n\\\n\
acRacgHRttgctatttaatgaattataYKttgtaagWgtYttgcHgMtaMattWaWVta\\n\\\n\
RgcttgYgttBHtYataSccStBtgtagMgtDtggcVaaSBaatagDttgBgtctttctc\\n\\\n\
attttaNagtHKtaMWcYactVcgcgtatMVtttRacVagDaatcttgctBBcRDgcaac\\n\\\n\
KttgatSKtYtagBMagaRtcgBattHcBWcaactgatttaatttWDccatttatcgagS\\n\\\n\
KaWttataHactaHMttaatHtggaHtHagaatgtKtaaRactgtttMatacgatcaagD\\n\\\n\
gatKaDctataMggtHDtggHacctttRtatcttYattttgacttgaaSaataaatYcgB\\n\\\n\
aaaaccgNatVBttMacHaKaataagtatKgtcaagactcttaHttcggaattgttDtct\\n\\\n\
aaccHttttWaaatgaaatataaaWattccYDtKtaaaacggtgaggWVtctattagtga\\n\\\n\
ctattaagtMgtttaagcatttgSgaaatatccHaaggMaaaattttcWtatKctagDtY\\n\\\n\
tMcctagagHcactttactatacaaacattaacttaHatcVMYattYgVgtMttaaRtga\\n\\\n\
aataaDatcaHgtHHatKcDYaatcttMtNcgatYatgSaMaNtcttKcWataScKggta\\n\\\n\
tcttacgcttWaaagNatgMgHtctttNtaacVtgttcMaaRatccggggactcMtttaY\\n\\\n\
MtcWRgNctgNccKatcttgYDcMgattNYaRagatHaaHgKctcataRDttacatBatc\\n\\\n\
cattgDWttatttaWgtcggagaaaaatacaatacSNtgggtttccttacSMaagBatta\\n\\\n\
caMaNcactMttatgaRBacYcYtcaaaWtagctSaacttWgDMHgaggatgBVgcHaDt\\n\\\n\
ggaactttggtcNatNgtaKaBcccaNtaagttBaacagtatacDYttcctNgWgcgSMc\\n\\\n\
acatStctHatgRcNcgtacacaatRttMggaNKKggataaaSaYcMVcMgtaMaHtgat\\n\\\n\
tYMatYcggtcttcctHtcDccgtgRatcattgcgccgatatMaaYaataaYSggatagc\\n\\\n\
gcBtNtaaaScaKgttBgagVagttaKagagtatVaactaSacWactSaKatWccaKaaa\\n\\\n\
atBKgaaKtDMattttgtaaatcRctMatcaaMagMttDgVatggMaaWgttcgaWatga\\n\\\n\
aatttgRtYtattaWHKcRgctacatKttctaccaaHttRatctaYattaaWatVNccat\\n\\\n\
NgagtcKttKataStRaatatattcctRWatDctVagttYDgSBaatYgttttgtVaatt\\n\\\n\
taatagcagMatRaacttBctattgtMagagattaaactaMatVtHtaaatctRgaaaaa\\n\\\n\
aaatttWacaacaYccYDSaattMatgaccKtaBKWBattgtcaagcHKaagttMMtaat\\n\\\n\
ttcKcMagNaaKagattggMagaggtaatttYacatcWaaDgatMgKHacMacgcVaaca\\n\\\n\
DtaDatatYggttBcgtatgWgaSatttgtagaHYRVacaRtctHaaRtatgaactaata\\n\\\n\
tctSSBgggaaHMWtcaagatKgagtDaSatagttgattVRatNtctMtcSaagaSHaat\\n\\\n\
aNataataRaaRgattctttaataaagWaRHcYgcatgtWRcttgaaggaMcaataBRaa\\n\\\n\
ccagStaaacNtttcaatataYtaatatgHaDgcStcWttaacctaRgtYaRtataKtgM\\n\\\n\
ttttatgactaaaatttacYatcccRWtttHRtattaaatgtttatatttgttYaatMca\\n\\\n\
RcSVaaDatcgtaYMcatgtagacatgaaattgRtcaaYaaYtRBatKacttataccaNa\\n\\\n\
aattVaBtctggacaagKaaYaaatatWtMtatcYaaVNtcgHaactBaagKcHgtctac\\n\\\n\
aatWtaDtSgtaHcataHtactgataNctRgttMtDcDttatHtcgtacatcccaggStt\\n\\\n\
aBgtcacacWtccNMcNatMVaVgtccDYStatMaccDatggYaRKaaagataRatttHK\\n\\\n\
tSaaatDgataaacttaHgttgVBtcttVttHgDacgaKatgtatatNYataactctSat\\n\\\n\
atatattgcHRRYttStggaactHgttttYtttaWtatMcttttctatctDtagVHYgMR\\n\\\n\
BgtHttcctaatYRttKtaagatggaVRataKDctaMtKBNtMtHNtWtttYcVtattMc\\n\\\n\
gRaacMcctNSctcatttaaagDcaHtYccSgatgcaatYaaaaDcttcgtaWtaattct\\n\\\n\
cgttttScttggtaatctttYgtctaactKataHacctMctcttacHtKataacacagcN\\n\\\n\
RatgKatttttSaaatRYcgDttaMRcgaaattactMtgcgtaagcgttatBtttttaat\\n\\\n\
taagtNacatHgttcRgacKcBBtVgatKttcgaBaatactDRgtRtgaNacWtcacYtt\\n\\\n\
aaKcgttctHaKttaNaMgWgWaggtctRgaKgWttSttBtDcNtgtttacaaatYcDRt\\n\\\n\
gVtgcctattcNtctaaaDMNttttNtggctgagaVctDaacVtWccaagtaacacaNct\\n\\\n\
gaScattccDHcVBatcgatgtMtaatBgHaatDctMYgagaatgYWKcctaatNaStHa\\n\\\n\
aaKccgHgcgtYaaYtattgtStgtgcaaRtattaKatattagaWVtcaMtBagttatta\\n\\\n\
gNaWHcVgcaattttDcMtgtaRHVYtHtctgtaaaaHVtMKacatcgNaatttMatatg\\n\\\n\
ttgttactagWYtaRacgataKagYNKcattataNaRtgaacKaYgcaaYYacaNccHat\\n\\\n\
MatDcNgtHttRaWttagaaDcaaaaaatagggtKDtStaDaRtaVtHWKNtgtattVct\\n\\\n\
SVgRgataDaRaWataBgaagaaKtaataaYgDcaStaNgtaDaaggtattHaRaWMYaY\\n\\\n\
aWtggttHYgagVtgtgcttttcaaDKcagVcgttagacNaaWtagtaataDttctggtt\\n\\\n\
VcatcataaagtgKaaaNaMtaBBaattaatWaattgctHaVKaSgDaaVKaHtatatat\\n\\\n\
HatcatSBagNgHtatcHYMHgttDgtaHtBttWatcgtttaRaattgStKgSKNWKatc\\n\\\n\
agDtctcagatttctRtYtBatBgHHtKaWtgYBgacVVWaKtacKcDttKMaKaVcggt\\n\\\n\
gttataagaataaHaatattagtataatMHgttYgaRttagtaRtcaaVatacggtcMcg\\n\\\n\
agtaaRttacWgactKRYataaaagSattYaWgagatYagKagatgSaagKgttaatMgg\\n\\\n\
tataatgttWYttatgagaaacctNVataatHcccKtDctcctaatactggctHggaSag\\n\\\n\
gRtKHaWaattcgSatMatttagaggcYtctaMcgctcataSatatgRagacNaaDagga\\n\\\n\
VBagaYttKtacNaKgtSYtagttggaWcatcWttaatctatgaVtcgtgtMtatcaYcg\\n\\\n\
tRccaaYgDctgcMgtgtWgacWtgataacacgcgctBtgttaKtYDtatDcatcagKaV\\n\\\n\
MctaatcttgVcaaRgcRMtDcgattaHttcaNatgaatMtactacVgtRgatggaWttt\\n\\\n\
actaaKatgagSaaKggtaNtactVaYtaaKRagaacccacaMtaaMtKtatBcttgtaa\\n\\\n\
WBtMctaataaVcDaaYtcRHBtcgttNtaaHatttBNgRStVDattBatVtaagttaYa\\n\\\n\
tVattaagaBcacggtSgtVtatttaRattgatgtaHDKgcaatattKtggcctatgaWD\\n\\\n\
KRYcggattgRctatNgatacaatMNttctgtcRBYRaaaHctNYattcHtaWcaattct\\n\\\n\
BtMKtVgYataatMgYtcagcttMDataVtggRtKtgaatgccNcRttcaMtRgattaac\\n\\\n\
attRcagcctHtWMtgtDRagaKaBtgDttYaaaaKatKgatctVaaYaacWcgcatagB\\n\\\n\
VtaNtRtYRaggBaaBtgKgttacataagagcatgtRattccacttaccatRaaatgWgD\\n\\\n\
aMHaYVgVtaSctatcgKaatatattaDgacccYagtgtaYNaaatKcagtBRgagtcca\\n\\\n\
tgKgaaaccBgaagBtgSttWtacgatWHaYatcgatttRaaNRgcaNaKVacaNtDgat\\n\\\n\
tgHVaatcDaagcgtatgcNttaDataatcSataaKcaataaHWataBtttatBtcaKtK\\n\\\n\
tatagttaDgSaYctacaRatNtaWctSaatatttYaKaKtaccWtatcRagacttaYtt\\n\\\n\
VcKgSDcgagaagatccHtaattctSttatggtKYgtMaHagVaBRatttctgtRgtcta\\n\\\n\
tgggtaHKgtHacHtSYacgtacacHatacKaaBaVaccaDtatcSaataaHaagagaat\\n\\\n\
ScagactataaRttagcaaVcaHataKgDacatWccccaagcaBgagWatctaYttgaaa\\n\\\n\
tctVNcYtttWagHcgcgcDcVaaatgttKcHtNtcaatagtgtNRaactttttcaatgg\\n\\\n\
WgBcgDtgVgtttctacMtaaataaaRggaaacWaHttaRtNtgctaaRRtVBctYtVta\\n\\\n\
tDcattDtgaccYatagatYRKatNYKttNgcctagtaWtgaactaMVaacctgaStttc\\n\\\n\
tgaKVtaaVaRKDttVtVctaDNtataaaDtccccaagtWtcgatcactDgYaBcatcct\\n\\\n\
MtVtacDaaBtYtMaKNatNtcaNacgDatYcatcgcaRatWBgaacWttKttagYtaat\\n\\\n\
tcggttgSWttttDWctttacYtatatWtcatDtMgtBttgRtVDggttaacYtacgtac\\n\\\n\
atgaattgaaWcttMStaDgtatattgaDtcRBcattSgaaVBRgagccaaKtttcDgcg\\n\\\n\
aSMtatgWattaKttWtgDBMaggBBttBaatWttRtgcNtHcgttttHtKtcWtagHSt\\n\\\n\
aacagttgatatBtaWSaWggtaataaMttaKacDaatactcBttcaatatHttcBaaSa\\n\\\n\
aatYggtaRtatNtHcaatcaHtagVtgtattataNggaMtcttHtNagctaaaggtaga\\n\\\n\
YctMattNaMVNtcKtactBKcaHHcBttaSagaKacataYgctaKaYgttYcgacWVtt\\n\\\n\
WtSagcaacatcccHaccKtcttaacgaKttcacKtNtacHtatatRtaaatacactaBt\\n\\\n\
ttgaHaRttggttWtatYagcatYDatcggagagcWBataagRtacctataRKgtBgatg\\n\\\n\
aDatataSttagBaHtaatNtaDWcWtgtaattacagKttcNtMagtattaNgtctcgtc\\n\\\n\
ctcttBaHaKcKccgtRcaaYagSattaagtKataDatatatagtcDtaacaWHcaKttD\\n\\\n\
gaaRcgtgYttgtcatatNtatttttatggccHtgDtYHtWgttatYaacaattcaWtat\\n\\\n\
NgctcaaaSttRgctaatcaaatNatcgtttaBtNNVtgttataagcaaagattBacgtD\\n\\\n\
atttNatttaaaDcBgtaSKgacgtagataatttcHMVNttgttBtDtgtaWKaaRMcKM\\n\\\n\
tHtaVtagataWctccNNaSWtVaHatctcMgggDgtNHtDaDttatatVWttgttattt\\n\\\n\
aacctttcacaaggaSaDcggttttttatatVtctgVtaacaStDVaKactaMtttaSNa\\n\\\n\
gtgaaattaNacttSKctattcctctaSagKcaVttaagNaVcttaVaaRNaHaaHttat\\n\\\n\
gtHttgtgatMccaggtaDcgaccgtWgtWMtttaHcRtattgScctatttKtaaccaag\\n\\\n\
tYagaHgtWcHaatgccKNRtttagtMYSgaDatctgtgaWDtccMNcgHgcaaacNDaa\\n\\\n\
aRaStDWtcaaaaHKtaNBctagBtgtattaactaattttVctagaatggcWSatMaccc\\n\\\n\
ttHttaSgSgtgMRcatRVKtatctgaaaccDNatYgaaVHNgatMgHRtacttaaaRta\\n\\\n\
tStRtDtatDttYatattHggaBcttHgcgattgaKcKtttcRataMtcgaVttWacatN\\n\\\n\
catacctRataDDatVaWNcggttgaHtgtMacVtttaBHtgagVttMaataattatgtt\\n\\\n\
cttagtttgtgcDtSatttgBtcaacHattaaBagVWcgcaSYttMgcttacYKtVtatc\\n\\\n\
aYaKctgBatgcgggcYcaaaaacgNtctagKBtattatctttKtaVttatagtaYtRag\\n\\\n\
NtaYataaVtgaatatcHgcaaRataHtacacatgtaNtgtcgYatWMatttgaactacR\\n\\\n\
ctaWtWtatacaatctBatatgYtaagtatgtgtatSttactVatcttYtaBcKgRaSgg\\n\\\n\
RaaaaatgcagtaaaWgtaRgcgataatcBaataccgtatttttccatcNHtatWYgatH\\n\\\n\
SaaaDHttgctgtccHtggggcctaataatttttctatattYWtcattBtgBRcVttaVM\\n\\\n\
RSgctaatMagtYtttaaaaatBRtcBttcaaVtaacagctccSaaSttKNtHtKYcagc\\n\\\n\
agaaaccccRtttttaaDcDtaStatccaagcgctHtatcttaDRYgatDHtWcaaaBcW\\n\\\n\
gKWHttHataagHacgMNKttMKHccaYcatMVaacgttaKgYcaVaaBtacgcaacttt\\n\\\n\
MctaaHaatgtBatgagaSatgtatgSRgHgWaVWgataaatatttccKagVgataattW\\n\\\n\
aHNcYggaaatgctHtKtaDtctaaagtMaatVDVactWtSaaWaaMtaHtaSKtcBRaN\\n\\\n\
cttStggtBttacNagcatagRgtKtgcgaacaacBcgKaatgataagatgaaaattgta\\n\\\n\
ctgcgggtccHHWHaaNacaBttNKtKtcaaBatatgctaHNgtKcDWgtttatNgVDHg\\n\\\n\
accaacWctKaaggHttgaRgYaatHcaBacaatgagcaaattactgtaVaaYaDtagat\\n\\\n\
tgagNKggtggtgKtWKaatacagDRtatRaMRtgattDggtcaaYRtatttNtagaDtc\\n\\\n\
acaaSDctDtataatcgtactaHttatacaatYaacaaHttHatHtgcgatRRttNgcat\\n\\\n\
SVtacWWgaaggagtatVMaVaaattScDDKNcaYBYaDatHgtctatBagcaacaagaa\\n\\\n\
tgagaaRcataaKNaRtBDatcaaacgcattttttaaBtcSgtacaRggatgtMNaattg\\n\\\n\
gatatWtgagtattaaaVctgcaYMtatgatttttYgaHtgtcttaagWBttHttgtctt\\n\\\n\
attDtcgtatWtataataSgctaHagcDVcNtaatcaagtaBDaWaDgtttagYctaNcc\\n\\\n\
DtaKtaHcttaataacccaRKtacaVaatNgcWRaMgaattatgaBaaagattVYaHMDc\\n\\\n\
aDHtcRcgYtcttaaaWaaaVKgatacRtttRRKYgaatacaWVacVcRtatMacaBtac\\n\\\n\
tggMataaattttHggNagSctacHgtBagcgtcgtgattNtttgatSaaggMttctttc\\n\\\n\
ttNtYNagBtaaacaaatttMgaccttacataattgYtcgacBtVMctgStgMDtagtaR\\n\\\n\
ctHtatgttcatatVRNWataDKatWcgaaaaagttaaaagcacgHNacgtaatctttMR\\n\\\n\
tgacttttDacctataaacgaaatatgattagaactccSYtaBctttaataacWgaaaYa\\n\\\n\
tagatgWttcatKtNgatttttcaagHtaYgaaRaDaagtaggagcttatVtagtctttc\\n\\\n\
attaaaatcgKtattaRttacagVaDatgcatVgattgggtctttHVtagKaaRBtaHta\\n\\\n\
aggccccaaaaKatggtttaMWgtBtaaacttcactttKHtcgatctccctaYaBacMgt\\n\\\n\
cttBaBaNgcgaaacaatctagtHccHtKttcRtRVttccVctttcatacYagMVtMcag\\n\\\n\
aMaaacaataBctgYtaatRaaagattaaccatVRatHtaRagcgcaBcgDttStttttc\\n\\\n\
VtttaDtKgcaaWaaaaatSccMcVatgtKgtaKgcgatatgtagtSaaaDttatacaaa\\n\\\n\
catYaRRcVRHctKtcgacKttaaVctaDaatgttMggRcWaacttttHaDaKaDaBctg\\n\\\n\
taggcgtttaHBccatccattcNHtDaYtaataMttacggctNVaacDattgatatttta\\n\\\n\
cVttSaattacaaRtataNDgacVtgaacataVRttttaDtcaaacataYDBtttaatBa\\n\\\n\
DtttYDaDaMccMttNBttatatgagaaMgaNtattHccNataattcaHagtgaaggDga\\n\\\n\
tgtatatatgYatgaStcataaBStWacgtcccataRMaaDattggttaaattcMKtctM\\n\\\n\
acaBSactcggaatDDgatDgcWctaacaccgggaVcacWKVacggtaNatatacctMta\\n\\\n\
tgatagtgcaKagggVaDtgtaacttggagtcKatatcgMcttRaMagcattaBRaStct\\n\\\n\
YSggaHYtacaactMBaagDcaBDRaaacMYacaHaattagcattaaaHgcgctaaggSc\\n\\\n\
cKtgaaKtNaBtatDDcKBSaVtgatVYaagVtctSgMctacgttaacWaaattctSgtD\\n\\\n\
actaaStaaattgcagBBRVctaatatacctNttMcRggctttMttagacRaHcaBaacV\\n\\\n\
KgaataHttttMgYgattcYaNRgttMgcVaaacaVVcDHaatttgKtMYgtatBtVVct\\n\\\n\
WgVtatHtacaaHttcacgatagcagtaaNattBatatatttcVgaDagcggttMaagtc\\n\\\n\
ScHagaaatgcYNggcgtttttMtStggtRatctacttaaatVVtBacttHNttttaRca\\n\\\n\
aatcacagHgagagtMgatcSWaNRacagDtatactaaDKaSRtgattctccatSaaRtt\\n\\\n\
aaYctacacNtaRtaactggatgaccYtacactttaattaattgattYgttcagDtNKtt\\n\\\n\
agDttaaaaaaaBtttaaNaYWKMBaaaacVcBMtatWtgBatatgaacVtattMtYatM\\n\\\n\
NYDKNcKgDttDaVtaaaatgggatttctgtaaatWtctcWgtVVagtcgRgacttcccc\\n\\\n\
taDcacagcRcagagtgtWSatgtacatgttaaSttgtaaHcgatgggMagtgaacttat\\n\\\n\
RtttaVcaccaWaMgtactaatSSaHtcMgaaYtatcgaaggYgggcgtgaNDtgttMNg\\n\\\n\
aNDMtaattcgVttttaacatgVatgtWVMatatcaKgaaattcaBcctccWcttgaaWH\\n\\\n\
tWgHtcgNWgaRgctcBgSgaattgcaaHtgattgtgNagtDttHHgBttaaWcaaWagc\\n\\\n\
aSaHHtaaaVctRaaMagtaDaatHtDMtcVaWMtagSagcttHSattaacaaagtRacM\\n\\\n\
tRtctgttagcMtcaBatVKtKtKacgagaSNatSactgtatatcBctgagVtYactgta\\n\\\n\
aattaaaggcYgDHgtaacatSRDatMMccHatKgttaacgactKtgKagtcttcaaHRV\\n\\\n\
tccttKgtSataatttacaactggatDNgaacttcaRtVaagDcaWatcBctctHYatHa\\n\\\n\
DaaatttagYatSatccaWtttagaaatVaacBatHcatcgtacaatatcgcNYRcaata\\n\\\n\
YaRaYtgattVttgaatgaVaactcRcaNStgtgtattMtgaggtNttBaDRcgaaaagc\\n\\\n\
tNgBcWaWgtSaDcVtgVaatMKBtttcgtttctaaHctaaagYactgMtatBDtcStga\\n\\\n\
ccgtSDattYaataHctgggaYYttcggttaWaatctggtRagWMaDagtaacBccacta\\n\\\n\
cgHWMKaatgatWatcctgHcaBaSctVtcMtgtDttacctaVgatYcWaDRaaaaRtag\\n\\\n\
atcgaMagtggaRaWctctgMgcWttaagKBRtaaDaaWtctgtaagYMttactaHtaat\\n\\\n\
cttcataacggcacBtSgcgttNHtgtHccatgttttaaagtatcgaKtMttVcataYBB\\n\\\n\
aKtaMVaVgtattNDSataHcagtWMtaggtaSaaKgttgBtVtttgttatcatKcgHac\\n\\\n\
acRtctHatNVagSBgatgHtgaRaSgttRcctaacaaattDNttgacctaaYtBgaaaa\\n\\\n\
tagttattactcttttgatgtNNtVtgtatMgtcttRttcatttgatgacacttcHSaaa\\n\\\n\
ccaWWDtWagtaRDDVNacVaRatgttBccttaatHtgtaaacStcVNtcacaSRttcYa\\n\\\n\
gacagaMMttttgMcNttBcgWBtactgVtaRttctccaaYHBtaaagaBattaYacgat\\n\\\n\
ttacatctgtaaMKaRYtttttactaaVatWgctBtttDVttctggcDaHaggDaagtcg\\n\\\n\
aWcaagtagtWttHtgKtVataStccaMcWcaagataagatcactctHatgtcYgaKcat\\n\\\n\
cagatactaagNSStHcctRRNtattgtccttagttagMVgtatagactaactctVcaat\\n\\\n\
MctgtttgtgttgccttatWgtaBVtttctggMcaaKgDWtcgtaaYStgSactatttHg\\n\\\n\
atctgKagtagBtVacRaagRtMctatgggcaaaKaaaatacttcHctaRtgtDcttDat\\n\\\n\
taggaaatttcYHaRaaBttaatggcacKtgctHVcaDcaaaVDaaaVcgMttgtNagcg\\n\\\n\
taDWgtcgttaatDgKgagcSatatcSHtagtagttggtgtHaWtaHKtatagctgtVga\\n\\\n\
ttaBVaatgaataagtaatVatSttaHctttKtttgtagttaccttaatcgtagtcctgB\\n\\\n\
cgactatttVcMacHaaaggaatgDatggKtaHtgStatattaaSagctWcctccRtata\\n\\\n\
BaDYcgttgcNaagaggatRaaaYtaWgNtSMcaatttactaacatttaaWttHtatBat\\n\\\n\
tgtcgacaatNgattgcNgtMaaaKaBDattHacttggtRtttaYaacgVactBtaBaKt\\n\\\n\
gBttatgVttgtVttcaatcWcNctDBaaBgaDHacBttattNtgtDtatttVSaaacag\\n\\\n\
gatgcRatSgtaSaNtgBatagttcHBgcBBaaattaHgtDattatDaKaatBaaYaaMa\\n\\\n\
ataaataKtttYtagtBgMatNcatgtttgaNagtgttgtgKaNaSagtttgaSMaYBca\\n\\\n\
aaacDStagttVacaaaaactaaWttBaagtctgtgcgtMgtaattctcctacctcaNtt\\n\\\n\
taaccaaaaVtBcacataacaccccBcWMtatVtggaatgaWtcaaWaaaaaaaaWtDta\\n\\\n\
atatRcctDWtcctaccMtVVatKttaWaaKaaatataaagScHBagaggBaSMtaWaVt\\n\\\n\
atattactSaaaKNaactatNatccttgaYctattcaaaVgatttYHcRagattttaSat\\n\\\n\
aggttattcVtaaagaKgtattattKtRttNcggcRgtgtgtWYtaacHgKatKgatYta\\n\\\n\
cYagDtWcHBDctctgRaYKaYagcactKcacSaRtBttttBHKcMtNtcBatttatttt\\n\\\n\
tgSatVgaaagaWtcDtagDatatgMacaacRgatatatgtttgtKtNRaatatNatgYc\\n\\\n\
aHtgHataacKtgagtagtaacYttaNccaaatHcacaacaVDtagtaYtccagcattNt\\n\\\n\
acKtBtactaaagaBatVtKaaHBctgStgtBgtatgaSNtgDataaccctgtagcaBgt\\n\\\n\
gatcttaDataStgaMaccaSBBgWagtacKcgattgaDgNNaaaacacagtSatBacKD\\n\\\n\
gcgtataBKcatacactaSaatYtYcDaactHttcatRtttaatcaattataRtttgtaa\\n\\\n\
gMcgNttcatcBtYBagtNWNMtSHcattcRctttttRWgaKacKttgggagBcgttcgc\\n\\\n\
MaWHtaatactgtctctatttataVgtttaBScttttaBMaNaatMacactYtBMggtHa\\n\\\n\
cMagtaRtctgcatttaHtcaaaatttgagKtgNtactBacaHtcgtatttctMaSRagc\\n\\\n\
agttaatgtNtaaattgagagWcKtaNttagVtacgatttgaatttcgRtgtWcVatcgt\\n\\\n\
taaDVctgtttBWgaccagaaagtcSgtVtatagaBccttttcctaaattgHtatcggRa\\n\\\n\
ttttcaaggcYSKaagWaWtRactaaaacccBatMtttBaatYtaagaactSttcgaaSc\\n\\\n\
aatagtattgaccaagtgttttctaacatgtttNVaatcaaagagaaaNattaaRtttta\\n\\\n\
VaaaccgcaggNMtatattVctcaagaggaacgBgtttaacaagttcKcYaatatactaa\\n\\\n\
ccBaaaSggttcNtattctagttRtBacgScVctcaatttaatYtaaaaaaatgSaatga\\n\\\n\
tagaMBRatgRcMcgttgaWHtcaVYgaatYtaatctttYttatRaWtctgBtDcgatNa\\n\\\n\
tcKaBaDgatgtaNatWKctccgatattaacattNaaacDatgBgttctgtDtaaaMggt\\n\\\n\
gaBaSHataacgccSctaBtttaRBtcNHcDatcDcctagagtcRtaBgWttDRVHagat\\n\\\n\
tYatgtatcWtaHtttYcattWtaaagtctNgtStggRNcgcggagSSaaagaaaatYcH\\n\\\n\
DtcgctttaatgYcKBVSgtattRaYBaDaaatBgtatgaHtaaRaRgcaSWNtagatHa\\n\\\n\
acttNctBtcaccatctMcatattccaSatttgcgaDagDgtatYtaaaVDtaagtttWV\\n\\\n\
aagtagYatRttaagDcNgacKBcScagHtattatcDaDactaaaaaYgHttBcgaDttg\\n\\\n\
gataaaKSRcBMaBcgaBSttcWtgNBatRaccgattcatttataacggHVtaattcaca\\n\\\n\
agagVttaaRaatVVRKcgWtVgacctgDgYaaHaWtctttcacMagggatVgactagMa\\n\\\n\
aataKaaNWagKatagNaaWtaaaatttgaattttatttgctaaVgaHatBatcaaBWcB\\n\\\n\
gttcMatcgBaaNgttcgSNaggSaRtttgHtRtattaNttcDcatSaVttttcgaaaaa\\n\\\n\
ttgHatctaRaggSaNatMDaaatDcacgattttagaHgHaWtYgattaatHNSttatMS\\n\\\n\
gggNtcKtYatRggtttgtMWVtttaYtagcagBagHaYagttatatggtBacYcattaR\\n\\\n\
SataBatMtttaaatctHcaaaSaaaagttNSaaWcWRccRtKaagtBWtcaaattSttM\\n\\\n\
tattggaaaccttaacgttBtWatttatatWcDaatagattcctScacctaagggRaaYt\\n\\\n\
aNaatgVtBcttaaBaacaMVaaattatStYgRcctgtactatcMcVKatttcgSgatRH\\n\\\n\
MaaaHtagtaaHtVgcaaataatatcgKKtgccaatBNgaaWcVttgagttaKatagttc\\n\\\n\
aggKDatDtattgaKaVcaKtaataDataataHSaHcattagttaatRVYcNaHtaRcaa\\n\\\n\
ggtNHcgtcaaccaBaaagYtHWaaaRcKgaYaaDttgcWYtataRgaatatgtYtgcKt\\n\\\n\
aNttWacatYHctRaDtYtattcBttttatcSataYaYgttWaRagcacHMgtttHtYtt\\n\\\n\
YaatcggtatStttcgtRSattaaDaKMaatatactaNBaWgctacacYtgaYVgtgHta\\n\\\n\
aaRaaRgHtagtWattataaaSDaaWtgMattatcgaaaagtaYRSaWtSgNtBgagcRY\\n\\\n\
aMDtactaacttaWgtatctagacaagNtattHggataatYttYatcataDcgHgttBtt\\n\\\n\
ctttVttgccgaaWtaaaacgKgtatctaaaaaNtccDtaDatBMaMggaatNKtatBaa\\n\\\n\
atVtccRaHtaSacataHattgtttKVYattcataVaattWtcgtgMttcttKtgtctaa\\n\\\n\
cVtatctatatBRataactcgKatStatattcatHHRttKtccaacgtgggtgRgtgaMt\\n\\\n\
attattggctatcgtgacMtRcBDtcttgtactaatRHttttaagatcgVMDStattatY\\n\\\n\
BtttDttgtBtNttgRcMtYtgBacHaWaBaatDKctaagtgaaactaatgRaaKgatcc\\n\\\n\
aagNaaaatattaggWNtaagtatacttttKcgtcggSYtcttgRctataYcttatataa\\n\\\n\
agtatattaatttataVaacacaDHatctatttttKYVatHRactttaBHccaWagtact\\n\\\n\
BtcacgaVgcgttRtttttttSVgtSagtBaaattctgaHgactcttgMcattttagVta\\n\\\n\
agaattHctHtcaDaaNtaacRggWatagttcgtSttgaDatcNgNagctagDgatcNtt\\n\\\n\
KgttgtaDtctttRaaYStRatDtgMggactSttaDtagSaVtBDttgtDgccatcacaM\\n\\\n\
attaaaMtNacaVcgSWcVaaDatcaHaatgaattaMtatccVtctBtaattgtWattat\\n\\\n\
BRcWcaatgNNtactWYtDaKttaaatcactcagtRaaRgatggtKgcgccaaHgaggat\\n\\\n\
StattYcaNMtcaBttacttatgagDaNtaMgaaWtgtttcttctaHtMNgttatctaWW\\n\\\n\
atMtBtaaatagDVatgtBYtatcggcttaagacMRtaHScgatatYgRDtcattatSDa\\n\\\n\
HggaaataNgaWSRRaaaBaatagBattaDctttgHWNttacaataaaaaaatacggttt\\n\\\n\
gHgVtaHtWMttNtBtctagtMcgKMgHgYtataHaNagWtcaacYattaataYRgtaWK\\n\\\n\
gaBctataaccgatttaHaNBRaRaMtccggtNgacMtctcatttgcaattcWgMactta\\n\\\n\
caaDaaNtactWatVtttagccttMaatcagVaagtctVaaDaBtattaattaYtNaYtg\\n\\\n\
gattaKtaKctYaMtattYgatattataatKtVgDcttatatNBtcgttgtStttttMag\\n\\\n\
aggttaHYSttcKgtcKtDNtataagttataagSgttatDtRttattgttttSNggRtca\\n\\\n\
aKMNatgaatattgtBWtaMacctgggYgaSgaagYataagattacgagaatBtggtRcV\\n\\\n\
HtgYggaDgaYaKagWagctatagacgaaHgtWaNgacttHRatVaWacKYtgRVNgVcS\\n\\\n\
gRWctacatcKSactctgWYtBggtataagcttNRttVtgRcaWaaatDMatYattaact\\n\\\n\
ttcgaagRatSctgccttgcRKaccHtttSNVagtagHagBagttagaccaRtataBcca\\n\\\n\
taatSHatRtcHagacBWatagcaMtacaRtgtgaaBatctKRtScttccaNaatcNgta\\n\\\n\
atatWtcaMgactctBtWtaaNactHaaaaRctcgcatggctMcaaNtcagaaaaacaca\\n\\\n\
gtggggWttRttagtaagaVctVMtcgaatcttcMaaaHcaHBttcgattatgtcaDagc\\n\\\n\
YRtBtYcgacMgtDcagcgaNgttaataatagcagKYYtcgtaBtYctMaRtaRtDagaa\\n\\\n\
aacacatgYaBttgattattcgaaNttBctSataaMataWRgaHtttccgtDgaYtatgg\\n\\\n\
tDgHKgMtatttVtMtVagttaRatMattRagataaccctKctMtSttgaHagtcStcta\\n\\\n\
tttccSagatgttccacgaggYNttHRacgattcDatatDcataaaatBBttatcgaHtN\\n\\\n\
HaaatatDNaggctgaNcaaggagttBttMgRagVatBcRtaWgatgBtSgaKtcgHttt\\n\\\n\
gaatcaaDaHttcSBgHcagtVaaSttDcagccgttNBtgttHagYtattctttRWaaVt\\n\\\n\
SttcatatKaaRaaaNacaVtVctMtSDtDtRHRcgtaatgctcttaaatSacacaatcg\\n\\\n\
HattcaWcttaaaatHaaatcNctWttaNMcMtaKctVtcctaagYgatgatcYaaaRac\\n\\\n\
tctaRDaYagtaacgtDgaggaaatctcaaacatcaScttcKttNtaccatNtaNataca\\n\\\n\
tttHaaDHgcaDatMWaaBttcRggctMaagctVYcacgatcaDttatYtaatcKatWat\\n\\\n\
caatVYtNagatttgattgaYttttYgacttVtcKaRagaaaHVgDtaMatKYagagttN\\n\\\n\
atWttaccNtYtcDWgSatgaRgtMatgKtcgacaagWtacttaagtcgKtgatccttNc\\n\\\n\
ttatagMatHVggtagcgHctatagccctYttggtaattKNaacgaaYatatVctaataM\\n\\\n\
aaaYtgVtcKaYtaataacagaatHcacVagatYWHttagaaSMaatWtYtgtaaagNaa\\n\\\n\
acaVgaWtcacNWgataNttcaSagctMDaRttgNactaccgataMaaatgtttattDtc\\n\\\n\
aagacgctDHYYatggttcaagccNctccttcMctttagacBtaaWtaWVHggaaaaNat\\n\\\n\
ttaDtDtgctaaHHtMtatNtMtagtcatttgcaaaRatacagRHtatDNtgtDgaatVg\\n\\\n\
tVNtcaaatYBMaaaagcaKgtgatgatMgWWMaHttttMgMagatDtataaattaacca\\n\\\n\
actMtacataaattgRataatacgBtKtaataattRgtatDagDtcRDacctatRcagag\\n\\\n\
cSHatNtcaScNtttggacNtaaggaccgtgKNttgttNcttgaaRgYgRtNtcagttBc\\n\\\n\
ttttcHtKtgcttYaaNgYagtaaatgaatggWaMattBHtatctatSgtcYtgcHtaat\\n\\\n\
tHgaaMtHcagaaSatggtatgccaHBtYtcNattWtgtNgctttaggtttgtWatNtgH\\n\\\n\
tgcDttactttttttgcNtactKtWRaVcttcatagtgSNKaNccgaataaBttataata\\n\\\n\
YtSagctttaaatSttggctaaKSaatRccgWHgagDttaaatcatgagMtcgagtVtaD\\n\\\n\
ggaBtatttgDacataaacgtagYRagBWtgDStKDgatgaagttcattatttaKWcata\\n\\\n\
aatWRgatataRgttRacaaNKttNtKagaaYaStaactScattattaacgatttaaatg\\n\\\n\
DtaattagatHgaYataaactatggggatVHtgccgtNgatNYcaStRtagaccacWcaM\\n\\\n\
tatRagHgVactYtWHtcttcatgatWgagaKggagtatgaWtDtVtNaNtcgYYgtaaa\\n\\\n\
ctttaDtBactagtaDctatagtaatatttatatataacgHaaaRagKattSagttYtSt\\n\\\n\
>THREE Homo sapiens frequency\\n\\\n\
agagagacgatgaaaattaatcgtcaatacgctggcgaacactgagggggacccaatgct\\n\\\n\
cttctcggtctaaaaaggaatgtgtcagaaattggtcagttcaaaagtagaccggatctt\\n\\\n\
tgcggagaacaattcacggaacgtagcgttgggaaatatcctttctaccacacatcggat\\n\\\n\
tttcgccctctcccattatttattgtgttctcacatagaattattgtttagacatccctc\\n\\\n\
gttgtatggagagttgcccgagcgtaaaggcataatccatataccgccgggtgagtgacc\\n\\\n\
tgaaattgtttttagttgggatttcgctatggattagcttacacgaagagattctaatgg\\n\\\n\
tactataggataattataatgctgcgtggcgcagtacaccgttacaaacgtcgttcgcat\\n\\\n\
atgtggctaacacggtgaaaatacctacatcgtatttgcaatttcggtcgtttcatagag\\n\\\n\
cgcattgaattactcaaaaattatatatgttgattatttgattagactgcgtggaaagaa\\n\\\n\
ggggtactcaagccatttgtaaaagctgcatctcgcttaagtttgagagcttacattagt\\n\\\n\
ctatttcagtcttctaggaaatgtctgtgtgagtggttgtcgtccataggtcactggcat\\n\\\n\
atgcgattcatgacatgctaaactaagaaagtagattactattaccggcatgcctaatgc\\n\\\n\
gattgcactgctatgaaggtgcggacgtcgcgcccatgtagccctgataataccaatact\\n\\\n\
tacatttggtcagcaattctgacattatacctagcacccataaatttactcagacttgag\\n\\\n\
gacaggctcttggagtcgatcttctgtttgtatgcatgtgatcatatagatgaataagcg\\n\\\n\
atgcgactagttagggcatagtatagatctgtgtatacagttcagctgaacgtccgcgag\\n\\\n\
tggaagtacagctgagatctatcctaaaatgcaaccatatcgttcacacatgatatgaac\\n\\\n\
ccagggggaaacattgagttcagttaaattggcagcgaatcccccaagaagaaggcggag\\n\\\n\
tgacgttgaacgggcttatggtttttcagtacttcctccgtataagttgagcgaaatgta\\n\\\n\
aacagaataatcgttgtgttaacaacattaaaatcgcggaatatgatgagaatacacagt\\n\\\n\
gtgagcatttcacttgtaaaatatctttggtagaacttactttgctttaaatatgttaaa\\n\\\n\
ccgatctaataatctacaaaacggtagattttgcctagcacattgcgtccttctctattc\\n\\\n\
agatagaggcaatactcagaaggttttatccaaagcactgtgttgactaacctaagtttt\\n\\\n\
agtctaataatcatgattgattataggtgccgtggactacatgactcgtccacaaataat\\n\\\n\
acttagcagatcagcaattggccaagcacccgacttttatttaatggttgtgcaatagtc\\n\\\n\
cagattcgtattcgggactctttcaaataatagtttcctggcatctaagtaagaaaagct\\n\\\n\
cataaggaagcgatattatgacacgctcttccgccgctgttttgaaacttgagtattgct\\n\\\n\
cgtccgaaattgagggtcacttcaaaatttactgagaagacgaagatcgactaaagttaa\\n\\\n\
aatgctagtccacagttggtcaagttgaattcatccacgagttatatagctattttaatt\\n\\\n\
tatagtcgagtgtacaaaaaacatccacaataagatttatcttagaataacaacccccgt\\n\\\n\
atcatcgaaatcctccgttatggcctgactcctcgagcttatagcatttgtgctggcgct\\n\\\n\
cttgccaggaacttgctcgcgaggtggtgacgagtgagatgatcagtttcattatgatga\\n\\\n\
tacgattttatcgcgactagttaatcatcatagcaagtaaaatttgaattatgtcattat\\n\\\n\
catgctccattaacaggttatttaattgatactgacgaaattttttcacaatgggttttc\\n\\\n\
tagaatttaatatcagtaattgaagccttcataggggtcctactagtatcctacacgacg\\n\\\n\
caggtccgcagtatcctggagggacgtgttactgattaaaagggtcaaaggaatgaaggc\\n\\\n\
tcacaatgttacctgcttcaccatagtgagccgatgagttttacattagtactaaatccc\\n\\\n\
aaatcatactttacgatgaggcttgctagcgctaaagagaatacatacaccaccacatag\\n\\\n\
aattgttagcgatgatatcaaatagactcctggaagtgtcagggggaaactgttcaatat\\n\\\n\
ttcgtccacaggactgaccaggcatggaaaagactgacgttggaaactataccatctcac\\n\\\n\
gcccgacgcttcactaattgatgatccaaaaaatatagcccggattcctgattagcaaag\\n\\\n\
ggttcacagagaaagatattatcgacgtatatcccaaaaaacagacgtaatgtgcatctt\\n\\\n\
cgaatcgggatgaatacttgtatcataaaaatgtgacctctagtatacaggttaatgtta\\n\\\n\
gtgatacacaatactcgtgggccatgggttctcaaataaaatgtaatattgcgtcgatca\\n\\\n\
ctcacccacgtatttggtctaattatgttttatttagtgacaatccaatagataaccggt\\n\\\n\
cctattaagggctatatttttagcgaccacgcgtttaaacaaaggattgtatgtagatgg\\n\\\n\
taccagtttaattgccagtgggcaatcctaagcaaaatgagattctatcctaaagtttgg\\n\\\n\
gcttgatataagatttcggatgtatgggttttataatcgttggagagctcaatcatgagc\\n\\\n\
taatacatggatttcgctacctcaccgagagaccttgcatgaagaattctaaccaaaagt\\n\\\n\
ttaataggccggattggattgagttaattaagaccttgttcagtcatagtaaaaaccctt\\n\\\n\
aaattttaccgattgacaaagtgagcagtcgcaataccctatgcgaaacgcctcgatagt\\n\\\n\
gactaggtatacaaggtttttgagttcctttgaaatagttaactaatttaaaattaatta\\n\\\n\
acgacatggaaatcacagaacctaatgctttgtaggagttatttatgctgtttactgcct\\n\\\n\
ctacaaccctaataaagcagtcctaagaatgaaacgcatcttttagttcagaaagtggta\\n\\\n\
tccagggtggtcaatttaataaattcaacatcgggtctcaggatattcggtcatataatt\\n\\\n\
tattaagggctcttcgagtcttactctgagtgaaattggaaacagtcatccttttcgttg\\n\\\n\
tgaggcatcttacaccgctatcgatatacaatgcattccaccgcggtgtcccgtacacaa\\n\\\n\
ggaaacttgttaccttggggatataagaaaactcacacgtctcattattaaactgagtac\\n\\\n\
aatttttgcacgagaaagtaatgcaatacaatatgatgaaagccagctaatgaaaaggga\\n\\\n\
tggaacgcacctcggatctgttgcactggattaaaatccgattatttttaaaaatattca\\n\\\n\
gtgctagagcatatcaggtctacttttttatctggtatgtaaagcccacggagcgatagt\\n\\\n\
gagatccttacgactcaacgaaaagttataacataactcccgttagccaaagcccaatcc\\n\\\n\
cgattactgccctaccctaacgtctgccatctaaatatcgaacttgttatgatcaatgtg\\n\\\n\
actacctcccaccctttccccttcatttgttccactggggataagctagcgttttcagaa\\n\\\n\
tcaatgcaataagaatagccaattgtctcacttcatcagagctcttggcaattccaggcg\\n\\\n\
ctacgtggttctggaatatattcatttttcaaatagtaatacgtttagtgttgctattgt\\n\\\n\
ctacacgtttggatattacgttatgtgagcggacatcaatagttgtctaactctttagta\\n\\\n\
agccagagatagcactcttagcgaatggataccatcttccataagtttagttaatagtcc\\n\\\n\
gaaacaactgcttcgagcatatttgaacctccttgtaggcaaatagcctcttcaaagcaa\\n\\\n\
tcttactaatagatagagtttgttttaagggactactagaaatgggacaatcttaatagt\\n\\\n\
atgacctaaactgacatttaaagatatatccaggtggcaagcataaagatcattgcgcca\\n\\\n\
cctccaccgtgggattacttatcagtcgatatcctatatgctaagtttgcgacggcagaa\\n\\\n\
tacaaactaagctgagttgatgctaaccttacctatgataccccattggaccggttaaca\\n\\\n\
gccctacttattccaaataaaagaacttttatgctgtagaagctattatagtgatgcctg\\n\\\n\
gtaacttcagtatattaaaatgacacacatacgccatatagagctcctggaactttgaat\\n\\\n\
aatgagcgaacttcgaagttgaagagcaagaaaccatatgtcacggttgcctaaagcccg\\n\\\n\
gtaaccagacatgtgctatcattgatcattatcgaggttttcataaccttgacccattat\\n\\\n\
cggctgtgcgcggacaagtacttaaatcactagtttcttcacctgcttatcggtaagaaa\\n\\\n\
taaggttggcaaagaatcgcataagacggacgtagagccgcagcgttgtgcgagtccagg\\n\\\n\
tgcatgcgcagcaataggattttaaattttgttccatttttaatttagccgtaaggatgt\\n\\\n\
ccgtaaatgattgaaaattggattcaatctttgggcctatgctactggaacctgatcgac\\n\\\n\
aaaatttcaaacatacgttaactccgaaagaccgtatttttgcggctagaatagtcagtc\\n\\\n\
gcttggagccatataccttaccacttaaacgacgtgctcctgtagttgaaatataaacag\\n\\\n\
aacacaaagactaccgatcatatcaactgaagatctttgtaactttgaggcgaagcaccc\\n\\\n\
tcttcgagacaactaagagtaaagtaccgggcgccgcaaggagtcgattgggaccctaaa\\n\\\n\
tcttgacgaattgctaagaggctcagagctaccactgtaatttctctagagcccataata\\n\\\n\
aatgaacgatacatccgtaggtagcacctaagggattataatggaagccaaatgcagtta\\n\\\n\
ataatattatatactggcgtacacgattcgacggatctctcacatagtgattcacgaccc\\n\\\n\
ccccctttgattgacacagcgtcagcattttgcaagaacgatcttctgcatagggtgcgc\\n\\\n\
caccgtaaggatgacgtcgaagctacaactgggtataatttaccatgcttccctgatgct\\n\\\n\
gagtgcaatacactaagaatgagtttttaccccatatcaccagtatttgttctgttattg\\n\\\n\
cgaagaaatggctatgctgagttggcgactaaagtcacccatcctttttattaggtaacc\\n\\\n\
ccctcccttaaactaactgatttgctggagctgccctgcatacatatactttatcattta\\n\\\n\
tggacgtccgtgacgcttattatccaccatagtcgatatgctacacggattcattaatgg\\n\\\n\
atcgtaggagtttaagttatatttactaagatcggtctcggctactatcccgccttaccc\\n\\\n\
ggcgctatttacggccatttttaatatattgacggtaattattcctatggtttcgaccgc\\n\\\n\
acgtccttggacaagaaagaatggcaaaaaaaatgtaaaagaaaaaaaatattgagtccc\\n\\\n\
taccatcatataaaaaatatgtgatgagtaacttgacgaaatgttagtggttattaaaga\\n\\\n\
ctatctattacaccttttgttttctgtcgtagtatattaaagtctagaagccttacagga\\n\\\n\
aaatcagggttatacagccgatactccgcagcatgaatcatcgaggaggtgtcctaccat\\n\\\n\
cgcgccttgtaatcttgtctgtgtatactgtatttagaccttttatacaaagtaaatatc\\n\\\n\
tcggctttatgtgattgggaggggcctactcaaacatgatgacttgacctaataatcact\\n\\\n\
gtgcgggcgtcttatgactagctattccttgaaatccaccaccaaatggttaatatgtaa\\n\\\n\
aaactttgacgatgaaacaaggtgaatgtgtagttactttgtgtaattagctgcgtcgag\\n\\\n\
cattgcttgtaaaaccgtcaatcgcacacgttacttccataaaatttctacgaatacacc\\n\\\n\
cttcttaaaaaaaacgtaggaattcacgagtttaacaaacgataactgtataaagtggaa\\n\\\n\
gtccgaagaaagcagatgcccgaactactcgaagatgtttcgttttcttaaccatagggg\\n\\\n\
cttcttaatggcccactacgcacattttgttcaagcccgagagggacatccccattacgg\\n\\\n\
gagtattactaaaactgttccgtaatacgttcagcaagggatgaaaaaggccactgctca\\n\\\n\
agttattgacgtgggagtattacatcggaagcctgaatcccacactatgatggtctgtac\\n\\\n\
aggcctagggactgcgtctagacggtattaccggcttctaatcatacgatcgtgagtctt\\n\\\n\
aacgggaagtaaggctcacacctaccccaaaccatttatctatgtaagtataaaattgtg\\n\\\n\
cgtaagtgttcaaagtggacaataaagacgtggcaaaaacccccgcacataagccgcttt\\n\\\n\
agatttcacaaataccaatgcggttaaaaacatccttgagtcgtacatacaccatactcg\\n\\\n\
cgttaaacggatataacagaagataataaatccggatgtggagtcggtgtaactatagaa\\n\\\n\
agccaagtgaaataatgcttaccagtcatttagctatacggctttcatttcatgtcaaga\\n\\\n\
gggtggagtttgacctgtacagttgatatatcaccgatacttagaactcacctaaagcta\\n\\\n\
aaattgctcgcagcgtgtaatccgcatattacaaacaatagatgggattcattatacata\\n\\\n\
agacacgatgatctgctttttcaggttgcgagatgttgcctatcgtcaatcgagtcctgc\\n\\\n\
cttacaccacttaaacaaaagtattgacagggaacctattttcgaggtattatatagtcc\\n\\\n\
agcttgaatatcaatttgacagttaacctagtgaaaatcagtaagaggaaatacgccaca\\n\\\n\
ttctccagtgaaattctacgggttatcgtctagtccaactatcaattataactcacgaga\\n\\\n\
tataagtaaattctcgtacttggcctgatttttattatactttggatccttagtaaacag\\n\\\n\
gaagggagaaaccttcaacgaaaaacactggattttgttttactctcaaagctcttatat\\n\\\n\
gacggaaataccctgtcaagtcttaactttattactagactaatgaaatgggcttggggt\\n\\\n\
ggccagaatcatagtacaatttagcggatacactattcggactttcctatcggctgtctg\\n\\\n\
gttggataagtatggggactaataggctagacatacctatacttaaactatacaggcgtc\\n\\\n\
atctatctctgcaactttggagttccctgatgttctcccgccctttgggttcacatcttc\\n\\\n\
tataccgacacccctaataacgattagtttgtgggttagagtaaattaatacggttaata\\n\\\n\
ttaatgtatcgttgaaaagctggtgtcgccaataaggtaaccggctaggcagagtatatg\\n\\\n\
tcacgaagtataactaccctaatgataagctgtaggaataaaattaatgctgtctctaag\\n\\\n\
cgaagagatatttccgactctgttttaatgacgaatctcattacttctgacttgcaaatg\\n\\\n\
ttcaatatggcacggtttcacggcacctttgtgacgcatataatgaacttagaagattat\\n\\\n\
aacgacggaactttatatgataatccgttacgattaaagaatctgttaaatatcataatg\\n\\\n\
gcattcagttctagaccgtgcatcatggtaaacttactttctctgcatggcgacatacat\\n\\\n\
ttcgctattcaaattcgcgtgtggttacacccactcgcacctttggaatattaagagaag\\n\\\n\
atgatcagaaaatccattcgctcaatttttctgacgtacgtctaatttatcctaggagac\\n\\\n\
aaatcgttttatgtctctcacatttttgaagaaaggttcgagagacaatactcaggtcct\\n\\\n\
gaactgctagaagatactcggtggagcgtggcaacaatgaaaaactcgtgacataaatga\\n\\\n\
atgatacttttccaagttcagttaagtgaatatgtttaacatacccggcttttcgatctt\\n\\\n\
aagctgacgctggacgtgcgagtaatgtcagtctcttacatacactagtgactccaagtt\\n\\\n\
tcgtcaaaaacgccccctcccttctcgagcccactcacgctatgtattgacgcgaacttg\\n\\\n\
ttcgggatcagacttttcaggagttcggtcgcgtgtccctatgtgctaatatataagtta\\n\\\n\
gatcgcattagatgctaatctgaatacttatagacgaccttcaacgagaacgggtaccac\\n\\\n\
cttgaggctagagttaggtgtgaaacgacaggtagggacatataaaatttgagtgcggct\\n\\\n\
ttagttaagggtttaattacctactcaaacatcacgctcgcgcccttcgtacgtaatcga\\n\\\n\
ccatctagaggctaaggggactgtactaggtagtgattaatgatatcctagacgcacgtg\\n\\\n\
ccttagatcttcagactctgatggtccgcgatcaccgtaattgtagtcctccaactcgat\\n\\\n\
cactttgttggcgtcaaagaaattacgatatctaaatacttataatacaataaccaagga\\n\\\n\
tgagaatgactcatcgcgttggagttatattgcttgaagttctatggaatgaaagcacgt\\n\\\n\
tatctgccgtcccaatatctccagtgagctaattcattggacggtccactttgatcaatc\\n\\\n\
cccgaggagatgttcggacactttagtctgtaacacttagcgttgagaccacgaacaatt\\n\\\n\
gattactcagtcttgaaggtgttttccaaagttcattttaaataagactacgataggcct\\n\\\n\
ttcctattgatataaactacccggctctgttgttcgtgtgagtcgtacttctctgtgttt\\n\\\n\
ttctgattatagcaagattcgattcttagtgtaaacagcgatttttatttgacccgtcaa\\n\\\n\
tgagaagcgcataggatctaagcaaaattatcaagttgtgccacaaggtaagatctttcc\\n\\\n\
agttattgcaggtaggatgtatcccacgttgatagtatgaggtctgacgtcaactgtcta\\n\\\n\
ggagagttgaccgcgtgcgggtacaccggatttgcatcgatgttgagaacgcagaactcc\\n\\\n\
cactgtcgtggcggcgttcctgatatttagcaagaggcgttgataaagccctcatcatct\\n\\\n\
agatctcgacctcatctgccctcttgctccatcattttctacacagactactttcctatc\\n\\\n\
tacgttagtataattgctttctatcttagtatcatttagagcttctccgtcaacaggttc\\n\\\n\
gtgctattaaagttagtacgaaagggacaacttgtagcaacgcatttaatcggttttcga\\n\\\n\
ctacttcgcacaaaatcagataaagaagtttgtcattctattagacattgaattgcgcaa\\n\\\n\
ttgacttgtaccacttatgatcgaacactgaatcaagactgtgattaactaaaatagaca\\n\\\n\
agccactatatcaactaataaaaacgcccctggtggtcgaacatagttgactacaggata\\n\\\n\
attaattggactggagccattacattctctacaatcgtatcacttcccaagtagacaact\\n\\\n\
ttgaccttgtagtttcatgtacaaaaaaatgctttcgcaggagcacattggtagttcaat\\n\\\n\
agtttcatgggaacctcttgagccgtcttctgtgggtgtgttcggatagtaggtactgat\\n\\\n\
aaagtcgtgtcgctttcgatgagagggaattcaccggaaaacaccttggttaacaggata\\n\\\n\
gtctatgtaaacttcgagacatgtttaagagttaccagcttaatccacggtgctctacta\\n\\\n\
gtatcatcagctgtcttgcctcgcctagaaatatgcattctatcgttatcctatcaacgg\\n\\\n\
ttgccgtactgagcagccttattgtggaagagtaatatataaatgtagtcttgtctttac\\n\\\n\
gaagcagacgtaagtaataatgacttggaataccaaaactaaacatagtggattatcata\\n\\\n\
ctcaagaactctccagataaataacagtttttacgatacgtcaccaatgagcttaaagat\\n\\\n\
taggatcctcaaaactgatacaaacgctaattcatttgttattggatccagtatcagtta\\n\\\n\
aactgaatggagtgaagattgtagaatgttgttctggcctcgcatggggtctaggtgata\\n\\\n\
tacaatttctcatacttacacggtagtggaaatctgattctagcttcgtagctgactata\\n\\\n\
ctcaaggaaccactgctcaaggtaggagactagttccgaccctacagtcaaagtggccga\\n\\\n\
agcttaaactatagactagttgttaaatgctgatttcaagatatcatctatatacagttt\\n\\\n\
ggacaattatgtgtgcgaaactaaaattcatgctattcagatggatttcacttatgcctt\\n\\\n\
agaaacagatattgcccgagctcaatcaacagttttagccggaaacaatcgaagcatagg\\n\\\n\
gacaatgtatcttttcctaaattgccatgtgcagatttctgagtgtcacgaagcgcataa\\n\\\n\
tagaatcttgtgttgcctcaactcgttgaaaagtttaaaacaatcgcagcagtctttttg\\n\\\n\
gggtctactgtgtgtttgcaaaataactgaaagaaacgcttgaacaactctgaagtagct\\n\\\n\
cgagtactcattaaagtgtaacacattagtgaatatcggccaatgaaccaaacgcttccc\\n\\\n\
ggtacgctatctctctcatcgggaggcgatgtgcaggttatctacgaaagcatcccttta\\n\\\n\
cgttgagagtgtcgatgcatgaacctcattgtaacaatagcccagcaaattctcatacgt\\n\\\n\
gcctcagggtccgggcgtactcctccatggaagggcgcgcatctagtgttataccaactc\\n\\\n\
gctttttaactactatgctgtagttctacaggcatagtggccagtattttctaacttctc\\n\\\n\
tggatagatgctctcactcctcatccatcacggcttcagtttacgtcttacttgcttgtt\\n\\\n\
cagcaacggatggaggcattaagtatcttcactgttccctaaaattgctgttcaatatca\\n\\\n\
aagtaaggacgatacagggaaagctcaagcacactcattgaatactgccccagttgcaac\\n\\\n\
ctcacttaatctgacaaaaataatgactactctaagtgttgcggaagcagtctcttccac\\n\\\n\
gagcttgtctgtatcacttcgtataggcatgtaactcgatagacacgaacaccgagtgag\\n\\\n\
aaactatattcttgcttccgtgtgtgtgacaccaggtaattgatgcggatataagctgga\\n\\\n\
gatcactcacgcccacacaaggcgctgctacctctttattccaatgtgtaagaatttgct\\n\\\n\
aacttcatttctagaccgcagctttgcggtcataatttcacggtacggacccttgggtta\\n\\\n\
gagacttgataacacacttcgcagtttccaccgcgcacatgttttagtggcttctaacat\\n\\\n\
agaatttttgttgtgacataaagagtgcgtgggagacttgcccgaccgttaagccataat\\n\\\n\
caattgaaagccccgtgagtcacatctaattggttgtactgcgcatttagctatccttta\\n\\\n\
gctgactcgaagagattcgattcctaatataggttaattagatggctgccgcgcgaagta\\n\\\n\
aaacgtgaaaaacgtagtgcgcagatctgcataactcgcgcttaattacttatgagtagt\\n\\\n\
tccaagttcgctacgttatgagagagattggaattaagcaaatatgttttatggtgattt\\n\\\n\
tgggatgagaaggactgctaagtacggctactaaacaaatttctaaaaccgccatctacc\\n\\\n\
ttatcttggagacatttaagttgtatatgtcactagtctagcttttgtctgtgggacgcg\\n\\\n\
ttctcggaatgagggaaatgcaagagccgattcatcaaatgcttatctaagaaagtagtg\\n\\\n\
gactattacaccaagcacgaatgccagggaactgctttcttgctcaggacctcgcgacaa\\n\\\n\
ggtaccccgcataagtcctagaattacatttggtcagcaatgctgacatttgaccgtgaa\\n\\\n\
aacataattttaatcagaaggcagctcacccgcttgctctagatcttatctttgtatgaa\\n\\\n\
tgtcagaatttactgcaatatccgttccgaatagtgagggcttagtatagttctctgtat\\n\\\n\
acaggtcacatcaaactccccctgtcctagtacagctctgagctttaattaattgcatac\\n\\\n\
atttccttcaatcatcagatgaaaacaccgcgaatcatgctcttctcgtatagggcaaga\\n\\\n\
gaagcaacaaacaactagcccgactcacgttcatccgccgtatccttgttcagttcttac\\n\\\n\
tccgtattaggtcagcgaaatctaatcagaataatcggtcgcgtatcaaaattaaaatcc\\n\\\n\
cgcttgaggttgacaattaaaacgctgagcagttatcggctattagatagtggggtgaaa\\n\\\n\
gtaattggctggaattatgttaaaacgtgatattaagctaaaatacgctacttgttgccg\\n\\\n\
acctaattcagtcattcgatattcagttagagccaagaataacaagcttgtataaattga\\n\\\n\
acggggtgcactaaacgatgtgttactctaatattcagcttggagtatacctgaaggcga\\n\\\n\
attcatgtatcggccaataataagacgttgaagatcacaatttggactagcaaaagaagg\\n\\\n\
tgatttatgcgtggggattgagtccactgtacgagtacggtctctggaaaattataggtt\\n\\\n\
cagggaatataaggaagtaaagataattaccaagagatttttggtatcgctatgacccag\\n\\\n\
aggtgttctaacgtctgttttgatccgcagaatttctgcctcaatgcatatttgacggac\\n\\\n\
ttgaactagagcctctaaagttaaatggcgacgcaactgttcctaaacttcaattattac\\n\\\n\
tactctttttttcctagggtattgtagaggccagtggacaaaataaatcaaatttaagat\\n\\\n\
gtttcggacattaacatcccccgtagcatagaaatcatcagttatccaatctctcatcga\\n\\\n\
gcttttacaatttctgctggcgctatggacagcatatgccgcgagacctccgcaagactc\\n\\\n\
acttgatcactgtaagtatcttcattagaggttagagcctatagttaagctgctgaccta\\n\\\n\
gtaaaattggtattttctaattttattgctcaagttaaaggttagtgaagggataatgac\\n\\\n\
gttatttttgaacaatgggttgtattcaattttatatcacgaatggaacccttcattccc\\n\\\n\
ggcataatactagacgacacgaacaagctccgatctatcagccaggcacgtgttaaggtt\\n\\\n\
taattccggcaaaccaatgaagcatcaaaaggtgacctgatgcaacttagggtcacgatg\\n\\\n\
agtttttcaggactacttattacctattaataagttaacatgagccttcataccccgtaa\\n\\\n\
gacaatacatactccaccaattagaattctgagccatcttatctttttgtatcatcgaag\\n\\\n\
ggtatggccgaataggttaattagttactcctaacgtctctacaggcatgcatttgacgc\\n\\\n\
accttcgaaaatagtcaatctctcgccacacgcgtctagtatgcagcatcaaaaatatag\\n\\\n\
tccacggtttccggattaccaaacgcggcaaagagaaacattgtatcgacggagataact\\n\\\n\
taatacagaaggaaggggcatcttcgaatacggatgaataattctatctgtttattctga\\n\\\n\
catcttgttttcaggttaatcttacgcattcaaatgacgcctgccccatgcgtgcgcaat\\n\\\n\
tattttctaatattgacgagagcaatctcactccttttgggtctatttatgttttattga\\n\\\n\
ggcacaagcctatacagaacaggtactattaaggccgtgagtgtgagactcaaaccgtgg\\n\\\n\
aaacaaaggatgggttgttcttggtacaagttttagtgcatgtgggcaatccttaccaaa\\n\\\n\
atcagatgctatccttaactttgggctgcatttaagatggcggttggaggcctgtgagaa\\n\\\n\
tcctgcgtgtcatctttaatgaccgaattcatccatgtagattcagatcacacactcatt\\n\\\n\
ccttgatgttgtctaaacaaaagttgttgtggacgcattggagggagttaagtaacaact\\n\\\n\
tgggatcgcatacttataaaaattatatgttaaactttcacaaacgctgaagtccaaagt\\n\\\n\
aactagcccaaacgcctcgagagtcactaggtattaatggtgtttgagttcctgtgaaat\\n\\\n\
agtgttcgaaggtaaaatttatgtaccaaatcgaaagaacacttaataaggcttgcttgc\\n\\\n\
acggaggtatgatgtttactgactctacaaccctaattttccagtacgtacattcattcc\\n\\\n\
aataggttagttctcaaagtgctatacaggctcctcaattgatgatatgcttcagccgct\\n\\\n\
ctatggatattagctcattttatttaggaagcccgcttagaggcttactatgagggaaat\\n\\\n\
gccaaaatgtcatacttttcggtgtgtcccatatgacaccgctttacatagaatttgaat\\n\\\n\
taaaacgcgctctcccgttcactaccatacttggtaccgtgcgcatattacatatagata\\n\\\n\
taggatcattttttaaagctgtactaggtttgatcgacaatcttatgctatactatatga\\n\\\n\
tgtaaccctcataatcaataccgatcgtacgatcctagcataggtggcaagcgattttat\\n\\\n\
gccgattattgtgttaaatagtctgtgagtgtgattatcagggctacgttggtagagggg\\n\\\n\
ttgtatagacctcgcacacattgtgacatacttaacaatatacgaaaactgatataataa\\n\\\n\
atccccttacccaaacaccaatcccgttgaatcaactaccataacgtctcccatataaat\\n\\\n\
tgcctacttgtttgcataaatctgaatacataacaccattgcaccttcttgtgttccaat\\n\\\n\
cccgttaagattgccttgtcagatgatatgcaagaacaatagcatttgctagcaattatt\\n\\\n\
aacagctcttcgaattgcctccacataacgcgggagggtatattttaatttggcaaatac\\n\\\n\
taagtactgttggcgtcatatgctattaacggttggatattaagttatgtcagccgtaag\\n\\\n\
caagagtgggcgaaatattttgttacccagtgagagcactcttagagtttggatacaata\\n\\\n\
ggccatatgttgacttaagaggacgtaactacgccgtacaccattgttcaaccgacttct\\n\\\n\
tggcaaatagaatcgtattagcaatcttaagaatagagacacgttcgtgttagggtatac\\n\\\n\
tacaaatccgaaaatcttaagaggatcacctaaactgaaatttatacatatttcaacgtg\\n\\\n\
gatagatttaacataattcagccacctccaacctgggagtaattttcagtagatttacta\\n\\\n\
gatgattagtggcccaacgcacttgactatataagatctggggatcctaacctgacctat\\n\\\n\
gagacaaaattggaaacgttaacagcccttatgtgtacaaagaaaagtaagttgttgctg\\n\\\n\
ttcaacagatgatagtcatgacgcgtaacttcactatagtaaattgaaacaaatacgcaa\\n\\\n\
tttagacagaatggtacggtcatgaatgacagtaattcgaagtgctagaccaacttaaaa\\n\\\n\
taggtaaacgtgcccgaaaccccccttaacagaaagctgctatcatggtgcagtatcgac\\n\\\n\
gtgttcagaaacttgtaacttttgagcaggtccgagcacatggaagtatatcacgtgttt\\n\\\n\
ctgaaccggcttatccctaagatatatccgtcgcaaactttcgatttagtcccacgtaga\\n\\\n\
gcccaagcgttgtgcgactccacgtgcatgcccagaaatacgagtttaaatttggttaca\\n\\\n\
tggttaattttgaccgaagcatcgcactttatgattgataattggattcaatatgtcgcc\\n\\\n\
ctatgcgaatgcaacatgatccacaatttggctataagacgtttaatccgtatcacactt\\n\\\n\
tgtttgcggctagtatagtaacgcccgtgcaccaagagtcagtaacaattataagtactc\\n\\\n\
cgcaggtacttcaaatataaaaactaatcaaacacgacccatatgatcatctgaagatat\\n\\\n\
ttggaactttctcgacaaccaccctcgtactcaatacttacactaatcgacaggcacacg\\n\\\n\
caacgtgtacagtcgcaccatattgagtcaagatttgcttagtggcgatgagcgtacacg\\n\\\n\
cttatttctctagtcacaattagttatctacgagacatcacgagggagcaaataagcgat\\n\\\n\
gttatggctacacataggcacgtatgaatatgatataagccagttaaacagtcgaaccat\\n\\\n\
cgagcaaattctcatgcaccaacccacacgttgaggcacaaagagtaagctgtttgaatg\\n\\\n\
taacttcttctgctgagcgggccccaacgtaaggatcaactagaagagaaaactcggtat\\n\\\n\
tagtttaaatgcgtcacggagcatgagtgcatttcactaagaatgtctgtgtaaccaata\\n\\\n\
taacatctatttgttatctgattgcctacttatggctttgcggtcgtggcgactaatgtc\\n\\\n\
tccaatccttttgaggtcggtaccaactccctttaaattacgctgtgcaggctcatgcac\\n\\\n\
tgcatacatatacggtagcaggtagggacctcacgcacccttattataatcaatagtagt\\n\\\n\
tatcagtcaacgaggcaggaatgctgaggtcgaggtgttggtatattttctatgtgccgt\\n\\\n\
ctaggcgactatcacgcattaccaggcgagatttaagccaattttgaatatagtcaacgt\\n\\\n\
aatttttactatgggttccaccgaaacgccttgcacaactaagaatcccataaaatatcg\\n\\\n\
atatcaaataaaagattgtgtcaataccttcatatatattttttcggttgactaacgtga\\n\\\n\
actaaggttaggggttttgtatgtctatataggaaacagtttcttttctgtcctacttta\\n\\\n\
gtaaagtcttcaagccttactccaaaatcacggtgattaagccgttactcagcagcatga\\n\\\n\
ttctgcctgctcgggtcctaaaatccagccttgtaagagtcgctgtgtattagctaggga\\n\\\n\
gacctttgttaaaaaggatatatcgcggcgggatgtgagtgcgtggcgcatactcaatct\\n\\\n\
tcagctcgtgtcattataatatctctcccccacgcttttcactagatatgccgtgtaagc\\n\\\n\
aaacaccttatgcttaatttcgaaaatattggtacttgaaaaaagctgtaggggtactta\\n\\\n\
atgtctggtaggagatcaggagagaattgagtgtaaaaccgtaaagccctcacctgactt\\n\\\n\
catgtaaatggcttagaagactccatgatttaataaatactacgaaggaaagactggatc\\n\\\n\
taaagataactctagtaaggccaactcccttcaatgctgttgccagttataatccaagag\\n\\\n\
ctgtccttttctgaaccatagcggcttctgaagcgaactagaagcaaagttggttctagc\\n\\\n\
cagacagccacataccctgtacgggtgtattactaaaactggtccggtattagttcacca\\n\\\n\
agggaggaattaggcaaaggatctaggtatgcaagtcggagtattacatccctaccctga\\n\\\n\
atccatcaataggttcctctgtactggccttcgcaatgagtattcaaggttgtacagccg\\n\\\n\
tataataataagatagtgactatgaacgggaagtaacccgctcaccttccccaaaacatt\\n\\\n\
gttatatctaagtattaaagtctgccgtagtgttaatactcgaaaataaacaactggcaa\\n\\\n\
attacaccgcacttaagccgcttttgatttatatttttccaatgcgcttttaaaaataat\\n\\\n\
tcagtcctacatactaattaagacccttaaacggagatatcacaagttaagttttaacca\\n\\\n\
tctcgactaggtggaactatagatacccaactcaatttatcattacctgtaatgttccta\\n\\\n\
gaaggattgcatttcatgtcaagacggtggagtttcacagcgaaacttcagtgtgaacag\\n\\\n\
attctgagaaatcacctaaacctattagtcagagcacccggttagaaccagttgtcaaaa\\n\\\n\
aatagagcggttgcatgagacagaagtaacgatgagatccgttgtaacgttgagacatct\\n\\\n\
ggcctatcgtcaatacagtcctcccttaaaaatatttttaaatactaggcaaacccaaca\\n\\\n\
taggttagtcctatgtgatacgccacatggtatatcattttgtaacgttacctagggata\\n\\\n\
atcaggaagtggaattacgcaaaagtagacagtgaaatgcttagggttatagtctagtcc\\n\\\n\
aaagataaaggataaagcacgtcagagaactatattagccgaatgggaatcattgttagg\\n\\\n\
agactgtggatcatgtctaaaaagcaacgcagaaacagtcatcgaaaaaatctcgttttt\\n\\\n\
gtttgaatctaaaagagctttgatgaccgatagtacctgtatactagttactgtattacg\\n\\\n\
tgtctaatgatttcggattggggtccccagaatcagacgtcattgtagacgattcaagtt\\n\\\n\
taccaatttaatttcccagctctccttggagaactatcgccaataattgcagtcactttc\\n\\\n\
cttttctgaaacgataaagccgtcagagttctctgcaacgttggacttacctgaggttct\\n\\\n\
aacccactttcggttctaatagtagttaacgacacaacgaataacctttactgtggggct\\n\\\n\
ttcacgatattttttcgcttattattaatggttacgtcataagctggtgtccaaattaag\\n\\\n\
gttaccggcttcgcagagtagttgtatccaagtataacttccctaatcataagatcgagg\\n\\\n\
tagaaaattaatgctgtctctaaccgaacagatatgtcccactatgtggtatggacgttg\\n\\\n\
ctaattacttctgaagggaaattggtcattatggatacgtgtctaccatcaggtcggacg\\n\\\n\
cagatatggttctgtcttcagttgatccaccgttctttataggataataactgacgatta\\n\\\n\
aagattatggtaaatagattaagccaattctcttcttgtcagtgaagcatccttaactga\\n\\\n\
cttgctctgcagcccctcatacatttagctattcaaagtaccggctcgtttcaaactctc\\n\\\n\
ccacctttggaagaggttgtcaacttgataagtatatcatttacagcattttttcggacg\\n\\\n\
tacctctaatgtttcattgcagaaaattagttttttctatcgcacattttgcaagtaacg\\n\\\n\
ttagagacacaattatctgcgaatgaactgctagatctgacgaccgggagcctcgcaaat\\n\\\n\
atcaaaaaagactgacatatatcaaggagtcgttgacaagtgctggtaagtcaattggtt\\n\\\n\
tatctgtcccggcgtttcgatcttaagctgaccatgcacggcagagtaatgtcactctcg\\n\\\n\
ttcttacaagtctgtctccaagggtcggcaaaaaagacccctccattctcgagcccactc\\n\\\n\
acgatatgtagggacgacaacttgtgcggcttatgaattgtctggactgcgggcgagggt\\n\\\n\
ccatatctccgaagttagaagggacatacctttagatgataagatcaattcttattgacg\\n\\\n\
aaattcatccacaacggggaacaacttcaccctagacttacgtctgaaaagacacctagc\\n\\\n\
gtcttataaaaggtcagtgccccgtttcgtaaggctggaattacctacgcaaacttaaac\\n\\\n\
ctcgcgcccttccttacgtatcgacaagatagaggctatcgcgaatgtactacggaggca\\n\\\n\
tgaatcatatactagaaccaagtgcctgtgatattaacaagatgatccgacgcgagcacc\\n\\\n\
gtaattctaggcataaaactccagcaatttgggggccgaaaacaaatgacgttagctaat\\n\\\n\
taattatatgacatgatcaaaggaggtcaatcacgcatcgagttcgacgtatattcattg\\n\\\n\
aacttcgtgcgtttgaaagaaacttttatgaaggcaaaattgatcctgtctcctatttca\\n\\\n\
tgcgtacctcctagttgataattccccgagcagtggttaggacacttttgtcggtatcaa\\n\\\n\
gttccggtctcaaaacgtaaaattctgtaatctgtatggatggtctgtgaattagttaat\\n\\\n\
ttttatgaagtcgtcgagacgcagttcctattgatttattctaaacggagatgtgcttcg\\n\\\n\
tgggactcggaagtagatctgtgtttatgattattgctactttagatgctgactgttaac\\n\\\n\
tccgtgttgtttttcaaccgtatatcacaaccgaattggatagaacctatagtttcaagt\\n\\\n\
tctgccacaaggtatcatatttacagttagtgctggttgcttctttcaaacgtggtgagt\\n\\\n\
ttgtgctatcacgtcaacggtagagctcagtggaccgagtgcgcgttcaaccctgttcca\\n\\\n\
gagagggtgtgatagcacatataccacgctcgtcgaggcgttcatgatagtttgcaagag\\n\\\n\
ccggtgttaaacacatattattattgttatccaactaatcggacctatgcataaagcatt\\n\\\n\
gtctaaacagaataattgcctatatacggtagttttagtgatttatatcttagtatcagt\\n\\\n\
tagagcttcgaactcttcaggttcctcatatttaacgttcttcgaaagcgaaaacttcta\\n\\\n\
caaacgaatgtaagcggttttccaagtagtacctataaatcacagaaagatctgtctcag\\n\\\n\
tatagttgaaatggtattcagctagtgacgtgtaccaattatcatagttcactcaagcaa\\n\\\n\
gacgctcattaacgaatatagacaagacactatatcatataataaaaaagaacatggtgc\\n\\\n\
tcgaacatagttgaattcaccatattgaaggggaatgctgacatgtaattcgctactaga\\n\\\n\
cgatcaattccctacttgtcaaagttgaactggtacgttcttggaattaaatatgattgc\\n\\\n\
gctggaccaaattgcgacttcttgagtttcagggcaaacgattgagccggaggatgtccg\\n\\\n\
tctcttacctttcttgcttatgataaacgacggtccctgtacatcactgggaattctcag\\n\\\n\
caaaaataattgggtaaatcgagactcgatgtattcggccacaaaggtgttagacgttaa\\n\\\n\
agattattcaacggggcgataataggatcataaccggtatgcaagcgcattgaaagagcc\\n\\\n\
atgagatccttatccgataaacgctgcacggtatgtgcagccttattgtcgatcacgaat\\n\\\n\
ttataaatgtagtctgggctgtaagttgaagacctaagttataatgaagtgcaataccaa\\n\\\n\
atcgattcatagtggattatcagactcaagatatctcctgataaattacagttgttaaga\\n\\\n\
tacggataaaatgagatttaagattagcagcctctaatctgtttcaatcccgttggaatg\\n\\\n\
tggtatgcgatcaaggttaagttaaaatcaagcctgtcttcagtcttgattcttgttctg\\n\\\n\
ccatcgcatgcggtctacgtgagttaatatgtagcttacgttctagcttgtgctaatctg\\n\\\n\
agtatagattcgtagaggaatattatcaagcttccacgcctcaacgtacgtgtattggtc\\n\\\n\
acacaagacactaaaagtggaagtagcgtaaactatagtctagttgttaaatgctcagtt\\n\\\n\
cttgttatattcgatatactcttggctaatttatgtctgagtatataaaattaatgatat\\n\\\n\
taacttgcatttcacggatcccttagaaaaagattttgaccgagcgcattataaacggtt\\n\\\n\
acaccgaatcaatagaagcatacccaatagctttctttgaatttattgcctgcgcaactt\\n\\\n\
ggctgactctctagatccgaataattctatatggtcgtgacgaaactagttcattactgt\\n\\\n\
ttaaaatgccaacatgtcttttgggccgataatggctctttgcaaaattactcaatgata\\n\\\n\
cgattgatcaaagcggtagttgctagtggtagcatgtaagtctatcaaatgtctgattat\\n\\\n\
ccgaaaatcttccaaaagagtccacgtaccatatctatctcatagcgacgcgaggggaac\\n\\\n\
cttatctaactatcattccatttaccgggtgactctcgatgcaggatccgattgggataa\\n\\\n\
attgcccagaaatggctcattcctgactaagggtaaggccgttctcagcaagggaacccc\\n\\\n\
gcgaatctaggcttataccatctagattgttaactacttgcctgtagttctacagccata\\n\\\n\
ctggacagttgtttctaaatgatcgggattcatgctagcactcctctgaatgcaccgcgt\\n\\\n\
aagtttaactattacgtccgtgggcagataaggatggaggctgtatgtatcttaactgtt\\n\\\n\
acctaatatggctggtaattatcaaagtaaggaccttaatgccatagcgctagcaatcgc\\n\\\n\
tttgtatactgaccatgtgccaacctctcttaatctgtaaaatataatgtcttagctaac\\n\\\n\
tgtggacgatcatgtctctgcctagagcttcgctgtatcaattcctatagccagcgtact\\n\\\n\
agtgacacaacaacaccgtgtgagaaaagatattagtccttacgtctgtctctctacagc\\n\\\n\
ttattgatgaggattgaacatggacatatagctccccctcaaaagcagatgctacctctt\\n\\\n\
tattccattctcgaacatttgccgaacttaatttcgacaaacctgaggtcacgtcttaat\\n\\\n\
ttatcggtaacgtcacgtccctttgagactggataaatatattaccaggggccaacgagc\\n\\\n\
aattgttggaggcgcttctataatacaaggtgtcttgtcaaagaaagacggcgtgcgtct\\n\\\n\
cgtgcaactcacttaaccaatattaatgtgaaacccccctctctcacatcttatgcggtg\\n\\\n\
tactgccctggtacatttcctgtacaggactccaacagtgtagattcctaagatagctgt\\n\\\n\
tggagttgcctcacgccagatcgaaaaactgaataaactagtgagctgagctgcagaaat\\n\\\n\
accgcttaattacttatgactagttcaaagggacctacgtgatgtcagacattgcaagga\\n\\\n\
agaaattaggtttgtgcgtcattttggctggactagcactccttacttcccctactattc\\n\\\n\
aaatgtcgtaaacagcatgagacaggatcgtgctgacatttaaggtctattgggaacgag\\n\\\n\
gctacctttggtcgcgcgctcgcgttctccgaatgaccgaaatgcatgagcacagtatgc\\n\\\n\
aattgcttatagatctaaggtctggtcgttgaaaccaagcacgtaggcctgggaaatcag\\n\\\n\
ttcttcctcagcaactacacaaaagcgtccaagcattagtacttgtagtaaatgtccgaa\\n\\\n\
cctatgcgctcatttgaaagtcaaaaaatatttttaagcagtaggcacctaacccgattc\\n\\\n\
ctctacttagtagctttctttgattctcagaattgactgcaatatcactgcacaattctg\\n\\\n\
tgccattactagacttctctgtattaacgtctcatcttactaacactcgcctaggacaca\\n\\\n\
tctgagagtgaagtatttcaatacatttactgaaatcttcagttctaaaatccccgaata\\n\\\n\
aggctcttatcggtttggccaacacaagaaaaaaacttcttgcaccactcaccttcatac\\n\\\n\
gcaggagcctggggaacttagtaataactatttcggcagacaaagcttataacaagttgc\\n\\\n\
cggcgcgtataatatttaaaagaccccttgagctgctcaattaaaacgctcacctggtat\\n\\\n\
aggctattagatagtgccgtcttagtaaggggcgggaattatcggataaactgatatttt\\n\\\n\
gataaaataaccgacttgttcacgacataagtcactaaggagattttatctttctccaaa\\n\\\n\
gtatatcttccttggataatttcaaagcgctgcaatttaagttctgttactagtttatgc\\n\\\n\
tgctgggaggtgaccggaaggcgtagtaatctagaggcaaattataagaagttcatcata\\n\\\n\
tcattttcgactacaaaaacaaggtgttgtatgccggcgcattgtgtaaactggacgagt\\n\\\n\
accctagatggaaaattatacgttaagccaagatttcgatgtaatgataattacctacac\\n\\\n\
atttttgctatccataggaacaagagctgttctataggctcgtggcatacgaacatttgc\\n\\\n\
tgccgctatgaatattggaagctcttcaactacagactctattcttaattgccgtcgaaa\\n\\\n\
atgggccgaatcggctattattaatactcggtttttccgaggggattgttgtcgacagtc\\n\\\n\
gtaattattattaatattgatgttggtgaggtcatttaaatacaaccttgcagacaatga\\n\\\n\
ataagggatccaatctctcatactccttttacaattgctcatgcccctatgcaaacctta\\n\\\n\
tgccgccacacctccgcaactctctcttctgaactgtaagtagcttcattactggtttga\\n\\\n\
gactatactgaagctgatgacattctaaaatggctattttcgaatgtgattcataatgtt\\n\\\n\
tatcgtttgggatggcagaatcacgttatttttgatatagcccgggtattctattgtata\\n\\\n\
gaacgtatgctacaagtcattccccgaagaagactagaagtaaacaacatgcgaccatcg\\n\\\n\
ttaagccacgcaaggctgtagctttatttcccgataacctatcttccataaatagcggac\\n\\\n\
agcaggatactgacgctcaacatcagtggttatggtctaatttttaacttttaataaggt\\n\\\n\
aacttcagcaggcatacacagtaactctttaatttataatcaaattagaagtctgacact\\n\\\n\
tcttatatttttctatcatccaacgcgatcgcccattagcttattgtgttactaataacg\\n\\\n\
tatctaaaccaatccttttcaagctactgcctatattgtcaatatatacaaacaacagga\\n\\\n\
tagtaggctgcttaaaaaatattgtcaaccgtgtacgctttacaatacccggaaatcaca\\n\\\n\
aactttgtagacaacgagtgaaatttatacactacgaagggccagcgtacaagacccatg\\n\\\n\
aattaggcgatatgtttattctgacatattggtttatccttaatctgtcgctgtaaaatg\\n\\\n\
aagccgcccccatccctgcgaattttttttcgaagattcacgactgaaatataaatacgt\\n\\\n\
ttggctatatttatgttggagggaggcaatagcctttactgttaaccgaagatttagcca\\n\\\n\
gtgagtgtgacactaaaacactggaataaatgcaggcgttcttctgggtaaaaggtttag\\n\\\n\
tcaatctcgcctataagttcatatagctctggatataattatctggcccatgcatttatc\\n\\\n\
atggcgcttggtgccctgtgtgaagccggcctctcatattgaaggtccgaagtattccat\\n\\\n\
gtacattaagatcactctctcattcatgcatcttggcttaacaaatctggttgtccaagc\\n\\\n\
tttccaggcacgtatggtacaaattcggatcgaatacttataaaaatgatatgttaaact\\n\\\n\
gtctaaaacgctcatctacaaagtaaagtgcactaaccaatagagtctcaagaccgtgta\\n\\\n\
atgctggtgcactgaatgtgtaatacggttagaagggattagttatgttacaaatccatt\\n\\\n\
gaaaacttaagaagcattgcgtgctcggagggtgcatcttttatcaagagactaacatta\\n\\\n\
ttttcaacgacgtacatgctttacaatagggtacttatcaaacgccgagaaacgcgccta\\n\\\n\
tagtgatgttatgattatgacccgatatccattggaccgaattttatgtaggttcccagc\\n\\\n\
gtactcgcgtaatatctcggtattgccataatgtaatacttgtcggtctctcccagatga\\n\\\n\
aaaagcgttacagagtatttcaatgaaaaacagcgcgcaacgtcaatacctttaggggta\\n\\\n\
acggccgctgatttcatatagatatacgataagttggtatagctctactaggtggcatcc\\n\\\n\
acaatcgttgcatttactatagctggttacaatcataatctataccgttccttacatact\\n\\\n\
accatagcgggatagcgtttttttgccgttgattgggtttaagaggatgtcagtctcatt\\n\\\n\
atatccgattcggtgggagagccgttgttttcaaatcgcacactttgtgacataatgtac\\n\\\n\
aagataacaaaactgatataagatataaactgtcaatatcaccttgacacttgaatcaaa\\n\\\n\
gtaaattaactcgcaaatataatttgactaattgggtgcagatttctcaattaataaaaa\\n\\\n\
aatggcaccggatgggcttacaagccccttatcattcacttgtatcatgatttccaagaa\\n\\\n\
caatagaatttgctagcaagtatgaacagagattcgaattgcatccacagtacgccggag\\n\\\n\
cgtttattttaatgtggatatgacgatgtactgttggcggcatttgctagtaaccggtcc\\n\\\n\
ttatttacgtagcgcacacgtaagcatgtctgggagaaatatggtggtacaatctcagag\\n\\\n\
aaagattacagtttggtttaaataggacttatcgggtcggaagtggaacttaataagcag\\n\\\n\
tacacaattgggcaacagacgtcttgcctattacaataggattacaatgcgttagatttc\\n\\\n\
agacacgttcgtgtttggctattcgtcaattccctaaatagttagacgatcaactattat\\n\\\n\
caaagtgattctttgttcatcctccattcatgtaacagatggcacactacgcataacgcc\\n\\\n\
gaggaattttaacgagatttaagagagcagttcgggcacaacccacttgactttataaca\\n\\\n\
gctcggcagcataaacggtaatatgtgacaaatttccaaacgttataagaacgtatgtgt\\n\\\n\
acttagaaaactaagtggttcatgttcaacagatgtgacgcagcaagcctaacttatcta\\n\\\n\
ttggttttgctataaaagaacaaagttacacagaatcctaagggcttgtttcacacttat\\n\\\n\
gcctagtgcttcaccatcttaaaatagcgaaaccggcacgaatcaaaccttaaaacaatg\\n\\\n\
cgcagatattggtgatggtgactccgggtatgataatggtaactgttgaccagcgcccac\\n\\\n\
ctcatcgaagtatagaaagtggttaggataaggatgagaccgaacttatttccggccata\\n\\\n\
actttagattttctacctagtacacaacatcagggcggacacgaaaccgccatcacatca\\n\\\n\
tataccaggtttaatttgcttaatgggggaagtgtcaacgaaccttcgaactttagcagg\\n\\\n\
catatggccattatatatggccccagagcagaatgctacagcagacaaaatttggattta\\n\\\n\
tgtagtttaatacctatcaaacttggtgtgaccatacttgtctaacgacagtgcacaaag\\n\\\n\
tgtaagttacaattattactactcagcagcttctgcaatgataaaatcttatcatacacg\\n\\\n\
tcacatatgataatatctacttagggggaacgggctccacaacctacatagtactcaata\\n\\\n\
cttacactattcgacaggcacaccaaacctgtacagtcccaaaagattgagtcaactttg\\n\\\n\
cagtactgcagatcacagtaatagcttagttagcgagtcaaaattagttttctacgagac\\n\\\n\
tgcacgaccgtgcaaatttccgatgtgttggctacaaatagcaacgtatgaatttgtttg\\n\\\n\
aagccacgtaaactgtacaaccttagagataagtctcaggctactaaaaacacgttgtgg\\n\\\n\
cactaacaggatcatggttgattcttacttattcggctgaccggcccaataagtaacctt\\n\\\n\
caactagaacagaataatcgggagtagtttaattcagtcaaggtgcaggtctcattgtaa\\n\\\n\
ctaacaagctctgtgtaaccaagttaaaatcgttttcttagcggattccctacttatgga\\n\\\n\
tttgagctcgtccacaatattcgatacaagaagtttgtggtccgtaacaacgaaatttta\\n\\\n\
attacgctgtgcagcctcatccaaggaattaatagaaggttgatggtaggctccgaacgc\\n\\\n\
tccatgattataatcaagtggactgtgcagtaaacgaggaaggtatcctgacgtcgtggt\\n\\\n\
gttcgtttttgttatttgtgccctatacgagtagataaaccatgaacagcacagtgtgaa\\n\\\n\
cccatggttgattttaggctaccttatttttaatttccgttacacagaaacgaattccac\\n\\\n\
aactaacatgccattaatttttcgatatcttataaaagatggtcgaaattcattcattta\\n\\\n\
ttttttttcggttctcgaaagtcaactaagctgtcgcgttttgtttctctttagaggtaa\\n\\\n\
aagtggctttgatctcctacgtttggatactagtcaaccattactccatttgatccgtga\\n\\\n\
gtatcacctgtctaacatccagcattatgactcctcggcgaagaaaagacacacttctta\\n\\\n\
gagtcgatgtgtattagctagggacacagttgtttaatacgatagtgagcccagggaggg\\n\\\n\
cagtgcgtcccccagtagatttattcagctagtgtaagtataagatatctcacccacgag\\n\\\n\
gttcaagtgatatgcagtcttagaataatacttatcctgaatttcgatattatgggtact\\n\\\n\
tcaataatccgctagcgctactttatgtctcgttggacagcaggacacatggcagtctta\\n\\\n\
aacactaaagacatcacctgaatgaatgtaatgggattacaagaatcaatgaggtattat\\n\\\n\
atacgacgtaggaaactctggatatatacagtaatctagttacgccatcgcacttcattc\\n\\\n\
ctctggaaacttagaagacatcagctgtacgtggaggaaccagacccccgtatgtagcca\\n\\\n\
aatagaaccaaagttgcttatacaaacacacccaatgacaatggaccgctggagttcgta\\n\\\n\
aactcggaacgtagtactgcacaaacccagcatttagcaataggagctacgtatgcaact\\n\\\n\
cccacgtggtaataccttcaagctatcaatatataggtgcctagctaatcgcattcgcaa\\n\\\n\
gcagtattcaagcttgtaaaccagtataataattacagaggctctatgaaacccaacttt\\n\\\n\
ccagctaaaagtcccaattaaatggttatttcgtacttttaaagtcgcccgttctgttat\\n\\\n\
tacgcgaattgattctactccaaaattaaacacaaattatcaaccgtttcatttatattt\\n\\\n\
gtcaatgcagctgtttaaaataaggctctactaaattataattaagacacttattaccag\\n\\\n\
atttctctagttaagtttgaaccagctcgactaccgcgaaagatacattcccttctctat\\n\\\n\
ttttcagttcatctatgggtcagagaagcattgaatttattctattcaccctcgtcgttc\\n\\\n\
acagcgaatcgtcagtgtgatcagtgtatgagaaatatcctaaaccgtttagtcagacca\\n\\\n\
cacgcttagaacaagtggtctaaaaagactgccctggaaggagtaagaagtatacagctg\\n\\\n\
atccggtgtatccttcagtcatctgccctatactaattacacgacgcaaggaaaaatagg\\n\\\n\
tttattttctaggcaaacccttcataggtgactccgatgtgttacgaatcatgcttgaga\\n\\\n\
atgtgctatcgttaccgacggataataacgatctccaatgaaccaaatgtagaatgtcta\\n\\\n\
ttgattacccttttactattcgacttagagataggagatagaacctcagtgtactttttt\\n\\\n\
agccgaatgggaatctttgggaggtgaatggccataaggtcgtaaatccaaccctcttaa\\n\\\n\
agtcttccatattatatcgttgttcgtggaatcgataacagatttgttgacccatagtaa\\n\\\n\
atgtatactagtttatgttgtaagtgtagattgttttccgattgccgtccaaactttatg\\n\\\n\
tcgtaattgtagaccagtaaagttgaccaaggtaagtgcccagcgatcctgcgagatcga\\n\\\n\
tcgccaatttttccagtcactgtaagtgtaggtttagataaagccgtatgagttatatca\\n\\\n\
taagggcctcggaaagcagcttcgaaccaaagttcccttataatagtagtttaactataa\\n\\\n\
aagtatatactggtctgtcgccctttcacgatttgttttaccggtttatgaagcgttacg\\n\\\n\
tcattagagcggctccaatttaaggttaacggcttccatgtgtagttgtatacaaggata\\n\\\n\
acttaaagtatctgttcagcgagctagttaagttatcctcgatagaacacaactcagagg\\n\\\n\
tcccaagatcgggtttgcaacttgctaatttattctcaaggcaaattgggaattatcgat\\n\\\n\
acctgtataccataaggtcgctcgatgtgatgcttatgtcttctggtgatcctaccttag\\n\\\n\
ttagtgctgattaacggaacattaatgtttatcgttttgagatttagccaattctctgat\\n\\\n\
tctaactcaagatgccttatctgacgtgctatgcagcccctaagtattttacattgtaat\\n\\\n\
aggacacgctcctttaaaactcgccaaaaggtcgttgtggttctctactggttaactata\\n\\\n\
taatttacagctttgttgagctagttcctctttggtttaagtcctcaatattagttggtt\\n\\\n\
cgagcgataagttggctagttaccttagtcactatattagatccgaatgttatgcttcat\\n\\\n\
ctgaagaccgccaccctccaaaatttcttttaagactcacttattgcaaggtgtaggtga\\n\\\n\
attcggctcgtttctcaagtggtgtatctgtacacgagtttccatattttcatcaacagc\\n\\\n\
caccgcacacttatgtcactctaggtattaaaagtcgctctacaaggggacgcaattaag\\n\\\n\
aaacagacatgctagtcaaaaataaacatagcgaggcaccactaattcggccgcttatca\\n\\\n\
atgggatgctctgcgcgagacgcgccagagctcagtagttagttcggacatacatttact\\n\\\n\
tcagatgatcaattagttttctacaaatgcttactctaccccgaaaaaagtcaccagact\\n\\\n\
cttacgtctctttagtatccttccgtcttatataaggtcagtcccccgtttcggtaccct\\n\\\n\
ggaatttactaagaataatgaaacagcccccaaggacgtacgtttacaaatgatagacca\\n\\\n\
gatcgcctagcttattccgacgcatgttgcatagaattgaaccaacggaatgtgagagta\\n\\\n\
actagatgagccgaccacagcacccgtttgcgtcgcagaatacgcctgatagttcggcca\\n\\\n\
cgaaatcatatgtcctttgagtattaagtatttgtaatgatcaatcgagctcaagcaagc\\n\\\n\
ttacacttcctcggatattcagggaacttagtgcctttgaaagatacgttgatcaacgaa\\n\\\n\
aaattgataatggctcatatggaatgcctacctcatagtgctgaattaacacagcactgc\\n\\\n\
ggacctaacttttcgaggtttcaagttcacgtctcaaaacctaataggctggaatatgta\\n\\\n\
gggatcctcggtgaatttgtgattgggtttgttgtagtactgaccaagtgaatattcttt\\n\\\n\
ttttctaaaagcagatctgctgccgggcactacgaaggagatctctgtgtatcattattg\\n\\\n\
cttcttgacatgatgactcttaaatcactgtgggtgtgcaaaacgatagcacaacccaat\\n\\\n\
tcgatagtacatattgttgatacttcgcactaaaccgttcatatttaaaggttgtgctcc\\n\\\n\
ttccttcgttaaatactggtgacttggtcctatctactattagctagacctctggggaac\\n\\\n\
cacgcccccgtaaaacctgtgcaagagagggggtcatacatcttagacatcgcgcctcca\\n\\\n\
ccagggaagcattgggtgattgaccaggtgtgtaacaaatatgattattcttatactaat\\n\\\n\
attagcaaagatgcataatgatttgtattaaatgtataattgaattgataagggtctttt\\n\\\n\
agtcagtgatagagtagtataaggtagacattagaactcttaaccggacgcagatttttc\\n\\\n\
ggtcttagtaagccaattagtcgacaaaacaaggtaagagcggttactagtagtacctat\\n\\\n\
aatgcactgaatcttcggtcgaagtatagttctaatgctatgcagattgtgacggcgaca\\n\\\n\
aatgttcagacttatatcatgaaacaagctcttgtaagtattgacaaatgaaaagattga\\n\\\n\
atatttttaaatacaaaatgcgcctacttattaggggaattaaccagattgaaggccaat\\n\\\n\
cctcacatgtaatgagataatagacgataaatgaaattcttgtaatagttgaactgctac\\n\\\n\
gtgatgggtattatatatgattgagatcctccaattgccgacgtcttgtcttgatgccca\\n\\\n\
aaagattgtcaacgaggagctccctcgcgtacctgtcgtccgtatcataaacgacgcgac\\n\\\n\
atgtacagcactccgaagtataagcaataataatgcgggtaatccagactagatcttttc\\n\\\n\
ggactcaatgcggtttcacggtaaacatgattaataccggagagtagtcgagcttatcag\\n\\\n\
cgatgcaagcgaattcattgtgccaggagatacgttgcagataaaaccggcaacgtatgt\\n\\\n\
caacaagttttggcgatctcgttgtttgtattcgacgaggcgcgggaacttcaagaacta\\n\\\n\
tcgtatattcaagtccattaccttttagtttcagactggtggagctgactaaagttatat\\n\\\n\
catcattttgtacactggtttagttaacgataatttcagatttaacatgaccagacgata\\n\\\n\
atcgctgtatatccagttggaatgtggtttgccagaaaggttaacttataatcaagcctc\\n\\\n\
tcttcagtcttgattcgtcgtatcccatccattgcgctatacctcagtgtatttggagct\\n\\\n\
gtagttataccgtgtgctaagatcagtagacatgacgagagcaatattatctaccttaca\\n\\\n\
agcatcaacggacgtctagtcggaacaaaagactctaaaactcgaacttcaggttaatat\\n\\\n\
actatagttctgtattcagcagttattcttatattcgatattatcttgcctattggatgt\\n\\\n\
ctgactttagtatattaatcatagtatctgccatgtaaaggtgccagtactaaatctgtt\\n\\\n\
tcacagtgcgaattataaacggttacaaccattaaagacaacaagaccctatagctttat\\n\\\n\
ttgaattttgtcaatgcgcaacttggagctcgcgatacatcccaattagtctatagggtc\\n\\\n\
gggacgattctacggcatttctggttataatgacaacatggattgtggcccgagaatcgc\\n\\\n\
tctttcattaattaagcaatcattacagtcttataagcgctacttccgagtggtagcagg\\n\\\n\
taactcgatataaggtcgcatgagccgaatagcttaaaaaacaggccaccgaacattgat\\n\\\n\
agagaataccgaccacagcgcaacctttgattactttcattaaattgtacggctcactcg\\n\\\n\
acatcaagcttaagattgcgataatgtgaactcaaatggatcagtactgaagaaccgtaa\\n\\\n\
cccacttcgcagaaagcgtacccagagaagatacgctgttacaatatacagggtgaaatt\\n\\\n\
attgcctgttcttcgtaaccatttcgccaaacttggttagaaatgatagccattcatgat\\n\\\n\
agaaataagctgaatgataccagtatctttaactatgtagtcagggggaagataacgatg\\n\\\n\
gtccatgtatgtttctgatatgtgacagtattggccgcgtaatttgctaacgaagctact\\n\\\n\
taatgcctttgagcttcatatagatttctttaatcaaaatcggcaaaaagatagtatgag\\n\\\n\
ctataatatatgctagtagagaactctggaccatcatctatatgaatactgattcgagcg\\n\\\n\
tgcaattactttagcctgcgtactactgactctacaaaacactctgagataagtttgtag\\n\\\n\
tcagtaagtcgctctctataaaccttttggatgaccattgtacagccacttatagatccc\\n\\\n\
aataaatagcacaggagacagagtttttcaatgctcgatcatttgccgatagtattttcg\\n\\\n\
tctaacctcagggcacctattatttgatacctaacctaacggccctttcacaatggagaa\\n\\\n\
atatatgacatcgggacaaacacaaatggtgggtggccaggagatatgacatggtggcgt\\n\\\n\
ctctaagaaacacggactccctctaggcaaactcacgtaaccaattttaatgtcaaacaa\\n\\\n\
aacgctcgaaaagattttgccgtgtaatgacctggtacattgactggtcaggaatacatc\\n\\\n\
actgtagttgccgtagtgtcctgttggtgttccatcaagacacatcgtataacgcaattt\\n\\\n\
acgacggacatcagatcaagttatacagattatttaagtatcacgtgtgcattgggacat\\n\\\n\
aagggatctcacacatgccttggaacatttttgctttgtgccgctttttcgctgcactac\\n\\\n\
caatccttacttaccagtatattcaaaggtcgttaacagaatgagaaaggttagggctct\\n\\\n\
aagttatcgtcgattgggatagacgagacatttgcgagcgccctccacggatacgaatct\\n\\\n\
cccatatcaatgtgaactggatgctatgcagtttagttcttacgtctcctagtggtaaaa\\n\\\n\
atcaaagtagcactcgcatagcagttattcagaacctaatacacaaaaccgtcaaacatt\\n\\\n\
ttctaattctaggtatgggccgatcataggagctaaggtgaaactcataaatgttttgtt\\n\\\n\
agatctagcatcctaaaaagatgcatatactgagtagctggcgtgcattctctcaattgt\\n\\\n\
atcctttttaactgaactagtcggtcccatttcgtgactgagatctattaaccgataaga\\n\\\n\
ttaataacactcgcattcgtatcagctcagagtgaagtttttcaataatttgactgatat\\n\\\n\
attaacttctaaaataaccctttaagcctcggatccgtttcccaatcacatcaaaaattc\\n\\\n\
ttattccaactatctacggattaacaacgtgcatggggatcgtagtaagaacttgttccg\\n\\\n\
atcactttgagtatatcaagttgacggcccggttattattgaatagaaacattcacctgc\\n\\\n\
taaattaaataccgcacatcggatacccgatttcagagggccgtcttactaagggcaggc\\n\\\n\
tttgttcggtttaactgagatgttcattattttacagtatgcttcaactaatatgtaacg\\n\\\n\
aaggacagtggatctgtctccatagtagatcttcagtcgtgaatttcataccgctcctat\\n\\\n\
ttaagttcgcgttcgagttgttgatcatggcacgtgaaagcaacccctagtattctagac\\n\\\n\
gaaaattttttctagttcatctgataatttgccaattcaaaaacaaccgctggtttcccg\\n\\\n\
gcgcattctctaaaatggaagtcgaacctagagccattatttgtcggtaacccatgagtt\\n\\\n\
ccttcttttcagaagttaatacactgtggtcctatacagaggaaaaacagcggttatata\\n\\\n\
cgatcgtggcataacaacattggatcaagatagcaatttggctacctattctaattctca\\n\\\n\
ctagattcggtattccactacaatatcggcagattaggattggatgaataatcggtgttt\\n\\\n\
aagtccggttgcgtctccaatctcctaatttttattaatattgatcttggtgacctattg\\n\\\n\
taaataaaaacttcaagactttgaataacggtgaaaagatagaagactcatttgaaaatg\\n\\\n\
gatcatccacagatccaaacattagcaagacactaatccccaactagctattctgatcgc\\n\\\n\
gatcgtgctgcagtactcctgtcacaatagtctgttcatgatctaattctttttgggctt\\n\\\n\
tgttcgatggtgattcagaatctttatccggtcgcttccctgtagctactttgtggggat\\n\\\n\
attgcccggggattatagggttgagatcgtttcctaaaagtatttaaaccaagtagactt\\n\\\n\
caactaaactacatcagaacatcgtgaagacaccatacgcggtacctttatttaccgata\\n\\\n\
acatttcttcaagaaataccggtaagcagcataatgaccctaaacagctcggggtatcgt\\n\\\n\
cgtagttttaaattttatttaggttactgctcaaggaataaaaactaactatttaattta\\n\\\n\
taataatattacaaggctcacactgattagatttgtctataagacttcgcgatcccccat\\n\\\n\
taccggattgtcttaagaataaactagataaaccatgcattttctagataaggcctttag\\n\\\n\
tctaattagatacaaaaaacacgatagttgcatccttaatttattgtgtcaaacctggaa\\n\\\n\
ccttttaattacccgcaaatcactttatgtcgagactacctctgaaatttattatctacc\\n\\\n\
taccgcatgaggacttgaaccatcttgtaggagttatgtttattagctaagattcgttta\\n\\\n\
tcctgtagcggtccatgtatattcaacaagcaaaaagcactcagaattgtttttagttga\\n\\\n\
gtcaagactgatatataaataagtttccctagttttttcgtggtgggacgatattgaatt\\n\\\n\
gaatcttaaccgaagagtttcccactctgtcgcacaataatacacgccaatatttccagc\\n\\\n\
cctgcttatgccttaatcggttactcaatctcccattgaagttcattttgatctgcatag\\n\\\n\
aagtttcgggcccagccttttttctgccaccttcctccaagctctgtagacgcactctaa\\n\\\n\
gattgatgctcacatgtattaattctacattaacataaatatataagtcatgcatcttcg\\n\\\n\
agtaaaatatctggttctccaacatgtcctggcacgtatcgttataatgcccatacatgt\\n\\\n\
agtattaaaatgattgggttaactggatattaagatcatcgaaattgtaaagtcaaatta\\n\\\n\
acaatactgtctcaagaccgtgtattcctcgtgctcggaagggctattacgcttacttcc\\n\\\n\
gttttggtatcttaatatgactttcaaaaattaagttgcagtgagtcctacctgcgtgca\\n\\\n\
tcggttagcaagagtataaaagttgtttaaacgaactacttgctttacaataccggtcgt\\n\\\n\
atatatcgccgtgaatccagaagattgtcttctttggattatcaaccgagatcctgtgga\\n\\\n\
ccgatgttttgggaccttcacagaggactccaggtagagctcgcttttgcattaatctaa\\n\\\n\
gaattgtacctctctaaaagatctaaaacagtgaatgtgtatttcatggaaaaacacaga\\n\\\n\
gaaacgtaaattactttaggccgaaaggcacatgagttattatacatatacgagatggtg\\n\\\n\
gtatacatcgaattcggggcatacactatagttgcattgtatttagctgctttaaataat\\n\\\n\
atgatattaccttccttacataagacattaccggcataccctggttttcaacttgtgggg\\n\\\n\
ctttttgacgatcgcactctcatttgatccgagtagggcggtgacccctgcttttcaaat\\n\\\n\
acaaaaatttcgctatgaaggtaatagattacttttcgctgttatgatagaaacggtaaa\\n\\\n\
tttaaaattgaaacttctagaaaagtaaagtaacgagaaatgattttgtgaataatgcgg\\n\\\n\
tcatgattgcgcaagtaagaaaaaaaggcaaaaggatgcgcggaatagaaacttatcagt\\n\\\n\
cacgggtatcttgatttcattcttcttgtcaattgccgacataggatgaaatcagattcc\\n\\\n\
aatgcaatacacagtaacccccacccttgattgtaatgtcgatttgaagttgtacgcgtc\\n\\\n\
gacgaagtggatagtatacgggccttttgtacggtgcgatcaactatgaatctcggcgag\\n\\\n\
ttagatggtcgtacaatctcacacatagaggtcacttgcctgtaatgacgaattttcggc\\n\\\n\
taggtactcgaactttattagaagtaaaaatgtgggcaaaagaaggattccattttacaa\\n\\\n\
gacgattacaatgagttacatgtctctcaacgtagtctttccctagtagtctttgaacta\\n\\\n\
tttaggtactccagaaaattttagcaaagggtttctgtgtgaatccgccattcatgttta\\n\\\n\
tgatggaacaataagaataacgccctcgtatgttatcgacagtgaagtcagcagttcggc\\n\\\n\
caaaaacatattcaatttagtacagatccccagaagttaagctaagtgctctaaaatggc\\n\\\n\
ctaaacggttatcaaagtaggtctaattactatactaacgggtgcatcgtaataactgct\\n\\\n\
gtcgatgcaacactatatgatagtgtcgttttgctatatatgtacaatgtgacaaagaag\\n\\\n\
ccttagcgattcttgcaaacttaggacttcggattctcaatcttaaatgtccgaaaacgc\\n\\\n\
aaagattcaaaaatttaatctatgagcagatatgcctgatggtgactacgcgtatgttaa\\n\\\n\
ggctaaatgttgacaaccgcacacataatcgaactattgatagtcgggagcataaccagg\\n\\\n\
tgaacgtactttgttcacgacatttattgacatgttctaaatacgtctcaaaatcacggc\\n\\\n\
gcactagaaaacgcaatcaaatcattgtcctggtttaagggccgtaatgccggtagtgtc\\n\\\n\
aaacttcatgagaactttagctggcttttggccagtatttagggaccaagagcactagcc\\n\\\n\
ttaagctgaatattttgccatttatctactgttataactttaaaacttggtggcaccaga\\n\\\n\
cttgtcgatacacacgcatcaatctgtaacgtaaaaggtttactaagaacaagcgtagga\\n\\\n\
attgagtttatattatatttaaactaaaagatgatattagcttctgagggcgatagggct\\n\\\n\
ccaaatcataaagaggaatatattattacacgattagaaacccacaacatacctcgaatc\\n\\\n\
gcccaaaagtttgacgaaacttggcagtactccacatctcagtaatacagttgggagagt\\n\\\n\
ctcaaatgttgttttattactcaatgaaccaccctcataatttcactgctgttccattaa\\n\\\n\
atttgcaaacgatcatttgctttgaagaaacgtaaaatcgacaaaattacagataagtag\\n\\\n\
atgcataataaaaaaaactgctcgctataacacgatcatcgtgcattcttacttaggagc\\n\\\n\
atcacccgcacaataacgtaccttaaactacaacactattagaccgagtactgtaattca\\n\\\n\
cgaaagctcaagctcgcattgtaaagaacttgctctctcgtaaaatgtgataatagtttg\\n\\\n\
cggagaggattcaattattttccattgcacctactccactagattcgataaaagaaggtg\\n\\\n\
gtcctcccttaaaaagaaatgttaagtaacatcggaaccataagcaaagcatgtaagtga\\n\\\n\
accgtcatccttccctaagaaacataaaggtttttaataatgtcgactgtgaactataac\\n\\\n\
tgcatcctttcctgacctactccggttccttgttgttatttctgaacgagaccagtagat\\n\\\n\
aaacaatgtaaaccacagtgggtaccaatggtgcatgtgacgctaccgttgttttaagtg\\n\\\n\
cccgtacaaacataagaagtcataatcttacttgaaattaattttgccttttattttttt\\n\\\n\
tcaggctcgaaattaatgatttgttttttttgaccttctagttacgctaatatgcggtcg\\n\\\n\
cctgtggtttctattgagtcctataacgggatgggatctaatacgtttggttactagtaa\\n\\\n\
acaaggtataaatttgataccggagtatcaactgtataacatcaagctttatgactcata\\n\\\n\
cgcgaagtaatgacacaaggctttcaggagatcgcgagtacagagccactaaggggtgta\\n\\\n\
ttacgatagtgacaccaccgagcgcactcactccccaagtagatttatgatcctacgcta\\n\\\n\
agtattagatatataaccaaagaggttctagtcagtgcaactcttagaataataattagc\\n\\\n\
cggttttgcctttttaggcctaatgcaatattcagctagcccttatgtatctcgcgttcc\\n\\\n\
acagcaccactcatggcacgcgtttaaactaatcaaatataatctatgaatgttatgcca\\n\\\n\
gtacttgaataaatcaggttttttataagtccttgcatactctcgttatatactgttaga\\n\\\n\
gtcttaccccatagaaattctttcatctgcaaacttagaagaattctcagctacggggag\\n\\\n\
cataaagtccccaggatgttgacaaatacaacaaatgtggcttatacaaacactccatat\\n\\\n\
gaaaatcgaaccctcgtggtagttttagccgaaccttgtacggataaatccctccatttt\\n\\\n\
ccaatagcagatacctatcctactacctcgtggtattaaattaaagcttgaaatatagag\\n\\\n\
ctgcatagcttatccaattcccaagcacgagtctaccgtcgtaaccacgatttgatttac\\n\\\n\
agacgctagagcaaacccatctttaaacatataagtaaaaattaaagggtgagtgcgtac\\n\\\n\
gtgtttactagcaacttcgcttattaagacaattgtttataagccataattaaaaacata\\n\\\n\
tgttcaacaggttcattgatatttgtaattgcacaggtttttaataaggatctacgtaag\\n\\\n\
tataatgaacaaactttttaccagagttatattctgtactttgaaaatgctcctctaccg\\n\\\n\
ccttagagactttcaattagattttttgcagttaatctatgcgtaagtgaaccatgcaag\\n\\\n\
ggatgcgattcaaccgcctcgtgctaaccctatcgtctgtctcataactgtaggtctaat\\n\\\n\
ataattttcagttttcgaacacataaccctttgaaaatctgctatttaatgtctcacctg\\n\\\n\
catgcactatcttctatactgctcagaacggctatacgtcactatgctccaagtgacgat\\n\\\n\
ttaaacgaagcaaggaataataggtttattttagtgcaaaacaattaagtgcggactacg\\n\\\n\
tgctctttacaataagccttgtgattgggctataggttaagtcccatattaacgatctcc\\n\\\n\
aatgtacaaaatcgacaatcgctttgcattacccggttactagtcgaattacagatagct\\n\\\n\
gttagatactcactctaattttggacaacaatcccaatcttggggtcgtctatcgcctga\\n\\\n\
agctcgtaaatccttccatcttaaacgattacatattatagacttgttcggggtagagat\\n\\\n\
atcacagttgtgcaaacattgtaaatcgatactagtttatgttggtagtctagttgcttt\\n\\\n\
taccattccccgaaaaacttgatctactatttcgacaacagtaaacttgaactaggtaag\\n\\\n\
tgaaaacagagaatgcctcatagtgccactatttgtccactatatgtaagtgtagcttta\\n\\\n\
cataatccactatgactgagatcattacggcctaggaaagcagcgtagaaaaaaagggcc\\n\\\n\
cggatattacgactgtaactataaaactagttactggtagcgcgccatgtatagatttgt\\n\\\n\
tttaccggttgtggttgcgttaacgaatttcagccgcgaaaattgatccgttaaccagtc\\n\\\n\
catctcgacttctataaaacgataaagtaaagttgatgttcagcctccttcttatggttg\\n\\\n\
catcgagagtacactactcagtgggaaatagatcggggttcctacttcagattgtattat\\n\\\n\
ctaggcaattgccgattgtgccatacctggataaaataagctacctacatgtgatgctta\\n\\\n\
tctattatcgtcatactaccttagggtgtcctgttgaacgctacattaatctttagccgt\\n\\\n\
ttgagatgttccaatggataggagtctaacgcatgatgaagtttaggaaggcagagcatc\\n\\\n\
ccactaagtatgtgacagtgtatttcgaaacgagacgttataaatagaaaaaaggtcctt\\n\\\n\
ctggttctattctgctgaactattgaatggaaagattggttgacctacgtactatttgct\\n\\\n\
tgaagtcatcaatttgacggggtgagagacatatggtgcatactttacggactctatatt\\n\\\n\
ttagatcagaagcttagcagtcttctctacaccccctcacgacataattgcttttaagaa\\n\\\n\
tctatgtttgattcctctacgggaattcggatccgttcgcatgtgcggtttatctaaacc\\n\\\n\
aggggacatatgttcagctaaagcatacgaacactttgctaactagacgtatgtatagta\\n\\\n\
gctataaatcccgacgatatttacaaaaagaaatgagactcaaatatatacatagcgacc\\n\\\n\
ctacacttattcgcaccctgatctaggcgatcctagcacccacacccgaaagtgagcact\\n\\\n\
agtgtcttccgtattaaatttactgcagttgagattttagttgtctactaaggattactc\\n\\\n\
taacccgtaataaggatcaagactcggtactagctttactatcattccctatgtgttttc\\n\\\n\
ctaactcacaagggtacgtaccagcctatgtaattacaataatgataaagacacaaagga\\n\\\n\
agtaactttacaaatgagtctccagttacactagcttagtccctcccatcttgctttgaa\\n\\\n\
gtctaaatacgcaatctctgaggatatacagcagaagaacactcataacgttggagtcca\\n\\\n\
agaattagactcatagggcccccaacatttaatatgtactgtgagtttgaaggtgttcta\\n\\\n\
ttgttaattcctgctcttgatacatgacacgtactccgtgtttaaggcttcggactgact\\n\\\n\
ttctttcataagttgagcaacgaaaatttcagaatcgataagttggattcactaactaat\\n\\\n\
acggctgattgaaaactccactccggacctatatggtcgacctttatacgtaaccgatat\\n\\\n\
aaaacttataggctggtatatcgagccttcctagcgcaatttcggatggggtttcttcta\\n\\\n\
ctactcaacaacggaatagtctttgtttagtaaaccagagctcaggacgcccaatacgta\\n\\\n\
ggagagcgctgtggagcatgtgtcattatggactggagcactcttaaatcactctgcgtg\\n\\\n\
tgctaaacgatagatcataacatgtcctgagtaaattttcttgatacgtcgcaatatacc\\n\\\n\
gttattagttaaacgttctcatccgtcatgcgtgaaatacggctgtcgtgctcagatata\\n\\\n\
ctattagcgactcatctcgcctaacacgcacacgtataaactcggaatgactgccgctct\\n\\\n\
tacatattagaaatacagactacaccacggaagcattgggtcattctcaaccgctgtata\\n\\\n\
aaagatgattagtcttataataagattaccaaagaggcagaatcatgggtagtaaatcta\\n\\\n\
ttattcaagtgattaccgtcgtgtaggcagggagtgaggacgagatggtactcaggacaa\\n\\\n\
atattaaccggacgaagtggtttacgtcgtactttcactattagtagtaaatacaaggta\\n\\\n\
acaccggggaatagtactaaatataatgatatctatcttcgggagaacgagtcgtctatt\\n\\\n\
gctttgaacattctcaaggcgtaaaatgtgctgacttatagcatgatacaaccgattgtt\\n\\\n\
acttttgtctattcaaaagattgaatagttttttatacaaaagccgcatacttatgacgg\\n\\\n\
ctagtatacagtttcatcccctagcatcaatgctatggacagtattgaacttataggaaa\\n\\\n\
ttcttctaatagggcaaatccgtcgtgatgcctattttttttcagtcacatcctcaaatg\\n\\\n\
gcactagtattgtcgggatcccattaacaggctcaaccacgagctcacgcgaggacatgt\\n\\\n\
agtccgtatctttaacgaagcgacagcgacagaactcccatggataaccaattataaggc\\n\\\n\
ccgtaatcctctagacatcgtttaccaataaatccgctttctccgtaatcatgttgaata\\n\\\n\
ccccagagtagtccagatgataaccgatgaaacacaagtctttctcaatgcacttacggt\\n\\\n\
gaacttattaccgccaacgtagctcatcaaggttgcgacatctagttgtgtgtttgcgac\\n\\\n\
gagcccagcgaacttcatcaactttcgtatattcaacgccttgtaattttactttaagac\\n\\\n\
gcctggtgatgtagattcttagataatcagtttgttatcggctgtactttaccataattt\\n\\\n\
cacaggtttcaggtcaagaagattatagctgtatatacagttccatgctcggtgcacaga\\n\\\n\
aacgtgatcggataataatcaatcgcttatgtcgtctttaggcgtatccaatacatgccc\\n\\\n\
cgataccgcagtgtatttcgacatgtaggtataccgtcgcatttgagctcgagtcaggac\\n\\\n\
gtcagctagattagattccttaatagaatataccgacctctagtccgaactaaactatag\\n\\\n\
ataacgccaacttcaggttaattgtctagtcgtctgtttgcagatgggattcttagatga\\n\\\n\
gtgagtatcggccatattggttcgagcactttagtttttgatgcataggatatgcaatgt\\n\\\n\
atagctgaaagtactttatctgtttcaaactcacattgattaaaccggtaaacctttaaa\\n\\\n\
gactacaagaaaatattcagtgagggcaattttgtcaatcacaatcttccagctagagat\\n\\\n\
acttcacaatttgtcttgaggctacgcaacattagacggattttcgcgttttattgaaat\\n\\\n\
aatcgaggggcccaagagtatccatagttcattttgtaagatttctttacaggcttatta\\n\\\n\
cagcttcttcagactcctacatgcttacgagttatatgctagcatgtgaacaatagatta\\n\\\n\
atatacaggaaaacgtacattgagagagatgaccctacacagcgcaaccgttgagtactt\\n\\\n\
tcattaaagggtaacgctctcgagacagcatccttaagatggccttattgtcaaatcatt\\n\\\n\
tgcagaagtacgcaagatccctaaccaacgtagaagaatccctacaaacacatgagacgc\\n\\\n\
ggtgaaaatagacagggtgttagtattcaatcttcggagtatcaatttcgccaatcttgg\\n\\\n\
tgagaaagcataccctttcttcagagaaagaagatcaatcataacactatctttaacgag\\n\\\n\
gtacgcacgcgcatcattacctgcctccatggatctttaggatagcggaaagtattggca\\n\\\n\
gcgtattgtgatttcgttcctactttatcaatttcacattcatatacatgtcttttatca\\n\\\n\
aaatcgccaataagataggatgagctatattagatgctagtagagttcgcgccaacatca\\n\\\n\
tcgataggaatactcaggacagcgtgataggacttttcaatccctaatactctctataat\\n\\\n\
tataactctctcttaagtttggaggcagtaacgcgctctatataatcagtttgctgcacc\\n\\\n\
attcttcagcctctgatacatacaaataaattccacagcagtaagagggtttaattgaga\\n\\\n\
catcttgggaacttaggattttactctaacatcaccgaaacgattattggataccgtacc\\n\\\n\
taaacgaactttctcaaggcagtaatataggacatccgcaataacacaaatgctgcctcc\\n\\\n\
ccaggagttatgtcttcctggaggctatatcttacacccactcactataggcaaactaaa\\n\\\n\
gtttaaatgttgattgtctaaaaaaaagatagataagagttggccggcgtagcacatgcg\\n\\\n\
aaagtgaatcgtaagctataattctctggacttgaagttctgtcctgttcctctgcaaga\\n\\\n\
aacaaacttcctttaaagctatttacgacgcacatctcagcaagttataaacatgttgga\\n\\\n\
agtttctagtcggaattcccaaagaacggatctatctaatgcattcctacatttttcctg\\n\\\n\
tctgccgatggtgccatcctattcaaagaatttcttaaaagtagattaaatgggactttt\\n\\\n\
aacaatgagtaaccttacgcctctaagggttcctcgagtgccatacaccagtcaggtccg\\n\\\n\
agccacatacacggagaacattctaacatagcattctcaactcgatcatttgcaggttac\\n\\\n\
ttctttcctatcctagtgctaaaaatcatacttgcaatcccatagcacggattaagaacc\\n\\\n\
taagaaacaattcagtaaaacatgttcgaattcttggtatgggaacatcattgcagctat\\n\\\n\
ggtctaacgcattaatgtttgggtacatcttccatcatataaacaggaagagtctgacga\\n\\\n\
cagggagtgcttgcgatcatgtctatcattgtgaaatcaaattgtagctcacatgtcgtc\\n\\\n\
tatgagagcgtgtatccgataagatttagaaaaatagaagtcgtataagatctcactgaa\\n\\\n\
cttttgaatgaatgtgaagcatatatgatctgctttaataaaactttatccataggatac\\n\\\n\
gtttccaaatcaattcaataattattagtcaaaatagataaggatgaacaacctgaaggc\\n\\\n\
cgatcggacgtagaaagtggtcccatcactttgagttgatattgttgaaccacacgttat\\n\\\n\
tatggttttcaaacagtctcaggatattgtatatacagataatccgataccagttgtctg\\n\\\n\
acgcccctcttacgtaccccaccctttgtgacgtttaaagcagttgttcagtattttaaa\\n\\\n\
ctaggcggcaactaatttggaaagaagcacagtggatatgtctaaattcttgttattcag\\n\\\n\
gcctgaatttaatacaccgcatagttaacttcgcggtagagttgttcatcatgcctcctc\\n\\\n\
taagctaccacttctatgatacaccaatagttgttctacggaatctgataattggccaag\\n\\\n\
tcataaacttccgctgcgttcaacccccttgctcgaatatccaactcgaaaagacagcct\\n\\\n\
tttggtgtccggaacaaatcagttacttcttttctgatgttaattctctgtggtcagata\\n\\\n\
cagaccaaaaactccgcggatttaccatcctccaagaacaaatttgcatcaacatagcat\\n\\\n\
tttggctacatattctaagtctcaatagtttaggttttcaactacattatcccaacatta\\n\\\n\
ggattggaggaataatagctgggtaagtccccttgcgtctacaatcgactattttttatg\\n\\\n\
aatatgcttctgccgcacctatggttattaaaaaagtcatgactttgaagaaccctgaaa\\n\\\n\
agatagatgaatcaggtgtaatggcagcagccaaagagcatataattagcaacactctaa\\n\\\n\
gaacattatagatatgatgatagcgatcgtcatgatgttatccggtcacaatagtagctt\\n\\\n\
catcagctaattcgttttgccagtggtgacttgcgctggaagaatcgttatacggtccct\\n\\\n\
tccctcttgatacggtgggggcttattcaaccgcgtggattgggttgtcatacttgcatt\\n\\\n\
aaacgatgtaaaccatctagtagtcaactatactaaatcacaaaatagtgatcaatacat\\n\\\n\
acccgcttcatggttttaaccatttaattgattaaagatattccgctaagaaccattatc\\n\\\n\
tacctaaactgatcgccgtatcctagtagtttgaaatttgatgtaccgtaatgatcaacg\\n\\\n\
aagtaaaacgttatattgtatgtagaataataggtcttggagctaaatgatgtgattggt\\n\\\n\
agtgaagacttacccttacaactttaccggtttctcggaagaatatactagagaatcaat\\n\\\n\
gcatgggctacataagcactttagtctaatgagataaaaaatacacgagtcttccatcat\\n\\\n\
gaattttttgtcgaaaaactcgaacctggtaatttaaaccatatatctttatgtcgtcaa\\n\\\n\
taactctcatatgttttatataacttcccaatcacgacttgtaactgcttgttcgactga\\n\\\n\
gctgtttgagctatgaggccgggatccggttgagctacatctatttgctacaagaaaaat\\n\\\n\
gaaagcacatttgttgggagttctggctacactcatagagaaataagtggcccgagtggg\\n\\\n\
tgcggcctgcctccatattcaagtgtatcttaaaccaagtggttccaacgctcgcgctaa\\n\\\n\
agaattaaagcctttatttcctccacggagtagcccgtaatccggttcgaaagagaccat\\n\\\n\
tgaagttaattttcatatccagtgaagtttaggcacaagcatgtgttctgccacatgcct\\n\\\n\
caaagcgctcttcaaccaagatatgattcatcctaacttcgatgaatgcgtctgtaacat\\n\\\n\
aaatatagaaggaatgattcggcgagttaattttcgccttctccaacatggcatccctac\\n\\\n\
gttcgttataaggaccatacatgtaggttttaaaggtttgcggttaatcgatatttacat\\n\\\n\
catagaaattctatagtcaaatttacaagactctagatactcactcgttgcagccggcta\\n\\\n\
ggaagcgctttgtaccttacttcccttttcgttgcgtaatatgaatttcatatagtaagt\\n\\\n\
tcaaggcactcatacctccgtgaagagggtagatagactattaaagttgtttaatagtac\\n\\\n\
gtattgatggaaatgacccgtaggagatttaccactcaatccacaagattcgctgctgtg\\n\\\n\
cattatcaaaacagtgcatgtcgaaacatgggttgggtccttcaaacacgaatccaggta\\n\\\n\
gagatacctttgcaattttt\\n\";\n\
\n\
dnaInput = dnaInput + dnaInput + dnaInput;\n\
\n\
var ilen, clen,\n\
 seqs = [\n\
  /agggtaaa|tttaccct/ig,\n\
  /[cgt]gggtaaa|tttaccc[acg]/ig,\n\
  /a[act]ggtaaa|tttacc[agt]t/ig,\n\
  /ag[act]gtaaa|tttac[agt]ct/ig,\n\
  /agg[act]taaa|ttta[agt]cct/ig,\n\
  /aggg[acg]aaa|ttt[cgt]ccct/ig,\n\
  /agggt[cgt]aa|tt[acg]accct/ig,\n\
  /agggta[cgt]a|t[acg]taccct/ig,\n\
  /agggtaa[cgt]|[acg]ttaccct/ig],\n\
 subs = {\n\
  B: '(c|g|t)', D: '(a|g|t)', H: '(a|c|t)', K: '(g|t)',\n\
  M: '(a|c)', N: '(a|c|g|t)', R: '(a|g)', S: '(c|t)',\n\
  V: '(a|c|g)', W: '(a|t)', Y: '(c|t)' }\n\
\n\
ilen = dnaInput.length;\n\
\n\
// There is no in-place substitution\n\
dnaInput = dnaInput.replace(/>.*\\n|\\n/g,\"\")\n\
clen = dnaInput.length\n\
\n\
var dnaOutputString = \"\";\n\
\n\
for(i in seqs)\n\
    dnaOutputString += seqs[i].source + \" \" + (dnaInput.match(seqs[i]) || []).length + \"\\n\";\n\
 // match returns null if no matches, so replace with empty\n\
\n\
for(k in subs)\n\
 dnaInput = dnaInput.replace(k, subs[k]) // FIXME: Would like this to be a global substitution in a future version of SunSpider.\n\
 // search string, replacement string, flags\n\
\n\
var expectedDNAOutputString = \"agggtaaa|tttaccct 0\\n[cgt]gggtaaa|tttaccc[acg] 9\\na[act]ggtaaa|tttacc[agt]t 27\\nag[act]gtaaa|tttac[agt]ct 24\\nagg[act]taaa|ttta[agt]cct 30\\naggg[acg]aaa|ttt[cgt]ccct 9\\nagggt[cgt]aa|tt[acg]accct 12\\nagggta[cgt]a|t[acg]taccct 9\\nagggtaa[cgt]|[acg]ttaccct 15\\n\";\n\
var expectedDNAInput = \"GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCctt(c|g|t)tatcatatgcta(g|t)gg(a|c|g|t)cataaa(c|t)atgtaaa(a|g|t)cD(a|g)tBggDtctttataattcBgtcgtactDtDagcctatttS(a|c|g)(a|c|t)tHttKtgtH(a|c)aSattg(a|t)aHKHttttagacatWatgtRgaaaNtactMcSMt(c|t)tcMgRtacttctWBacgaaatatagScDtttgaagacacatagtVgYgtcattHWtMMWcStgttaggKtSgaYaaccWStcgBttgcgaMttBYatcWtgacaYcagagtaBDtRacttttcWatMttDBcatWtatcttactaBgaYtcttgttttttttYaaScYaHgtgttNtSatcMtcVaaaStccRcctDaataataStcYtRDSaMtDttgttSagtRRcatttHatSttMtWgtcgtatSSagactYaaattcaMtWatttaSgYttaRgKaRtccactttattRggaMcDaWaWagttttgacatgttctacaaaRaatataataaMttcgDacgaSStacaStYRctVaNMtMgtaggcKatcttttattaaaaagVWaHKYagtttttatttaaccttacgtVtcVaattVMBcttaMtttaStgacttagattWWacVtgWYagWVRctDattBYtgtttaagaagattattgacVatMaacattVctgtBSgaVtgWWggaKHaatKWcBScSWaaccRVacacaaactaccScattRatatKVtactatatttHttaagtttSKtRtacaaagtRDttcaaaaWgcacatWaDgtDKacgaacaattacaRNWaatHtttStgttattaaMtgttgDcgtMgcatBtgcttcgcgaDWgagctgcgaggggVtaaScNatttacttaatgacagcccccacatYScaMgtaggtYaNgttctgaMaacNaMRaacaaacaKctacatagYWctgttWaaataaaataRattagHacacaagcgKatacBttRttaagtatttccgatctHSaatactcNttMaagtattMtgRtgaMgcataatHcMtaBSaRattagttgatHtMttaaKaggYtaaBataSaVatactWtataVWgKgttaaaacagtgcgRatatacatVtHRtVYataSaKtWaStVcNKHKttactatccctcatgWHatWaRcttactaggatctataDtDHBttataaaaHgtacVtagaYttYaKcctattcttcttaataNDaaggaaaDYgcggctaaWSctBaaNtgctggMBaKctaMVKagBaactaWaDaMaccYVtNtaHtVWtKgRtcaaNtYaNacggtttNattgVtttctgtBaWgtaattcaagtcaVWtactNggattctttaYtaaagccgctcttagHVggaYtgtNcDaVagctctctKgacgtatagYcctRYHDtgBattDaaDgccKtcHaaStttMcctagtattgcRgWBaVatHaaaataYtgtttagMDMRtaataaggatMtttctWgtNtgtgaaaaMaatatRtttMtDgHHtgtcattttcWattRSHcVagaagtacgggtaKVattKYagactNaatgtttgKMMgYNtcccgSKttctaStatatNVataYHgtNaBKRgNacaactgatttcctttaNcgatttctctataScaHtataRagtcRVttacDSDttaRtSatacHgtSKacYagttMHtWataggatgactNtatSaNctataVtttRNKtgRacctttYtatgttactttttcctttaaacatacaHactMacacggtWataMtBVacRaSaatccgtaBVttccagccBcttaRKtgtgcctttttRtgtcagcRttKtaaacKtaaatctcacaattgcaNtSBaaccgggttattaaBcKatDagttactcttcattVtttHaaggctKKgatacatcBggScagtVcacattttgaHaDSgHatRMaHWggtatatRgccDttcgtatcgaaacaHtaagttaRatgaVacttagattVKtaaYttaaatcaNatccRttRRaMScNaaaDgttVHWgtcHaaHgacVaWtgttScactaagSgttatcttagggDtaccagWattWtRtgttHWHacgattBtgVcaYatcggttgagKcWtKKcaVtgaYgWctgYggVctgtHgaNcVtaBtWaaYatcDRaaRtSctgaHaYRttagatMatgcatttNattaDttaattgttctaaccctcccctagaWBtttHtBccttagaVaatMcBHagaVcWcagBVttcBtaYMccagatgaaaaHctctaacgttagNWRtcggattNatcRaNHttcagtKttttgWatWttcSaNgggaWtactKKMaacatKatacNattgctWtatctaVgagctatgtRaHtYcWcttagccaatYttWttaWSSttaHcaaaaagVacVgtaVaRMgattaVcDactttcHHggHRtgNcctttYatcatKgctcctctatVcaaaaKaaaagtatatctgMtWtaaaacaStttMtcgactttaSatcgDataaactaaacaagtaaVctaggaSccaatMVtaaSKNVattttgHccatcacBVctgcaVatVttRtactgtVcaattHgtaaattaaattttYtatattaaRSgYtgBagaHSBDgtagcacRHtYcBgtcacttacactaYcgctWtattgSHtSatcataaatataHtcgtYaaMNgBaatttaRgaMaatatttBtttaaaHHKaatctgatWatYaacttMctcttttVctagctDaaagtaVaKaKRtaacBgtatccaaccactHHaagaagaaggaNaaatBWattccgStaMSaMatBttgcatgRSacgttVVtaaDMtcSgVatWcaSatcttttVatagttactttacgatcaccNtaDVgSRcgVcgtgaacgaNtaNatatagtHtMgtHcMtagaaattBgtataRaaaacaYKgtRccYtatgaagtaataKgtaaMttgaaRVatgcagaKStctHNaaatctBBtcttaYaBWHgtVtgacagcaRcataWctcaBcYacYgatDgtDHcctaaagacYRcaggattHaYgtKtaatgcVcaataMYacccatatcacgWDBtgaatcBaatacKcttRaRtgatgaBDacggtaattaaYtataStgVHDtDctgactcaaatKtacaatgcgYatBtRaDatHaactgtttatatDttttaaaKVccYcaaccNcBcgHaaVcattHctcgattaaatBtatgcaaaaatYMctSactHatacgaWacattacMBgHttcgaatVaaaacaBatatVtctgaaaaWtctRacgBMaatSgRgtgtcgactatcRtattaScctaStagKgaDcWgtYtDDWKRgRtHatRtggtcgaHgggcgtattaMgtcagccaBggWVcWctVaaattcgNaatcKWagcNaHtgaaaSaaagctcYctttRVtaaaatNtataaccKtaRgtttaMtgtKaBtRtNaggaSattHatatWactcagtgtactaKctatttgRYYatKatgtccgtRtttttatttaatatVgKtttgtatgtNtataRatWYNgtRtHggtaaKaYtKSDcatcKgtaaYatcSRctaVtSMWtVtRWHatttagataDtVggacagVcgKWagBgatBtaaagNcaRtagcataBggactaacacRctKgttaatcctHgDgttKHHagttgttaatgHBtatHcDaagtVaBaRccctVgtgDtacRHSctaagagcggWYaBtSaKtHBtaaactYacgNKBaVYgtaacttagtVttcttaatgtBtatMtMtttaattaatBWccatRtttcatagVgMMtagctStKctaMactacDNYgKYHgaWcgaHgagattacVgtttgtRaSttaWaVgataatgtgtYtaStattattMtNgWtgttKaccaatagNYttattcgtatHcWtctaaaNVYKKttWtggcDtcgaagtNcagatacgcattaagaccWctgcagcttggNSgaNcHggatgtVtcatNtRaaBNcHVagagaaBtaaSggDaatWaatRccaVgggStctDaacataKttKatttggacYtattcSatcttagcaatgaVBMcttDattctYaaRgatgcattttNgVHtKcYRaatRKctgtaaacRatVSagctgtWacBtKVatctgttttKcgtctaaDcaagtatcSataWVgcKKataWaYttcccSaatgaaaacccWgcRctWatNcWtBRttYaattataaNgacacaatagtttVNtataNaYtaatRaVWKtBatKagtaatataDaNaaaaataMtaagaaStccBcaatNgaataWtHaNactgtcDtRcYaaVaaaaaDgtttRatctatgHtgttKtgaaNSgatactttcgagWaaatctKaaDaRttgtggKKagcDgataaattgSaacWaVtaNMacKtcaDaaatttctRaaVcagNacaScRBatatctRatcctaNatWgRtcDcSaWSgttRtKaRtMtKaatgttBHcYaaBtgatSgaSWaScMgatNtctcctatttctYtatMatMtRRtSaattaMtagaaaaStcgVgRttSVaScagtgDtttatcatcatacRcatatDcttatcatVRtttataaHtattcYtcaaaatactttgVctagtaaYttagatagtSYacKaaacgaaKtaaatagataatSatatgaaatSgKtaatVtttatcctgKHaatHattagaaccgtYaaHactRcggSBNgtgctaaBagBttgtRttaaattYtVRaaaattgtaatVatttctcttcatgBcVgtgKgaHaaatattYatagWacNctgaaMcgaattStagWaSgtaaKagttttaagaDgatKcctgtaHtcatggKttVDatcaaggtYcgccagNgtgcVttttagagatgctaccacggggtNttttaSHaNtatNcctcatSaaVgtactgBHtagcaYggYVKNgtaKBcRttgaWatgaatVtagtcgattYgatgtaatttacDacSctgctaaaStttaWMagDaaatcaVYctccgggcgaVtaaWtStaKMgDtttcaaMtVgBaatccagNaaatcYRMBggttWtaaScKttMWtYataRaDBMaDataatHBcacDaaKDactaMgagttDattaHatHtaYatDtattDcRNStgaatattSDttggtattaaNSYacttcDMgYgBatWtaMagactVWttctttgYMaYaacRgHWaattgRtaagcattctMKVStatactacHVtatgatcBtVNataaBttYtSttacKgggWgYDtgaVtYgatDaacattYgatggtRDaVDttNactaSaMtgNttaacaaSaBStcDctaccacagacgcaHatMataWKYtaYattMcaMtgSttDagcHacgatcaHttYaKHggagttccgatYcaatgatRaVRcaagatcagtatggScctatattaNtagcgacgtgKaaWaactSgagtMYtcttccaKtStaacggMtaagNttattatcgtctaRcactctctDtaacWYtgaYaSaagaWtNtatttRacatgNaatgttattgWDDcNaHcctgaaHacSgaataaRaataMHttatMtgaSDSKatatHHaNtacagtccaYatWtcactaactatKDacSaStcggataHgYatagKtaatKagStaNgtatactatggRHacttgtattatgtDVagDVaRctacMYattDgtttYgtctatggtKaRSttRccRtaaccttagagRatagSaaMaacgcaNtatgaaatcaRaagataatagatactcHaaYKBctccaagaRaBaStNagataggcgaatgaMtagaatgtcaKttaaatgtaWcaBttaatRcggtgNcacaaKtttScRtWtgcatagtttWYaagBttDKgcctttatMggNttattBtctagVtacataaaYttacacaaRttcYtWttgHcaYYtaMgBaBatctNgcDtNttacgacDcgataaSatYaSttWtcctatKaatgcagHaVaacgctgcatDtgttaSataaaaYSNttatagtaNYtaDaaaNtggggacttaBggcHgcgtNtaaMcctggtVtaKcgNacNtatVaSWctWtgaWcggNaBagctctgaYataMgaagatBSttctatacttgtgtKtaattttRagtDtacatatatatgatNHVgBMtKtaKaNttDHaagatactHaccHtcatttaaagttVaMcNgHatatKtaNtgYMccttatcaaNagctggacStttcNtggcaVtattactHaSttatgNMVattMMDtMactattattgWMSgtHBttStStgatatRaDaagattttctatMtaaaaaggtactaaVttaSacNaatactgMttgacHaHRttgMacaaaatagttaatatWKRgacDgaRtatatttattatcYttaWtgtBRtWatgHaaattHataagtVaDtWaVaWtgStcgtMSgaSRgMKtaaataVacataatgtaSaatttagtcgaaHtaKaatgcacatcggRaggSKctDcagtcSttcccStYtccRtctctYtcaaKcgagtaMttttcRaYDttgttatctaatcataNctctgctatcaMatactataggDaHaaSttMtaDtcNatataattctMcStaaBYtaNagatgtaatHagagSttgWHVcttatKaYgDctcttggtgttMcRaVgSgggtagacaataaDtaattSaDaNaHaBctattgNtaccaaRgaVtKNtaaYggHtaKKgHcatctWtctDtttctttggSDtNtaStagttataaacaattgcaBaBWggHgcaaaBtYgctaatgaaatWcDcttHtcMtWWattBHatcatcaaatctKMagtDNatttWaBtHaaaNgMttaaStagttctctaatDtcRVaYttgttMtRtgtcaSaaYVgSWDRtaatagctcagDgcWWaaaBaaRaBctgVgggNgDWStNaNBKcBctaaKtttDcttBaaggBttgaccatgaaaNgttttttttatctatgttataccaaDRaaSagtaVtDtcaWatBtacattaWacttaSgtattggDgKaaatScaattacgWcagKHaaccaYcRcaRttaDttRtttHgaHVggcttBaRgtccctDatKaVtKtcRgYtaKttacgtatBtStaagcaattaagaRgBagSaattccSWYtttattVaataNctgHgttaaNBgcVYgtRtcccagWNaaaacaDNaBcaaaaRVtcWMgBagMtttattacgDacttBtactatcattggaaatVccggttRttcatagttVYcatYaSHaHcttaaagcNWaHataaaRWtctVtRYtagHtaaaYMataHYtNBctNtKaatattStgaMcBtRgctaKtgcScSttDgYatcVtggaaKtaagatWccHccgKYctaNNctacaWcttttgcRtgtVcgaKttcMRHgctaHtVaataaDtatgKDcttatBtDttggNtacttttMtgaacRattaaNagaactcaaaBBVtcDtcgaStaDctgaaaSgttMaDtcgttcaccaaaaggWtcKcgSMtcDtatgtttStaaBtatagDcatYatWtaaaBacaKgcaDatgRggaaYctaRtccagattDaWtttggacBaVcHtHtaacDacYgtaatataMagaatgHMatcttatacgtatttttatattacHactgttataMgStYaattYaccaattgagtcaaattaYtgtatcatgMcaDcgggtcttDtKgcatgWRtataatatRacacNRBttcHtBgcRttgtgcgtcatacMtttBctatctBaatcattMttMYgattaaVYatgDaatVagtattDacaacDMatcMtHcccataagatgBggaccattVWtRtSacatgctcaaggggYtttDtaaNgNtaaBatggaatgtctRtaBgBtcNYatatNRtagaacMgagSaSDDSaDcctRagtVWSHtVSRggaacaBVaccgtttaStagaacaMtactccagtttVctaaRaaHttNcttagcaatttattaatRtaaaatctaacDaBttggSagagctacHtaaRWgattcaaBtctRtSHaNtgtacattVcaHaNaagtataccacaWtaRtaaVKgMYaWgttaKggKMtKcgWatcaDatYtKSttgtacgaccNctSaattcDcatcttcaaaDKttacHtggttHggRRaRcaWacaMtBWVHSHgaaMcKattgtaRWttScNattBBatYtaNRgcggaagacHSaattRtttcYgaccBRccMacccKgatgaacttcgDgHcaaaaaRtatatDtatYVtttttHgSHaSaatagctNYtaHYaVYttattNtttgaaaYtaKttWtctaNtgagaaaNctNDctaaHgttagDcRttatagccBaacgcaRBtRctRtggtaMYYttWtgataatcgaataattattataVaaaaattacNRVYcaaMacNatRttcKatMctgaagactaattataaYgcKcaSYaatMNctcaacgtgatttttBacNtgatDccaattattKWWcattttatatatgatBcDtaaaagttgaaVtaHtaHHtBtataRBgtgDtaataMttRtDgDcttattNtggtctatctaaBcatctaRatgNacWtaatgaagtcMNaacNgHttatactaWgcNtaStaRgttaaHacccgaYStacaaaatWggaYaWgaattattcMaactcBKaaaRVNcaNRDcYcgaBctKaacaaaaaSgctccYBBHYaVagaatagaaaacagYtctVccaMtcgtttVatcaatttDRtgWctagtacRttMctgtDctttcKtWttttataaatgVttgBKtgtKWDaWagMtaaagaaattDVtaggttacatcatttatgtcgMHaVcttaBtVRtcgtaYgBRHatttHgaBcKaYWaatcNSctagtaaaaatttacaatcactSWacgtaatgKttWattagttttNaggtctcaagtcactattcttctaagKggaataMgtttcataagataaaaatagattatDgcBVHWgaBKttDgcatRHaagcaYcRaattattatgtMatatattgHDtcaDtcaaaHctStattaatHaccgacNattgatatattttgtgtDtRatagSacaMtcRtcattcccgacacSattgttKaWattNHcaacttccgtttSRtgtctgDcgctcaaMagVtBctBMcMcWtgtaacgactctcttRggRKSttgYtYatDccagttDgaKccacgVatWcataVaaagaataMgtgataaKYaaatcHDaacgataYctRtcYatcgcaMgtNttaBttttgatttaRtStgcaacaaaataccVgaaDgtVgDcStctatatttattaaaaRKDatagaaagaKaaYYcaYSgKStctccSttacagtcNactttDVttagaaagMHttRaNcSaRaMgBttattggtttaRMggatggcKDgWRtNaataataWKKacttcKWaaagNaBttaBatMHtccattaacttccccYtcBcYRtagattaagctaaYBDttaNtgaaaccHcaRMtKtaaHMcNBttaNaNcVcgVttWNtDaBatgataaVtcWKcttRggWatcattgaRagHgaattNtatttctctattaattaatgaDaaMatacgttgggcHaYVaaNaDDttHtcaaHtcVVDgBVagcMacgtgttaaBRNtatRtcagtaagaggtttaagacaVaaggttaWatctccgtVtaDtcDatttccVatgtacNtttccgtHttatKgScBatgtVgHtYcWagcaKtaMYaaHgtaattaSaHcgcagtWNaatNccNNYcacgVaagaRacttctcattcccRtgtgtaattagcSttaaStWaMtctNNcSMacattataaactaDgtatWgtagtttaagaaaattgtagtNagtcaataaatttgatMMYactaatatcggBWDtVcYttcDHtVttatacYaRgaMaacaStaatcRttttVtagaDtcacWatttWtgaaaagaaagNRacDtttStVatBaDNtaactatatcBSMcccaSttccggaMatgattaaWatKMaBaBatttgataNctgttKtVaagtcagScgaaaDggaWgtgttttKtWtatttHaatgtagttcactaaKMagttSYBtKtaYgaactcagagRtatagtVtatcaaaWYagcgNtaDagtacNSaaYDgatBgtcgataacYDtaaactacagWDcYKaagtttattagcatcgagttKcatDaattgattatDtcagRtWSKtcgNtMaaaaacaMttKcaWcaaSVMaaaccagMVtaMaDtMaHaBgaacataBBVtaatVYaNSWcSgNtDNaaKacacBtttatKtgtttcaaHaMctcagtaacgtcgYtactDcgcctaNgagagcYgatattttaaatttccattttacatttDaaRctattttWctttacgtDatYtttcagacgcaaVttagtaaKaaaRtgVtccataBggacttatttgtttaWNtgttVWtaWNVDaattgtatttBaagcBtaaBttaaVatcHcaVgacattccNggtcgacKttaaaRtagRtctWagaYggtgMtataatMtgaaRttattttgWcttNtDRRgMDKacagaaaaggaaaRStcccagtYccVattaNaaKStNWtgacaVtagaagcttSaaDtcacaacgDYacWDYtgtttKatcVtgcMaDaSKStVcgtagaaWaKaagtttcHaHgMgMtctataagBtKaaaKKcactggagRRttaagaBaaNatVVcgRcKSttDaactagtSttSattgttgaaRYatggttVttaataaHttccaagDtgatNWtaagHtgcYtaactRgcaatgMgtgtRaatRaNaacHKtagactactggaatttcgccataacgMctRgatgttaccctaHgtgWaYcactcacYaattcttaBtgacttaaacctgYgaWatgBttcttVttcgttWttMcNYgtaaaatctYgMgaaattacNgaHgaacDVVMtttggtHtctaaRgtacagacgHtVtaBMNBgattagcttaRcttacaHcRctgttcaaDBggttKaacatgKtttYataVaNattccgMcgcgtagtRaVVaattaKaatggttRgaMcagtatcWBttNtHagctaatctagaaNaaacaYBctatcgcVctBtgcaaagDgttVtgaHtactSNYtaaNccatgtgDacgaVtDcgKaRtacDcttgctaagggcagMDagggtBWRtttSgccttttttaacgtcHctaVtVDtagatcaNMaVtcVacatHctDWNaataRgcgtaVHaggtaaaaSgtttMtattDgBtctgatSgtRagagYtctSaKWaataMgattRKtaacatttYcgtaacacattRWtBtcggtaaatMtaaacBatttctKagtcDtttgcBtKYYBaKttctVttgttaDtgattttcttccacttgSaaacggaaaNDaattcYNNaWcgaaYattttMgcBtcatRtgtaaagatgaWtgaccaYBHgaatagataVVtHtttVgYBtMctaMtcctgaDcYttgtccaaaRNtacagcMctKaaaggatttacatgtttaaWSaYaKttBtagDacactagctMtttNaKtctttcNcSattNacttggaacaatDagtattRtgSHaataatgccVgacccgatactatccctgtRctttgagaSgatcatatcgDcagWaaHSgctYYWtatHttggttctttatVattatcgactaagtgtagcatVgtgHMtttgtttcgttaKattcMatttgtttWcaaStNatgtHcaaaDtaagBaKBtRgaBgDtSagtatMtaacYaatYtVcKatgtgcaacVaaaatactKcRgtaYtgtNgBBNcKtcttaccttKgaRaYcaNKtactttgagSBtgtRagaNgcaaaNcacagtVtttHWatgttaNatBgtttaatNgVtctgaatatcaRtattcttttttttRaaKcRStctcggDgKagattaMaaaKtcaHacttaataataKtaRgDtKVBttttcgtKaggHHcatgttagHggttNctcgtatKKagVagRaaaggaaBtNatttVKcRttaHctaHtcaaatgtaggHccaBataNaNaggttgcWaatctgatYcaaaHaatWtaVgaaBttagtaagaKKtaaaKtRHatMaDBtBctagcatWtatttgWttVaaaScMNattRactttgtYtttaaaagtaagtMtaMaSttMBtatgaBtttaKtgaatgagYgtNNacMtcNRacMMHcttWtgtRtctttaacaacattattcYaMagBaacYttMatcttKcRMtgMNccattaRttNatHaHNaSaaHMacacaVaatacaKaSttHatattMtVatWgattttttaYctttKttHgScWaacgHtttcaVaaMgaacagNatcgttaacaaaaagtacaHBNaattgttKtcttVttaaBtctgctacgBgcWtttcaggacacatMgacatcccagcggMgaVKaBattgacttaatgacacacaaaaaatRKaaBctacgtRaDcgtagcVBaacDSBHaaaaSacatatacagacRNatcttNaaVtaaaataHattagtaaaaSWccgtatWatggDttaactattgcccatcttHaSgYataBttBaactattBtcHtgatcaataSttaBtatKSHYttWggtcYtttBttaataccRgVatStaHaKagaatNtagRMNgtcttYaaSaactcagDSgagaaYtMttDtMRVgWKWtgMaKtKaDttttgactatacataatcNtatNaHattVagacgYgatatatttttgtStWaaatctWaMgagaRttRatacgStgattcttaagaDtaWccaaatRcagcagaaNKagtaaDggcgccBtYtagSBMtactaaataMataBSacRMgDgattMMgtcHtcaYDtRaDaacggttDaggcMtttatgttaNctaattaVacgaaMMtaatDccSgtattgaRtWWaccaccgagtactMcgVNgctDctaMScatagcgtcaactatacRacgHRttgctatttaatgaattataYKttgtaagWgtYttgcHgMtaMattWaWVtaRgcttgYgttBHtYataSccStBtgtagMgtDtggcVaaSBaatagDttgBgtctttctcattttaNagtHKtaMWcYactVcgcgtatMVtttRacVagDaatcttgctBBcRDgcaacKttgatSKtYtagBMagaRtcgBattHcBWcaactgatttaatttWDccatttatcgagSKaWttataHactaHMttaatHtggaHtHagaatgtKtaaRactgtttMatacgatcaagDgatKaDctataMggtHDtggHacctttRtatcttYattttgacttgaaSaataaatYcgBaaaaccgNatVBttMacHaKaataagtatKgtcaagactcttaHttcggaattgttDtctaaccHttttWaaatgaaatataaaWattccYDtKtaaaacggtgaggWVtctattagtgactattaagtMgtttaagcatttgSgaaatatccHaaggMaaaattttcWtatKctagDtYtMcctagagHcactttactatacaaacattaacttaHatcVMYattYgVgtMttaaRtgaaataaDatcaHgtHHatKcDYaatcttMtNcgatYatgSaMaNtcttKcWataScKggtatcttacgcttWaaagNatgMgHtctttNtaacVtgttcMaaRatccggggactcMtttaYMtcWRgNctgNccKatcttgYDcMgattNYaRagatHaaHgKctcataRDttacatBatccattgDWttatttaWgtcggagaaaaatacaatacSNtgggtttccttacSMaagBattacaMaNcactMttatgaRBacYcYtcaaaWtagctSaacttWgDMHgaggatgBVgcHaDtggaactttggtcNatNgtaKaBcccaNtaagttBaacagtatacDYttcctNgWgcgSMcacatStctHatgRcNcgtacacaatRttMggaNKKggataaaSaYcMVcMgtaMaHtgattYMatYcggtcttcctHtcDccgtgRatcattgcgccgatatMaaYaataaYSggatagcgcBtNtaaaScaKgttBgagVagttaKagagtatVaactaSacWactSaKatWccaKaaaatBKgaaKtDMattttgtaaatcRctMatcaaMagMttDgVatggMaaWgttcgaWatgaaatttgRtYtattaWHKcRgctacatKttctaccaaHttRatctaYattaaWatVNccatNgagtcKttKataStRaatatattcctRWatDctVagttYDgSBaatYgttttgtVaatttaatagcagMatRaacttBctattgtMagagattaaactaMatVtHtaaatctRgaaaaaaaatttWacaacaYccYDSaattMatgaccKtaBKWBattgtcaagcHKaagttMMtaatttcKcMagNaaKagattggMagaggtaatttYacatcWaaDgatMgKHacMacgcVaacaDtaDatatYggttBcgtatgWgaSatttgtagaHYRVacaRtctHaaRtatgaactaatatctSSBgggaaHMWtcaagatKgagtDaSatagttgattVRatNtctMtcSaagaSHaataNataataRaaRgattctttaataaagWaRHcYgcatgtWRcttgaaggaMcaataBRaaccagStaaacNtttcaatataYtaatatgHaDgcStcWttaacctaRgtYaRtataKtgMttttatgactaaaatttacYatcccRWtttHRtattaaatgtttatatttgttYaatMcaRcSVaaDatcgtaYMcatgtagacatgaaattgRtcaaYaaYtRBatKacttataccaNaaattVaBtctggacaagKaaYaaatatWtMtatcYaaVNtcgHaactBaagKcHgtctacaatWtaDtSgtaHcataHtactgataNctRgttMtDcDttatHtcgtacatcccaggSttaBgtcacacWtccNMcNatMVaVgtccDYStatMaccDatggYaRKaaagataRatttHKtSaaatDgataaacttaHgttgVBtcttVttHgDacgaKatgtatatNYataactctSatatatattgcHRRYttStggaactHgttttYtttaWtatMcttttctatctDtagVHYgMRBgtHttcctaatYRttKtaagatggaVRataKDctaMtKBNtMtHNtWtttYcVtattMcgRaacMcctNSctcatttaaagDcaHtYccSgatgcaatYaaaaDcttcgtaWtaattctcgttttScttggtaatctttYgtctaactKataHacctMctcttacHtKataacacagcNRatgKatttttSaaatRYcgDttaMRcgaaattactMtgcgtaagcgttatBtttttaattaagtNacatHgttcRgacKcBBtVgatKttcgaBaatactDRgtRtgaNacWtcacYttaaKcgttctHaKttaNaMgWgWaggtctRgaKgWttSttBtDcNtgtttacaaatYcDRtgVtgcctattcNtctaaaDMNttttNtggctgagaVctDaacVtWccaagtaacacaNctgaScattccDHcVBatcgatgtMtaatBgHaatDctMYgagaatgYWKcctaatNaStHaaaKccgHgcgtYaaYtattgtStgtgcaaRtattaKatattagaWVtcaMtBagttattagNaWHcVgcaattttDcMtgtaRHVYtHtctgtaaaaHVtMKacatcgNaatttMatatgttgttactagWYtaRacgataKagYNKcattataNaRtgaacKaYgcaaYYacaNccHatMatDcNgtHttRaWttagaaDcaaaaaatagggtKDtStaDaRtaVtHWKNtgtattVctSVgRgataDaRaWataBgaagaaKtaataaYgDcaStaNgtaDaaggtattHaRaWMYaYaWtggttHYgagVtgtgcttttcaaDKcagVcgttagacNaaWtagtaataDttctggttVcatcataaagtgKaaaNaMtaBBaattaatWaattgctHaVKaSgDaaVKaHtatatatHatcatSBagNgHtatcHYMHgttDgtaHtBttWatcgtttaRaattgStKgSKNWKatcagDtctcagatttctRtYtBatBgHHtKaWtgYBgacVVWaKtacKcDttKMaKaVcggtgttataagaataaHaatattagtataatMHgttYgaRttagtaRtcaaVatacggtcMcgagtaaRttacWgactKRYataaaagSattYaWgagatYagKagatgSaagKgttaatMggtataatgttWYttatgagaaacctNVataatHcccKtDctcctaatactggctHggaSaggRtKHaWaattcgSatMatttagaggcYtctaMcgctcataSatatgRagacNaaDaggaVBagaYttKtacNaKgtSYtagttggaWcatcWttaatctatgaVtcgtgtMtatcaYcgtRccaaYgDctgcMgtgtWgacWtgataacacgcgctBtgttaKtYDtatDcatcagKaVMctaatcttgVcaaRgcRMtDcgattaHttcaNatgaatMtactacVgtRgatggaWtttactaaKatgagSaaKggtaNtactVaYtaaKRagaacccacaMtaaMtKtatBcttgtaaWBtMctaataaVcDaaYtcRHBtcgttNtaaHatttBNgRStVDattBatVtaagttaYatVattaagaBcacggtSgtVtatttaRattgatgtaHDKgcaatattKtggcctatgaWDKRYcggattgRctatNgatacaatMNttctgtcRBYRaaaHctNYattcHtaWcaattctBtMKtVgYataatMgYtcagcttMDataVtggRtKtgaatgccNcRttcaMtRgattaacattRcagcctHtWMtgtDRagaKaBtgDttYaaaaKatKgatctVaaYaacWcgcatagBVtaNtRtYRaggBaaBtgKgttacataagagcatgtRattccacttaccatRaaatgWgDaMHaYVgVtaSctatcgKaatatattaDgacccYagtgtaYNaaatKcagtBRgagtccatgKgaaaccBgaagBtgSttWtacgatWHaYatcgatttRaaNRgcaNaKVacaNtDgattgHVaatcDaagcgtatgcNttaDataatcSataaKcaataaHWataBtttatBtcaKtKtatagttaDgSaYctacaRatNtaWctSaatatttYaKaKtaccWtatcRagacttaYttVcKgSDcgagaagatccHtaattctSttatggtKYgtMaHagVaBRatttctgtRgtctatgggtaHKgtHacHtSYacgtacacHatacKaaBaVaccaDtatcSaataaHaagagaatScagactataaRttagcaaVcaHataKgDacatWccccaagcaBgagWatctaYttgaaatctVNcYtttWagHcgcgcDcVaaatgttKcHtNtcaatagtgtNRaactttttcaatggWgBcgDtgVgtttctacMtaaataaaRggaaacWaHttaRtNtgctaaRRtVBctYtVtatDcattDtgaccYatagatYRKatNYKttNgcctagtaWtgaactaMVaacctgaStttctgaKVtaaVaRKDttVtVctaDNtataaaDtccccaagtWtcgatcactDgYaBcatcctMtVtacDaaBtYtMaKNatNtcaNacgDatYcatcgcaRatWBgaacWttKttagYtaattcggttgSWttttDWctttacYtatatWtcatDtMgtBttgRtVDggttaacYtacgtacatgaattgaaWcttMStaDgtatattgaDtcRBcattSgaaVBRgagccaaKtttcDgcgaSMtatgWattaKttWtgDBMaggBBttBaatWttRtgcNtHcgttttHtKtcWtagHStaacagttgatatBtaWSaWggtaataaMttaKacDaatactcBttcaatatHttcBaaSaaatYggtaRtatNtHcaatcaHtagVtgtattataNggaMtcttHtNagctaaaggtagaYctMattNaMVNtcKtactBKcaHHcBttaSagaKacataYgctaKaYgttYcgacWVttWtSagcaacatcccHaccKtcttaacgaKttcacKtNtacHtatatRtaaatacactaBtttgaHaRttggttWtatYagcatYDatcggagagcWBataagRtacctataRKgtBgatgaDatataSttagBaHtaatNtaDWcWtgtaattacagKttcNtMagtattaNgtctcgtcctcttBaHaKcKccgtRcaaYagSattaagtKataDatatatagtcDtaacaWHcaKttDgaaRcgtgYttgtcatatNtatttttatggccHtgDtYHtWgttatYaacaattcaWtatNgctcaaaSttRgctaatcaaatNatcgtttaBtNNVtgttataagcaaagattBacgtDatttNatttaaaDcBgtaSKgacgtagataatttcHMVNttgttBtDtgtaWKaaRMcKMtHtaVtagataWctccNNaSWtVaHatctcMgggDgtNHtDaDttatatVWttgttatttaacctttcacaaggaSaDcggttttttatatVtctgVtaacaStDVaKactaMtttaSNagtgaaattaNacttSKctattcctctaSagKcaVttaagNaVcttaVaaRNaHaaHttatgtHttgtgatMccaggtaDcgaccgtWgtWMtttaHcRtattgScctatttKtaaccaagtYagaHgtWcHaatgccKNRtttagtMYSgaDatctgtgaWDtccMNcgHgcaaacNDaaaRaStDWtcaaaaHKtaNBctagBtgtattaactaattttVctagaatggcWSatMacccttHttaSgSgtgMRcatRVKtatctgaaaccDNatYgaaVHNgatMgHRtacttaaaRtatStRtDtatDttYatattHggaBcttHgcgattgaKcKtttcRataMtcgaVttWacatNcatacctRataDDatVaWNcggttgaHtgtMacVtttaBHtgagVttMaataattatgttcttagtttgtgcDtSatttgBtcaacHattaaBagVWcgcaSYttMgcttacYKtVtatcaYaKctgBatgcgggcYcaaaaacgNtctagKBtattatctttKtaVttatagtaYtRagNtaYataaVtgaatatcHgcaaRataHtacacatgtaNtgtcgYatWMatttgaactacRctaWtWtatacaatctBatatgYtaagtatgtgtatSttactVatcttYtaBcKgRaSggRaaaaatgcagtaaaWgtaRgcgataatcBaataccgtatttttccatcNHtatWYgatHSaaaDHttgctgtccHtggggcctaataatttttctatattYWtcattBtgBRcVttaVMRSgctaatMagtYtttaaaaatBRtcBttcaaVtaacagctccSaaSttKNtHtKYcagcagaaaccccRtttttaaDcDtaStatccaagcgctHtatcttaDRYgatDHtWcaaaBcWgKWHttHataagHacgMNKttMKHccaYcatMVaacgttaKgYcaVaaBtacgcaactttMctaaHaatgtBatgagaSatgtatgSRgHgWaVWgataaatatttccKagVgataattWaHNcYggaaatgctHtKtaDtctaaagtMaatVDVactWtSaaWaaMtaHtaSKtcBRaNcttStggtBttacNagcatagRgtKtgcgaacaacBcgKaatgataagatgaaaattgtactgcgggtccHHWHaaNacaBttNKtKtcaaBatatgctaHNgtKcDWgtttatNgVDHgaccaacWctKaaggHttgaRgYaatHcaBacaatgagcaaattactgtaVaaYaDtagattgagNKggtggtgKtWKaatacagDRtatRaMRtgattDggtcaaYRtatttNtagaDtcacaaSDctDtataatcgtactaHttatacaatYaacaaHttHatHtgcgatRRttNgcatSVtacWWgaaggagtatVMaVaaattScDDKNcaYBYaDatHgtctatBagcaacaagaatgagaaRcataaKNaRtBDatcaaacgcattttttaaBtcSgtacaRggatgtMNaattggatatWtgagtattaaaVctgcaYMtatgatttttYgaHtgtcttaagWBttHttgtcttattDtcgtatWtataataSgctaHagcDVcNtaatcaagtaBDaWaDgtttagYctaNccDtaKtaHcttaataacccaRKtacaVaatNgcWRaMgaattatgaBaaagattVYaHMDcaDHtcRcgYtcttaaaWaaaVKgatacRtttRRKYgaatacaWVacVcRtatMacaBtactggMataaattttHggNagSctacHgtBagcgtcgtgattNtttgatSaaggMttctttcttNtYNagBtaaacaaatttMgaccttacataattgYtcgacBtVMctgStgMDtagtaRctHtatgttcatatVRNWataDKatWcgaaaaagttaaaagcacgHNacgtaatctttMRtgacttttDacctataaacgaaatatgattagaactccSYtaBctttaataacWgaaaYatagatgWttcatKtNgatttttcaagHtaYgaaRaDaagtaggagcttatVtagtctttcattaaaatcgKtattaRttacagVaDatgcatVgattgggtctttHVtagKaaRBtaHtaaggccccaaaaKatggtttaMWgtBtaaacttcactttKHtcgatctccctaYaBacMgtcttBaBaNgcgaaacaatctagtHccHtKttcRtRVttccVctttcatacYagMVtMcagaMaaacaataBctgYtaatRaaagattaaccatVRatHtaRagcgcaBcgDttStttttcVtttaDtKgcaaWaaaaatSccMcVatgtKgtaKgcgatatgtagtSaaaDttatacaaacatYaRRcVRHctKtcgacKttaaVctaDaatgttMggRcWaacttttHaDaKaDaBctgtaggcgtttaHBccatccattcNHtDaYtaataMttacggctNVaacDattgatattttacVttSaattacaaRtataNDgacVtgaacataVRttttaDtcaaacataYDBtttaatBaDtttYDaDaMccMttNBttatatgagaaMgaNtattHccNataattcaHagtgaaggDgatgtatatatgYatgaStcataaBStWacgtcccataRMaaDattggttaaattcMKtctMacaBSactcggaatDDgatDgcWctaacaccgggaVcacWKVacggtaNatatacctMtatgatagtgcaKagggVaDtgtaacttggagtcKatatcgMcttRaMagcattaBRaStctYSggaHYtacaactMBaagDcaBDRaaacMYacaHaattagcattaaaHgcgctaaggSccKtgaaKtNaBtatDDcKBSaVtgatVYaagVtctSgMctacgttaacWaaattctSgtDactaaStaaattgcagBBRVctaatatacctNttMcRggctttMttagacRaHcaBaacVKgaataHttttMgYgattcYaNRgttMgcVaaacaVVcDHaatttgKtMYgtatBtVVctWgVtatHtacaaHttcacgatagcagtaaNattBatatatttcVgaDagcggttMaagtcScHagaaatgcYNggcgtttttMtStggtRatctacttaaatVVtBacttHNttttaRcaaatcacagHgagagtMgatcSWaNRacagDtatactaaDKaSRtgattctccatSaaRttaaYctacacNtaRtaactggatgaccYtacactttaattaattgattYgttcagDtNKttagDttaaaaaaaBtttaaNaYWKMBaaaacVcBMtatWtgBatatgaacVtattMtYatMNYDKNcKgDttDaVtaaaatgggatttctgtaaatWtctcWgtVVagtcgRgacttcccctaDcacagcRcagagtgtWSatgtacatgttaaSttgtaaHcgatgggMagtgaacttatRtttaVcaccaWaMgtactaatSSaHtcMgaaYtatcgaaggYgggcgtgaNDtgttMNgaNDMtaattcgVttttaacatgVatgtWVMatatcaKgaaattcaBcctccWcttgaaWHtWgHtcgNWgaRgctcBgSgaattgcaaHtgattgtgNagtDttHHgBttaaWcaaWagcaSaHHtaaaVctRaaMagtaDaatHtDMtcVaWMtagSagcttHSattaacaaagtRacMtRtctgttagcMtcaBatVKtKtKacgagaSNatSactgtatatcBctgagVtYactgtaaattaaaggcYgDHgtaacatSRDatMMccHatKgttaacgactKtgKagtcttcaaHRVtccttKgtSataatttacaactggatDNgaacttcaRtVaagDcaWatcBctctHYatHaDaaatttagYatSatccaWtttagaaatVaacBatHcatcgtacaatatcgcNYRcaataYaRaYtgattVttgaatgaVaactcRcaNStgtgtattMtgaggtNttBaDRcgaaaagctNgBcWaWgtSaDcVtgVaatMKBtttcgtttctaaHctaaagYactgMtatBDtcStgaccgtSDattYaataHctgggaYYttcggttaWaatctggtRagWMaDagtaacBccactacgHWMKaatgatWatcctgHcaBaSctVtcMtgtDttacctaVgatYcWaDRaaaaRtagatcgaMagtggaRaWctctgMgcWttaagKBRtaaDaaWtctgtaagYMttactaHtaatcttcataacggcacBtSgcgttNHtgtHccatgttttaaagtatcgaKtMttVcataYBBaKtaMVaVgtattNDSataHcagtWMtaggtaSaaKgttgBtVtttgttatcatKcgHacacRtctHatNVagSBgatgHtgaRaSgttRcctaacaaattDNttgacctaaYtBgaaaatagttattactcttttgatgtNNtVtgtatMgtcttRttcatttgatgacacttcHSaaaccaWWDtWagtaRDDVNacVaRatgttBccttaatHtgtaaacStcVNtcacaSRttcYagacagaMMttttgMcNttBcgWBtactgVtaRttctccaaYHBtaaagaBattaYacgatttacatctgtaaMKaRYtttttactaaVatWgctBtttDVttctggcDaHaggDaagtcgaWcaagtagtWttHtgKtVataStccaMcWcaagataagatcactctHatgtcYgaKcatcagatactaagNSStHcctRRNtattgtccttagttagMVgtatagactaactctVcaatMctgtttgtgttgccttatWgtaBVtttctggMcaaKgDWtcgtaaYStgSactatttHgatctgKagtagBtVacRaagRtMctatgggcaaaKaaaatacttcHctaRtgtDcttDattaggaaatttcYHaRaaBttaatggcacKtgctHVcaDcaaaVDaaaVcgMttgtNagcgtaDWgtcgttaatDgKgagcSatatcSHtagtagttggtgtHaWtaHKtatagctgtVgattaBVaatgaataagtaatVatSttaHctttKtttgtagttaccttaatcgtagtcctgBcgactatttVcMacHaaaggaatgDatggKtaHtgStatattaaSagctWcctccRtataBaDYcgttgcNaagaggatRaaaYtaWgNtSMcaatttactaacatttaaWttHtatBattgtcgacaatNgattgcNgtMaaaKaBDattHacttggtRtttaYaacgVactBtaBaKtgBttatgVttgtVttcaatcWcNctDBaaBgaDHacBttattNtgtDtatttVSaaacaggatgcRatSgtaSaNtgBatagttcHBgcBBaaattaHgtDattatDaKaatBaaYaaMaataaataKtttYtagtBgMatNcatgtttgaNagtgttgtgKaNaSagtttgaSMaYBcaaaacDStagttVacaaaaactaaWttBaagtctgtgcgtMgtaattctcctacctcaNtttaaccaaaaVtBcacataacaccccBcWMtatVtggaatgaWtcaaWaaaaaaaaWtDtaatatRcctDWtcctaccMtVVatKttaWaaKaaatataaagScHBagaggBaSMtaWaVtatattactSaaaKNaactatNatccttgaYctattcaaaVgatttYHcRagattttaSataggttattcVtaaagaKgtattattKtRttNcggcRgtgtgtWYtaacHgKatKgatYtacYagDtWcHBDctctgRaYKaYagcactKcacSaRtBttttBHKcMtNtcBatttatttttgSatVgaaagaWtcDtagDatatgMacaacRgatatatgtttgtKtNRaatatNatgYcaHtgHataacKtgagtagtaacYttaNccaaatHcacaacaVDtagtaYtccagcattNtacKtBtactaaagaBatVtKaaHBctgStgtBgtatgaSNtgDataaccctgtagcaBgtgatcttaDataStgaMaccaSBBgWagtacKcgattgaDgNNaaaacacagtSatBacKDgcgtataBKcatacactaSaatYtYcDaactHttcatRtttaatcaattataRtttgtaagMcgNttcatcBtYBagtNWNMtSHcattcRctttttRWgaKacKttgggagBcgttcgcMaWHtaatactgtctctatttataVgtttaBScttttaBMaNaatMacactYtBMggtHacMagtaRtctgcatttaHtcaaaatttgagKtgNtactBacaHtcgtatttctMaSRagcagttaatgtNtaaattgagagWcKtaNttagVtacgatttgaatttcgRtgtWcVatcgttaaDVctgtttBWgaccagaaagtcSgtVtatagaBccttttcctaaattgHtatcggRattttcaaggcYSKaagWaWtRactaaaacccBatMtttBaatYtaagaactSttcgaaScaatagtattgaccaagtgttttctaacatgtttNVaatcaaagagaaaNattaaRttttaVaaaccgcaggNMtatattVctcaagaggaacgBgtttaacaagttcKcYaatatactaaccBaaaSggttcNtattctagttRtBacgScVctcaatttaatYtaaaaaaatgSaatgatagaMBRatgRcMcgttgaWHtcaVYgaatYtaatctttYttatRaWtctgBtDcgatNatcKaBaDgatgtaNatWKctccgatattaacattNaaacDatgBgttctgtDtaaaMggtgaBaSHataacgccSctaBtttaRBtcNHcDatcDcctagagtcRtaBgWttDRVHagattYatgtatcWtaHtttYcattWtaaagtctNgtStggRNcgcggagSSaaagaaaatYcHDtcgctttaatgYcKBVSgtattRaYBaDaaatBgtatgaHtaaRaRgcaSWNtagatHaacttNctBtcaccatctMcatattccaSatttgcgaDagDgtatYtaaaVDtaagtttWVaagtagYatRttaagDcNgacKBcScagHtattatcDaDactaaaaaYgHttBcgaDttggataaaKSRcBMaBcgaBSttcWtgNBatRaccgattcatttataacggHVtaattcacaagagVttaaRaatVVRKcgWtVgacctgDgYaaHaWtctttcacMagggatVgactagMaaataKaaNWagKatagNaaWtaaaatttgaattttatttgctaaVgaHatBatcaaBWcBgttcMatcgBaaNgttcgSNaggSaRtttgHtRtattaNttcDcatSaVttttcgaaaaattgHatctaRaggSaNatMDaaatDcacgattttagaHgHaWtYgattaatHNSttatMSgggNtcKtYatRggtttgtMWVtttaYtagcagBagHaYagttatatggtBacYcattaRSataBatMtttaaatctHcaaaSaaaagttNSaaWcWRccRtKaagtBWtcaaattSttMtattggaaaccttaacgttBtWatttatatWcDaatagattcctScacctaagggRaaYtaNaatgVtBcttaaBaacaMVaaattatStYgRcctgtactatcMcVKatttcgSgatRHMaaaHtagtaaHtVgcaaataatatcgKKtgccaatBNgaaWcVttgagttaKatagttcaggKDatDtattgaKaVcaKtaataDataataHSaHcattagttaatRVYcNaHtaRcaaggtNHcgtcaaccaBaaagYtHWaaaRcKgaYaaDttgcWYtataRgaatatgtYtgcKtaNttWacatYHctRaDtYtattcBttttatcSataYaYgttWaRagcacHMgtttHtYttYaatcggtatStttcgtRSattaaDaKMaatatactaNBaWgctacacYtgaYVgtgHtaaaRaaRgHtagtWattataaaSDaaWtgMattatcgaaaagtaYRSaWtSgNtBgagcRYaMDtactaacttaWgtatctagacaagNtattHggataatYttYatcataDcgHgttBttctttVttgccgaaWtaaaacgKgtatctaaaaaNtccDtaDatBMaMggaatNKtatBaaatVtccRaHtaSacataHattgtttKVYattcataVaattWtcgtgMttcttKtgtctaacVtatctatatBRataactcgKatStatattcatHHRttKtccaacgtgggtgRgtgaMtattattggctatcgtgacMtRcBDtcttgtactaatRHttttaagatcgVMDStattatYBtttDttgtBtNttgRcMtYtgBacHaWaBaatDKctaagtgaaactaatgRaaKgatccaagNaaaatattaggWNtaagtatacttttKcgtcggSYtcttgRctataYcttatataaagtatattaatttataVaacacaDHatctatttttKYVatHRactttaBHccaWagtactBtcacgaVgcgttRtttttttSVgtSagtBaaattctgaHgactcttgMcattttagVtaagaattHctHtcaDaaNtaacRggWatagttcgtSttgaDatcNgNagctagDgatcNttKgttgtaDtctttRaaYStRatDtgMggactSttaDtagSaVtBDttgtDgccatcacaMattaaaMtNacaVcgSWcVaaDatcaHaatgaattaMtatccVtctBtaattgtWattatBRcWcaatgNNtactWYtDaKttaaatcactcagtRaaRgatggtKgcgccaaHgaggatStattYcaNMtcaBttacttatgagDaNtaMgaaWtgtttcttctaHtMNgttatctaWWatMtBtaaatagDVatgtBYtatcggcttaagacMRtaHScgatatYgRDtcattatSDaHggaaataNgaWSRRaaaBaatagBattaDctttgHWNttacaataaaaaaatacggtttgHgVtaHtWMttNtBtctagtMcgKMgHgYtataHaNagWtcaacYattaataYRgtaWKgaBctataaccgatttaHaNBRaRaMtccggtNgacMtctcatttgcaattcWgMacttacaaDaaNtactWatVtttagccttMaatcagVaagtctVaaDaBtattaattaYtNaYtggattaKtaKctYaMtattYgatattataatKtVgDcttatatNBtcgttgtStttttMagaggttaHYSttcKgtcKtDNtataagttataagSgttatDtRttattgttttSNggRtcaaKMNatgaatattgtBWtaMacctgggYgaSgaagYataagattacgagaatBtggtRcVHtgYggaDgaYaKagWagctatagacgaaHgtWaNgacttHRatVaWacKYtgRVNgVcSgRWctacatcKSactctgWYtBggtataagcttNRttVtgRcaWaaatDMatYattaactttcgaagRatSctgccttgcRKaccHtttSNVagtagHagBagttagaccaRtataBccataatSHatRtcHagacBWatagcaMtacaRtgtgaaBatctKRtScttccaNaatcNgtaatatWtcaMgactctBtWtaaNactHaaaaRctcgcatggctMcaaNtcagaaaaacacagtggggWttRttagtaagaVctVMtcgaatcttcMaaaHcaHBttcgattatgtcaDagcYRtBtYcgacMgtDcagcgaNgttaataatagcagKYYtcgtaBtYctMaRtaRtDagaaaacacatgYaBttgattattcgaaNttBctSataaMataWRgaHtttccgtDgaYtatggtDgHKgMtatttVtMtVagttaRatMattRagataaccctKctMtSttgaHagtcStctatttccSagatgttccacgaggYNttHRacgattcDatatDcataaaatBBttatcgaHtNHaaatatDNaggctgaNcaaggagttBttMgRagVatBcRtaWgatgBtSgaKtcgHtttgaatcaaDaHttcSBgHcagtVaaSttDcagccgttNBtgttHagYtattctttRWaaVtSttcatatKaaRaaaNacaVtVctMtSDtDtRHRcgtaatgctcttaaatSacacaatcgHattcaWcttaaaatHaaatcNctWttaNMcMtaKctVtcctaagYgatgatcYaaaRactctaRDaYagtaacgtDgaggaaatctcaaacatcaScttcKttNtaccatNtaNatacatttHaaDHgcaDatMWaaBttcRggctMaagctVYcacgatcaDttatYtaatcKatWatcaatVYtNagatttgattgaYttttYgacttVtcKaRagaaaHVgDtaMatKYagagttNatWttaccNtYtcDWgSatgaRgtMatgKtcgacaagWtacttaagtcgKtgatccttNcttatagMatHVggtagcgHctatagccctYttggtaattKNaacgaaYatatVctaataMaaaYtgVtcKaYtaataacagaatHcacVagatYWHttagaaSMaatWtYtgtaaagNaaacaVgaWtcacNWgataNttcaSagctMDaRttgNactaccgataMaaatgtttattDtcaagacgctDHYYatggttcaagccNctccttcMctttagacBtaaWtaWVHggaaaaNatttaDtDtgctaaHHtMtatNtMtagtcatttgcaaaRatacagRHtatDNtgtDgaatVgtVNtcaaatYBMaaaagcaKgtgatgatMgWWMaHttttMgMagatDtataaattaaccaactMtacataaattgRataatacgBtKtaataattRgtatDagDtcRDacctatRcagagcSHatNtcaScNtttggacNtaaggaccgtgKNttgttNcttgaaRgYgRtNtcagttBcttttcHtKtgcttYaaNgYagtaaatgaatggWaMattBHtatctatSgtcYtgcHtaattHgaaMtHcagaaSatggtatgccaHBtYtcNattWtgtNgctttaggtttgtWatNtgHtgcDttactttttttgcNtactKtWRaVcttcatagtgSNKaNccgaataaBttataataYtSagctttaaatSttggctaaKSaatRccgWHgagDttaaatcatgagMtcgagtVtaDggaBtatttgDacataaacgtagYRagBWtgDStKDgatgaagttcattatttaKWcataaatWRgatataRgttRacaaNKttNtKagaaYaStaactScattattaacgatttaaatgDtaattagatHgaYataaactatggggatVHtgccgtNgatNYcaStRtagaccacWcaMtatRagHgVactYtWHtcttcatgatWgagaKggagtatgaWtDtVtNaNtcgYYgtaaactttaDtBactagtaDctatagtaatatttatatataacgHaaaRagKattSagttYtStagagagacgatgaaaattaatcgtcaatacgctggcgaacactgagggggacccaatgctcttctcggtctaaaaaggaatgtgtcagaaattggtcagttcaaaagtagaccggatctttgcggagaacaattcacggaacgtagcgttgggaaatatcctttctaccacacatcggattttcgccctctcccattatttattgtgttctcacatagaattattgtttagacatccctcgttgtatggagagttgcccgagcgtaaaggcataatccatataccgccgggtgagtgacctgaaattgtttttagttgggatttcgctatggattagcttacacgaagagattctaatggtactataggataattataatgctgcgtggcgcagtacaccgttacaaacgtcgttcgcatatgtggctaacacggtgaaaatacctacatcgtatttgcaatttcggtcgtttcatagagcgcattgaattactcaaaaattatatatgttgattatttgattagactgcgtggaaagaaggggtactcaagccatttgtaaaagctgcatctcgcttaagtttgagagcttacattagtctatttcagtcttctaggaaatgtctgtgtgagtggttgtcgtccataggtcactggcatatgcgattcatgacatgctaaactaagaaagtagattactattaccggcatgcctaatgcgattgcactgctatgaaggtgcggacgtcgcgcccatgtagccctgataataccaatacttacatttggtcagcaattctgacattatacctagcacccataaatttactcagacttgaggacaggctcttggagtcgatcttctgtttgtatgcatgtgatcatatagatgaataagcgatgcgactagttagggcatagtatagatctgtgtatacagttcagctgaacgtccgcgagtggaagtacagctgagatctatcctaaaatgcaaccatatcgttcacacatgatatgaacccagggggaaacattgagttcagttaaattggcagcgaatcccccaagaagaaggcggagtgacgttgaacgggcttatggtttttcagtacttcctccgtataagttgagcgaaatgtaaacagaataatcgttgtgttaacaacattaaaatcgcggaatatgatgagaatacacagtgtgagcatttcacttgtaaaatatctttggtagaacttactttgctttaaatatgttaaaccgatctaataatctacaaaacggtagattttgcctagcacattgcgtccttctctattcagatagaggcaatactcagaaggttttatccaaagcactgtgttgactaacctaagttttagtctaataatcatgattgattataggtgccgtggactacatgactcgtccacaaataatacttagcagatcagcaattggccaagcacccgacttttatttaatggttgtgcaatagtccagattcgtattcgggactctttcaaataatagtttcctggcatctaagtaagaaaagctcataaggaagcgatattatgacacgctcttccgccgctgttttgaaacttgagtattgctcgtccgaaattgagggtcacttcaaaatttactgagaagacgaagatcgactaaagttaaaatgctagtccacagttggtcaagttgaattcatccacgagttatatagctattttaatttatagtcgagtgtacaaaaaacatccacaataagatttatcttagaataacaacccccgtatcatcgaaatcctccgttatggcctgactcctcgagcttatagcatttgtgctggcgctcttgccaggaacttgctcgcgaggtggtgacgagtgagatgatcagtttcattatgatgatacgattttatcgcgactagttaatcatcatagcaagtaaaatttgaattatgtcattatcatgctccattaacaggttatttaattgatactgacgaaattttttcacaatgggttttctagaatttaatatcagtaattgaagccttcataggggtcctactagtatcctacacgacgcaggtccgcagtatcctggagggacgtgttactgattaaaagggtcaaaggaatgaaggctcacaatgttacctgcttcaccatagtgagccgatgagttttacattagtactaaatcccaaatcatactttacgatgaggcttgctagcgctaaagagaatacatacaccaccacatagaattgttagcgatgatatcaaatagactcctggaagtgtcagggggaaactgttcaatatttcgtccacaggactgaccaggcatggaaaagactgacgttggaaactataccatctcacgcccgacgcttcactaattgatgatccaaaaaatatagcccggattcctgattagcaaagggttcacagagaaagatattatcgacgtatatcccaaaaaacagacgtaatgtgcatcttcgaatcgggatgaatacttgtatcataaaaatgtgacctctagtatacaggttaatgttagtgatacacaatactcgtgggccatgggttctcaaataaaatgtaatattgcgtcgatcactcacccacgtatttggtctaattatgttttatttagtgacaatccaatagataaccggtcctattaagggctatatttttagcgaccacgcgtttaaacaaaggattgtatgtagatggtaccagtttaattgccagtgggcaatcctaagcaaaatgagattctatcctaaagtttgggcttgatataagatttcggatgtatgggttttataatcgttggagagctcaatcatgagctaatacatggatttcgctacctcaccgagagaccttgcatgaagaattctaaccaaaagtttaataggccggattggattgagttaattaagaccttgttcagtcatagtaaaaacccttaaattttaccgattgacaaagtgagcagtcgcaataccctatgcgaaacgcctcgatagtgactaggtatacaaggtttttgagttcctttgaaatagttaactaatttaaaattaattaacgacatggaaatcacagaacctaatgctttgtaggagttatttatgctgtttactgcctctacaaccctaataaagcagtcctaagaatgaaacgcatcttttagttcagaaagtggtatccagggtggtcaatttaataaattcaacatcgggtctcaggatattcggtcatataatttattaagggctcttcgagtcttactctgagtgaaattggaaacagtcatccttttcgttgtgaggcatcttacaccgctatcgatatacaatgcattccaccgcggtgtcccgtacacaaggaaacttgttaccttggggatataagaaaactcacacgtctcattattaaactgagtacaatttttgcacgagaaagtaatgcaatacaatatgatgaaagccagctaatgaaaagggatggaacgcacctcggatctgttgcactggattaaaatccgattatttttaaaaatattcagtgctagagcatatcaggtctacttttttatctggtatgtaaagcccacggagcgatagtgagatccttacgactcaacgaaaagttataacataactcccgttagccaaagcccaatcccgattactgccctaccctaacgtctgccatctaaatatcgaacttgttatgatcaatgtgactacctcccaccctttccccttcatttgttccactggggataagctagcgttttcagaatcaatgcaataagaatagccaattgtctcacttcatcagagctcttggcaattccaggcgctacgtggttctggaatatattcatttttcaaatagtaatacgtttagtgttgctattgtctacacgtttggatattacgttatgtgagcggacatcaatagttgtctaactctttagtaagccagagatagcactcttagcgaatggataccatcttccataagtttagttaatagtccgaaacaactgcttcgagcatatttgaacctccttgtaggcaaatagcctcttcaaagcaatcttactaatagatagagtttgttttaagggactactagaaatgggacaatcttaatagtatgacctaaactgacatttaaagatatatccaggtggcaagcataaagatcattgcgccacctccaccgtgggattacttatcagtcgatatcctatatgctaagtttgcgacggcagaatacaaactaagctgagttgatgctaaccttacctatgataccccattggaccggttaacagccctacttattccaaataaaagaacttttatgctgtagaagctattatagtgatgcctggtaacttcagtatattaaaatgacacacatacgccatatagagctcctggaactttgaataatgagcgaacttcgaagttgaagagcaagaaaccatatgtcacggttgcctaaagcccggtaaccagacatgtgctatcattgatcattatcgaggttttcataaccttgacccattatcggctgtgcgcggacaagtacttaaatcactagtttcttcacctgcttatcggtaagaaataaggttggcaaagaatcgcataagacggacgtagagccgcagcgttgtgcgagtccaggtgcatgcgcagcaataggattttaaattttgttccatttttaatttagccgtaaggatgtccgtaaatgattgaaaattggattcaatctttgggcctatgctactggaacctgatcgacaaaatttcaaacatacgttaactccgaaagaccgtatttttgcggctagaatagtcagtcgcttggagccatataccttaccacttaaacgacgtgctcctgtagttgaaatataaacagaacacaaagactaccgatcatatcaactgaagatctttgtaactttgaggcgaagcaccctcttcgagacaactaagagtaaagtaccgggcgccgcaaggagtcgattgggaccctaaatcttgacgaattgctaagaggctcagagctaccactgtaatttctctagagcccataataaatgaacgatacatccgtaggtagcacctaagggattataatggaagccaaatgcagttaataatattatatactggcgtacacgattcgacggatctctcacatagtgattcacgacccccccctttgattgacacagcgtcagcattttgcaagaacgatcttctgcatagggtgcgccaccgtaaggatgacgtcgaagctacaactgggtataatttaccatgcttccctgatgctgagtgcaatacactaagaatgagtttttaccccatatcaccagtatttgttctgttattgcgaagaaatggctatgctgagttggcgactaaagtcacccatcctttttattaggtaaccccctcccttaaactaactgatttgctggagctgccctgcatacatatactttatcatttatggacgtccgtgacgcttattatccaccatagtcgatatgctacacggattcattaatggatcgtaggagtttaagttatatttactaagatcggtctcggctactatcccgccttacccggcgctatttacggccatttttaatatattgacggtaattattcctatggtttcgaccgcacgtccttggacaagaaagaatggcaaaaaaaatgtaaaagaaaaaaaatattgagtccctaccatcatataaaaaatatgtgatgagtaacttgacgaaatgttagtggttattaaagactatctattacaccttttgttttctgtcgtagtatattaaagtctagaagccttacaggaaaatcagggttatacagccgatactccgcagcatgaatcatcgaggaggtgtcctaccatcgcgccttgtaatcttgtctgtgtatactgtatttagaccttttatacaaagtaaatatctcggctttatgtgattgggaggggcctactcaaacatgatgacttgacctaataatcactgtgcgggcgtcttatgactagctattccttgaaatccaccaccaaatggttaatatgtaaaaactttgacgatgaaacaaggtgaatgtgtagttactttgtgtaattagctgcgtcgagcattgcttgtaaaaccgtcaatcgcacacgttacttccataaaatttctacgaatacacccttcttaaaaaaaacgtaggaattcacgagtttaacaaacgataactgtataaagtggaagtccgaagaaagcagatgcccgaactactcgaagatgtttcgttttcttaaccataggggcttcttaatggcccactacgcacattttgttcaagcccgagagggacatccccattacgggagtattactaaaactgttccgtaatacgttcagcaagggatgaaaaaggccactgctcaagttattgacgtgggagtattacatcggaagcctgaatcccacactatgatggtctgtacaggcctagggactgcgtctagacggtattaccggcttctaatcatacgatcgtgagtcttaacgggaagtaaggctcacacctaccccaaaccatttatctatgtaagtataaaattgtgcgtaagtgttcaaagtggacaataaagacgtggcaaaaacccccgcacataagccgctttagatttcacaaataccaatgcggttaaaaacatccttgagtcgtacatacaccatactcgcgttaaacggatataacagaagataataaatccggatgtggagtcggtgtaactatagaaagccaagtgaaataatgcttaccagtcatttagctatacggctttcatttcatgtcaagagggtggagtttgacctgtacagttgatatatcaccgatacttagaactcacctaaagctaaaattgctcgcagcgtgtaatccgcatattacaaacaatagatgggattcattatacataagacacgatgatctgctttttcaggttgcgagatgttgcctatcgtcaatcgagtcctgccttacaccacttaaacaaaagtattgacagggaacctattttcgaggtattatatagtccagcttgaatatcaatttgacagttaacctagtgaaaatcagtaagaggaaatacgccacattctccagtgaaattctacgggttatcgtctagtccaactatcaattataactcacgagatataagtaaattctcgtacttggcctgatttttattatactttggatccttagtaaacaggaagggagaaaccttcaacgaaaaacactggattttgttttactctcaaagctcttatatgacggaaataccctgtcaagtcttaactttattactagactaatgaaatgggcttggggtggccagaatcatagtacaatttagcggatacactattcggactttcctatcggctgtctggttggataagtatggggactaataggctagacatacctatacttaaactatacaggcgtcatctatctctgcaactttggagttccctgatgttctcccgccctttgggttcacatcttctataccgacacccctaataacgattagtttgtgggttagagtaaattaatacggttaatattaatgtatcgttgaaaagctggtgtcgccaataaggtaaccggctaggcagagtatatgtcacgaagtataactaccctaatgataagctgtaggaataaaattaatgctgtctctaagcgaagagatatttccgactctgttttaatgacgaatctcattacttctgacttgcaaatgttcaatatggcacggtttcacggcacctttgtgacgcatataatgaacttagaagattataacgacggaactttatatgataatccgttacgattaaagaatctgttaaatatcataatggcattcagttctagaccgtgcatcatggtaaacttactttctctgcatggcgacatacatttcgctattcaaattcgcgtgtggttacacccactcgcacctttggaatattaagagaagatgatcagaaaatccattcgctcaatttttctgacgtacgtctaatttatcctaggagacaaatcgttttatgtctctcacatttttgaagaaaggttcgagagacaatactcaggtcctgaactgctagaagatactcggtggagcgtggcaacaatgaaaaactcgtgacataaatgaatgatacttttccaagttcagttaagtgaatatgtttaacatacccggcttttcgatcttaagctgacgctggacgtgcgagtaatgtcagtctcttacatacactagtgactccaagtttcgtcaaaaacgccccctcccttctcgagcccactcacgctatgtattgacgcgaacttgttcgggatcagacttttcaggagttcggtcgcgtgtccctatgtgctaatatataagttagatcgcattagatgctaatctgaatacttatagacgaccttcaacgagaacgggtaccaccttgaggctagagttaggtgtgaaacgacaggtagggacatataaaatttgagtgcggctttagttaagggtttaattacctactcaaacatcacgctcgcgcccttcgtacgtaatcgaccatctagaggctaaggggactgtactaggtagtgattaatgatatcctagacgcacgtgccttagatcttcagactctgatggtccgcgatcaccgtaattgtagtcctccaactcgatcactttgttggcgtcaaagaaattacgatatctaaatacttataatacaataaccaaggatgagaatgactcatcgcgttggagttatattgcttgaagttctatggaatgaaagcacgttatctgccgtcccaatatctccagtgagctaattcattggacggtccactttgatcaatccccgaggagatgttcggacactttagtctgtaacacttagcgttgagaccacgaacaattgattactcagtcttgaaggtgttttccaaagttcattttaaataagactacgataggcctttcctattgatataaactacccggctctgttgttcgtgtgagtcgtacttctctgtgtttttctgattatagcaagattcgattcttagtgtaaacagcgatttttatttgacccgtcaatgagaagcgcataggatctaagcaaaattatcaagttgtgccacaaggtaagatctttccagttattgcaggtaggatgtatcccacgttgatagtatgaggtctgacgtcaactgtctaggagagttgaccgcgtgcgggtacaccggatttgcatcgatgttgagaacgcagaactcccactgtcgtggcggcgttcctgatatttagcaagaggcgttgataaagccctcatcatctagatctcgacctcatctgccctcttgctccatcattttctacacagactactttcctatctacgttagtataattgctttctatcttagtatcatttagagcttctccgtcaacaggttcgtgctattaaagttagtacgaaagggacaacttgtagcaacgcatttaatcggttttcgactacttcgcacaaaatcagataaagaagtttgtcattctattagacattgaattgcgcaattgacttgtaccacttatgatcgaacactgaatcaagactgtgattaactaaaatagacaagccactatatcaactaataaaaacgcccctggtggtcgaacatagttgactacaggataattaattggactggagccattacattctctacaatcgtatcacttcccaagtagacaactttgaccttgtagtttcatgtacaaaaaaatgctttcgcaggagcacattggtagttcaatagtttcatgggaacctcttgagccgtcttctgtgggtgtgttcggatagtaggtactgataaagtcgtgtcgctttcgatgagagggaattcaccggaaaacaccttggttaacaggatagtctatgtaaacttcgagacatgtttaagagttaccagcttaatccacggtgctctactagtatcatcagctgtcttgcctcgcctagaaatatgcattctatcgttatcctatcaacggttgccgtactgagcagccttattgtggaagagtaatatataaatgtagtcttgtctttacgaagcagacgtaagtaataatgacttggaataccaaaactaaacatagtggattatcatactcaagaactctccagataaataacagtttttacgatacgtcaccaatgagcttaaagattaggatcctcaaaactgatacaaacgctaattcatttgttattggatccagtatcagttaaactgaatggagtgaagattgtagaatgttgttctggcctcgcatggggtctaggtgatatacaatttctcatacttacacggtagtggaaatctgattctagcttcgtagctgactatactcaaggaaccactgctcaaggtaggagactagttccgaccctacagtcaaagtggccgaagcttaaactatagactagttgttaaatgctgatttcaagatatcatctatatacagtttggacaattatgtgtgcgaaactaaaattcatgctattcagatggatttcacttatgccttagaaacagatattgcccgagctcaatcaacagttttagccggaaacaatcgaagcatagggacaatgtatcttttcctaaattgccatgtgcagatttctgagtgtcacgaagcgcataatagaatcttgtgttgcctcaactcgttgaaaagtttaaaacaatcgcagcagtctttttggggtctactgtgtgtttgcaaaataactgaaagaaacgcttgaacaactctgaagtagctcgagtactcattaaagtgtaacacattagtgaatatcggccaatgaaccaaacgcttcccggtacgctatctctctcatcgggaggcgatgtgcaggttatctacgaaagcatccctttacgttgagagtgtcgatgcatgaacctcattgtaacaatagcccagcaaattctcatacgtgcctcagggtccgggcgtactcctccatggaagggcgcgcatctagtgttataccaactcgctttttaactactatgctgtagttctacaggcatagtggccagtattttctaacttctctggatagatgctctcactcctcatccatcacggcttcagtttacgtcttacttgcttgttcagcaacggatggaggcattaagtatcttcactgttccctaaaattgctgttcaatatcaaagtaaggacgatacagggaaagctcaagcacactcattgaatactgccccagttgcaacctcacttaatctgacaaaaataatgactactctaagtgttgcggaagcagtctcttccacgagcttgtctgtatcacttcgtataggcatgtaactcgatagacacgaacaccgagtgagaaactatattcttgcttccgtgtgtgtgacaccaggtaattgatgcggatataagctggagatcactcacgcccacacaaggcgctgctacctctttattccaatgtgtaagaatttgctaacttcatttctagaccgcagctttgcggtcataatttcacggtacggacccttgggttagagacttgataacacacttcgcagtttccaccgcgcacatgttttagtggcttctaacatagaatttttgttgtgacataaagagtgcgtgggagacttgcccgaccgttaagccataatcaattgaaagccccgtgagtcacatctaattggttgtactgcgcatttagctatcctttagctgactcgaagagattcgattcctaatataggttaattagatggctgccgcgcgaagtaaaacgtgaaaaacgtagtgcgcagatctgcataactcgcgcttaattacttatgagtagttccaagttcgctacgttatgagagagattggaattaagcaaatatgttttatggtgattttgggatgagaaggactgctaagtacggctactaaacaaatttctaaaaccgccatctaccttatcttggagacatttaagttgtatatgtcactagtctagcttttgtctgtgggacgcgttctcggaatgagggaaatgcaagagccgattcatcaaatgcttatctaagaaagtagtggactattacaccaagcacgaatgccagggaactgctttcttgctcaggacctcgcgacaaggtaccccgcataagtcctagaattacatttggtcagcaatgctgacatttgaccgtgaaaacataattttaatcagaaggcagctcacccgcttgctctagatcttatctttgtatgaatgtcagaatttactgcaatatccgttccgaatagtgagggcttagtatagttctctgtatacaggtcacatcaaactccccctgtcctagtacagctctgagctttaattaattgcatacatttccttcaatcatcagatgaaaacaccgcgaatcatgctcttctcgtatagggcaagagaagcaacaaacaactagcccgactcacgttcatccgccgtatccttgttcagttcttactccgtattaggtcagcgaaatctaatcagaataatcggtcgcgtatcaaaattaaaatcccgcttgaggttgacaattaaaacgctgagcagttatcggctattagatagtggggtgaaagtaattggctggaattatgttaaaacgtgatattaagctaaaatacgctacttgttgccgacctaattcagtcattcgatattcagttagagccaagaataacaagcttgtataaattgaacggggtgcactaaacgatgtgttactctaatattcagcttggagtatacctgaaggcgaattcatgtatcggccaataataagacgttgaagatcacaatttggactagcaaaagaaggtgatttatgcgtggggattgagtccactgtacgagtacggtctctggaaaattataggttcagggaatataaggaagtaaagataattaccaagagatttttggtatcgctatgacccagaggtgttctaacgtctgttttgatccgcagaatttctgcctcaatgcatatttgacggacttgaactagagcctctaaagttaaatggcgacgcaactgttcctaaacttcaattattactactctttttttcctagggtattgtagaggccagtggacaaaataaatcaaatttaagatgtttcggacattaacatcccccgtagcatagaaatcatcagttatccaatctctcatcgagcttttacaatttctgctggcgctatggacagcatatgccgcgagacctccgcaagactcacttgatcactgtaagtatcttcattagaggttagagcctatagttaagctgctgacctagtaaaattggtattttctaattttattgctcaagttaaaggttagtgaagggataatgacgttatttttgaacaatgggttgtattcaattttatatcacgaatggaacccttcattcccggcataatactagacgacacgaacaagctccgatctatcagccaggcacgtgttaaggtttaattccggcaaaccaatgaagcatcaaaaggtgacctgatgcaacttagggtcacgatgagtttttcaggactacttattacctattaataagttaacatgagccttcataccccgtaagacaatacatactccaccaattagaattctgagccatcttatctttttgtatcatcgaagggtatggccgaataggttaattagttactcctaacgtctctacaggcatgcatttgacgcaccttcgaaaatagtcaatctctcgccacacgcgtctagtatgcagcatcaaaaatatagtccacggtttccggattaccaaacgcggcaaagagaaacattgtatcgacggagataacttaatacagaaggaaggggcatcttcgaatacggatgaataattctatctgtttattctgacatcttgttttcaggttaatcttacgcattcaaatgacgcctgccccatgcgtgcgcaattattttctaatattgacgagagcaatctcactccttttgggtctatttatgttttattgaggcacaagcctatacagaacaggtactattaaggccgtgagtgtgagactcaaaccgtggaaacaaaggatgggttgttcttggtacaagttttagtgcatgtgggcaatccttaccaaaatcagatgctatccttaactttgggctgcatttaagatggcggttggaggcctgtgagaatcctgcgtgtcatctttaatgaccgaattcatccatgtagattcagatcacacactcattccttgatgttgtctaaacaaaagttgttgtggacgcattggagggagttaagtaacaacttgggatcgcatacttataaaaattatatgttaaactttcacaaacgctgaagtccaaagtaactagcccaaacgcctcgagagtcactaggtattaatggtgtttgagttcctgtgaaatagtgttcgaaggtaaaatttatgtaccaaatcgaaagaacacttaataaggcttgcttgcacggaggtatgatgtttactgactctacaaccctaattttccagtacgtacattcattccaataggttagttctcaaagtgctatacaggctcctcaattgatgatatgcttcagccgctctatggatattagctcattttatttaggaagcccgcttagaggcttactatgagggaaatgccaaaatgtcatacttttcggtgtgtcccatatgacaccgctttacatagaatttgaattaaaacgcgctctcccgttcactaccatacttggtaccgtgcgcatattacatatagatataggatcattttttaaagctgtactaggtttgatcgacaatcttatgctatactatatgatgtaaccctcataatcaataccgatcgtacgatcctagcataggtggcaagcgattttatgccgattattgtgttaaatagtctgtgagtgtgattatcagggctacgttggtagaggggttgtatagacctcgcacacattgtgacatacttaacaatatacgaaaactgatataataaatccccttacccaaacaccaatcccgttgaatcaactaccataacgtctcccatataaattgcctacttgtttgcataaatctgaatacataacaccattgcaccttcttgtgttccaatcccgttaagattgccttgtcagatgatatgcaagaacaatagcatttgctagcaattattaacagctcttcgaattgcctccacataacgcgggagggtatattttaatttggcaaatactaagtactgttggcgtcatatgctattaacggttggatattaagttatgtcagccgtaagcaagagtgggcgaaatattttgttacccagtgagagcactcttagagtttggatacaataggccatatgttgacttaagaggacgtaactacgccgtacaccattgttcaaccgacttcttggcaaatagaatcgtattagcaatcttaagaatagagacacgttcgtgttagggtatactacaaatccgaaaatcttaagaggatcacctaaactgaaatttatacatatttcaacgtggatagatttaacataattcagccacctccaacctgggagtaattttcagtagatttactagatgattagtggcccaacgcacttgactatataagatctggggatcctaacctgacctatgagacaaaattggaaacgttaacagcccttatgtgtacaaagaaaagtaagttgttgctgttcaacagatgatagtcatgacgcgtaacttcactatagtaaattgaaacaaatacgcaatttagacagaatggtacggtcatgaatgacagtaattcgaagtgctagaccaacttaaaataggtaaacgtgcccgaaaccccccttaacagaaagctgctatcatggtgcagtatcgacgtgttcagaaacttgtaacttttgagcaggtccgagcacatggaagtatatcacgtgtttctgaaccggcttatccctaagatatatccgtcgcaaactttcgatttagtcccacgtagagcccaagcgttgtgcgactccacgtgcatgcccagaaatacgagtttaaatttggttacatggttaattttgaccgaagcatcgcactttatgattgataattggattcaatatgtcgccctatgcgaatgcaacatgatccacaatttggctataagacgtttaatccgtatcacactttgtttgcggctagtatagtaacgcccgtgcaccaagagtcagtaacaattataagtactccgcaggtacttcaaatataaaaactaatcaaacacgacccatatgatcatctgaagatatttggaactttctcgacaaccaccctcgtactcaatacttacactaatcgacaggcacacgcaacgtgtacagtcgcaccatattgagtcaagatttgcttagtggcgatgagcgtacacgcttatttctctagtcacaattagttatctacgagacatcacgagggagcaaataagcgatgttatggctacacataggcacgtatgaatatgatataagccagttaaacagtcgaaccatcgagcaaattctcatgcaccaacccacacgttgaggcacaaagagtaagctgtttgaatgtaacttcttctgctgagcgggccccaacgtaaggatcaactagaagagaaaactcggtattagtttaaatgcgtcacggagcatgagtgcatttcactaagaatgtctgtgtaaccaatataacatctatttgttatctgattgcctacttatggctttgcggtcgtggcgactaatgtctccaatccttttgaggtcggtaccaactccctttaaattacgctgtgcaggctcatgcactgcatacatatacggtagcaggtagggacctcacgcacccttattataatcaatagtagttatcagtcaacgaggcaggaatgctgaggtcgaggtgttggtatattttctatgtgccgtctaggcgactatcacgcattaccaggcgagatttaagccaattttgaatatagtcaacgtaatttttactatgggttccaccgaaacgccttgcacaactaagaatcccataaaatatcgatatcaaataaaagattgtgtcaataccttcatatatattttttcggttgactaacgtgaactaaggttaggggttttgtatgtctatataggaaacagtttcttttctgtcctactttagtaaagtcttcaagccttactccaaaatcacggtgattaagccgttactcagcagcatgattctgcctgctcgggtcctaaaatccagccttgtaagagtcgctgtgtattagctagggagacctttgttaaaaaggatatatcgcggcgggatgtgagtgcgtggcgcatactcaatcttcagctcgtgtcattataatatctctcccccacgcttttcactagatatgccgtgtaagcaaacaccttatgcttaatttcgaaaatattggtacttgaaaaaagctgtaggggtacttaatgtctggtaggagatcaggagagaattgagtgtaaaaccgtaaagccctcacctgacttcatgtaaatggcttagaagactccatgatttaataaatactacgaaggaaagactggatctaaagataactctagtaaggccaactcccttcaatgctgttgccagttataatccaagagctgtccttttctgaaccatagcggcttctgaagcgaactagaagcaaagttggttctagccagacagccacataccctgtacgggtgtattactaaaactggtccggtattagttcaccaagggaggaattaggcaaaggatctaggtatgcaagtcggagtattacatccctaccctgaatccatcaataggttcctctgtactggccttcgcaatgagtattcaaggttgtacagccgtataataataagatagtgactatgaacgggaagtaacccgctcaccttccccaaaacattgttatatctaagtattaaagtctgccgtagtgttaatactcgaaaataaacaactggcaaattacaccgcacttaagccgcttttgatttatatttttccaatgcgcttttaaaaataattcagtcctacatactaattaagacccttaaacggagatatcacaagttaagttttaaccatctcgactaggtggaactatagatacccaactcaatttatcattacctgtaatgttcctagaaggattgcatttcatgtcaagacggtggagtttcacagcgaaacttcagtgtgaacagattctgagaaatcacctaaacctattagtcagagcacccggttagaaccagttgtcaaaaaatagagcggttgcatgagacagaagtaacgatgagatccgttgtaacgttgagacatctggcctatcgtcaatacagtcctcccttaaaaatatttttaaatactaggcaaacccaacataggttagtcctatgtgatacgccacatggtatatcattttgtaacgttacctagggataatcaggaagtggaattacgcaaaagtagacagtgaaatgcttagggttatagtctagtccaaagataaaggataaagcacgtcagagaactatattagccgaatgggaatcattgttaggagactgtggatcatgtctaaaaagcaacgcagaaacagtcatcgaaaaaatctcgtttttgtttgaatctaaaagagctttgatgaccgatagtacctgtatactagttactgtattacgtgtctaatgatttcggattggggtccccagaatcagacgtcattgtagacgattcaagtttaccaatttaatttcccagctctccttggagaactatcgccaataattgcagtcactttccttttctgaaacgataaagccgtcagagttctctgcaacgttggacttacctgaggttctaacccactttcggttctaatagtagttaacgacacaacgaataacctttactgtggggctttcacgatattttttcgcttattattaatggttacgtcataagctggtgtccaaattaaggttaccggcttcgcagagtagttgtatccaagtataacttccctaatcataagatcgaggtagaaaattaatgctgtctctaaccgaacagatatgtcccactatgtggtatggacgttgctaattacttctgaagggaaattggtcattatggatacgtgtctaccatcaggtcggacgcagatatggttctgtcttcagttgatccaccgttctttataggataataactgacgattaaagattatggtaaatagattaagccaattctcttcttgtcagtgaagcatccttaactgacttgctctgcagcccctcatacatttagctattcaaagtaccggctcgtttcaaactctcccacctttggaagaggttgtcaacttgataagtatatcatttacagcattttttcggacgtacctctaatgtttcattgcagaaaattagttttttctatcgcacattttgcaagtaacgttagagacacaattatctgcgaatgaactgctagatctgacgaccgggagcctcgcaaatatcaaaaaagactgacatatatcaaggagtcgttgacaagtgctggtaagtcaattggtttatctgtcccggcgtttcgatcttaagctgaccatgcacggcagagtaatgtcactctcgttcttacaagtctgtctccaagggtcggcaaaaaagacccctccattctcgagcccactcacgatatgtagggacgacaacttgtgcggcttatgaattgtctggactgcgggcgagggtccatatctccgaagttagaagggacatacctttagatgataagatcaattcttattgacgaaattcatccacaacggggaacaacttcaccctagacttacgtctgaaaagacacctagcgtcttataaaaggtcagtgccccgtttcgtaaggctggaattacctacgcaaacttaaacctcgcgcccttccttacgtatcgacaagatagaggctatcgcgaatgtactacggaggcatgaatcatatactagaaccaagtgcctgtgatattaacaagatgatccgacgcgagcaccgtaattctaggcataaaactccagcaatttgggggccgaaaacaaatgacgttagctaattaattatatgacatgatcaaaggaggtcaatcacgcatcgagttcgacgtatattcattgaacttcgtgcgtttgaaagaaacttttatgaaggcaaaattgatcctgtctcctatttcatgcgtacctcctagttgataattccccgagcagtggttaggacacttttgtcggtatcaagttccggtctcaaaacgtaaaattctgtaatctgtatggatggtctgtgaattagttaatttttatgaagtcgtcgagacgcagttcctattgatttattctaaacggagatgtgcttcgtgggactcggaagtagatctgtgtttatgattattgctactttagatgctgactgttaactccgtgttgtttttcaaccgtatatcacaaccgaattggatagaacctatagtttcaagttctgccacaaggtatcatatttacagttagtgctggttgcttctttcaaacgtggtgagtttgtgctatcacgtcaacggtagagctcagtggaccgagtgcgcgttcaaccctgttccagagagggtgtgatagcacatataccacgctcgtcgaggcgttcatgatagtttgcaagagccggtgttaaacacatattattattgttatccaactaatcggacctatgcataaagcattgtctaaacagaataattgcctatatacggtagttttagtgatttatatcttagtatcagttagagcttcgaactcttcaggttcctcatatttaacgttcttcgaaagcgaaaacttctacaaacgaatgtaagcggttttccaagtagtacctataaatcacagaaagatctgtctcagtatagttgaaatggtattcagctagtgacgtgtaccaattatcatagttcactcaagcaagacgctcattaacgaatatagacaagacactatatcatataataaaaaagaacatggtgctcgaacatagttgaattcaccatattgaaggggaatgctgacatgtaattcgctactagacgatcaattccctacttgtcaaagttgaactggtacgttcttggaattaaatatgattgcgctggaccaaattgcgacttcttgagtttcagggcaaacgattgagccggaggatgtccgtctcttacctttcttgcttatgataaacgacggtccctgtacatcactgggaattctcagcaaaaataattgggtaaatcgagactcgatgtattcggccacaaaggtgttagacgttaaagattattcaacggggcgataataggatcataaccggtatgcaagcgcattgaaagagccatgagatccttatccgataaacgctgcacggtatgtgcagccttattgtcgatcacgaatttataaatgtagtctgggctgtaagttgaagacctaagttataatgaagtgcaataccaaatcgattcatagtggattatcagactcaagatatctcctgataaattacagttgttaagatacggataaaatgagatttaagattagcagcctctaatctgtttcaatcccgttggaatgtggtatgcgatcaaggttaagttaaaatcaagcctgtcttcagtcttgattcttgttctgccatcgcatgcggtctacgtgagttaatatgtagcttacgttctagcttgtgctaatctgagtatagattcgtagaggaatattatcaagcttccacgcctcaacgtacgtgtattggtcacacaagacactaaaagtggaagtagcgtaaactatagtctagttgttaaatgctcagttcttgttatattcgatatactcttggctaatttatgtctgagtatataaaattaatgatattaacttgcatttcacggatcccttagaaaaagattttgaccgagcgcattataaacggttacaccgaatcaatagaagcatacccaatagctttctttgaatttattgcctgcgcaacttggctgactctctagatccgaataattctatatggtcgtgacgaaactagttcattactgtttaaaatgccaacatgtcttttgggccgataatggctctttgcaaaattactcaatgatacgattgatcaaagcggtagttgctagtggtagcatgtaagtctatcaaatgtctgattatccgaaaatcttccaaaagagtccacgtaccatatctatctcatagcgacgcgaggggaaccttatctaactatcattccatttaccgggtgactctcgatgcaggatccgattgggataaattgcccagaaatggctcattcctgactaagggtaaggccgttctcagcaagggaaccccgcgaatctaggcttataccatctagattgttaactacttgcctgtagttctacagccatactggacagttgtttctaaatgatcgggattcatgctagcactcctctgaatgcaccgcgtaagtttaactattacgtccgtgggcagataaggatggaggctgtatgtatcttaactgttacctaatatggctggtaattatcaaagtaaggaccttaatgccatagcgctagcaatcgctttgtatactgaccatgtgccaacctctcttaatctgtaaaatataatgtcttagctaactgtggacgatcatgtctctgcctagagcttcgctgtatcaattcctatagccagcgtactagtgacacaacaacaccgtgtgagaaaagatattagtccttacgtctgtctctctacagcttattgatgaggattgaacatggacatatagctccccctcaaaagcagatgctacctctttattccattctcgaacatttgccgaacttaatttcgacaaacctgaggtcacgtcttaatttatcggtaacgtcacgtccctttgagactggataaatatattaccaggggccaacgagcaattgttggaggcgcttctataatacaaggtgtcttgtcaaagaaagacggcgtgcgtctcgtgcaactcacttaaccaatattaatgtgaaacccccctctctcacatcttatgcggtgtactgccctggtacatttcctgtacaggactccaacagtgtagattcctaagatagctgttggagttgcctcacgccagatcgaaaaactgaataaactagtgagctgagctgcagaaataccgcttaattacttatgactagttcaaagggacctacgtgatgtcagacattgcaaggaagaaattaggtttgtgcgtcattttggctggactagcactccttacttcccctactattcaaatgtcgtaaacagcatgagacaggatcgtgctgacatttaaggtctattgggaacgaggctacctttggtcgcgcgctcgcgttctccgaatgaccgaaatgcatgagcacagtatgcaattgcttatagatctaaggtctggtcgttgaaaccaagcacgtaggcctgggaaatcagttcttcctcagcaactacacaaaagcgtccaagcattagtacttgtagtaaatgtccgaacctatgcgctcatttgaaagtcaaaaaatatttttaagcagtaggcacctaacccgattcctctacttagtagctttctttgattctcagaattgactgcaatatcactgcacaattctgtgccattactagacttctctgtattaacgtctcatcttactaacactcgcctaggacacatctgagagtgaagtatttcaatacatttactgaaatcttcagttctaaaatccccgaataaggctcttatcggtttggccaacacaagaaaaaaacttcttgcaccactcaccttcatacgcaggagcctggggaacttagtaataactatttcggcagacaaagcttataacaagttgccggcgcgtataatatttaaaagaccccttgagctgctcaattaaaacgctcacctggtataggctattagatagtgccgtcttagtaaggggcgggaattatcggataaactgatattttgataaaataaccgacttgttcacgacataagtcactaaggagattttatctttctccaaagtatatcttccttggataatttcaaagcgctgcaatttaagttctgttactagtttatgctgctgggaggtgaccggaaggcgtagtaatctagaggcaaattataagaagttcatcatatcattttcgactacaaaaacaaggtgttgtatgccggcgcattgtgtaaactggacgagtaccctagatggaaaattatacgttaagccaagatttcgatgtaatgataattacctacacatttttgctatccataggaacaagagctgttctataggctcgtggcatacgaacatttgctgccgctatgaatattggaagctcttcaactacagactctattcttaattgccgtcgaaaatgggccgaatcggctattattaatactcggtttttccgaggggattgttgtcgacagtcgtaattattattaatattgatgttggtgaggtcatttaaatacaaccttgcagacaatgaataagggatccaatctctcatactccttttacaattgctcatgcccctatgcaaaccttatgccgccacacctccgcaactctctcttctgaactgtaagtagcttcattactggtttgagactatactgaagctgatgacattctaaaatggctattttcgaatgtgattcataatgtttatcgtttgggatggcagaatcacgttatttttgatatagcccgggtattctattgtatagaacgtatgctacaagtcattccccgaagaagactagaagtaaacaacatgcgaccatcgttaagccacgcaaggctgtagctttatttcccgataacctatcttccataaatagcggacagcaggatactgacgctcaacatcagtggttatggtctaatttttaacttttaataaggtaacttcagcaggcatacacagtaactctttaatttataatcaaattagaagtctgacacttcttatatttttctatcatccaacgcgatcgcccattagcttattgtgttactaataacgtatctaaaccaatccttttcaagctactgcctatattgtcaatatatacaaacaacaggatagtaggctgcttaaaaaatattgtcaaccgtgtacgctttacaatacccggaaatcacaaactttgtagacaacgagtgaaatttatacactacgaagggccagcgtacaagacccatgaattaggcgatatgtttattctgacatattggtttatccttaatctgtcgctgtaaaatgaagccgcccccatccctgcgaattttttttcgaagattcacgactgaaatataaatacgtttggctatatttatgttggagggaggcaatagcctttactgttaaccgaagatttagccagtgagtgtgacactaaaacactggaataaatgcaggcgttcttctgggtaaaaggtttagtcaatctcgcctataagttcatatagctctggatataattatctggcccatgcatttatcatggcgcttggtgccctgtgtgaagccggcctctcatattgaaggtccgaagtattccatgtacattaagatcactctctcattcatgcatcttggcttaacaaatctggttgtccaagctttccaggcacgtatggtacaaattcggatcgaatacttataaaaatgatatgttaaactgtctaaaacgctcatctacaaagtaaagtgcactaaccaatagagtctcaagaccgtgtaatgctggtgcactgaatgtgtaatacggttagaagggattagttatgttacaaatccattgaaaacttaagaagcattgcgtgctcggagggtgcatcttttatcaagagactaacattattttcaacgacgtacatgctttacaatagggtacttatcaaacgccgagaaacgcgcctatagtgatgttatgattatgacccgatatccattggaccgaattttatgtaggttcccagcgtactcgcgtaatatctcggtattgccataatgtaatacttgtcggtctctcccagatgaaaaagcgttacagagtatttcaatgaaaaacagcgcgcaacgtcaatacctttaggggtaacggccgctgatttcatatagatatacgataagttggtatagctctactaggtggcatccacaatcgttgcatttactatagctggttacaatcataatctataccgttccttacatactaccatagcgggatagcgtttttttgccgttgattgggtttaagaggatgtcagtctcattatatccgattcggtgggagagccgttgttttcaaatcgcacactttgtgacataatgtacaagataacaaaactgatataagatataaactgtcaatatcaccttgacacttgaatcaaagtaaattaactcgcaaatataatttgactaattgggtgcagatttctcaattaataaaaaaatggcaccggatgggcttacaagccccttatcattcacttgtatcatgatttccaagaacaatagaatttgctagcaagtatgaacagagattcgaattgcatccacagtacgccggagcgtttattttaatgtggatatgacgatgtactgttggcggcatttgctagtaaccggtccttatttacgtagcgcacacgtaagcatgtctgggagaaatatggtggtacaatctcagagaaagattacagtttggtttaaataggacttatcgggtcggaagtggaacttaataagcagtacacaattgggcaacagacgtcttgcctattacaataggattacaatgcgttagatttcagacacgttcgtgtttggctattcgtcaattccctaaatagttagacgatcaactattatcaaagtgattctttgttcatcctccattcatgtaacagatggcacactacgcataacgccgaggaattttaacgagatttaagagagcagttcgggcacaacccacttgactttataacagctcggcagcataaacggtaatatgtgacaaatttccaaacgttataagaacgtatgtgtacttagaaaactaagtggttcatgttcaacagatgtgacgcagcaagcctaacttatctattggttttgctataaaagaacaaagttacacagaatcctaagggcttgtttcacacttatgcctagtgcttcaccatcttaaaatagcgaaaccggcacgaatcaaaccttaaaacaatgcgcagatattggtgatggtgactccgggtatgataatggtaactgttgaccagcgcccacctcatcgaagtatagaaagtggttaggataaggatgagaccgaacttatttccggccataactttagattttctacctagtacacaacatcagggcggacacgaaaccgccatcacatcatataccaggtttaatttgcttaatgggggaagtgtcaacgaaccttcgaactttagcaggcatatggccattatatatggccccagagcagaatgctacagcagacaaaatttggatttatgtagtttaatacctatcaaacttggtgtgaccatacttgtctaacgacagtgcacaaagtgtaagttacaattattactactcagcagcttctgcaatgataaaatcttatcatacacgtcacatatgataatatctacttagggggaacgggctccacaacctacatagtactcaatacttacactattcgacaggcacaccaaacctgtacagtcccaaaagattgagtcaactttgcagtactgcagatcacagtaatagcttagttagcgagtcaaaattagttttctacgagactgcacgaccgtgcaaatttccgatgtgttggctacaaatagcaacgtatgaatttgtttgaagccacgtaaactgtacaaccttagagataagtctcaggctactaaaaacacgttgtggcactaacaggatcatggttgattcttacttattcggctgaccggcccaataagtaaccttcaactagaacagaataatcgggagtagtttaattcagtcaaggtgcaggtctcattgtaactaacaagctctgtgtaaccaagttaaaatcgttttcttagcggattccctacttatggatttgagctcgtccacaatattcgatacaagaagtttgtggtccgtaacaacgaaattttaattacgctgtgcagcctcatccaaggaattaatagaaggttgatggtaggctccgaacgctccatgattataatcaagtggactgtgcagtaaacgaggaaggtatcctgacgtcgtggtgttcgtttttgttatttgtgccctatacgagtagataaaccatgaacagcacagtgtgaacccatggttgattttaggctaccttatttttaatttccgttacacagaaacgaattccacaactaacatgccattaatttttcgatatcttataaaagatggtcgaaattcattcatttattttttttcggttctcgaaagtcaactaagctgtcgcgttttgtttctctttagaggtaaaagtggctttgatctcctacgtttggatactagtcaaccattactccatttgatccgtgagtatcacctgtctaacatccagcattatgactcctcggcgaagaaaagacacacttcttagagtcgatgtgtattagctagggacacagttgtttaatacgatagtgagcccagggagggcagtgcgtcccccagtagatttattcagctagtgtaagtataagatatctcacccacgaggttcaagtgatatgcagtcttagaataatacttatcctgaatttcgatattatgggtacttcaataatccgctagcgctactttatgtctcgttggacagcaggacacatggcagtcttaaacactaaagacatcacctgaatgaatgtaatgggattacaagaatcaatgaggtattatatacgacgtaggaaactctggatatatacagtaatctagttacgccatcgcacttcattcctctggaaacttagaagacatcagctgtacgtggaggaaccagacccccgtatgtagccaaatagaaccaaagttgcttatacaaacacacccaatgacaatggaccgctggagttcgtaaactcggaacgtagtactgcacaaacccagcatttagcaataggagctacgtatgcaactcccacgtggtaataccttcaagctatcaatatataggtgcctagctaatcgcattcgcaagcagtattcaagcttgtaaaccagtataataattacagaggctctatgaaacccaactttccagctaaaagtcccaattaaatggttatttcgtacttttaaagtcgcccgttctgttattacgcgaattgattctactccaaaattaaacacaaattatcaaccgtttcatttatatttgtcaatgcagctgtttaaaataaggctctactaaattataattaagacacttattaccagatttctctagttaagtttgaaccagctcgactaccgcgaaagatacattcccttctctatttttcagttcatctatgggtcagagaagcattgaatttattctattcaccctcgtcgttcacagcgaatcgtcagtgtgatcagtgtatgagaaatatcctaaaccgtttagtcagaccacacgcttagaacaagtggtctaaaaagactgccctggaaggagtaagaagtatacagctgatccggtgtatccttcagtcatctgccctatactaattacacgacgcaaggaaaaataggtttattttctaggcaaacccttcataggtgactccgatgtgttacgaatcatgcttgagaatgtgctatcgttaccgacggataataacgatctccaatgaaccaaatgtagaatgtctattgattacccttttactattcgacttagagataggagatagaacctcagtgtacttttttagccgaatgggaatctttgggaggtgaatggccataaggtcgtaaatccaaccctcttaaagtcttccatattatatcgttgttcgtggaatcgataacagatttgttgacccatagtaaatgtatactagtttatgttgtaagtgtagattgttttccgattgccgtccaaactttatgtcgtaattgtagaccagtaaagttgaccaaggtaagtgcccagcgatcctgcgagatcgatcgccaatttttccagtcactgtaagtgtaggtttagataaagccgtatgagttatatcataagggcctcggaaagcagcttcgaaccaaagttcccttataatagtagtttaactataaaagtatatactggtctgtcgccctttcacgatttgttttaccggtttatgaagcgttacgtcattagagcggctccaatttaaggttaacggcttccatgtgtagttgtatacaaggataacttaaagtatctgttcagcgagctagttaagttatcctcgatagaacacaactcagaggtcccaagatcgggtttgcaacttgctaatttattctcaaggcaaattgggaattatcgatacctgtataccataaggtcgctcgatgtgatgcttatgtcttctggtgatcctaccttagttagtgctgattaacggaacattaatgtttatcgttttgagatttagccaattctctgattctaactcaagatgccttatctgacgtgctatgcagcccctaagtattttacattgtaataggacacgctcctttaaaactcgccaaaaggtcgttgtggttctctactggttaactatataatttacagctttgttgagctagttcctctttggtttaagtcctcaatattagttggttcgagcgataagttggctagttaccttagtcactatattagatccgaatgttatgcttcatctgaagaccgccaccctccaaaatttcttttaagactcacttattgcaaggtgtaggtgaattcggctcgtttctcaagtggtgtatctgtacacgagtttccatattttcatcaacagccaccgcacacttatgtcactctaggtattaaaagtcgctctacaaggggacgcaattaagaaacagacatgctagtcaaaaataaacatagcgaggcaccactaattcggccgcttatcaatgggatgctctgcgcgagacgcgccagagctcagtagttagttcggacatacatttacttcagatgatcaattagttttctacaaatgcttactctaccccgaaaaaagtcaccagactcttacgtctctttagtatccttccgtcttatataaggtcagtcccccgtttcggtaccctggaatttactaagaataatgaaacagcccccaaggacgtacgtttacaaatgatagaccagatcgcctagcttattccgacgcatgttgcatagaattgaaccaacggaatgtgagagtaactagatgagccgaccacagcacccgtttgcgtcgcagaatacgcctgatagttcggccacgaaatcatatgtcctttgagtattaagtatttgtaatgatcaatcgagctcaagcaagcttacacttcctcggatattcagggaacttagtgcctttgaaagatacgttgatcaacgaaaaattgataatggctcatatggaatgcctacctcatagtgctgaattaacacagcactgcggacctaacttttcgaggtttcaagttcacgtctcaaaacctaataggctggaatatgtagggatcctcggtgaatttgtgattgggtttgttgtagtactgaccaagtgaatattctttttttctaaaagcagatctgctgccgggcactacgaaggagatctctgtgtatcattattgcttcttgacatgatgactcttaaatcactgtgggtgtgcaaaacgatagcacaacccaattcgatagtacatattgttgatacttcgcactaaaccgttcatatttaaaggttgtgctccttccttcgttaaatactggtgacttggtcctatctactattagctagacctctggggaaccacgcccccgtaaaacctgtgcaagagagggggtcatacatcttagacatcgcgcctccaccagggaagcattgggtgattgaccaggtgtgtaacaaatatgattattcttatactaatattagcaaagatgcataatgatttgtattaaatgtataattgaattgataagggtcttttagtcagtgatagagtagtataaggtagacattagaactcttaaccggacgcagatttttcggtcttagtaagccaattagtcgacaaaacaaggtaagagcggttactagtagtacctataatgcactgaatcttcggtcgaagtatagttctaatgctatgcagattgtgacggcgacaaatgttcagacttatatcatgaaacaagctcttgtaagtattgacaaatgaaaagattgaatatttttaaatacaaaatgcgcctacttattaggggaattaaccagattgaaggccaatcctcacatgtaatgagataatagacgataaatgaaattcttgtaatagttgaactgctacgtgatgggtattatatatgattgagatcctccaattgccgacgtcttgtcttgatgcccaaaagattgtcaacgaggagctccctcgcgtacctgtcgtccgtatcataaacgacgcgacatgtacagcactccgaagtataagcaataataatgcgggtaatccagactagatcttttcggactcaatgcggtttcacggtaaacatgattaataccggagagtagtcgagcttatcagcgatgcaagcgaattcattgtgccaggagatacgttgcagataaaaccggcaacgtatgtcaacaagttttggcgatctcgttgtttgtattcgacgaggcgcgggaacttcaagaactatcgtatattcaagtccattaccttttagtttcagactggtggagctgactaaagttatatcatcattttgtacactggtttagttaacgataatttcagatttaacatgaccagacgataatcgctgtatatccagttggaatgtggtttgccagaaaggttaacttataatcaagcctctcttcagtcttgattcgtcgtatcccatccattgcgctatacctcagtgtatttggagctgtagttataccgtgtgctaagatcagtagacatgacgagagcaatattatctaccttacaagcatcaacggacgtctagtcggaacaaaagactctaaaactcgaacttcaggttaatatactatagttctgtattcagcagttattcttatattcgatattatcttgcctattggatgtctgactttagtatattaatcatagtatctgccatgtaaaggtgccagtactaaatctgtttcacagtgcgaattataaacggttacaaccattaaagacaacaagaccctatagctttatttgaattttgtcaatgcgcaacttggagctcgcgatacatcccaattagtctatagggtcgggacgattctacggcatttctggttataatgacaacatggattgtggcccgagaatcgctctttcattaattaagcaatcattacagtcttataagcgctacttccgagtggtagcaggtaactcgatataaggtcgcatgagccgaatagcttaaaaaacaggccaccgaacattgatagagaataccgaccacagcgcaacctttgattactttcattaaattgtacggctcactcgacatcaagcttaagattgcgataatgtgaactcaaatggatcagtactgaagaaccgtaacccacttcgcagaaagcgtacccagagaagatacgctgttacaatatacagggtgaaattattgcctgttcttcgtaaccatttcgccaaacttggttagaaatgatagccattcatgatagaaataagctgaatgataccagtatctttaactatgtagtcagggggaagataacgatggtccatgtatgtttctgatatgtgacagtattggccgcgtaatttgctaacgaagctacttaatgcctttgagcttcatatagatttctttaatcaaaatcggcaaaaagatagtatgagctataatatatgctagtagagaactctggaccatcatctatatgaatactgattcgagcgtgcaattactttagcctgcgtactactgactctacaaaacactctgagataagtttgtagtcagtaagtcgctctctataaaccttttggatgaccattgtacagccacttatagatcccaataaatagcacaggagacagagtttttcaatgctcgatcatttgccgatagtattttcgtctaacctcagggcacctattatttgatacctaacctaacggccctttcacaatggagaaatatatgacatcgggacaaacacaaatggtgggtggccaggagatatgacatggtggcgtctctaagaaacacggactccctctaggcaaactcacgtaaccaattttaatgtcaaacaaaacgctcgaaaagattttgccgtgtaatgacctggtacattgactggtcaggaatacatcactgtagttgccgtagtgtcctgttggtgttccatcaagacacatcgtataacgcaatttacgacggacatcagatcaagttatacagattatttaagtatcacgtgtgcattgggacataagggatctcacacatgccttggaacatttttgctttgtgccgctttttcgctgcactaccaatccttacttaccagtatattcaaaggtcgttaacagaatgagaaaggttagggctctaagttatcgtcgattgggatagacgagacatttgcgagcgccctccacggatacgaatctcccatatcaatgtgaactggatgctatgcagtttagttcttacgtctcctagtggtaaaaatcaaagtagcactcgcatagcagttattcagaacctaatacacaaaaccgtcaaacattttctaattctaggtatgggccgatcataggagctaaggtgaaactcataaatgttttgttagatctagcatcctaaaaagatgcatatactgagtagctggcgtgcattctctcaattgtatcctttttaactgaactagtcggtcccatttcgtgactgagatctattaaccgataagattaataacactcgcattcgtatcagctcagagtgaagtttttcaataatttgactgatatattaacttctaaaataaccctttaagcctcggatccgtttcccaatcacatcaaaaattcttattccaactatctacggattaacaacgtgcatggggatcgtagtaagaacttgttccgatcactttgagtatatcaagttgacggcccggttattattgaatagaaacattcacctgctaaattaaataccgcacatcggatacccgatttcagagggccgtcttactaagggcaggctttgttcggtttaactgagatgttcattattttacagtatgcttcaactaatatgtaacgaaggacagtggatctgtctccatagtagatcttcagtcgtgaatttcataccgctcctatttaagttcgcgttcgagttgttgatcatggcacgtgaaagcaacccctagtattctagacgaaaattttttctagttcatctgataatttgccaattcaaaaacaaccgctggtttcccggcgcattctctaaaatggaagtcgaacctagagccattatttgtcggtaacccatgagttccttcttttcagaagttaatacactgtggtcctatacagaggaaaaacagcggttatatacgatcgtggcataacaacattggatcaagatagcaatttggctacctattctaattctcactagattcggtattccactacaatatcggcagattaggattggatgaataatcggtgtttaagtccggttgcgtctccaatctcctaatttttattaatattgatcttggtgacctattgtaaataaaaacttcaagactttgaataacggtgaaaagatagaagactcatttgaaaatggatcatccacagatccaaacattagcaagacactaatccccaactagctattctgatcgcgatcgtgctgcagtactcctgtcacaatagtctgttcatgatctaattctttttgggctttgttcgatggtgattcagaatctttatccggtcgcttccctgtagctactttgtggggatattgcccggggattatagggttgagatcgtttcctaaaagtatttaaaccaagtagacttcaactaaactacatcagaacatcgtgaagacaccatacgcggtacctttatttaccgataacatttcttcaagaaataccggtaagcagcataatgaccctaaacagctcggggtatcgtcgtagttttaaattttatttaggttactgctcaaggaataaaaactaactatttaatttataataatattacaaggctcacactgattagatttgtctataagacttcgcgatcccccattaccggattgtcttaagaataaactagataaaccatgcattttctagataaggcctttagtctaattagatacaaaaaacacgatagttgcatccttaatttattgtgtcaaacctggaaccttttaattacccgcaaatcactttatgtcgagactacctctgaaatttattatctacctaccgcatgaggacttgaaccatcttgtaggagttatgtttattagctaagattcgtttatcctgtagcggtccatgtatattcaacaagcaaaaagcactcagaattgtttttagttgagtcaagactgatatataaataagtttccctagttttttcgtggtgggacgatattgaattgaatcttaaccgaagagtttcccactctgtcgcacaataatacacgccaatatttccagccctgcttatgccttaatcggttactcaatctcccattgaagttcattttgatctgcatagaagtttcgggcccagccttttttctgccaccttcctccaagctctgtagacgcactctaagattgatgctcacatgtattaattctacattaacataaatatataagtcatgcatcttcgagtaaaatatctggttctccaacatgtcctggcacgtatcgttataatgcccatacatgtagtattaaaatgattgggttaactggatattaagatcatcgaaattgtaaagtcaaattaacaatactgtctcaagaccgtgtattcctcgtgctcggaagggctattacgcttacttccgttttggtatcttaatatgactttcaaaaattaagttgcagtgagtcctacctgcgtgcatcggttagcaagagtataaaagttgtttaaacgaactacttgctttacaataccggtcgtatatatcgccgtgaatccagaagattgtcttctttggattatcaaccgagatcctgtggaccgatgttttgggaccttcacagaggactccaggtagagctcgcttttgcattaatctaagaattgtacctctctaaaagatctaaaacagtgaatgtgtatttcatggaaaaacacagagaaacgtaaattactttaggccgaaaggcacatgagttattatacatatacgagatggtggtatacatcgaattcggggcatacactatagttgcattgtatttagctgctttaaataatatgatattaccttccttacataagacattaccggcataccctggttttcaacttgtggggctttttgacgatcgcactctcatttgatccgagtagggcggtgacccctgcttttcaaatacaaaaatttcgctatgaaggtaatagattacttttcgctgttatgatagaaacggtaaatttaaaattgaaacttctagaaaagtaaagtaacgagaaatgattttgtgaataatgcggtcatgattgcgcaagtaagaaaaaaaggcaaaaggatgcgcggaatagaaacttatcagtcacgggtatcttgatttcattcttcttgtcaattgccgacataggatgaaatcagattccaatgcaatacacagtaacccccacccttgattgtaatgtcgatttgaagttgtacgcgtcgacgaagtggatagtatacgggccttttgtacggtgcgatcaactatgaatctcggcgagttagatggtcgtacaatctcacacatagaggtcacttgcctgtaatgacgaattttcggctaggtactcgaactttattagaagtaaaaatgtgggcaaaagaaggattccattttacaagacgattacaatgagttacatgtctctcaacgtagtctttccctagtagtctttgaactatttaggtactccagaaaattttagcaaagggtttctgtgtgaatccgccattcatgtttatgatggaacaataagaataacgccctcgtatgttatcgacagtgaagtcagcagttcggccaaaaacatattcaatttagtacagatccccagaagttaagctaagtgctctaaaatggcctaaacggttatcaaagtaggtctaattactatactaacgggtgcatcgtaataactgctgtcgatgcaacactatatgatagtgtcgttttgctatatatgtacaatgtgacaaagaagccttagcgattcttgcaaacttaggacttcggattctcaatcttaaatgtccgaaaacgcaaagattcaaaaatttaatctatgagcagatatgcctgatggtgactacgcgtatgttaaggctaaatgttgacaaccgcacacataatcgaactattgatagtcgggagcataaccaggtgaacgtactttgttcacgacatttattgacatgttctaaatacgtctcaaaatcacggcgcactagaaaacgcaatcaaatcattgtcctggtttaagggccgtaatgccggtagtgtcaaacttcatgagaactttagctggcttttggccagtatttagggaccaagagcactagccttaagctgaatattttgccatttatctactgttataactttaaaacttggtggcaccagacttgtcgatacacacgcatcaatctgtaacgtaaaaggtttactaagaacaagcgtaggaattgagtttatattatatttaaactaaaagatgatattagcttctgagggcgatagggctccaaatcataaagaggaatatattattacacgattagaaacccacaacatacctcgaatcgcccaaaagtttgacgaaacttggcagtactccacatctcagtaatacagttgggagagtctcaaatgttgttttattactcaatgaaccaccctcataatttcactgctgttccattaaatttgcaaacgatcatttgctttgaagaaacgtaaaatcgacaaaattacagataagtagatgcataataaaaaaaactgctcgctataacacgatcatcgtgcattcttacttaggagcatcacccgcacaataacgtaccttaaactacaacactattagaccgagtactgtaattcacgaaagctcaagctcgcattgtaaagaacttgctctctcgtaaaatgtgataatagtttgcggagaggattcaattattttccattgcacctactccactagattcgataaaagaaggtggtcctcccttaaaaagaaatgttaagtaacatcggaaccataagcaaagcatgtaagtgaaccgtcatccttccctaagaaacataaaggtttttaataatgtcgactgtgaactataactgcatcctttcctgacctactccggttccttgttgttatttctgaacgagaccagtagataaacaatgtaaaccacagtgggtaccaatggtgcatgtgacgctaccgttgttttaagtgcccgtacaaacataagaagtcataatcttacttgaaattaattttgccttttattttttttcaggctcgaaattaatgatttgttttttttgaccttctagttacgctaatatgcggtcgcctgtggtttctattgagtcctataacgggatgggatctaatacgtttggttactagtaaacaaggtataaatttgataccggagtatcaactgtataacatcaagctttatgactcatacgcgaagtaatgacacaaggctttcaggagatcgcgagtacagagccactaaggggtgtattacgatagtgacaccaccgagcgcactcactccccaagtagatttatgatcctacgctaagtattagatatataaccaaagaggttctagtcagtgcaactcttagaataataattagccggttttgcctttttaggcctaatgcaatattcagctagcccttatgtatctcgcgttccacagcaccactcatggcacgcgtttaaactaatcaaatataatctatgaatgttatgccagtacttgaataaatcaggttttttataagtccttgcatactctcgttatatactgttagagtcttaccccatagaaattctttcatctgcaaacttagaagaattctcagctacggggagcataaagtccccaggatgttgacaaatacaacaaatgtggcttatacaaacactccatatgaaaatcgaaccctcgtggtagttttagccgaaccttgtacggataaatccctccattttccaatagcagatacctatcctactacctcgtggtattaaattaaagcttgaaatatagagctgcatagcttatccaattcccaagcacgagtctaccgtcgtaaccacgatttgatttacagacgctagagcaaacccatctttaaacatataagtaaaaattaaagggtgagtgcgtacgtgtttactagcaacttcgcttattaagacaattgtttataagccataattaaaaacatatgttcaacaggttcattgatatttgtaattgcacaggtttttaataaggatctacgtaagtataatgaacaaactttttaccagagttatattctgtactttgaaaatgctcctctaccgccttagagactttcaattagattttttgcagttaatctatgcgtaagtgaaccatgcaagggatgcgattcaaccgcctcgtgctaaccctatcgtctgtctcataactgtaggtctaatataattttcagttttcgaacacataaccctttgaaaatctgctatttaatgtctcacctgcatgcactatcttctatactgctcagaacggctatacgtcactatgctccaagtgacgatttaaacgaagcaaggaataataggtttattttagtgcaaaacaattaagtgcggactacgtgctctttacaataagccttgtgattgggctataggttaagtcccatattaacgatctccaatgtacaaaatcgacaatcgctttgcattacccggttactagtcgaattacagatagctgttagatactcactctaattttggacaacaatcccaatcttggggtcgtctatcgcctgaagctcgtaaatccttccatcttaaacgattacatattatagacttgttcggggtagagatatcacagttgtgcaaacattgtaaatcgatactagtttatgttggtagtctagttgcttttaccattccccgaaaaacttgatctactatttcgacaacagtaaacttgaactaggtaagtgaaaacagagaatgcctcatagtgccactatttgtccactatatgtaagtgtagctttacataatccactatgactgagatcattacggcctaggaaagcagcgtagaaaaaaagggcccggatattacgactgtaactataaaactagttactggtagcgcgccatgtatagatttgttttaccggttgtggttgcgttaacgaatttcagccgcgaaaattgatccgttaaccagtccatctcgacttctataaaacgataaagtaaagttgatgttcagcctccttcttatggttgcatcgagagtacactactcagtgggaaatagatcggggttcctacttcagattgtattatctaggcaattgccgattgtgccatacctggataaaataagctacctacatgtgatgcttatctattatcgtcatactaccttagggtgtcctgttgaacgctacattaatctttagccgtttgagatgttccaatggataggagtctaacgcatgatgaagtttaggaaggcagagcatcccactaagtatgtgacagtgtatttcgaaacgagacgttataaatagaaaaaaggtccttctggttctattctgctgaactattgaatggaaagattggttgacctacgtactatttgcttgaagtcatcaatttgacggggtgagagacatatggtgcatactttacggactctatattttagatcagaagcttagcagtcttctctacaccccctcacgacataattgcttttaagaatctatgtttgattcctctacgggaattcggatccgttcgcatgtgcggtttatctaaaccaggggacatatgttcagctaaagcatacgaacactttgctaactagacgtatgtatagtagctataaatcccgacgatatttacaaaaagaaatgagactcaaatatatacatagcgaccctacacttattcgcaccctgatctaggcgatcctagcacccacacccgaaagtgagcactagtgtcttccgtattaaatttactgcagttgagattttagttgtctactaaggattactctaacccgtaataaggatcaagactcggtactagctttactatcattccctatgtgttttcctaactcacaagggtacgtaccagcctatgtaattacaataatgataaagacacaaaggaagtaactttacaaatgagtctccagttacactagcttagtccctcccatcttgctttgaagtctaaatacgcaatctctgaggatatacagcagaagaacactcataacgttggagtccaagaattagactcatagggcccccaacatttaatatgtactgtgagtttgaaggtgttctattgttaattcctgctcttgatacatgacacgtactccgtgtttaaggcttcggactgactttctttcataagttgagcaacgaaaatttcagaatcgataagttggattcactaactaatacggctgattgaaaactccactccggacctatatggtcgacctttatacgtaaccgatataaaacttataggctggtatatcgagccttcctagcgcaatttcggatggggtttcttctactactcaacaacggaatagtctttgtttagtaaaccagagctcaggacgcccaatacgtaggagagcgctgtggagcatgtgtcattatggactggagcactcttaaatcactctgcgtgtgctaaacgatagatcataacatgtcctgagtaaattttcttgatacgtcgcaatataccgttattagttaaacgttctcatccgtcatgcgtgaaatacggctgtcgtgctcagatatactattagcgactcatctcgcctaacacgcacacgtataaactcggaatgactgccgctcttacatattagaaatacagactacaccacggaagcattgggtcattctcaaccgctgtataaaagatgattagtcttataataagattaccaaagaggcagaatcatgggtagtaaatctattattcaagtgattaccgtcgtgtaggcagggagtgaggacgagatggtactcaggacaaatattaaccggacgaagtggtttacgtcgtactttcactattagtagtaaatacaaggtaacaccggggaatagtactaaatataatgatatctatcttcgggagaacgagtcgtctattgctttgaacattctcaaggcgtaaaatgtgctgacttatagcatgatacaaccgattgttacttttgtctattcaaaagattgaatagttttttatacaaaagccgcatacttatgacggctagtatacagtttcatcccctagcatcaatgctatggacagtattgaacttataggaaattcttctaatagggcaaatccgtcgtgatgcctattttttttcagtcacatcctcaaatggcactagtattgtcgggatcccattaacaggctcaaccacgagctcacgcgaggacatgtagtccgtatctttaacgaagcgacagcgacagaactcccatggataaccaattataaggcccgtaatcctctagacatcgtttaccaataaatccgctttctccgtaatcatgttgaataccccagagtagtccagatgataaccgatgaaacacaagtctttctcaatgcacttacggtgaacttattaccgccaacgtagctcatcaaggttgcgacatctagttgtgtgtttgcgacgagcccagcgaacttcatcaactttcgtatattcaacgccttgtaattttactttaagacgcctggtgatgtagattcttagataatcagtttgttatcggctgtactttaccataatttcacaggtttcaggtcaagaagattatagctgtatatacagttccatgctcggtgcacagaaacgtgatcggataataatcaatcgcttatgtcgtctttaggcgtatccaatacatgccccgataccgcagtgtatttcgacatgtaggtataccgtcgcatttgagctcgagtcaggacgtcagctagattagattccttaatagaatataccgacctctagtccgaactaaactatagataacgccaacttcaggttaattgtctagtcgtctgtttgcagatgggattcttagatgagtgagtatcggccatattggttcgagcactttagtttttgatgcataggatatgcaatgtatagctgaaagtactttatctgtttcaaactcacattgattaaaccggtaaacctttaaagactacaagaaaatattcagtgagggcaattttgtcaatcacaatcttccagctagagatacttcacaatttgtcttgaggctacgcaacattagacggattttcgcgttttattgaaataatcgaggggcccaagagtatccatagttcattttgtaagatttctttacaggcttattacagcttcttcagactcctacatgcttacgagttatatgctagcatgtgaacaatagattaatatacaggaaaacgtacattgagagagatgaccctacacagcgcaaccgttgagtactttcattaaagggtaacgctctcgagacagcatccttaagatggccttattgtcaaatcatttgcagaagtacgcaagatccctaaccaacgtagaagaatccctacaaacacatgagacgcggtgaaaatagacagggtgttagtattcaatcttcggagtatcaatttcgccaatcttggtgagaaagcataccctttcttcagagaaagaagatcaatcataacactatctttaacgaggtacgcacgcgcatcattacctgcctccatggatctttaggatagcggaaagtattggcagcgtattgtgatttcgttcctactttatcaatttcacattcatatacatgtcttttatcaaaatcgccaataagataggatgagctatattagatgctagtagagttcgcgccaacatcatcgataggaatactcaggacagcgtgataggacttttcaatccctaatactctctataattataactctctcttaagtttggaggcagtaacgcgctctatataatcagtttgctgcaccattcttcagcctctgatacatacaaataaattccacagcagtaagagggtttaattgagacatcttgggaacttaggattttactctaacatcaccgaaacgattattggataccgtacctaaacgaactttctcaaggcagtaatataggacatccgcaataacacaaatgctgcctccccaggagttatgtcttcctggaggctatatcttacacccactcactataggcaaactaaagtttaaatgttgattgtctaaaaaaaagatagataagagttggccggcgtagcacatgcgaaagtgaatcgtaagctataattctctggacttgaagttctgtcctgttcctctgcaagaaacaaacttcctttaaagctatttacgacgcacatctcagcaagttataaacatgttggaagtttctagtcggaattcccaaagaacggatctatctaatgcattcctacatttttcctgtctgccgatggtgccatcctattcaaagaatttcttaaaagtagattaaatgggacttttaacaatgagtaaccttacgcctctaagggttcctcgagtgccatacaccagtcaggtccgagccacatacacggagaacattctaacatagcattctcaactcgatcatttgcaggttacttctttcctatcctagtgctaaaaatcatacttgcaatcccatagcacggattaagaacctaagaaacaattcagtaaaacatgttcgaattcttggtatgggaacatcattgcagctatggtctaacgcattaatgtttgggtacatcttccatcatataaacaggaagagtctgacgacagggagtgcttgcgatcatgtctatcattgtgaaatcaaattgtagctcacatgtcgtctatgagagcgtgtatccgataagatttagaaaaatagaagtcgtataagatctcactgaacttttgaatgaatgtgaagcatatatgatctgctttaataaaactttatccataggatacgtttccaaatcaattcaataattattagtcaaaatagataaggatgaacaacctgaaggccgatcggacgtagaaagtggtcccatcactttgagttgatattgttgaaccacacgttattatggttttcaaacagtctcaggatattgtatatacagataatccgataccagttgtctgacgcccctcttacgtaccccaccctttgtgacgtttaaagcagttgttcagtattttaaactaggcggcaactaatttggaaagaagcacagtggatatgtctaaattcttgttattcaggcctgaatttaatacaccgcatagttaacttcgcggtagagttgttcatcatgcctcctctaagctaccacttctatgatacaccaatagttgttctacggaatctgataattggccaagtcataaacttccgctgcgttcaacccccttgctcgaatatccaactcgaaaagacagccttttggtgtccggaacaaatcagttacttcttttctgatgttaattctctgtggtcagatacagaccaaaaactccgcggatttaccatcctccaagaacaaatttgcatcaacatagcattttggctacatattctaagtctcaatagtttaggttttcaactacattatcccaacattaggattggaggaataatagctgggtaagtccccttgcgtctacaatcgactattttttatgaatatgcttctgccgcacctatggttattaaaaaagtcatgactttgaagaaccctgaaaagatagatgaatcaggtgtaatggcagcagccaaagagcatataattagcaacactctaagaacattatagatatgatgatagcgatcgtcatgatgttatccggtcacaatagtagcttcatcagctaattcgttttgccagtggtgacttgcgctggaagaatcgttatacggtcccttccctcttgatacggtgggggcttattcaaccgcgtggattgggttgtcatacttgcattaaacgatgtaaaccatctagtagtcaactatactaaatcacaaaatagtgatcaatacatacccgcttcatggttttaaccatttaattgattaaagatattccgctaagaaccattatctacctaaactgatcgccgtatcctagtagtttgaaatttgatgtaccgtaatgatcaacgaagtaaaacgttatattgtatgtagaataataggtcttggagctaaatgatgtgattggtagtgaagacttacccttacaactttaccggtttctcggaagaatatactagagaatcaatgcatgggctacataagcactttagtctaatgagataaaaaatacacgagtcttccatcatgaattttttgtcgaaaaactcgaacctggtaatttaaaccatatatctttatgtcgtcaataactctcatatgttttatataacttcccaatcacgacttgtaactgcttgttcgactgagctgtttgagctatgaggccgggatccggttgagctacatctatttgctacaagaaaaatgaaagcacatttgttgggagttctggctacactcatagagaaataagtggcccgagtgggtgcggcctgcctccatattcaagtgtatcttaaaccaagtggttccaacgctcgcgctaaagaattaaagcctttatttcctccacggagtagcccgtaatccggttcgaaagagaccattgaagttaattttcatatccagtgaagtttaggcacaagcatgtgttctgccacatgcctcaaagcgctcttcaaccaagatatgattcatcctaacttcgatgaatgcgtctgtaacataaatatagaaggaatgattcggcgagttaattttcgccttctccaacatggcatccctacgttcgttataaggaccatacatgtaggttttaaaggtttgcggttaatcgatatttacatcatagaaattctatagtcaaatttacaagactctagatactcactcgttgcagccggctaggaagcgctttgtaccttacttcccttttcgttgcgtaatatgaatttcatatagtaagttcaaggcactcatacctccgtgaagagggtagatagactattaaagttgtttaatagtacgtattgatggaaatgacccgtaggagatttaccactcaatccacaagattcgctgctgtgcattatcaaaacagtgcatgtcgaaacatgggttgggtccttcaaacacgaatccaggtagagatacctttgcaatttttGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCcttBtatcatatgctaKggNcataaaSatgtaaaDcDRtBggDtctttataattcBgtcgtactDtDagcctatttSVHtHttKtgtHMaSattgWaHKHttttagacatWatgtRgaaaNtactMcSMtYtcMgRtacttctWBacgaaatatagScDtttgaagacacatagtVgYgtcattHWtMMWcStgttaggKtSgaYaaccWStcgBttgcgaMttBYatcWtgacaYcagagtaBDtRacttttcWatMttDBcatWtatcttactaBgaYtcttgttttttttYaaScYaHgtgttNtSatcMtcVaaaStccRcctDaataataStcYtRDSaMtDttgttSagtRRcatttHatSttMtWgtcgtatSSagactYaaattcaMtWatttaSgYttaRgKaRtccactttattRggaMcDaWaWagttttgacatgttctacaaaRaatataataaMttcgDacgaSStacaStYRctVaNMtMgtaggcKatcttttattaaaaagVWaHKYagtttttatttaaccttacgtVtcVaattVMBcttaMtttaStgacttagattWWacVtgWYagWVRctDattBYtgtttaagaagattattgacVatMaacattVctgtBSgaVtgWWggaKHaatKWcBScSWaaccRVacacaaactaccScattRatatKVtactatatttHttaagtttSKtRtacaaagtRDttcaaaaWgcacatWaDgtDKacgaacaattacaRNWaatHtttStgttattaaMtgttgDcgtMgcatBtgcttcgcgaDWgagctgcgaggggVtaaScNatttacttaatgacagcccccacatYScaMgtaggtYaNgttctgaMaacNaMRaacaaacaKctacatagYWctgttWaaataaaataRattagHacacaagcgKatacBttRttaagtatttccgatctHSaatactcNttMaagtattMtgRtgaMgcataatHcMtaBSaRattagttgatHtMttaaKaggYtaaBataSaVatactWtataVWgKgttaaaacagtgcgRatatacatVtHRtVYataSaKtWaStVcNKHKttactatccctcatgWHatWaRcttactaggatctataDtDHBttataaaaHgtacVtagaYttYaKcctattcttcttaataNDaaggaaaDYgcggctaaWSctBaaNtgctggMBaKctaMVKagBaactaWaDaMaccYVtNtaHtVWtKgRtcaaNtYaNacggtttNattgVtttctgtBaWgtaattcaagtcaVWtactNggattctttaYtaaagccgctcttagHVggaYtgtNcDaVagctctctKgacgtatagYcctRYHDtgBattDaaDgccKtcHaaStttMcctagtattgcRgWBaVatHaaaataYtgtttagMDMRtaataaggatMtttctWgtNtgtgaaaaMaatatRtttMtDgHHtgtcattttcWattRSHcVagaagtacgggtaKVattKYagactNaatgtttgKMMgYNtcccgSKttctaStatatNVataYHgtNaBKRgNacaactgatttcctttaNcgatttctctataScaHtataRagtcRVttacDSDttaRtSatacHgtSKacYagttMHtWataggatgactNtatSaNctataVtttRNKtgRacctttYtatgttactttttcctttaaacatacaHactMacacggtWataMtBVacRaSaatccgtaBVttccagccBcttaRKtgtgcctttttRtgtcagcRttKtaaacKtaaatctcacaattgcaNtSBaaccgggttattaaBcKatDagttactcttcattVtttHaaggctKKgatacatcBggScagtVcacattttgaHaDSgHatRMaHWggtatatRgccDttcgtatcgaaacaHtaagttaRatgaVacttagattVKtaaYttaaatcaNatccRttRRaMScNaaaDgttVHWgtcHaaHgacVaWtgttScactaagSgttatcttagggDtaccagWattWtRtgttHWHacgattBtgVcaYatcggttgagKcWtKKcaVtgaYgWctgYggVctgtHgaNcVtaBtWaaYatcDRaaRtSctgaHaYRttagatMatgcatttNattaDttaattgttctaaccctcccctagaWBtttHtBccttagaVaatMcBHagaVcWcagBVttcBtaYMccagatgaaaaHctctaacgttagNWRtcggattNatcRaNHttcagtKttttgWatWttcSaNgggaWtactKKMaacatKatacNattgctWtatctaVgagctatgtRaHtYcWcttagccaatYttWttaWSSttaHcaaaaagVacVgtaVaRMgattaVcDactttcHHggHRtgNcctttYatcatKgctcctctatVcaaaaKaaaagtatatctgMtWtaaaacaStttMtcgactttaSatcgDataaactaaacaagtaaVctaggaSccaatMVtaaSKNVattttgHccatcacBVctgcaVatVttRtactgtVcaattHgtaaattaaattttYtatattaaRSgYtgBagaHSBDgtagcacRHtYcBgtcacttacactaYcgctWtattgSHtSatcataaatataHtcgtYaaMNgBaatttaRgaMaatatttBtttaaaHHKaatctgatWatYaacttMctcttttVctagctDaaagtaVaKaKRtaacBgtatccaaccactHHaagaagaaggaNaaatBWattccgStaMSaMatBttgcatgRSacgttVVtaaDMtcSgVatWcaSatcttttVatagttactttacgatcaccNtaDVgSRcgVcgtgaacgaNtaNatatagtHtMgtHcMtagaaattBgtataRaaaacaYKgtRccYtatgaagtaataKgtaaMttgaaRVatgcagaKStctHNaaatctBBtcttaYaBWHgtVtgacagcaRcataWctcaBcYacYgatDgtDHcctaaagacYRcaggattHaYgtKtaatgcVcaataMYacccatatcacgWDBtgaatcBaatacKcttRaRtgatgaBDacggtaattaaYtataStgVHDtDctgactcaaatKtacaatgcgYatBtRaDatHaactgtttatatDttttaaaKVccYcaaccNcBcgHaaVcattHctcgattaaatBtatgcaaaaatYMctSactHatacgaWacattacMBgHttcgaatVaaaacaBatatVtctgaaaaWtctRacgBMaatSgRgtgtcgactatcRtattaScctaStagKgaDcWgtYtDDWKRgRtHatRtggtcgaHgggcgtattaMgtcagccaBggWVcWctVaaattcgNaatcKWagcNaHtgaaaSaaagctcYctttRVtaaaatNtataaccKtaRgtttaMtgtKaBtRtNaggaSattHatatWactcagtgtactaKctatttgRYYatKatgtccgtRtttttatttaatatVgKtttgtatgtNtataRatWYNgtRtHggtaaKaYtKSDcatcKgtaaYatcSRctaVtSMWtVtRWHatttagataDtVggacagVcgKWagBgatBtaaagNcaRtagcataBggactaacacRctKgttaatcctHgDgttKHHagttgttaatgHBtatHcDaagtVaBaRccctVgtgDtacRHSctaagagcggWYaBtSaKtHBtaaactYacgNKBaVYgtaacttagtVttcttaatgtBtatMtMtttaattaatBWccatRtttcatagVgMMtagctStKctaMactacDNYgKYHgaWcgaHgagattacVgtttgtRaSttaWaVgataatgtgtYtaStattattMtNgWtgttKaccaatagNYttattcgtatHcWtctaaaNVYKKttWtggcDtcgaagtNcagatacgcattaagaccWctgcagcttggNSgaNcHggatgtVtcatNtRaaBNcHVagagaaBtaaSggDaatWaatRccaVgggStctDaacataKttKatttggacYtattcSatcttagcaatgaVBMcttDattctYaaRgatgcattttNgVHtKcYRaatRKctgtaaacRatVSagctgtWacBtKVatctgttttKcgtctaaDcaagtatcSataWVgcKKataWaYttcccSaatgaaaacccWgcRctWatNcWtBRttYaattataaNgacacaatagtttVNtataNaYtaatRaVWKtBatKagtaatataDaNaaaaataMtaagaaStccBcaatNgaataWtHaNactgtcDtRcYaaVaaaaaDgtttRatctatgHtgttKtgaaNSgatactttcgagWaaatctKaaDaRttgtggKKagcDgataaattgSaacWaVtaNMacKtcaDaaatttctRaaVcagNacaScRBatatctRatcctaNatWgRtcDcSaWSgttRtKaRtMtKaatgttBHcYaaBtgatSgaSWaScMgatNtctcctatttctYtatMatMtRRtSaattaMtagaaaaStcgVgRttSVaScagtgDtttatcatcatacRcatatDcttatcatVRtttataaHtattcYtcaaaatactttgVctagtaaYttagatagtSYacKaaacgaaKtaaatagataatSatatgaaatSgKtaatVtttatcctgKHaatHattagaaccgtYaaHactRcggSBNgtgctaaBagBttgtRttaaattYtVRaaaattgtaatVatttctcttcatgBcVgtgKgaHaaatattYatagWacNctgaaMcgaattStagWaSgtaaKagttttaagaDgatKcctgtaHtcatggKttVDatcaaggtYcgccagNgtgcVttttagagatgctaccacggggtNttttaSHaNtatNcctcatSaaVgtactgBHtagcaYggYVKNgtaKBcRttgaWatgaatVtagtcgattYgatgtaatttacDacSctgctaaaStttaWMagDaaatcaVYctccgggcgaVtaaWtStaKMgDtttcaaMtVgBaatccagNaaatcYRMBggttWtaaScKttMWtYataRaDBMaDataatHBcacDaaKDactaMgagttDattaHatHtaYatDtattDcRNStgaatattSDttggtattaaNSYacttcDMgYgBatWtaMagactVWttctttgYMaYaacRgHWaattgRtaagcattctMKVStatactacHVtatgatcBtVNataaBttYtSttacKgggWgYDtgaVtYgatDaacattYgatggtRDaVDttNactaSaMtgNttaacaaSaBStcDctaccacagacgcaHatMataWKYtaYattMcaMtgSttDagcHacgatcaHttYaKHggagttccgatYcaatgatRaVRcaagatcagtatggScctatattaNtagcgacgtgKaaWaactSgagtMYtcttccaKtStaacggMtaagNttattatcgtctaRcactctctDtaacWYtgaYaSaagaWtNtatttRacatgNaatgttattgWDDcNaHcctgaaHacSgaataaRaataMHttatMtgaSDSKatatHHaNtacagtccaYatWtcactaactatKDacSaStcggataHgYatagKtaatKagStaNgtatactatggRHacttgtattatgtDVagDVaRctacMYattDgtttYgtctatggtKaRSttRccRtaaccttagagRatagSaaMaacgcaNtatgaaatcaRaagataatagatactcHaaYKBctccaagaRaBaStNagataggcgaatgaMtagaatgtcaKttaaatgtaWcaBttaatRcggtgNcacaaKtttScRtWtgcatagtttWYaagBttDKgcctttatMggNttattBtctagVtacataaaYttacacaaRttcYtWttgHcaYYtaMgBaBatctNgcDtNttacgacDcgataaSatYaSttWtcctatKaatgcagHaVaacgctgcatDtgttaSataaaaYSNttatagtaNYtaDaaaNtggggacttaBggcHgcgtNtaaMcctggtVtaKcgNacNtatVaSWctWtgaWcggNaBagctctgaYataMgaagatBSttctatacttgtgtKtaattttRagtDtacatatatatgatNHVgBMtKtaKaNttDHaagatactHaccHtcatttaaagttVaMcNgHatatKtaNtgYMccttatcaaNagctggacStttcNtggcaVtattactHaSttatgNMVattMMDtMactattattgWMSgtHBttStStgatatRaDaagattttctatMtaaaaaggtactaaVttaSacNaatactgMttgacHaHRttgMacaaaatagttaatatWKRgacDgaRtatatttattatcYttaWtgtBRtWatgHaaattHataagtVaDtWaVaWtgStcgtMSgaSRgMKtaaataVacataatgtaSaatttagtcgaaHtaKaatgcacatcggRaggSKctDcagtcSttcccStYtccRtctctYtcaaKcgagtaMttttcRaYDttgttatctaatcataNctctgctatcaMatactataggDaHaaSttMtaDtcNatataattctMcStaaBYtaNagatgtaatHagagSttgWHVcttatKaYgDctcttggtgttMcRaVgSgggtagacaataaDtaattSaDaNaHaBctattgNtaccaaRgaVtKNtaaYggHtaKKgHcatctWtctDtttctttggSDtNtaStagttataaacaattgcaBaBWggHgcaaaBtYgctaatgaaatWcDcttHtcMtWWattBHatcatcaaatctKMagtDNatttWaBtHaaaNgMttaaStagttctctaatDtcRVaYttgttMtRtgtcaSaaYVgSWDRtaatagctcagDgcWWaaaBaaRaBctgVgggNgDWStNaNBKcBctaaKtttDcttBaaggBttgaccatgaaaNgttttttttatctatgttataccaaDRaaSagtaVtDtcaWatBtacattaWacttaSgtattggDgKaaatScaattacgWcagKHaaccaYcRcaRttaDttRtttHgaHVggcttBaRgtccctDatKaVtKtcRgYtaKttacgtatBtStaagcaattaagaRgBagSaattccSWYtttattVaataNctgHgttaaNBgcVYgtRtcccagWNaaaacaDNaBcaaaaRVtcWMgBagMtttattacgDacttBtactatcattggaaatVccggttRttcatagttVYcatYaSHaHcttaaagcNWaHataaaRWtctVtRYtagHtaaaYMataHYtNBctNtKaatattStgaMcBtRgctaKtgcScSttDgYatcVtggaaKtaagatWccHccgKYctaNNctacaWcttttgcRtgtVcgaKttcMRHgctaHtVaataaDtatgKDcttatBtDttggNtacttttMtgaacRattaaNagaactcaaaBBVtcDtcgaStaDctgaaaSgttMaDtcgttcaccaaaaggWtcKcgSMtcDtatgtttStaaBtatagDcatYatWtaaaBacaKgcaDatgRggaaYctaRtccagattDaWtttggacBaVcHtHtaacDacYgtaatataMagaatgHMatcttatacgtatttttatattacHactgttataMgStYaattYaccaattgagtcaaattaYtgtatcatgMcaDcgggtcttDtKgcatgWRtataatatRacacNRBttcHtBgcRttgtgcgtcatacMtttBctatctBaatcattMttMYgattaaVYatgDaatVagtattDacaacDMatcMtHcccataagatgBggaccattVWtRtSacatgctcaaggggYtttDtaaNgNtaaBatggaatgtctRtaBgBtcNYatatNRtagaacMgagSaSDDSaDcctRagtVWSHtVSRggaacaBVaccgtttaStagaacaMtactccagtttVctaaRaaHttNcttagcaatttattaatRtaaaatctaacDaBttggSagagctacHtaaRWgattcaaBtctRtSHaNtgtacattVcaHaNaagtataccacaWtaRtaaVKgMYaWgttaKggKMtKcgWatcaDatYtKSttgtacgaccNctSaattcDcatcttcaaaDKttacHtggttHggRRaRcaWacaMtBWVHSHgaaMcKattgtaRWttScNattBBatYtaNRgcggaagacHSaattRtttcYgaccBRccMacccKgatgaacttcgDgHcaaaaaRtatatDtatYVtttttHgSHaSaatagctNYtaHYaVYttattNtttgaaaYtaKttWtctaNtgagaaaNctNDctaaHgttagDcRttatagccBaacgcaRBtRctRtggtaMYYttWtgataatcgaataattattataVaaaaattacNRVYcaaMacNatRttcKatMctgaagactaattataaYgcKcaSYaatMNctcaacgtgatttttBacNtgatDccaattattKWWcattttatatatgatBcDtaaaagttgaaVtaHtaHHtBtataRBgtgDtaataMttRtDgDcttattNtggtctatctaaBcatctaRatgNacWtaatgaagtcMNaacNgHttatactaWgcNtaStaRgttaaHacccgaYStacaaaatWggaYaWgaattattcMaactcBKaaaRVNcaNRDcYcgaBctKaacaaaaaSgctccYBBHYaVagaatagaaaacagYtctVccaMtcgtttVatcaatttDRtgWctagtacRttMctgtDctttcKtWttttataaatgVttgBKtgtKWDaWagMtaaagaaattDVtaggttacatcatttatgtcgMHaVcttaBtVRtcgtaYgBRHatttHgaBcKaYWaatcNSctagtaaaaatttacaatcactSWacgtaatgKttWattagttttNaggtctcaagtcactattcttctaagKggaataMgtttcataagataaaaatagattatDgcBVHWgaBKttDgcatRHaagcaYcRaattattatgtMatatattgHDtcaDtcaaaHctStattaatHaccgacNattgatatattttgtgtDtRatagSacaMtcRtcattcccgacacSattgttKaWattNHcaacttccgtttSRtgtctgDcgctcaaMagVtBctBMcMcWtgtaacgactctcttRggRKSttgYtYatDccagttDgaKccacgVatWcataVaaagaataMgtgataaKYaaatcHDaacgataYctRtcYatcgcaMgtNttaBttttgatttaRtStgcaacaaaataccVgaaDgtVgDcStctatatttattaaaaRKDatagaaagaKaaYYcaYSgKStctccSttacagtcNactttDVttagaaagMHttRaNcSaRaMgBttattggtttaRMggatggcKDgWRtNaataataWKKacttcKWaaagNaBttaBatMHtccattaacttccccYtcBcYRtagattaagctaaYBDttaNtgaaaccHcaRMtKtaaHMcNBttaNaNcVcgVttWNtDaBatgataaVtcWKcttRggWatcattgaRagHgaattNtatttctctattaattaatgaDaaMatacgttgggcHaYVaaNaDDttHtcaaHtcVVDgBVagcMacgtgttaaBRNtatRtcagtaagaggtttaagacaVaaggttaWatctccgtVtaDtcDatttccVatgtacNtttccgtHttatKgScBatgtVgHtYcWagcaKtaMYaaHgtaattaSaHcgcagtWNaatNccNNYcacgVaagaRacttctcattcccRtgtgtaattagcSttaaStWaMtctNNcSMacattataaactaDgtatWgtagtttaagaaaattgtagtNagtcaataaatttgatMMYactaatatcggBWDtVcYttcDHtVttatacYaRgaMaacaStaatcRttttVtagaDtcacWatttWtgaaaagaaagNRacDtttStVatBaDNtaactatatcBSMcccaSttccggaMatgattaaWatKMaBaBatttgataNctgttKtVaagtcagScgaaaDggaWgtgttttKtWtatttHaatgtagttcactaaKMagttSYBtKtaYgaactcagagRtatagtVtatcaaaWYagcgNtaDagtacNSaaYDgatBgtcgataacYDtaaactacagWDcYKaagtttattagcatcgagttKcatDaattgattatDtcagRtWSKtcgNtMaaaaacaMttKcaWcaaSVMaaaccagMVtaMaDtMaHaBgaacataBBVtaatVYaNSWcSgNtDNaaKacacBtttatKtgtttcaaHaMctcagtaacgtcgYtactDcgcctaNgagagcYgatattttaaatttccattttacatttDaaRctattttWctttacgtDatYtttcagacgcaaVttagtaaKaaaRtgVtccataBggacttatttgtttaWNtgttVWtaWNVDaattgtatttBaagcBtaaBttaaVatcHcaVgacattccNggtcgacKttaaaRtagRtctWagaYggtgMtataatMtgaaRttattttgWcttNtDRRgMDKacagaaaaggaaaRStcccagtYccVattaNaaKStNWtgacaVtagaagcttSaaDtcacaacgDYacWDYtgtttKatcVtgcMaDaSKStVcgtagaaWaKaagtttcHaHgMgMtctataagBtKaaaKKcactggagRRttaagaBaaNatVVcgRcKSttDaactagtSttSattgttgaaRYatggttVttaataaHttccaagDtgatNWtaagHtgcYtaactRgcaatgMgtgtRaatRaNaacHKtagactactggaatttcgccataacgMctRgatgttaccctaHgtgWaYcactcacYaattcttaBtgacttaaacctgYgaWatgBttcttVttcgttWttMcNYgtaaaatctYgMgaaattacNgaHgaacDVVMtttggtHtctaaRgtacagacgHtVtaBMNBgattagcttaRcttacaHcRctgttcaaDBggttKaacatgKtttYataVaNattccgMcgcgtagtRaVVaattaKaatggttRgaMcagtatcWBttNtHagctaatctagaaNaaacaYBctatcgcVctBtgcaaagDgttVtgaHtactSNYtaaNccatgtgDacgaVtDcgKaRtacDcttgctaagggcagMDagggtBWRtttSgccttttttaacgtcHctaVtVDtagatcaNMaVtcVacatHctDWNaataRgcgtaVHaggtaaaaSgtttMtattDgBtctgatSgtRagagYtctSaKWaataMgattRKtaacatttYcgtaacacattRWtBtcggtaaatMtaaacBatttctKagtcDtttgcBtKYYBaKttctVttgttaDtgattttcttccacttgSaaacggaaaNDaattcYNNaWcgaaYattttMgcBtcatRtgtaaagatgaWtgaccaYBHgaatagataVVtHtttVgYBtMctaMtcctgaDcYttgtccaaaRNtacagcMctKaaaggatttacatgtttaaWSaYaKttBtagDacactagctMtttNaKtctttcNcSattNacttggaacaatDagtattRtgSHaataatgccVgacccgatactatccctgtRctttgagaSgatcatatcgDcagWaaHSgctYYWtatHttggttctttatVattatcgactaagtgtagcatVgtgHMtttgtttcgttaKattcMatttgtttWcaaStNatgtHcaaaDtaagBaKBtRgaBgDtSagtatMtaacYaatYtVcKatgtgcaacVaaaatactKcRgtaYtgtNgBBNcKtcttaccttKgaRaYcaNKtactttgagSBtgtRagaNgcaaaNcacagtVtttHWatgttaNatBgtttaatNgVtctgaatatcaRtattcttttttttRaaKcRStctcggDgKagattaMaaaKtcaHacttaataataKtaRgDtKVBttttcgtKaggHHcatgttagHggttNctcgtatKKagVagRaaaggaaBtNatttVKcRttaHctaHtcaaatgtaggHccaBataNaNaggttgcWaatctgatYcaaaHaatWtaVgaaBttagtaagaKKtaaaKtRHatMaDBtBctagcatWtatttgWttVaaaScMNattRactttgtYtttaaaagtaagtMtaMaSttMBtatgaBtttaKtgaatgagYgtNNacMtcNRacMMHcttWtgtRtctttaacaacattattcYaMagBaacYttMatcttKcRMtgMNccattaRttNatHaHNaSaaHMacacaVaatacaKaSttHatattMtVatWgattttttaYctttKttHgScWaacgHtttcaVaaMgaacagNatcgttaacaaaaagtacaHBNaattgttKtcttVttaaBtctgctacgBgcWtttcaggacacatMgacatcccagcggMgaVKaBattgacttaatgacacacaaaaaatRKaaBctacgtRaDcgtagcVBaacDSBHaaaaSacatatacagacRNatcttNaaVtaaaataHattagtaaaaSWccgtatWatggDttaactattgcccatcttHaSgYataBttBaactattBtcHtgatcaataSttaBtatKSHYttWggtcYtttBttaataccRgVatStaHaKagaatNtagRMNgtcttYaaSaactcagDSgagaaYtMttDtMRVgWKWtgMaKtKaDttttgactatacataatcNtatNaHattVagacgYgatatatttttgtStWaaatctWaMgagaRttRatacgStgattcttaagaDtaWccaaatRcagcagaaNKagtaaDggcgccBtYtagSBMtactaaataMataBSacRMgDgattMMgtcHtcaYDtRaDaacggttDaggcMtttatgttaNctaattaVacgaaMMtaatDccSgtattgaRtWWaccaccgagtactMcgVNgctDctaMScatagcgtcaactatacRacgHRttgctatttaatgaattataYKttgtaagWgtYttgcHgMtaMattWaWVtaRgcttgYgttBHtYataSccStBtgtagMgtDtggcVaaSBaatagDttgBgtctttctcattttaNagtHKtaMWcYactVcgcgtatMVtttRacVagDaatcttgctBBcRDgcaacKttgatSKtYtagBMagaRtcgBattHcBWcaactgatttaatttWDccatttatcgagSKaWttataHactaHMttaatHtggaHtHagaatgtKtaaRactgtttMatacgatcaagDgatKaDctataMggtHDtggHacctttRtatcttYattttgacttgaaSaataaatYcgBaaaaccgNatVBttMacHaKaataagtatKgtcaagactcttaHttcggaattgttDtctaaccHttttWaaatgaaatataaaWattccYDtKtaaaacggtgaggWVtctattagtgactattaagtMgtttaagcatttgSgaaatatccHaaggMaaaattttcWtatKctagDtYtMcctagagHcactttactatacaaacattaacttaHatcVMYattYgVgtMttaaRtgaaataaDatcaHgtHHatKcDYaatcttMtNcgatYatgSaMaNtcttKcWataScKggtatcttacgcttWaaagNatgMgHtctttNtaacVtgttcMaaRatccggggactcMtttaYMtcWRgNctgNccKatcttgYDcMgattNYaRagatHaaHgKctcataRDttacatBatccattgDWttatttaWgtcggagaaaaatacaatacSNtgggtttccttacSMaagBattacaMaNcactMttatgaRBacYcYtcaaaWtagctSaacttWgDMHgaggatgBVgcHaDtggaactttggtcNatNgtaKaBcccaNtaagttBaacagtatacDYttcctNgWgcgSMcacatStctHatgRcNcgtacacaatRttMggaNKKggataaaSaYcMVcMgtaMaHtgattYMatYcggtcttcctHtcDccgtgRatcattgcgccgatatMaaYaataaYSggatagcgcBtNtaaaScaKgttBgagVagttaKagagtatVaactaSacWactSaKatWccaKaaaatBKgaaKtDMattttgtaaatcRctMatcaaMagMttDgVatggMaaWgttcgaWatgaaatttgRtYtattaWHKcRgctacatKttctaccaaHttRatctaYattaaWatVNccatNgagtcKttKataStRaatatattcctRWatDctVagttYDgSBaatYgttttgtVaatttaatagcagMatRaacttBctattgtMagagattaaactaMatVtHtaaatctRgaaaaaaaatttWacaacaYccYDSaattMatgaccKtaBKWBattgtcaagcHKaagttMMtaatttcKcMagNaaKagattggMagaggtaatttYacatcWaaDgatMgKHacMacgcVaacaDtaDatatYggttBcgtatgWgaSatttgtagaHYRVacaRtctHaaRtatgaactaatatctSSBgggaaHMWtcaagatKgagtDaSatagttgattVRatNtctMtcSaagaSHaataNataataRaaRgattctttaataaagWaRHcYgcatgtWRcttgaaggaMcaataBRaaccagStaaacNtttcaatataYtaatatgHaDgcStcWttaacctaRgtYaRtataKtgMttttatgactaaaatttacYatcccRWtttHRtattaaatgtttatatttgttYaatMcaRcSVaaDatcgtaYMcatgtagacatgaaattgRtcaaYaaYtRBatKacttataccaNaaattVaBtctggacaagKaaYaaatatWtMtatcYaaVNtcgHaactBaagKcHgtctacaatWtaDtSgtaHcataHtactgataNctRgttMtDcDttatHtcgtacatcccaggSttaBgtcacacWtccNMcNatMVaVgtccDYStatMaccDatggYaRKaaagataRatttHKtSaaatDgataaacttaHgttgVBtcttVttHgDacgaKatgtatatNYataactctSatatatattgcHRRYttStggaactHgttttYtttaWtatMcttttctatctDtagVHYgMRBgtHttcctaatYRttKtaagatggaVRataKDctaMtKBNtMtHNtWtttYcVtattMcgRaacMcctNSctcatttaaagDcaHtYccSgatgcaatYaaaaDcttcgtaWtaattctcgttttScttggtaatctttYgtctaactKataHacctMctcttacHtKataacacagcNRatgKatttttSaaatRYcgDttaMRcgaaattactMtgcgtaagcgttatBtttttaattaagtNacatHgttcRgacKcBBtVgatKttcgaBaatactDRgtRtgaNacWtcacYttaaKcgttctHaKttaNaMgWgWaggtctRgaKgWttSttBtDcNtgtttacaaatYcDRtgVtgcctattcNtctaaaDMNttttNtggctgagaVctDaacVtWccaagtaacacaNctgaScattccDHcVBatcgatgtMtaatBgHaatDctMYgagaatgYWKcctaatNaStHaaaKccgHgcgtYaaYtattgtStgtgcaaRtattaKatattagaWVtcaMtBagttattagNaWHcVgcaattttDcMtgtaRHVYtHtctgtaaaaHVtMKacatcgNaatttMatatgttgttactagWYtaRacgataKagYNKcattataNaRtgaacKaYgcaaYYacaNccHatMatDcNgtHttRaWttagaaDcaaaaaatagggtKDtStaDaRtaVtHWKNtgtattVctSVgRgataDaRaWataBgaagaaKtaataaYgDcaStaNgtaDaaggtattHaRaWMYaYaWtggttHYgagVtgtgcttttcaaDKcagVcgttagacNaaWtagtaataDttctggttVcatcataaagtgKaaaNaMtaBBaattaatWaattgctHaVKaSgDaaVKaHtatatatHatcatSBagNgHtatcHYMHgttDgtaHtBttWatcgtttaRaattgStKgSKNWKatcagDtctcagatttctRtYtBatBgHHtKaWtgYBgacVVWaKtacKcDttKMaKaVcggtgttataagaataaHaatattagtataatMHgttYgaRttagtaRtcaaVatacggtcMcgagtaaRttacWgactKRYataaaagSattYaWgagatYagKagatgSaagKgttaatMggtataatgttWYttatgagaaacctNVataatHcccKtDctcctaatactggctHggaSaggRtKHaWaattcgSatMatttagaggcYtctaMcgctcataSatatgRagacNaaDaggaVBagaYttKtacNaKgtSYtagttggaWcatcWttaatctatgaVtcgtgtMtatcaYcgtRccaaYgDctgcMgtgtWgacWtgataacacgcgctBtgttaKtYDtatDcatcagKaVMctaatcttgVcaaRgcRMtDcgattaHttcaNatgaatMtactacVgtRgatggaWtttactaaKatgagSaaKggtaNtactVaYtaaKRagaacccacaMtaaMtKtatBcttgtaaWBtMctaataaVcDaaYtcRHBtcgttNtaaHatttBNgRStVDattBatVtaagttaYatVattaagaBcacggtSgtVtatttaRattgatgtaHDKgcaatattKtggcctatgaWDKRYcggattgRctatNgatacaatMNttctgtcRBYRaaaHctNYattcHtaWcaattctBtMKtVgYataatMgYtcagcttMDataVtggRtKtgaatgccNcRttcaMtRgattaacattRcagcctHtWMtgtDRagaKaBtgDttYaaaaKatKgatctVaaYaacWcgcatagBVtaNtRtYRaggBaaBtgKgttacataagagcatgtRattccacttaccatRaaatgWgDaMHaYVgVtaSctatcgKaatatattaDgacccYagtgtaYNaaatKcagtBRgagtccatgKgaaaccBgaagBtgSttWtacgatWHaYatcgatttRaaNRgcaNaKVacaNtDgattgHVaatcDaagcgtatgcNttaDataatcSataaKcaataaHWataBtttatBtcaKtKtatagttaDgSaYctacaRatNtaWctSaatatttYaKaKtaccWtatcRagacttaYttVcKgSDcgagaagatccHtaattctSttatggtKYgtMaHagVaBRatttctgtRgtctatgggtaHKgtHacHtSYacgtacacHatacKaaBaVaccaDtatcSaataaHaagagaatScagactataaRttagcaaVcaHataKgDacatWccccaagcaBgagWatctaYttgaaatctVNcYtttWagHcgcgcDcVaaatgttKcHtNtcaatagtgtNRaactttttcaatggWgBcgDtgVgtttctacMtaaataaaRggaaacWaHttaRtNtgctaaRRtVBctYtVtatDcattDtgaccYatagatYRKatNYKttNgcctagtaWtgaactaMVaacctgaStttctgaKVtaaVaRKDttVtVctaDNtataaaDtccccaagtWtcgatcactDgYaBcatcctMtVtacDaaBtYtMaKNatNtcaNacgDatYcatcgcaRatWBgaacWttKttagYtaattcggttgSWttttDWctttacYtatatWtcatDtMgtBttgRtVDggttaacYtacgtacatgaattgaaWcttMStaDgtatattgaDtcRBcattSgaaVBRgagccaaKtttcDgcgaSMtatgWattaKttWtgDBMaggBBttBaatWttRtgcNtHcgttttHtKtcWtagHStaacagttgatatBtaWSaWggtaataaMttaKacDaatactcBttcaatatHttcBaaSaaatYggtaRtatNtHcaatcaHtagVtgtattataNggaMtcttHtNagctaaaggtagaYctMattNaMVNtcKtactBKcaHHcBttaSagaKacataYgctaKaYgttYcgacWVttWtSagcaacatcccHaccKtcttaacgaKttcacKtNtacHtatatRtaaatacactaBtttgaHaRttggttWtatYagcatYDatcggagagcWBataagRtacctataRKgtBgatgaDatataSttagBaHtaatNtaDWcWtgtaattacagKttcNtMagtattaNgtctcgtcctcttBaHaKcKccgtRcaaYagSattaagtKataDatatatagtcDtaacaWHcaKttDgaaRcgtgYttgtcatatNtatttttatggccHtgDtYHtWgttatYaacaattcaWtatNgctcaaaSttRgctaatcaaatNatcgtttaBtNNVtgttataagcaaagattBacgtDatttNatttaaaDcBgtaSKgacgtagataatttcHMVNttgttBtDtgtaWKaaRMcKMtHtaVtagataWctccNNaSWtVaHatctcMgggDgtNHtDaDttatatVWttgttatttaacctttcacaaggaSaDcggttttttatatVtctgVtaacaStDVaKactaMtttaSNagtgaaattaNacttSKctattcctctaSagKcaVttaagNaVcttaVaaRNaHaaHttatgtHttgtgatMccaggtaDcgaccgtWgtWMtttaHcRtattgScctatttKtaaccaagtYagaHgtWcHaatgccKNRtttagtMYSgaDatctgtgaWDtccMNcgHgcaaacNDaaaRaStDWtcaaaaHKtaNBctagBtgtattaactaattttVctagaatggcWSatMacccttHttaSgSgtgMRcatRVKtatctgaaaccDNatYgaaVHNgatMgHRtacttaaaRtatStRtDtatDttYatattHggaBcttHgcgattgaKcKtttcRataMtcgaVttWacatNcatacctRataDDatVaWNcggttgaHtgtMacVtttaBHtgagVttMaataattatgttcttagtttgtgcDtSatttgBtcaacHattaaBagVWcgcaSYttMgcttacYKtVtatcaYaKctgBatgcgggcYcaaaaacgNtctagKBtattatctttKtaVttatagtaYtRagNtaYataaVtgaatatcHgcaaRataHtacacatgtaNtgtcgYatWMatttgaactacRctaWtWtatacaatctBatatgYtaagtatgtgtatSttactVatcttYtaBcKgRaSggRaaaaatgcagtaaaWgtaRgcgataatcBaataccgtatttttccatcNHtatWYgatHSaaaDHttgctgtccHtggggcctaataatttttctatattYWtcattBtgBRcVttaVMRSgctaatMagtYtttaaaaatBRtcBttcaaVtaacagctccSaaSttKNtHtKYcagcagaaaccccRtttttaaDcDtaStatccaagcgctHtatcttaDRYgatDHtWcaaaBcWgKWHttHataagHacgMNKttMKHccaYcatMVaacgttaKgYcaVaaBtacgcaactttMctaaHaatgtBatgagaSatgtatgSRgHgWaVWgataaatatttccKagVgataattWaHNcYggaaatgctHtKtaDtctaaagtMaatVDVactWtSaaWaaMtaHtaSKtcBRaNcttStggtBttacNagcatagRgtKtgcgaacaacBcgKaatgataagatgaaaattgtactgcgggtccHHWHaaNacaBttNKtKtcaaBatatgctaHNgtKcDWgtttatNgVDHgaccaacWctKaaggHttgaRgYaatHcaBacaatgagcaaattactgtaVaaYaDtagattgagNKggtggtgKtWKaatacagDRtatRaMRtgattDggtcaaYRtatttNtagaDtcacaaSDctDtataatcgtactaHttatacaatYaacaaHttHatHtgcgatRRttNgcatSVtacWWgaaggagtatVMaVaaattScDDKNcaYBYaDatHgtctatBagcaacaagaatgagaaRcataaKNaRtBDatcaaacgcattttttaaBtcSgtacaRggatgtMNaattggatatWtgagtattaaaVctgcaYMtatgatttttYgaHtgtcttaagWBttHttgtcttattDtcgtatWtataataSgctaHagcDVcNtaatcaagtaBDaWaDgtttagYctaNccDtaKtaHcttaataacccaRKtacaVaatNgcWRaMgaattatgaBaaagattVYaHMDcaDHtcRcgYtcttaaaWaaaVKgatacRtttRRKYgaatacaWVacVcRtatMacaBtactggMataaattttHggNagSctacHgtBagcgtcgtgattNtttgatSaaggMttctttcttNtYNagBtaaacaaatttMgaccttacataattgYtcgacBtVMctgStgMDtagtaRctHtatgttcatatVRNWataDKatWcgaaaaagttaaaagcacgHNacgtaatctttMRtgacttttDacctataaacgaaatatgattagaactccSYtaBctttaataacWgaaaYatagatgWttcatKtNgatttttcaagHtaYgaaRaDaagtaggagcttatVtagtctttcattaaaatcgKtattaRttacagVaDatgcatVgattgggtctttHVtagKaaRBtaHtaaggccccaaaaKatggtttaMWgtBtaaacttcactttKHtcgatctccctaYaBacMgtcttBaBaNgcgaaacaatctagtHccHtKttcRtRVttccVctttcatacYagMVtMcagaMaaacaataBctgYtaatRaaagattaaccatVRatHtaRagcgcaBcgDttStttttcVtttaDtKgcaaWaaaaatSccMcVatgtKgtaKgcgatatgtagtSaaaDttatacaaacatYaRRcVRHctKtcgacKttaaVctaDaatgttMggRcWaacttttHaDaKaDaBctgtaggcgtttaHBccatccattcNHtDaYtaataMttacggctNVaacDattgatattttacVttSaattacaaRtataNDgacVtgaacataVRttttaDtcaaacataYDBtttaatBaDtttYDaDaMccMttNBttatatgagaaMgaNtattHccNataattcaHagtgaaggDgatgtatatatgYatgaStcataaBStWacgtcccataRMaaDattggttaaattcMKtctMacaBSactcggaatDDgatDgcWctaacaccgggaVcacWKVacggtaNatatacctMtatgatagtgcaKagggVaDtgtaacttggagtcKatatcgMcttRaMagcattaBRaStctYSggaHYtacaactMBaagDcaBDRaaacMYacaHaattagcattaaaHgcgctaaggSccKtgaaKtNaBtatDDcKBSaVtgatVYaagVtctSgMctacgttaacWaaattctSgtDactaaStaaattgcagBBRVctaatatacctNttMcRggctttMttagacRaHcaBaacVKgaataHttttMgYgattcYaNRgttMgcVaaacaVVcDHaatttgKtMYgtatBtVVctWgVtatHtacaaHttcacgatagcagtaaNattBatatatttcVgaDagcggttMaagtcScHagaaatgcYNggcgtttttMtStggtRatctacttaaatVVtBacttHNttttaRcaaatcacagHgagagtMgatcSWaNRacagDtatactaaDKaSRtgattctccatSaaRttaaYctacacNtaRtaactggatgaccYtacactttaattaattgattYgttcagDtNKttagDttaaaaaaaBtttaaNaYWKMBaaaacVcBMtatWtgBatatgaacVtattMtYatMNYDKNcKgDttDaVtaaaatgggatttctgtaaatWtctcWgtVVagtcgRgacttcccctaDcacagcRcagagtgtWSatgtacatgttaaSttgtaaHcgatgggMagtgaacttatRtttaVcaccaWaMgtactaatSSaHtcMgaaYtatcgaaggYgggcgtgaNDtgttMNgaNDMtaattcgVttttaacatgVatgtWVMatatcaKgaaattcaBcctccWcttgaaWHtWgHtcgNWgaRgctcBgSgaattgcaaHtgattgtgNagtDttHHgBttaaWcaaWagcaSaHHtaaaVctRaaMagtaDaatHtDMtcVaWMtagSagcttHSattaacaaagtRacMtRtctgttagcMtcaBatVKtKtKacgagaSNatSactgtatatcBctgagVtYactgtaaattaaaggcYgDHgtaacatSRDatMMccHatKgttaacgactKtgKagtcttcaaHRVtccttKgtSataatttacaactggatDNgaacttcaRtVaagDcaWatcBctctHYatHaDaaatttagYatSatccaWtttagaaatVaacBatHcatcgtacaatatcgcNYRcaataYaRaYtgattVttgaatgaVaactcRcaNStgtgtattMtgaggtNttBaDRcgaaaagctNgBcWaWgtSaDcVtgVaatMKBtttcgtttctaaHctaaagYactgMtatBDtcStgaccgtSDattYaataHctgggaYYttcggttaWaatctggtRagWMaDagtaacBccactacgHWMKaatgatWatcctgHcaBaSctVtcMtgtDttacctaVgatYcWaDRaaaaRtagatcgaMagtggaRaWctctgMgcWttaagKBRtaaDaaWtctgtaagYMttactaHtaatcttcataacggcacBtSgcgttNHtgtHccatgttttaaagtatcgaKtMttVcataYBBaKtaMVaVgtattNDSataHcagtWMtaggtaSaaKgttgBtVtttgttatcatKcgHacacRtctHatNVagSBgatgHtgaRaSgttRcctaacaaattDNttgacctaaYtBgaaaatagttattactcttttgatgtNNtVtgtatMgtcttRttcatttgatgacacttcHSaaaccaWWDtWagtaRDDVNacVaRatgttBccttaatHtgtaaacStcVNtcacaSRttcYagacagaMMttttgMcNttBcgWBtactgVtaRttctccaaYHBtaaagaBattaYacgatttacatctgtaaMKaRYtttttactaaVatWgctBtttDVttctggcDaHaggDaagtcgaWcaagtagtWttHtgKtVataStccaMcWcaagataagatcactctHatgtcYgaKcatcagatactaagNSStHcctRRNtattgtccttagttagMVgtatagactaactctVcaatMctgtttgtgttgccttatWgtaBVtttctggMcaaKgDWtcgtaaYStgSactatttHgatctgKagtagBtVacRaagRtMctatgggcaaaKaaaatacttcHctaRtgtDcttDattaggaaatttcYHaRaaBttaatggcacKtgctHVcaDcaaaVDaaaVcgMttgtNagcgtaDWgtcgttaatDgKgagcSatatcSHtagtagttggtgtHaWtaHKtatagctgtVgattaBVaatgaataagtaatVatSttaHctttKtttgtagttaccttaatcgtagtcctgBcgactatttVcMacHaaaggaatgDatggKtaHtgStatattaaSagctWcctccRtataBaDYcgttgcNaagaggatRaaaYtaWgNtSMcaatttactaacatttaaWttHtatBattgtcgacaatNgattgcNgtMaaaKaBDattHacttggtRtttaYaacgVactBtaBaKtgBttatgVttgtVttcaatcWcNctDBaaBgaDHacBttattNtgtDtatttVSaaacaggatgcRatSgtaSaNtgBatagttcHBgcBBaaattaHgtDattatDaKaatBaaYaaMaataaataKtttYtagtBgMatNcatgtttgaNagtgttgtgKaNaSagtttgaSMaYBcaaaacDStagttVacaaaaactaaWttBaagtctgtgcgtMgtaattctcctacctcaNtttaaccaaaaVtBcacataacaccccBcWMtatVtggaatgaWtcaaWaaaaaaaaWtDtaatatRcctDWtcctaccMtVVatKttaWaaKaaatataaagScHBagaggBaSMtaWaVtatattactSaaaKNaactatNatccttgaYctattcaaaVgatttYHcRagattttaSataggttattcVtaaagaKgtattattKtRttNcggcRgtgtgtWYtaacHgKatKgatYtacYagDtWcHBDctctgRaYKaYagcactKcacSaRtBttttBHKcMtNtcBatttatttttgSatVgaaagaWtcDtagDatatgMacaacRgatatatgtttgtKtNRaatatNatgYcaHtgHataacKtgagtagtaacYttaNccaaatHcacaacaVDtagtaYtccagcattNtacKtBtactaaagaBatVtKaaHBctgStgtBgtatgaSNtgDataaccctgtagcaBgtgatcttaDataStgaMaccaSBBgWagtacKcgattgaDgNNaaaacacagtSatBacKDgcgtataBKcatacactaSaatYtYcDaactHttcatRtttaatcaattataRtttgtaagMcgNttcatcBtYBagtNWNMtSHcattcRctttttRWgaKacKttgggagBcgttcgcMaWHtaatactgtctctatttataVgtttaBScttttaBMaNaatMacactYtBMggtHacMagtaRtctgcatttaHtcaaaatttgagKtgNtactBacaHtcgtatttctMaSRagcagttaatgtNtaaattgagagWcKtaNttagVtacgatttgaatttcgRtgtWcVatcgttaaDVctgtttBWgaccagaaagtcSgtVtatagaBccttttcctaaattgHtatcggRattttcaaggcYSKaagWaWtRactaaaacccBatMtttBaatYtaagaactSttcgaaScaatagtattgaccaagtgttttctaacatgtttNVaatcaaagagaaaNattaaRttttaVaaaccgcaggNMtatattVctcaagaggaacgBgtttaacaagttcKcYaatatactaaccBaaaSggttcNtattctagttRtBacgScVctcaatttaatYtaaaaaaatgSaatgatagaMBRatgRcMcgttgaWHtcaVYgaatYtaatctttYttatRaWtctgBtDcgatNatcKaBaDgatgtaNatWKctccgatattaacattNaaacDatgBgttctgtDtaaaMggtgaBaSHataacgccSctaBtttaRBtcNHcDatcDcctagagtcRtaBgWttDRVHagattYatgtatcWtaHtttYcattWtaaagtctNgtStggRNcgcggagSSaaagaaaatYcHDtcgctttaatgYcKBVSgtattRaYBaDaaatBgtatgaHtaaRaRgcaSWNtagatHaacttNctBtcaccatctMcatattccaSatttgcgaDagDgtatYtaaaVDtaagtttWVaagtagYatRttaagDcNgacKBcScagHtattatcDaDactaaaaaYgHttBcgaDttggataaaKSRcBMaBcgaBSttcWtgNBatRaccgattcatttataacggHVtaattcacaagagVttaaRaatVVRKcgWtVgacctgDgYaaHaWtctttcacMagggatVgactagMaaataKaaNWagKatagNaaWtaaaatttgaattttatttgctaaVgaHatBatcaaBWcBgttcMatcgBaaNgttcgSNaggSaRtttgHtRtattaNttcDcatSaVttttcgaaaaattgHatctaRaggSaNatMDaaatDcacgattttagaHgHaWtYgattaatHNSttatMSgggNtcKtYatRggtttgtMWVtttaYtagcagBagHaYagttatatggtBacYcattaRSataBatMtttaaatctHcaaaSaaaagttNSaaWcWRccRtKaagtBWtcaaattSttMtattggaaaccttaacgttBtWatttatatWcDaatagattcctScacctaagggRaaYtaNaatgVtBcttaaBaacaMVaaattatStYgRcctgtactatcMcVKatttcgSgatRHMaaaHtagtaaHtVgcaaataatatcgKKtgccaatBNgaaWcVttgagttaKatagttcaggKDatDtattgaKaVcaKtaataDataataHSaHcattagttaatRVYcNaHtaRcaaggtNHcgtcaaccaBaaagYtHWaaaRcKgaYaaDttgcWYtataRgaatatgtYtgcKtaNttWacatYHctRaDtYtattcBttttatcSataYaYgttWaRagcacHMgtttHtYttYaatcggtatStttcgtRSattaaDaKMaatatactaNBaWgctacacYtgaYVgtgHtaaaRaaRgHtagtWattataaaSDaaWtgMattatcgaaaagtaYRSaWtSgNtBgagcRYaMDtactaacttaWgtatctagacaagNtattHggataatYttYatcataDcgHgttBttctttVttgccgaaWtaaaacgKgtatctaaaaaNtccDtaDatBMaMggaatNKtatBaaatVtccRaHtaSacataHattgtttKVYattcataVaattWtcgtgMttcttKtgtctaacVtatctatatBRataactcgKatStatattcatHHRttKtccaacgtgggtgRgtgaMtattattggctatcgtgacMtRcBDtcttgtactaatRHttttaagatcgVMDStattatYBtttDttgtBtNttgRcMtYtgBacHaWaBaatDKctaagtgaaactaatgRaaKgatccaagNaaaatattaggWNtaagtatacttttKcgtcggSYtcttgRctataYcttatataaagtatattaatttataVaacacaDHatctatttttKYVatHRactttaBHccaWagtactBtcacgaVgcgttRtttttttSVgtSagtBaaattctgaHgactcttgMcattttagVtaagaattHctHtcaDaaNtaacRggWatagttcgtSttgaDatcNgNagctagDgatcNttKgttgtaDtctttRaaYStRatDtgMggactSttaDtagSaVtBDttgtDgccatcacaMattaaaMtNacaVcgSWcVaaDatcaHaatgaattaMtatccVtctBtaattgtWattatBRcWcaatgNNtactWYtDaKttaaatcactcagtRaaRgatggtKgcgccaaHgaggatStattYcaNMtcaBttacttatgagDaNtaMgaaWtgtttcttctaHtMNgttatctaWWatMtBtaaatagDVatgtBYtatcggcttaagacMRtaHScgatatYgRDtcattatSDaHggaaataNgaWSRRaaaBaatagBattaDctttgHWNttacaataaaaaaatacggtttgHgVtaHtWMttNtBtctagtMcgKMgHgYtataHaNagWtcaacYattaataYRgtaWKgaBctataaccgatttaHaNBRaRaMtccggtNgacMtctcatttgcaattcWgMacttacaaDaaNtactWatVtttagccttMaatcagVaagtctVaaDaBtattaattaYtNaYtggattaKtaKctYaMtattYgatattataatKtVgDcttatatNBtcgttgtStttttMagaggttaHYSttcKgtcKtDNtataagttataagSgttatDtRttattgttttSNggRtcaaKMNatgaatattgtBWtaMacctgggYgaSgaagYataagattacgagaatBtggtRcVHtgYggaDgaYaKagWagctatagacgaaHgtWaNgacttHRatVaWacKYtgRVNgVcSgRWctacatcKSactctgWYtBggtataagcttNRttVtgRcaWaaatDMatYattaactttcgaagRatSctgccttgcRKaccHtttSNVagtagHagBagttagaccaRtataBccataatSHatRtcHagacBWatagcaMtacaRtgtgaaBatctKRtScttccaNaatcNgtaatatWtcaMgactctBtWtaaNactHaaaaRctcgcatggctMcaaNtcagaaaaacacagtggggWttRttagtaagaVctVMtcgaatcttcMaaaHcaHBttcgattatgtcaDagcYRtBtYcgacMgtDcagcgaNgttaataatagcagKYYtcgtaBtYctMaRtaRtDagaaaacacatgYaBttgattattcgaaNttBctSataaMataWRgaHtttccgtDgaYtatggtDgHKgMtatttVtMtVagttaRatMattRagataaccctKctMtSttgaHagtcStctatttccSagatgttccacgaggYNttHRacgattcDatatDcataaaatBBttatcgaHtNHaaatatDNaggctgaNcaaggagttBttMgRagVatBcRtaWgatgBtSgaKtcgHtttgaatcaaDaHttcSBgHcagtVaaSttDcagccgttNBtgttHagYtattctttRWaaVtSttcatatKaaRaaaNacaVtVctMtSDtDtRHRcgtaatgctcttaaatSacacaatcgHattcaWcttaaaatHaaatcNctWttaNMcMtaKctVtcctaagYgatgatcYaaaRactctaRDaYagtaacgtDgaggaaatctcaaacatcaScttcKttNtaccatNtaNatacatttHaaDHgcaDatMWaaBttcRggctMaagctVYcacgatcaDttatYtaatcKatWatcaatVYtNagatttgattgaYttttYgacttVtcKaRagaaaHVgDtaMatKYagagttNatWttaccNtYtcDWgSatgaRgtMatgKtcgacaagWtacttaagtcgKtgatccttNcttatagMatHVggtagcgHctatagccctYttggtaattKNaacgaaYatatVctaataMaaaYtgVtcKaYtaataacagaatHcacVagatYWHttagaaSMaatWtYtgtaaagNaaacaVgaWtcacNWgataNttcaSagctMDaRttgNactaccgataMaaatgtttattDtcaagacgctDHYYatggttcaagccNctccttcMctttagacBtaaWtaWVHggaaaaNatttaDtDtgctaaHHtMtatNtMtagtcatttgcaaaRatacagRHtatDNtgtDgaatVgtVNtcaaatYBMaaaagcaKgtgatgatMgWWMaHttttMgMagatDtataaattaaccaactMtacataaattgRataatacgBtKtaataattRgtatDagDtcRDacctatRcagagcSHatNtcaScNtttggacNtaaggaccgtgKNttgttNcttgaaRgYgRtNtcagttBcttttcHtKtgcttYaaNgYagtaaatgaatggWaMattBHtatctatSgtcYtgcHtaattHgaaMtHcagaaSatggtatgccaHBtYtcNattWtgtNgctttaggtttgtWatNtgHtgcDttactttttttgcNtactKtWRaVcttcatagtgSNKaNccgaataaBttataataYtSagctttaaatSttggctaaKSaatRccgWHgagDttaaatcatgagMtcgagtVtaDggaBtatttgDacataaacgtagYRagBWtgDStKDgatgaagttcattatttaKWcataaatWRgatataRgttRacaaNKttNtKagaaYaStaactScattattaacgatttaaatgDtaattagatHgaYataaactatggggatVHtgccgtNgatNYcaStRtagaccacWcaMtatRagHgVactYtWHtcttcatgatWgagaKggagtatgaWtDtVtNaNtcgYYgtaaactttaDtBactagtaDctatagtaatatttatatataacgHaaaRagKattSagttYtStagagagacgatgaaaattaatcgtcaatacgctggcgaacactgagggggacccaatgctcttctcggtctaaaaaggaatgtgtcagaaattggtcagttcaaaagtagaccggatctttgcggagaacaattcacggaacgtagcgttgggaaatatcctttctaccacacatcggattttcgccctctcccattatttattgtgttctcacatagaattattgtttagacatccctcgttgtatggagagttgcccgagcgtaaaggcataatccatataccgccgggtgagtgacctgaaattgtttttagttgggatttcgctatggattagcttacacgaagagattctaatggtactataggataattataatgctgcgtggcgcagtacaccgttacaaacgtcgttcgcatatgtggctaacacggtgaaaatacctacatcgtatttgcaatttcggtcgtttcatagagcgcattgaattactcaaaaattatatatgttgattatttgattagactgcgtggaaagaaggggtactcaagccatttgtaaaagctgcatctcgcttaagtttgagagcttacattagtctatttcagtcttctaggaaatgtctgtgtgagtggttgtcgtccataggtcactggcatatgcgattcatgacatgctaaactaagaaagtagattactattaccggcatgcctaatgcgattgcactgctatgaaggtgcggacgtcgcgcccatgtagccctgataataccaatacttacatttggtcagcaattctgacattatacctagcacccataaatttactcagacttgaggacaggctcttggagtcgatcttctgtttgtatgcatgtgatcatatagatgaataagcgatgcgactagttagggcatagtatagatctgtgtatacagttcagctgaacgtccgcgagtggaagtacagctgagatctatcctaaaatgcaaccatatcgttcacacatgatatgaacccagggggaaacattgagttcagttaaattggcagcgaatcccccaagaagaaggcggagtgacgttgaacgggcttatggtttttcagtacttcctccgtataagttgagcgaaatgtaaacagaataatcgttgtgttaacaacattaaaatcgcggaatatgatgagaatacacagtgtgagcatttcacttgtaaaatatctttggtagaacttactttgctttaaatatgttaaaccgatctaataatctacaaaacggtagattttgcctagcacattgcgtccttctctattcagatagaggcaatactcagaaggttttatccaaagcactgtgttgactaacctaagttttagtctaataatcatgattgattataggtgccgtggactacatgactcgtccacaaataatacttagcagatcagcaattggccaagcacccgacttttatttaatggttgtgcaatagtccagattcgtattcgggactctttcaaataatagtttcctggcatctaagtaagaaaagctcataaggaagcgatattatgacacgctcttccgccgctgttttgaaacttgagtattgctcgtccgaaattgagggtcacttcaaaatttactgagaagacgaagatcgactaaagttaaaatgctagtccacagttggtcaagttgaattcatccacgagttatatagctattttaatttatagtcgagtgtacaaaaaacatccacaataagatttatcttagaataacaacccccgtatcatcgaaatcctccgttatggcctgactcctcgagcttatagcatttgtgctggcgctcttgccaggaacttgctcgcgaggtggtgacgagtgagatgatcagtttcattatgatgatacgattttatcgcgactagttaatcatcatagcaagtaaaatttgaattatgtcattatcatgctccattaacaggttatttaattgatactgacgaaattttttcacaatgggttttctagaatttaatatcagtaattgaagccttcataggggtcctactagtatcctacacgacgcaggtccgcagtatcctggagggacgtgttactgattaaaagggtcaaaggaatgaaggctcacaatgttacctgcttcaccatagtgagccgatgagttttacattagtactaaatcccaaatcatactttacgatgaggcttgctagcgctaaagagaatacatacaccaccacatagaattgttagcgatgatatcaaatagactcctggaagtgtcagggggaaactgttcaatatttcgtccacaggactgaccaggcatggaaaagactgacgttggaaactataccatctcacgcccgacgcttcactaattgatgatccaaaaaatatagcccggattcctgattagcaaagggttcacagagaaagatattatcgacgtatatcccaaaaaacagacgtaatgtgcatcttcgaatcgggatgaatacttgtatcataaaaatgtgacctctagtatacaggttaatgttagtgatacacaatactcgtgggccatgggttctcaaataaaatgtaatattgcgtcgatcactcacccacgtatttggtctaattatgttttatttagtgacaatccaatagataaccggtcctattaagggctatatttttagcgaccacgcgtttaaacaaaggattgtatgtagatggtaccagtttaattgccagtgggcaatcctaagcaaaatgagattctatcctaaagtttgggcttgatataagatttcggatgtatgggttttataatcgttggagagctcaatcatgagctaatacatggatttcgctacctcaccgagagaccttgcatgaagaattctaaccaaaagtttaataggccggattggattgagttaattaagaccttgttcagtcatagtaaaaacccttaaattttaccgattgacaaagtgagcagtcgcaataccctatgcgaaacgcctcgatagtgactaggtatacaaggtttttgagttcctttgaaatagttaactaatttaaaattaattaacgacatggaaatcacagaacctaatgctttgtaggagttatttatgctgtttactgcctctacaaccctaataaagcagtcctaagaatgaaacgcatcttttagttcagaaagtggtatccagggtggtcaatttaataaattcaacatcgggtctcaggatattcggtcatataatttattaagggctcttcgagtcttactctgagtgaaattggaaacagtcatccttttcgttgtgaggcatcttacaccgctatcgatatacaatgcattccaccgcggtgtcccgtacacaaggaaacttgttaccttggggatataagaaaactcacacgtctcattattaaactgagtacaatttttgcacgagaaagtaatgcaatacaatatgatgaaagccagctaatgaaaagggatggaacgcacctcggatctgttgcactggattaaaatccgattatttttaaaaatattcagtgctagagcatatcaggtctacttttttatctggtatgtaaagcccacggagcgatagtgagatccttacgactcaacgaaaagttataacataactcccgttagccaaagcccaatcccgattactgccctaccctaacgtctgccatctaaatatcgaacttgttatgatcaatgtgactacctcccaccctttccccttcatttgttccactggggataagctagcgttttcagaatcaatgcaataagaatagccaattgtctcacttcatcagagctcttggcaattccaggcgctacgtggttctggaatatattcatttttcaaatagtaatacgtttagtgttgctattgtctacacgtttggatattacgttatgtgagcggacatcaatagttgtctaactctttagtaagccagagatagcactcttagcgaatggataccatcttccataagtttagttaatagtccgaaacaactgcttcgagcatatttgaacctccttgtaggcaaatagcctcttcaaagcaatcttactaatagatagagtttgttttaagggactactagaaatgggacaatcttaatagtatgacctaaactgacatttaaagatatatccaggtggcaagcataaagatcattgcgccacctccaccgtgggattacttatcagtcgatatcctatatgctaagtttgcgacggcagaatacaaactaagctgagttgatgctaaccttacctatgataccccattggaccggttaacagccctacttattccaaataaaagaacttttatgctgtagaagctattatagtgatgcctggtaacttcagtatattaaaatgacacacatacgccatatagagctcctggaactttgaataatgagcgaacttcgaagttgaagagcaagaaaccatatgtcacggttgcctaaagcccggtaaccagacatgtgctatcattgatcattatcgaggttttcataaccttgacccattatcggctgtgcgcggacaagtacttaaatcactagtttcttcacctgcttatcggtaagaaataaggttggcaaagaatcgcataagacggacgtagagccgcagcgttgtgcgagtccaggtgcatgcgcagcaataggattttaaattttgttccatttttaatttagccgtaaggatgtccgtaaatgattgaaaattggattcaatctttgggcctatgctactggaacctgatcgacaaaatttcaaacatacgttaactccgaaagaccgtatttttgcggctagaatagtcagtcgcttggagccatataccttaccacttaaacgacgtgctcctgtagttgaaatataaacagaacacaaagactaccgatcatatcaactgaagatctttgtaactttgaggcgaagcaccctcttcgagacaactaagagtaaagtaccgggcgccgcaaggagtcgattgggaccctaaatcttgacgaattgctaagaggctcagagctaccactgtaatttctctagagcccataataaatgaacgatacatccgtaggtagcacctaagggattataatggaagccaaatgcagttaataatattatatactggcgtacacgattcgacggatctctcacatagtgattcacgacccccccctttgattgacacagcgtcagcattttgcaagaacgatcttctgcatagggtgcgccaccgtaaggatgacgtcgaagctacaactgggtataatttaccatgcttccctgatgctgagtgcaatacactaagaatgagtttttaccccatatcaccagtatttgttctgttattgcgaagaaatggctatgctgagttggcgactaaagtcacccatcctttttattaggtaaccccctcccttaaactaactgatttgctggagctgccctgcatacatatactttatcatttatggacgtccgtgacgcttattatccaccatagtcgatatgctacacggattcattaatggatcgtaggagtttaagttatatttactaagatcggtctcggctactatcccgccttacccggcgctatttacggccatttttaatatattgacggtaattattcctatggtttcgaccgcacgtccttggacaagaaagaatggcaaaaaaaatgtaaaagaaaaaaaatattgagtccctaccatcatataaaaaatatgtgatgagtaacttgacgaaatgttagtggttattaaagactatctattacaccttttgttttctgtcgtagtatattaaagtctagaagccttacaggaaaatcagggttatacagccgatactccgcagcatgaatcatcgaggaggtgtcctaccatcgcgccttgtaatcttgtctgtgtatactgtatttagaccttttatacaaagtaaatatctcggctttatgtgattgggaggggcctactcaaacatgatgacttgacctaataatcactgtgcgggcgtcttatgactagctattccttgaaatccaccaccaaatggttaatatgtaaaaactttgacgatgaaacaaggtgaatgtgtagttactttgtgtaattagctgcgtcgagcattgcttgtaaaaccgtcaatcgcacacgttacttccataaaatttctacgaatacacccttcttaaaaaaaacgtaggaattcacgagtttaacaaacgataactgtataaagtggaagtccgaagaaagcagatgcccgaactactcgaagatgtttcgttttcttaaccataggggcttcttaatggcccactacgcacattttgttcaagcccgagagggacatccccattacgggagtattactaaaactgttccgtaatacgttcagcaagggatgaaaaaggccactgctcaagttattgacgtgggagtattacatcggaagcctgaatcccacactatgatggtctgtacaggcctagggactgcgtctagacggtattaccggcttctaatcatacgatcgtgagtcttaacgggaagtaaggctcacacctaccccaaaccatttatctatgtaagtataaaattgtgcgtaagtgttcaaagtggacaataaagacgtggcaaaaacccccgcacataagccgctttagatttcacaaataccaatgcggttaaaaacatccttgagtcgtacatacaccatactcgcgttaaacggatataacagaagataataaatccggatgtggagtcggtgtaactatagaaagccaagtgaaataatgcttaccagtcatttagctatacggctttcatttcatgtcaagagggtggagtttgacctgtacagttgatatatcaccgatacttagaactcacctaaagctaaaattgctcgcagcgtgtaatccgcatattacaaacaatagatgggattcattatacataagacacgatgatctgctttttcaggttgcgagatgttgcctatcgtcaatcgagtcctgccttacaccacttaaacaaaagtattgacagggaacctattttcgaggtattatatagtccagcttgaatatcaatttgacagttaacctagtgaaaatcagtaagaggaaatacgccacattctccagtgaaattctacgggttatcgtctagtccaactatcaattataactcacgagatataagtaaattctcgtacttggcctgatttttattatactttggatccttagtaaacaggaagggagaaaccttcaacgaaaaacactggattttgttttactctcaaagctcttatatgacggaaataccctgtcaagtcttaactttattactagactaatgaaatgggcttggggtggccagaatcatagtacaatttagcggatacactattcggactttcctatcggctgtctggttggataagtatggggactaataggctagacatacctatacttaaactatacaggcgtcatctatctctgcaactttggagttccctgatgttctcccgccctttgggttcacatcttctataccgacacccctaataacgattagtttgtgggttagagtaaattaatacggttaatattaatgtatcgttgaaaagctggtgtcgccaataaggtaaccggctaggcagagtatatgtcacgaagtataactaccctaatgataagctgtaggaataaaattaatgctgtctctaagcgaagagatatttccgactctgttttaatgacgaatctcattacttctgacttgcaaatgttcaatatggcacggtttcacggcacctttgtgacgcatataatgaacttagaagattataacgacggaactttatatgataatccgttacgattaaagaatctgttaaatatcataatggcattcagttctagaccgtgcatcatggtaaacttactttctctgcatggcgacatacatttcgctattcaaattcgcgtgtggttacacccactcgcacctttggaatattaagagaagatgatcagaaaatccattcgctcaatttttctgacgtacgtctaatttatcctaggagacaaatcgttttatgtctctcacatttttgaagaaaggttcgagagacaatactcaggtcctgaactgctagaagatactcggtggagcgtggcaacaatgaaaaactcgtgacataaatgaatgatacttttccaagttcagttaagtgaatatgtttaacatacccggcttttcgatcttaagctgacgctggacgtgcgagtaatgtcagtctcttacatacactagtgactccaagtttcgtcaaaaacgccccctcccttctcgagcccactcacgctatgtattgacgcgaacttgttcgggatcagacttttcaggagttcggtcgcgtgtccctatgtgctaatatataagttagatcgcattagatgctaatctgaatacttatagacgaccttcaacgagaacgggtaccaccttgaggctagagttaggtgtgaaacgacaggtagggacatataaaatttgagtgcggctttagttaagggtttaattacctactcaaacatcacgctcgcgcccttcgtacgtaatcgaccatctagaggctaaggggactgtactaggtagtgattaatgatatcctagacgcacgtgccttagatcttcagactctgatggtccgcgatcaccgtaattgtagtcctccaactcgatcactttgttggcgtcaaagaaattacgatatctaaatacttataatacaataaccaaggatgagaatgactcatcgcgttggagttatattgcttgaagttctatggaatgaaagcacgttatctgccgtcccaatatctccagtgagctaattcattggacggtccactttgatcaatccccgaggagatgttcggacactttagtctgtaacacttagcgttgagaccacgaacaattgattactcagtcttgaaggtgttttccaaagttcattttaaataagactacgataggcctttcctattgatataaactacccggctctgttgttcgtgtgagtcgtacttctctgtgtttttctgattatagcaagattcgattcttagtgtaaacagcgatttttatttgacccgtcaatgagaagcgcataggatctaagcaaaattatcaagttgtgccacaaggtaagatctttccagttattgcaggtaggatgtatcccacgttgatagtatgaggtctgacgtcaactgtctaggagagttgaccgcgtgcgggtacaccggatttgcatcgatgttgagaacgcagaactcccactgtcgtggcggcgttcctgatatttagcaagaggcgttgataaagccctcatcatctagatctcgacctcatctgccctcttgctccatcattttctacacagactactttcctatctacgttagtataattgctttctatcttagtatcatttagagcttctccgtcaacaggttcgtgctattaaagttagtacgaaagggacaacttgtagcaacgcatttaatcggttttcgactacttcgcacaaaatcagataaagaagtttgtcattctattagacattgaattgcgcaattgacttgtaccacttatgatcgaacactgaatcaagactgtgattaactaaaatagacaagccactatatcaactaataaaaacgcccctggtggtcgaacatagttgactacaggataattaattggactggagccattacattctctacaatcgtatcacttcccaagtagacaactttgaccttgtagtttcatgtacaaaaaaatgctttcgcaggagcacattggtagttcaatagtttcatgggaacctcttgagccgtcttctgtgggtgtgttcggatagtaggtactgataaagtcgtgtcgctttcgatgagagggaattcaccggaaaacaccttggttaacaggatagtctatgtaaacttcgagacatgtttaagagttaccagcttaatccacggtgctctactagtatcatcagctgtcttgcctcgcctagaaatatgcattctatcgttatcctatcaacggttgccgtactgagcagccttattgtggaagagtaatatataaatgtagtcttgtctttacgaagcagacgtaagtaataatgacttggaataccaaaactaaacatagtggattatcatactcaagaactctccagataaataacagtttttacgatacgtcaccaatgagcttaaagattaggatcctcaaaactgatacaaacgctaattcatttgttattggatccagtatcagttaaactgaatggagtgaagattgtagaatgttgttctggcctcgcatggggtctaggtgatatacaatttctcatacttacacggtagtggaaatctgattctagcttcgtagctgactatactcaaggaaccactgctcaaggtaggagactagttccgaccctacagtcaaagtggccgaagcttaaactatagactagttgttaaatgctgatttcaagatatcatctatatacagtttggacaattatgtgtgcgaaactaaaattcatgctattcagatggatttcacttatgccttagaaacagatattgcccgagctcaatcaacagttttagccggaaacaatcgaagcatagggacaatgtatcttttcctaaattgccatgtgcagatttctgagtgtcacgaagcgcataatagaatcttgtgttgcctcaactcgttgaaaagtttaaaacaatcgcagcagtctttttggggtctactgtgtgtttgcaaaataactgaaagaaacgcttgaacaactctgaagtagctcgagtactcattaaagtgtaacacattagtgaatatcggccaatgaaccaaacgcttcccggtacgctatctctctcatcgggaggcgatgtgcaggttatctacgaaagcatccctttacgttgagagtgtcgatgcatgaacctcattgtaacaatagcccagcaaattctcatacgtgcctcagggtccgggcgtactcctccatggaagggcgcgcatctagtgttataccaactcgctttttaactactatgctgtagttctacaggcatagtggccagtattttctaacttctctggatagatgctctcactcctcatccatcacggcttcagtttacgtcttacttgcttgttcagcaacggatggaggcattaagtatcttcactgttccctaaaattgctgttcaatatcaaagtaaggacgatacagggaaagctcaagcacactcattgaatactgccccagttgcaacctcacttaatctgacaaaaataatgactactctaagtgttgcggaagcagtctcttccacgagcttgtctgtatcacttcgtataggcatgtaactcgatagacacgaacaccgagtgagaaactatattcttgcttccgtgtgtgtgacaccaggtaattgatgcggatataagctggagatcactcacgcccacacaaggcgctgctacctctttattccaatgtgtaagaatttgctaacttcatttctagaccgcagctttgcggtcataatttcacggtacggacccttgggttagagacttgataacacacttcgcagtttccaccgcgcacatgttttagtggcttctaacatagaatttttgttgtgacataaagagtgcgtgggagacttgcccgaccgttaagccataatcaattgaaagccccgtgagtcacatctaattggttgtactgcgcatttagctatcctttagctgactcgaagagattcgattcctaatataggttaattagatggctgccgcgcgaagtaaaacgtgaaaaacgtagtgcgcagatctgcataactcgcgcttaattacttatgagtagttccaagttcgctacgttatgagagagattggaattaagcaaatatgttttatggtgattttgggatgagaaggactgctaagtacggctactaaacaaatttctaaaaccgccatctaccttatcttggagacatttaagttgtatatgtcactagtctagcttttgtctgtgggacgcgttctcggaatgagggaaatgcaagagccgattcatcaaatgcttatctaagaaagtagtggactattacaccaagcacgaatgccagggaactgctttcttgctcaggacctcgcgacaaggtaccccgcataagtcctagaattacatttggtcagcaatgctgacatttgaccgtgaaaacataattttaatcagaaggcagctcacccgcttgctctagatcttatctttgtatgaatgtcagaatttactgcaatatccgttccgaatagtgagggcttagtatagttctctgtatacaggtcacatcaaactccccctgtcctagtacagctctgagctttaattaattgcatacatttccttcaatcatcagatgaaaacaccgcgaatcatgctcttctcgtatagggcaagagaagcaacaaacaactagcccgactcacgttcatccgccgtatccttgttcagttcttactccgtattaggtcagcgaaatctaatcagaataatcggtcgcgtatcaaaattaaaatcccgcttgaggttgacaattaaaacgctgagcagttatcggctattagatagtggggtgaaagtaattggctggaattatgttaaaacgtgatattaagctaaaatacgctacttgttgccgacctaattcagtcattcgatattcagttagagccaagaataacaagcttgtataaattgaacggggtgcactaaacgatgtgttactctaatattcagcttggagtatacctgaaggcgaattcatgtatcggccaataataagacgttgaagatcacaatttggactagcaaaagaaggtgatttatgcgtggggattgagtccactgtacgagtacggtctctggaaaattataggttcagggaatataaggaagtaaagataattaccaagagatttttggtatcgctatgacccagaggtgttctaacgtctgttttgatccgcagaatttctgcctcaatgcatatttgacggacttgaactagagcctctaaagttaaatggcgacgcaactgttcctaaacttcaattattactactctttttttcctagggtattgtagaggccagtggacaaaataaatcaaatttaagatgtttcggacattaacatcccccgtagcatagaaatcatcagttatccaatctctcatcgagcttttacaatttctgctggcgctatggacagcatatgccgcgagacctccgcaagactcacttgatcactgtaagtatcttcattagaggttagagcctatagttaagctgctgacctagtaaaattggtattttctaattttattgctcaagttaaaggttagtgaagggataatgacgttatttttgaacaatgggttgtattcaattttatatcacgaatggaacccttcattcccggcataatactagacgacacgaacaagctccgatctatcagccaggcacgtgttaaggtttaattccggcaaaccaatgaagcatcaaaaggtgacctgatgcaacttagggtcacgatgagtttttcaggactacttattacctattaataagttaacatgagccttcataccccgtaagacaatacatactccaccaattagaattctgagccatcttatctttttgtatcatcgaagggtatggccgaataggttaattagttactcctaacgtctctacaggcatgcatttgacgcaccttcgaaaatagtcaatctctcgccacacgcgtctagtatgcagcatcaaaaatatagtccacggtttccggattaccaaacgcggcaaagagaaacattgtatcgacggagataacttaatacagaaggaaggggcatcttcgaatacggatgaataattctatctgtttattctgacatcttgttttcaggttaatcttacgcattcaaatgacgcctgccccatgcgtgcgcaattattttctaatattgacgagagcaatctcactccttttgggtctatttatgttttattgaggcacaagcctatacagaacaggtactattaaggccgtgagtgtgagactcaaaccgtggaaacaaaggatgggttgttcttggtacaagttttagtgcatgtgggcaatccttaccaaaatcagatgctatccttaactttgggctgcatttaagatggcggttggaggcctgtgagaatcctgcgtgtcatctttaatgaccgaattcatccatgtagattcagatcacacactcattccttgatgttgtctaaacaaaagttgttgtggacgcattggagggagttaagtaacaacttgggatcgcatacttataaaaattatatgttaaactttcacaaacgctgaagtccaaagtaactagcccaaacgcctcgagagtcactaggtattaatggtgtttgagttcctgtgaaatagtgttcgaaggtaaaatttatgtaccaaatcgaaagaacacttaataaggcttgcttgcacggaggtatgatgtttactgactctacaaccctaattttccagtacgtacattcattccaataggttagttctcaaagtgctatacaggctcctcaattgatgatatgcttcagccgctctatggatattagctcattttatttaggaagcccgcttagaggcttactatgagggaaatgccaaaatgtcatacttttcggtgtgtcccatatgacaccgctttacatagaatttgaattaaaacgcgctctcccgttcactaccatacttggtaccgtgcgcatattacatatagatataggatcattttttaaagctgtactaggtttgatcgacaatcttatgctatactatatgatgtaaccctcataatcaataccgatcgtacgatcctagcataggtggcaagcgattttatgccgattattgtgttaaatagtctgtgagtgtgattatcagggctacgttggtagaggggttgtatagacctcgcacacattgtgacatacttaacaatatacgaaaactgatataataaatccccttacccaaacaccaatcccgttgaatcaactaccataacgtctcccatataaattgcctacttgtttgcataaatctgaatacataacaccattgcaccttcttgtgttccaatcccgttaagattgccttgtcagatgatatgcaagaacaatagcatttgctagcaattattaacagctcttcgaattgcctccacataacgcgggagggtatattttaatttggcaaatactaagtactgttggcgtcatatgctattaacggttggatattaagttatgtcagccgtaagcaagagtgggcgaaatattttgttacccagtgagagcactcttagagtttggatacaataggccatatgttgacttaagaggacgtaactacgccgtacaccattgttcaaccgacttcttggcaaatagaatcgtattagcaatcttaagaatagagacacgttcgtgttagggtatactacaaatccgaaaatcttaagaggatcacctaaactgaaatttatacatatttcaacgtggatagatttaacataattcagccacctccaacctgggagtaattttcagtagatttactagatgattagtggcccaacgcacttgactatataagatctggggatcctaacctgacctatgagacaaaattggaaacgttaacagcccttatgtgtacaaagaaaagtaagttgttgctgttcaacagatgatagtcatgacgcgtaacttcactatagtaaattgaaacaaatacgcaatttagacagaatggtacggtcatgaatgacagtaattcgaagtgctagaccaacttaaaataggtaaacgtgcccgaaaccccccttaacagaaagctgctatcatggtgcagtatcgacgtgttcagaaacttgtaacttttgagcaggtccgagcacatggaagtatatcacgtgtttctgaaccggcttatccctaagatatatccgtcgcaaactttcgatttagtcccacgtagagcccaagcgttgtgcgactccacgtgcatgcccagaaatacgagtttaaatttggttacatggttaattttgaccgaagcatcgcactttatgattgataattggattcaatatgtcgccctatgcgaatgcaacatgatccacaatttggctataagacgtttaatccgtatcacactttgtttgcggctagtatagtaacgcccgtgcaccaagagtcagtaacaattataagtactccgcaggtacttcaaatataaaaactaatcaaacacgacccatatgatcatctgaagatatttggaactttctcgacaaccaccctcgtactcaatacttacactaatcgacaggcacacgcaacgtgtacagtcgcaccatattgagtcaagatttgcttagtggcgatgagcgtacacgcttatttctctagtcacaattagttatctacgagacatcacgagggagcaaataagcgatgttatggctacacataggcacgtatgaatatgatataagccagttaaacagtcgaaccatcgagcaaattctcatgcaccaacccacacgttgaggcacaaagagtaagctgtttgaatgtaacttcttctgctgagcgggccccaacgtaaggatcaactagaagagaaaactcggtattagtttaaatgcgtcacggagcatgagtgcatttcactaagaatgtctgtgtaaccaatataacatctatttgttatctgattgcctacttatggctttgcggtcgtggcgactaatgtctccaatccttttgaggtcggtaccaactccctttaaattacgctgtgcaggctcatgcactgcatacatatacggtagcaggtagggacctcacgcacccttattataatcaatagtagttatcagtcaacgaggcaggaatgctgaggtcgaggtgttggtatattttctatgtgccgtctaggcgactatcacgcattaccaggcgagatttaagccaattttgaatatagtcaacgtaatttttactatgggttccaccgaaacgccttgcacaactaagaatcccataaaatatcgatatcaaataaaagattgtgtcaataccttcatatatattttttcggttgactaacgtgaactaaggttaggggttttgtatgtctatataggaaacagtttcttttctgtcctactttagtaaagtcttcaagccttactccaaaatcacggtgattaagccgttactcagcagcatgattctgcctgctcgggtcctaaaatccagccttgtaagagtcgctgtgtattagctagggagacctttgttaaaaaggatatatcgcggcgggatgtgagtgcgtggcgcatactcaatcttcagctcgtgtcattataatatctctcccccacgcttttcactagatatgccgtgtaagcaaacaccttatgcttaatttcgaaaatattggtacttgaaaaaagctgtaggggtacttaatgtctggtaggagatcaggagagaattgagtgtaaaaccgtaaagccctcacctgacttcatgtaaatggcttagaagactccatgatttaataaatactacgaaggaaagactggatctaaagataactctagtaaggccaactcccttcaatgctgttgccagttataatccaagagctgtccttttctgaaccatagcggcttctgaagcgaactagaagcaaagttggttctagccagacagccacataccctgtacgggtgtattactaaaactggtccggtattagttcaccaagggaggaattaggcaaaggatctaggtatgcaagtcggagtattacatccctaccctgaatccatcaataggttcctctgtactggccttcgcaatgagtattcaaggttgtacagccgtataataataagatagtgactatgaacgggaagtaacccgctcaccttccccaaaacattgttatatctaagtattaaagtctgccgtagtgttaatactcgaaaataaacaactggcaaattacaccgcacttaagccgcttttgatttatatttttccaatgcgcttttaaaaataattcagtcctacatactaattaagacccttaaacggagatatcacaagttaagttttaaccatctcgactaggtggaactatagatacccaactcaatttatcattacctgtaatgttcctagaaggattgcatttcatgtcaagacggtggagtttcacagcgaaacttcagtgtgaacagattctgagaaatcacctaaacctattagtcagagcacccggttagaaccagttgtcaaaaaatagagcggttgcatgagacagaagtaacgatgagatccgttgtaacgttgagacatctggcctatcgtcaatacagtcctcccttaaaaatatttttaaatactaggcaaacccaacataggttagtcctatgtgatacgccacatggtatatcattttgtaacgttacctagggataatcaggaagtggaattacgcaaaagtagacagtgaaatgcttagggttatagtctagtccaaagataaaggataaagcacgtcagagaactatattagccgaatgggaatcattgttaggagactgtggatcatgtctaaaaagcaacgcagaaacagtcatcgaaaaaatctcgtttttgtttgaatctaaaagagctttgatgaccgatagtacctgtatactagttactgtattacgtgtctaatgatttcggattggggtccccagaatcagacgtcattgtagacgattcaagtttaccaatttaatttcccagctctccttggagaactatcgccaataattgcagtcactttccttttctgaaacgataaagccgtcagagttctctgcaacgttggacttacctgaggttctaacccactttcggttctaatagtagttaacgacacaacgaataacctttactgtggggctttcacgatattttttcgcttattattaatggttacgtcataagctggtgtccaaattaaggttaccggcttcgcagagtagttgtatccaagtataacttccctaatcataagatcgaggtagaaaattaatgctgtctctaaccgaacagatatgtcccactatgtggtatggacgttgctaattacttctgaagggaaattggtcattatggatacgtgtctaccatcaggtcggacgcagatatggttctgtcttcagttgatccaccgttctttataggataataactgacgattaaagattatggtaaatagattaagccaattctcttcttgtcagtgaagcatccttaactgacttgctctgcagcccctcatacatttagctattcaaagtaccggctcgtttcaaactctcccacctttggaagaggttgtcaacttgataagtatatcatttacagcattttttcggacgtacctctaatgtttcattgcagaaaattagttttttctatcgcacattttgcaagtaacgttagagacacaattatctgcgaatgaactgctagatctgacgaccgggagcctcgcaaatatcaaaaaagactgacatatatcaaggagtcgttgacaagtgctggtaagtcaattggtttatctgtcccggcgtttcgatcttaagctgaccatgcacggcagagtaatgtcactctcgttcttacaagtctgtctccaagggtcggcaaaaaagacccctccattctcgagcccactcacgatatgtagggacgacaacttgtgcggcttatgaattgtctggactgcgggcgagggtccatatctccgaagttagaagggacatacctttagatgataagatcaattcttattgacgaaattcatccacaacggggaacaacttcaccctagacttacgtctgaaaagacacctagcgtcttataaaaggtcagtgccccgtttcgtaaggctggaattacctacgcaaacttaaacctcgcgcccttccttacgtatcgacaagatagaggctatcgcgaatgtactacggaggcatgaatcatatactagaaccaagtgcctgtgatattaacaagatgatccgacgcgagcaccgtaattctaggcataaaactccagcaatttgggggccgaaaacaaatgacgttagctaattaattatatgacatgatcaaaggaggtcaatcacgcatcgagttcgacgtatattcattgaacttcgtgcgtttgaaagaaacttttatgaaggcaaaattgatcctgtctcctatttcatgcgtacctcctagttgataattccccgagcagtggttaggacacttttgtcggtatcaagttccggtctcaaaacgtaaaattctgtaatctgtatggatggtctgtgaattagttaatttttatgaagtcgtcgagacgcagttcctattgatttattctaaacggagatgtgcttcgtgggactcggaagtagatctgtgtttatgattattgctactttagatgctgactgttaactccgtgttgtttttcaaccgtatatcacaaccgaattggatagaacctatagtttcaagttctgccacaaggtatcatatttacagttagtgctggttgcttctttcaaacgtggtgagtttgtgctatcacgtcaacggtagagctcagtggaccgagtgcgcgttcaaccctgttccagagagggtgtgatagcacatataccacgctcgtcgaggcgttcatgatagtttgcaagagccggtgttaaacacatattattattgttatccaactaatcggacctatgcataaagcattgtctaaacagaataattgcctatatacggtagttttagtgatttatatcttagtatcagttagagcttcgaactcttcaggttcctcatatttaacgttcttcgaaagcgaaaacttctacaaacgaatgtaagcggttttccaagtagtacctataaatcacagaaagatctgtctcagtatagttgaaatggtattcagctagtgacgtgtaccaattatcatagttcactcaagcaagacgctcattaacgaatatagacaagacactatatcatataataaaaaagaacatggtgctcgaacatagttgaattcaccatattgaaggggaatgctgacatgtaattcgctactagacgatcaattccctacttgtcaaagttgaactggtacgttcttggaattaaatatgattgcgctggaccaaattgcgacttcttgagtttcagggcaaacgattgagccggaggatgtccgtctcttacctttcttgcttatgataaacgacggtccctgtacatcactgggaattctcagcaaaaataattgggtaaatcgagactcgatgtattcggccacaaaggtgttagacgttaaagattattcaacggggcgataataggatcataaccggtatgcaagcgcattgaaagagccatgagatccttatccgataaacgctgcacggtatgtgcagccttattgtcgatcacgaatttataaatgtagtctgggctgtaagttgaagacctaagttataatgaagtgcaataccaaatcgattcatagtggattatcagactcaagatatctcctgataaattacagttgttaagatacggataaaatgagatttaagattagcagcctctaatctgtttcaatcccgttggaatgtggtatgcgatcaaggttaagttaaaatcaagcctgtcttcagtcttgattcttgttctgccatcgcatgcggtctacgtgagttaatatgtagcttacgttctagcttgtgctaatctgagtatagattcgtagaggaatattatcaagcttccacgcctcaacgtacgtgtattggtcacacaagacactaaaagtggaagtagcgtaaactatagtctagttgttaaatgctcagttcttgttatattcgatatactcttggctaatttatgtctgagtatataaaattaatgatattaacttgcatttcacggatcccttagaaaaagattttgaccgagcgcattataaacggttacaccgaatcaatagaagcatacccaatagctttctttgaatttattgcctgcgcaacttggctgactctctagatccgaataattctatatggtcgtgacgaaactagttcattactgtttaaaatgccaacatgtcttttgggccgataatggctctttgcaaaattactcaatgatacgattgatcaaagcggtagttgctagtggtagcatgtaagtctatcaaatgtctgattatccgaaaatcttccaaaagagtccacgtaccatatctatctcatagcgacgcgaggggaaccttatctaactatcattccatttaccgggtgactctcgatgcaggatccgattgggataaattgcccagaaatggctcattcctgactaagggtaaggccgttctcagcaagggaaccccgcgaatctaggcttataccatctagattgttaactacttgcctgtagttctacagccatactggacagttgtttctaaatgatcgggattcatgctagcactcctctgaatgcaccgcgtaagtttaactattacgtccgtgggcagataaggatggaggctgtatgtatcttaactgttacctaatatggctggtaattatcaaagtaaggaccttaatgccatagcgctagcaatcgctttgtatactgaccatgtgccaacctctcttaatctgtaaaatataatgtcttagctaactgtggacgatcatgtctctgcctagagcttcgctgtatcaattcctatagccagcgtactagtgacacaacaacaccgtgtgagaaaagatattagtccttacgtctgtctctctacagcttattgatgaggattgaacatggacatatagctccccctcaaaagcagatgctacctctttattccattctcgaacatttgccgaacttaatttcgacaaacctgaggtcacgtcttaatttatcggtaacgtcacgtccctttgagactggataaatatattaccaggggccaacgagcaattgttggaggcgcttctataatacaaggtgtcttgtcaaagaaagacggcgtgcgtctcgtgcaactcacttaaccaatattaatgtgaaacccccctctctcacatcttatgcggtgtactgccctggtacatttcctgtacaggactccaacagtgtagattcctaagatagctgttggagttgcctcacgccagatcgaaaaactgaataaactagtgagctgagctgcagaaataccgcttaattacttatgactagttcaaagggacctacgtgatgtcagacattgcaaggaagaaattaggtttgtgcgtcattttggctggactagcactccttacttcccctactattcaaatgtcgtaaacagcatgagacaggatcgtgctgacatttaaggtctattgggaacgaggctacctttggtcgcgcgctcgcgttctccgaatgaccgaaatgcatgagcacagtatgcaattgcttatagatctaaggtctggtcgttgaaaccaagcacgtaggcctgggaaatcagttcttcctcagcaactacacaaaagcgtccaagcattagtacttgtagtaaatgtccgaacctatgcgctcatttgaaagtcaaaaaatatttttaagcagtaggcacctaacccgattcctctacttagtagctttctttgattctcagaattgactgcaatatcactgcacaattctgtgccattactagacttctctgtattaacgtctcatcttactaacactcgcctaggacacatctgagagtgaagtatttcaatacatttactgaaatcttcagttctaaaatccccgaataaggctcttatcggtttggccaacacaagaaaaaaacttcttgcaccactcaccttcatacgcaggagcctggggaacttagtaataactatttcggcagacaaagcttataacaagttgccggcgcgtataatatttaaaagaccccttgagctgctcaattaaaacgctcacctggtataggctattagatagtgccgtcttagtaaggggcgggaattatcggataaactgatattttgataaaataaccgacttgttcacgacataagtcactaaggagattttatctttctccaaagtatatcttccttggataatttcaaagcgctgcaatttaagttctgttactagtttatgctgctgggaggtgaccggaaggcgtagtaatctagaggcaaattataagaagttcatcatatcattttcgactacaaaaacaaggtgttgtatgccggcgcattgtgtaaactggacgagtaccctagatggaaaattatacgttaagccaagatttcgatgtaatgataattacctacacatttttgctatccataggaacaagagctgttctataggctcgtggcatacgaacatttgctgccgctatgaatattggaagctcttcaactacagactctattcttaattgccgtcgaaaatgggccgaatcggctattattaatactcggtttttccgaggggattgttgtcgacagtcgtaattattattaatattgatgttggtgaggtcatttaaatacaaccttgcagacaatgaataagggatccaatctctcatactccttttacaattgctcatgcccctatgcaaaccttatgccgccacacctccgcaactctctcttctgaactgtaagtagcttcattactggtttgagactatactgaagctgatgacattctaaaatggctattttcgaatgtgattcataatgtttatcgtttgggatggcagaatcacgttatttttgatatagcccgggtattctattgtatagaacgtatgctacaagtcattccccgaagaagactagaagtaaacaacatgcgaccatcgttaagccacgcaaggctgtagctttatttcccgataacctatcttccataaatagcggacagcaggatactgacgctcaacatcagtggttatggtctaatttttaacttttaataaggtaacttcagcaggcatacacagtaactctttaatttataatcaaattagaagtctgacacttcttatatttttctatcatccaacgcgatcgcccattagcttattgtgttactaataacgtatctaaaccaatccttttcaagctactgcctatattgtcaatatatacaaacaacaggatagtaggctgcttaaaaaatattgtcaaccgtgtacgctttacaatacccggaaatcacaaactttgtagacaacgagtgaaatttatacactacgaagggccagcgtacaagacccatgaattaggcgatatgtttattctgacatattggtttatccttaatctgtcgctgtaaaatgaagccgcccccatccctgcgaattttttttcgaagattcacgactgaaatataaatacgtttggctatatttatgttggagggaggcaatagcctttactgttaaccgaagatttagccagtgagtgtgacactaaaacactggaataaatgcaggcgttcttctgggtaaaaggtttagtcaatctcgcctataagttcatatagctctggatataattatctggcccatgcatttatcatggcgcttggtgccctgtgtgaagccggcctctcatattgaaggtccgaagtattccatgtacattaagatcactctctcattcatgcatcttggcttaacaaatctggttgtccaagctttccaggcacgtatggtacaaattcggatcgaatacttataaaaatgatatgttaaactgtctaaaacgctcatctacaaagtaaagtgcactaaccaatagagtctcaagaccgtgtaatgctggtgcactgaatgtgtaatacggttagaagggattagttatgttacaaatccattgaaaacttaagaagcattgcgtgctcggagggtgcatcttttatcaagagactaacattattttcaacgacgtacatgctttacaatagggtacttatcaaacgccgagaaacgcgcctatagtgatgttatgattatgacccgatatccattggaccgaattttatgtaggttcccagcgtactcgcgtaatatctcggtattgccataatgtaatacttgtcggtctctcccagatgaaaaagcgttacagagtatttcaatgaaaaacagcgcgcaacgtcaatacctttaggggtaacggccgctgatttcatatagatatacgataagttggtatagctctactaggtggcatccacaatcgttgcatttactatagctggttacaatcataatctataccgttccttacatactaccatagcgggatagcgtttttttgccgttgattgggtttaagaggatgtcagtctcattatatccgattcggtgggagagccgttgttttcaaatcgcacactttgtgacataatgtacaagataacaaaactgatataagatataaactgtcaatatcaccttgacacttgaatcaaagtaaattaactcgcaaatataatttgactaattgggtgcagatttctcaattaataaaaaaatggcaccggatgggcttacaagccccttatcattcacttgtatcatgatttccaagaacaatagaatttgctagcaagtatgaacagagattcgaattgcatccacagtacgccggagcgtttattttaatgtggatatgacgatgtactgttggcggcatttgctagtaaccggtccttatttacgtagcgcacacgtaagcatgtctgggagaaatatggtggtacaatctcagagaaagattacagtttggtttaaataggacttatcgggtcggaagtggaacttaataagcagtacacaattgggcaacagacgtcttgcctattacaataggattacaatgcgttagatttcagacacgttcgtgtttggctattcgtcaattccctaaatagttagacgatcaactattatcaaagtgattctttgttcatcctccattcatgtaacagatggcacactacgcataacgccgaggaattttaacgagatttaagagagcagttcgggcacaacccacttgactttataacagctcggcagcataaacggtaatatgtgacaaatttccaaacgttataagaacgtatgtgtacttagaaaactaagtggttcatgttcaacagatgtgacgcagcaagcctaacttatctattggttttgctataaaagaacaaagttacacagaatcctaagggcttgtttcacacttatgcctagtgcttcaccatcttaaaatagcgaaaccggcacgaatcaaaccttaaaacaatgcgcagatattggtgatggtgactccgggtatgataatggtaactgttgaccagcgcccacctcatcgaagtatagaaagtggttaggataaggatgagaccgaacttatttccggccataactttagattttctacctagtacacaacatcagggcggacacgaaaccgccatcacatcatataccaggtttaatttgcttaatgggggaagtgtcaacgaaccttcgaactttagcaggcatatggccattatatatggccccagagcagaatgctacagcagacaaaatttggatttatgtagtttaatacctatcaaacttggtgtgaccatacttgtctaacgacagtgcacaaagtgtaagttacaattattactactcagcagcttctgcaatgataaaatcttatcatacacgtcacatatgataatatctacttagggggaacgggctccacaacctacatagtactcaatacttacactattcgacaggcacaccaaacctgtacagtcccaaaagattgagtcaactttgcagtactgcagatcacagtaatagcttagttagcgagtcaaaattagttttctacgagactgcacgaccgtgcaaatttccgatgtgttggctacaaatagcaacgtatgaatttgtttgaagccacgtaaactgtacaaccttagagataagtctcaggctactaaaaacacgttgtggcactaacaggatcatggttgattcttacttattcggctgaccggcccaataagtaaccttcaactagaacagaataatcgggagtagtttaattcagtcaaggtgcaggtctcattgtaactaacaagctctgtgtaaccaagttaaaatcgttttcttagcggattccctacttatggatttgagctcgtccacaatattcgatacaagaagtttgtggtccgtaacaacgaaattttaattacgctgtgcagcctcatccaaggaattaatagaaggttgatggtaggctccgaacgctccatgattataatcaagtggactgtgcagtaaacgaggaaggtatcctgacgtcgtggtgttcgtttttgttatttgtgccctatacgagtagataaaccatgaacagcacagtgtgaacccatggttgattttaggctaccttatttttaatttccgttacacagaaacgaattccacaactaacatgccattaatttttcgatatcttataaaagatggtcgaaattcattcatttattttttttcggttctcgaaagtcaactaagctgtcgcgttttgtttctctttagaggtaaaagtggctttgatctcctacgtttggatactagtcaaccattactccatttgatccgtgagtatcacctgtctaacatccagcattatgactcctcggcgaagaaaagacacacttcttagagtcgatgtgtattagctagggacacagttgtttaatacgatagtgagcccagggagggcagtgcgtcccccagtagatttattcagctagtgtaagtataagatatctcacccacgaggttcaagtgatatgcagtcttagaataatacttatcctgaatttcgatattatgggtacttcaataatccgctagcgctactttatgtctcgttggacagcaggacacatggcagtcttaaacactaaagacatcacctgaatgaatgtaatgggattacaagaatcaatgaggtattatatacgacgtaggaaactctggatatatacagtaatctagttacgccatcgcacttcattcctctggaaacttagaagacatcagctgtacgtggaggaaccagacccccgtatgtagccaaatagaaccaaagttgcttatacaaacacacccaatgacaatggaccgctggagttcgtaaactcggaacgtagtactgcacaaacccagcatttagcaataggagctacgtatgcaactcccacgtggtaataccttcaagctatcaatatataggtgcctagctaatcgcattcgcaagcagtattcaagcttgtaaaccagtataataattacagaggctctatgaaacccaactttccagctaaaagtcccaattaaatggttatttcgtacttttaaagtcgcccgttctgttattacgcgaattgattctactccaaaattaaacacaaattatcaaccgtttcatttatatttgtcaatgcagctgtttaaaataaggctctactaaattataattaagacacttattaccagatttctctagttaagtttgaaccagctcgactaccgcgaaagatacattcccttctctatttttcagttcatctatgggtcagagaagcattgaatttattctattcaccctcgtcgttcacagcgaatcgtcagtgtgatcagtgtatgagaaatatcctaaaccgtttagtcagaccacacgcttagaacaagtggtctaaaaagactgccctggaaggagtaagaagtatacagctgatccggtgtatccttcagtcatctgccctatactaattacacgacgcaaggaaaaataggtttattttctaggcaaacccttcataggtgactccgatgtgttacgaatcatgcttgagaatgtgctatcgttaccgacggataataacgatctccaatgaaccaaatgtagaatgtctattgattacccttttactattcgacttagagataggagatagaacctcagtgtacttttttagccgaatgggaatctttgggaggtgaatggccataaggtcgtaaatccaaccctcttaaagtcttccatattatatcgttgttcgtggaatcgataacagatttgttgacccatagtaaatgtatactagtttatgttgtaagtgtagattgttttccgattgccgtccaaactttatgtcgtaattgtagaccagtaaagttgaccaaggtaagtgcccagcgatcctgcgagatcgatcgccaatttttccagtcactgtaagtgtaggtttagataaagccgtatgagttatatcataagggcctcggaaagcagcttcgaaccaaagttcccttataatagtagtttaactataaaagtatatactggtctgtcgccctttcacgatttgttttaccggtttatgaagcgttacgtcattagagcggctccaatttaaggttaacggcttccatgtgtagttgtatacaaggataacttaaagtatctgttcagcgagctagttaagttatcctcgatagaacacaactcagaggtcccaagatcgggtttgcaacttgctaatttattctcaaggcaaattgggaattatcgatacctgtataccataaggtcgctcgatgtgatgcttatgtcttctggtgatcctaccttagttagtgctgattaacggaacattaatgtttatcgttttgagatttagccaattctctgattctaactcaagatgccttatctgacgtgctatgcagcccctaagtattttacattgtaataggacacgctcctttaaaactcgccaaaaggtcgttgtggttctctactggttaactatataatttacagctttgttgagctagttcctctttggtttaagtcctcaatattagttggttcgagcgataagttggctagttaccttagtcactatattagatccgaatgttatgcttcatctgaagaccgccaccctccaaaatttcttttaagactcacttattgcaaggtgtaggtgaattcggctcgtttctcaagtggtgtatctgtacacgagtttccatattttcatcaacagccaccgcacacttatgtcactctaggtattaaaagtcgctctacaaggggacgcaattaagaaacagacatgctagtcaaaaataaacatagcgaggcaccactaattcggccgcttatcaatgggatgctctgcgcgagacgcgccagagctcagtagttagttcggacatacatttacttcagatgatcaattagttttctacaaatgcttactctaccccgaaaaaagtcaccagactcttacgtctctttagtatccttccgtcttatataaggtcagtcccccgtttcggtaccctggaatttactaagaataatgaaacagcccccaaggacgtacgtttacaaatgatagaccagatcgcctagcttattccgacgcatgttgcatagaattgaaccaacggaatgtgagagtaactagatgagccgaccacagcacccgtttgcgtcgcagaatacgcctgatagttcggccacgaaatcatatgtcctttgagtattaagtatttgtaatgatcaatcgagctcaagcaagcttacacttcctcggatattcagggaacttagtgcctttgaaagatacgttgatcaacgaaaaattgataatggctcatatggaatgcctacctcatagtgctgaattaacacagcactgcggacctaacttttcgaggtttcaagttcacgtctcaaaacctaataggctggaatatgtagggatcctcggtgaatttgtgattgggtttgttgtagtactgaccaagtgaatattctttttttctaaaagcagatctgctgccgggcactacgaaggagatctctgtgtatcattattgcttcttgacatgatgactcttaaatcactgtgggtgtgcaaaacgatagcacaacccaattcgatagtacatattgttgatacttcgcactaaaccgttcatatttaaaggttgtgctccttccttcgttaaatactggtgacttggtcctatctactattagctagacctctggggaaccacgcccccgtaaaacctgtgcaagagagggggtcatacatcttagacatcgcgcctccaccagggaagcattgggtgattgaccaggtgtgtaacaaatatgattattcttatactaatattagcaaagatgcataatgatttgtattaaatgtataattgaattgataagggtcttttagtcagtgatagagtagtataaggtagacattagaactcttaaccggacgcagatttttcggtcttagtaagccaattagtcgacaaaacaaggtaagagcggttactagtagtacctataatgcactgaatcttcggtcgaagtatagttctaatgctatgcagattgtgacggcgacaaatgttcagacttatatcatgaaacaagctcttgtaagtattgacaaatgaaaagattgaatatttttaaatacaaaatgcgcctacttattaggggaattaaccagattgaaggccaatcctcacatgtaatgagataatagacgataaatgaaattcttgtaatagttgaactgctacgtgatgggtattatatatgattgagatcctccaattgccgacgtcttgtcttgatgcccaaaagattgtcaacgaggagctccctcgcgtacctgtcgtccgtatcataaacgacgcgacatgtacagcactccgaagtataagcaataataatgcgggtaatccagactagatcttttcggactcaatgcggtttcacggtaaacatgattaataccggagagtagtcgagcttatcagcgatgcaagcgaattcattgtgccaggagatacgttgcagataaaaccggcaacgtatgtcaacaagttttggcgatctcgttgtttgtattcgacgaggcgcgggaacttcaagaactatcgtatattcaagtccattaccttttagtttcagactggtggagctgactaaagttatatcatcattttgtacactggtttagttaacgataatttcagatttaacatgaccagacgataatcgctgtatatccagttggaatgtggtttgccagaaaggttaacttataatcaagcctctcttcagtcttgattcgtcgtatcccatccattgcgctatacctcagtgtatttggagctgtagttataccgtgtgctaagatcagtagacatgacgagagcaatattatctaccttacaagcatcaacggacgtctagtcggaacaaaagactctaaaactcgaacttcaggttaatatactatagttctgtattcagcagttattcttatattcgatattatcttgcctattggatgtctgactttagtatattaatcatagtatctgccatgtaaaggtgccagtactaaatctgtttcacagtgcgaattataaacggttacaaccattaaagacaacaagaccctatagctttatttgaattttgtcaatgcgcaacttggagctcgcgatacatcccaattagtctatagggtcgggacgattctacggcatttctggttataatgacaacatggattgtggcccgagaatcgctctttcattaattaagcaatcattacagtcttataagcgctacttccgagtggtagcaggtaactcgatataaggtcgcatgagccgaatagcttaaaaaacaggccaccgaacattgatagagaataccgaccacagcgcaacctttgattactttcattaaattgtacggctcactcgacatcaagcttaagattgcgataatgtgaactcaaatggatcagtactgaagaaccgtaacccacttcgcagaaagcgtacccagagaagatacgctgttacaatatacagggtgaaattattgcctgttcttcgtaaccatttcgccaaacttggttagaaatgatagccattcatgatagaaataagctgaatgataccagtatctttaactatgtagtcagggggaagataacgatggtccatgtatgtttctgatatgtgacagtattggccgcgtaatttgctaacgaagctacttaatgcctttgagcttcatatagatttctttaatcaaaatcggcaaaaagatagtatgagctataatatatgctagtagagaactctggaccatcatctatatgaatactgattcgagcgtgcaattactttagcctgcgtactactgactctacaaaacactctgagataagtttgtagtcagtaagtcgctctctataaaccttttggatgaccattgtacagccacttatagatcccaataaatagcacaggagacagagtttttcaatgctcgatcatttgccgatagtattttcgtctaacctcagggcacctattatttgatacctaacctaacggccctttcacaatggagaaatatatgacatcgggacaaacacaaatggtgggtggccaggagatatgacatggtggcgtctctaagaaacacggactccctctaggcaaactcacgtaaccaattttaatgtcaaacaaaacgctcgaaaagattttgccgtgtaatgacctggtacattgactggtcaggaatacatcactgtagttgccgtagtgtcctgttggtgttccatcaagacacatcgtataacgcaatttacgacggacatcagatcaagttatacagattatttaagtatcacgtgtgcattgggacataagggatctcacacatgccttggaacatttttgctttgtgccgctttttcgctgcactaccaatccttacttaccagtatattcaaaggtcgttaacagaatgagaaaggttagggctctaagttatcgtcgattgggatagacgagacatttgcgagcgccctccacggatacgaatctcccatatcaatgtgaactggatgctatgcagtttagttcttacgtctcctagtggtaaaaatcaaagtagcactcgcatagcagttattcagaacctaatacacaaaaccgtcaaacattttctaattctaggtatgggccgatcataggagctaaggtgaaactcataaatgttttgttagatctagcatcctaaaaagatgcatatactgagtagctggcgtgcattctctcaattgtatcctttttaactgaactagtcggtcccatttcgtgactgagatctattaaccgataagattaataacactcgcattcgtatcagctcagagtgaagtttttcaataatttgactgatatattaacttctaaaataaccctttaagcctcggatccgtttcccaatcacatcaaaaattcttattccaactatctacggattaacaacgtgcatggggatcgtagtaagaacttgttccgatcactttgagtatatcaagttgacggcccggttattattgaatagaaacattcacctgctaaattaaataccgcacatcggatacccgatttcagagggccgtcttactaagggcaggctttgttcggtttaactgagatgttcattattttacagtatgcttcaactaatatgtaacgaaggacagtggatctgtctccatagtagatcttcagtcgtgaatttcataccgctcctatttaagttcgcgttcgagttgttgatcatggcacgtgaaagcaacccctagtattctagacgaaaattttttctagttcatctgataatttgccaattcaaaaacaaccgctggtttcccggcgcattctctaaaatggaagtcgaacctagagccattatttgtcggtaacccatgagttccttcttttcagaagttaatacactgtggtcctatacagaggaaaaacagcggttatatacgatcgtggcataacaacattggatcaagatagcaatttggctacctattctaattctcactagattcggtattccactacaatatcggcagattaggattggatgaataatcggtgtttaagtccggttgcgtctccaatctcctaatttttattaatattgatcttggtgacctattgtaaataaaaacttcaagactttgaataacggtgaaaagatagaagactcatttgaaaatggatcatccacagatccaaacattagcaagacactaatccccaactagctattctgatcgcgatcgtgctgcagtactcctgtcacaatagtctgttcatgatctaattctttttgggctttgttcgatggtgattcagaatctttatccggtcgcttccctgtagctactttgtggggatattgcccggggattatagggttgagatcgtttcctaaaagtatttaaaccaagtagacttcaactaaactacatcagaacatcgtgaagacaccatacgcggtacctttatttaccgataacatttcttcaagaaataccggtaagcagcataatgaccctaaacagctcggggtatcgtcgtagttttaaattttatttaggttactgctcaaggaataaaaactaactatttaatttataataatattacaaggctcacactgattagatttgtctataagacttcgcgatcccccattaccggattgtcttaagaataaactagataaaccatgcattttctagataaggcctttagtctaattagatacaaaaaacacgatagttgcatccttaatttattgtgtcaaacctggaaccttttaattacccgcaaatcactttatgtcgagactacctctgaaatttattatctacctaccgcatgaggacttgaaccatcttgtaggagttatgtttattagctaagattcgtttatcctgtagcggtccatgtatattcaacaagcaaaaagcactcagaattgtttttagttgagtcaagactgatatataaataagtttccctagttttttcgtggtgggacgatattgaattgaatcttaaccgaagagtttcccactctgtcgcacaataatacacgccaatatttccagccctgcttatgccttaatcggttactcaatctcccattgaagttcattttgatctgcatagaagtttcgggcccagccttttttctgccaccttcctccaagctctgtagacgcactctaagattgatgctcacatgtattaattctacattaacataaatatataagtcatgcatcttcgagtaaaatatctggttctccaacatgtcctggcacgtatcgttataatgcccatacatgtagtattaaaatgattgggttaactggatattaagatcatcgaaattgtaaagtcaaattaacaatactgtctcaagaccgtgtattcctcgtgctcggaagggctattacgcttacttccgttttggtatcttaatatgactttcaaaaattaagttgcagtgagtcctacctgcgtgcatcggttagcaagagtataaaagttgtttaaacgaactacttgctttacaataccggtcgtatatatcgccgtgaatccagaagattgtcttctttggattatcaaccgagatcctgtggaccgatgttttgggaccttcacagaggactccaggtagagctcgcttttgcattaatctaagaattgtacctctctaaaagatctaaaacagtgaatgtgtatttcatggaaaaacacagagaaacgtaaattactttaggccgaaaggcacatgagttattatacatatacgagatggtggtatacatcgaattcggggcatacactatagttgcattgtatttagctgctttaaataatatgatattaccttccttacataagacattaccggcataccctggttttcaacttgtggggctttttgacgatcgcactctcatttgatccgagtagggcggtgacccctgcttttcaaatacaaaaatttcgctatgaaggtaatagattacttttcgctgttatgatagaaacggtaaatttaaaattgaaacttctagaaaagtaaagtaacgagaaatgattttgtgaataatgcggtcatgattgcgcaagtaagaaaaaaaggcaaaaggatgcgcggaatagaaacttatcagtcacgggtatcttgatttcattcttcttgtcaattgccgacataggatgaaatcagattccaatgcaatacacagtaacccccacccttgattgtaatgtcgatttgaagttgtacgcgtcgacgaagtggatagtatacgggccttttgtacggtgcgatcaactatgaatctcggcgagttagatggtcgtacaatctcacacatagaggtcacttgcctgtaatgacgaattttcggctaggtactcgaactttattagaagtaaaaatgtgggcaaaagaaggattccattttacaagacgattacaatgagttacatgtctctcaacgtagtctttccctagtagtctttgaactatttaggtactccagaaaattttagcaaagggtttctgtgtgaatccgccattcatgtttatgatggaacaataagaataacgccctcgtatgttatcgacagtgaagtcagcagttcggccaaaaacatattcaatttagtacagatccccagaagttaagctaagtgctctaaaatggcctaaacggttatcaaagtaggtctaattactatactaacgggtgcatcgtaataactgctgtcgatgcaacactatatgatagtgtcgttttgctatatatgtacaatgtgacaaagaagccttagcgattcttgcaaacttaggacttcggattctcaatcttaaatgtccgaaaacgcaaagattcaaaaatttaatctatgagcagatatgcctgatggtgactacgcgtatgttaaggctaaatgttgacaaccgcacacataatcgaactattgatagtcgggagcataaccaggtgaacgtactttgttcacgacatttattgacatgttctaaatacgtctcaaaatcacggcgcactagaaaacgcaatcaaatcattgtcctggtttaagggccgtaatgccggtagtgtcaaacttcatgagaactttagctggcttttggccagtatttagggaccaagagcactagccttaagctgaatattttgccatttatctactgttataactttaaaacttggtggcaccagacttgtcgatacacacgcatcaatctgtaacgtaaaaggtttactaagaacaagcgtaggaattgagtttatattatatttaaactaaaagatgatattagcttctgagggcgatagggctccaaatcataaagaggaatatattattacacgattagaaacccacaacatacctcgaatcgcccaaaagtttgacgaaacttggcagtactccacatctcagtaatacagttgggagagtctcaaatgttgttttattactcaatgaaccaccctcataatttcactgctgttccattaaatttgcaaacgatcatttgctttgaagaaacgtaaaatcgacaaaattacagataagtagatgcataataaaaaaaactgctcgctataacacgatcatcgtgcattcttacttaggagcatcacccgcacaataacgtaccttaaactacaacactattagaccgagtactgtaattcacgaaagctcaagctcgcattgtaaagaacttgctctctcgtaaaatgtgataatagtttgcggagaggattcaattattttccattgcacctactccactagattcgataaaagaaggtggtcctcccttaaaaagaaatgttaagtaacatcggaaccataagcaaagcatgtaagtgaaccgtcatccttccctaagaaacataaaggtttttaataatgtcgactgtgaactataactgcatcctttcctgacctactccggttccttgttgttatttctgaacgagaccagtagataaacaatgtaaaccacagtgggtaccaatggtgcatgtgacgctaccgttgttttaagtgcccgtacaaacataagaagtcataatcttacttgaaattaattttgccttttattttttttcaggctcgaaattaatgatttgttttttttgaccttctagttacgctaatatgcggtcgcctgtggtttctattgagtcctataacgggatgggatctaatacgtttggttactagtaaacaaggtataaatttgataccggagtatcaactgtataacatcaagctttatgactcatacgcgaagtaatgacacaaggctttcaggagatcgcgagtacagagccactaaggggtgtattacgatagtgacaccaccgagcgcactcactccccaagtagatttatgatcctacgctaagtattagatatataaccaaagaggttctagtcagtgcaactcttagaataataattagccggttttgcctttttaggcctaatgcaatattcagctagcccttatgtatctcgcgttccacagcaccactcatggcacgcgtttaaactaatcaaatataatctatgaatgttatgccagtacttgaataaatcaggttttttataagtccttgcatactctcgttatatactgttagagtcttaccccatagaaattctttcatctgcaaacttagaagaattctcagctacggggagcataaagtccccaggatgttgacaaatacaacaaatgtggcttatacaaacactccatatgaaaatcgaaccctcgtggtagttttagccgaaccttgtacggataaatccctccattttccaatagcagatacctatcctactacctcgtggtattaaattaaagcttgaaatatagagctgcatagcttatccaattcccaagcacgagtctaccgtcgtaaccacgatttgatttacagacgctagagcaaacccatctttaaacatataagtaaaaattaaagggtgagtgcgtacgtgtttactagcaacttcgcttattaagacaattgtttataagccataattaaaaacatatgttcaacaggttcattgatatttgtaattgcacaggtttttaataaggatctacgtaagtataatgaacaaactttttaccagagttatattctgtactttgaaaatgctcctctaccgccttagagactttcaattagattttttgcagttaatctatgcgtaagtgaaccatgcaagggatgcgattcaaccgcctcgtgctaaccctatcgtctgtctcataactgtaggtctaatataattttcagttttcgaacacataaccctttgaaaatctgctatttaatgtctcacctgcatgcactatcttctatactgctcagaacggctatacgtcactatgctccaagtgacgatttaaacgaagcaaggaataataggtttattttagtgcaaaacaattaagtgcggactacgtgctctttacaataagccttgtgattgggctataggttaagtcccatattaacgatctccaatgtacaaaatcgacaatcgctttgcattacccggttactagtcgaattacagatagctgttagatactcactctaattttggacaacaatcccaatcttggggtcgtctatcgcctgaagctcgtaaatccttccatcttaaacgattacatattatagacttgttcggggtagagatatcacagttgtgcaaacattgtaaatcgatactagtttatgttggtagtctagttgcttttaccattccccgaaaaacttgatctactatttcgacaacagtaaacttgaactaggtaagtgaaaacagagaatgcctcatagtgccactatttgtccactatatgtaagtgtagctttacataatccactatgactgagatcattacggcctaggaaagcagcgtagaaaaaaagggcccggatattacgactgtaactataaaactagttactggtagcgcgccatgtatagatttgttttaccggttgtggttgcgttaacgaatttcagccgcgaaaattgatccgttaaccagtccatctcgacttctataaaacgataaagtaaagttgatgttcagcctccttcttatggttgcatcgagagtacactactcagtgggaaatagatcggggttcctacttcagattgtattatctaggcaattgccgattgtgccatacctggataaaataagctacctacatgtgatgcttatctattatcgtcatactaccttagggtgtcctgttgaacgctacattaatctttagccgtttgagatgttccaatggataggagtctaacgcatgatgaagtttaggaaggcagagcatcccactaagtatgtgacagtgtatttcgaaacgagacgttataaatagaaaaaaggtccttctggttctattctgctgaactattgaatggaaagattggttgacctacgtactatttgcttgaagtcatcaatttgacggggtgagagacatatggtgcatactttacggactctatattttagatcagaagcttagcagtcttctctacaccccctcacgacataattgcttttaagaatctatgtttgattcctctacgggaattcggatccgttcgcatgtgcggtttatctaaaccaggggacatatgttcagctaaagcatacgaacactttgctaactagacgtatgtatagtagctataaatcccgacgatatttacaaaaagaaatgagactcaaatatatacatagcgaccctacacttattcgcaccctgatctaggcgatcctagcacccacacccgaaagtgagcactagtgtcttccgtattaaatttactgcagttgagattttagttgtctactaaggattactctaacccgtaataaggatcaagactcggtactagctttactatcattccctatgtgttttcctaactcacaagggtacgtaccagcctatgtaattacaataatgataaagacacaaaggaagtaactttacaaatgagtctccagttacactagcttagtccctcccatcttgctttgaagtctaaatacgcaatctctgaggatatacagcagaagaacactcataacgttggagtccaagaattagactcatagggcccccaacatttaatatgtactgtgagtttgaaggtgttctattgttaattcctgctcttgatacatgacacgtactccgtgtttaaggcttcggactgactttctttcataagttgagcaacgaaaatttcagaatcgataagttggattcactaactaatacggctgattgaaaactccactccggacctatatggtcgacctttatacgtaaccgatataaaacttataggctggtatatcgagccttcctagcgcaatttcggatggggtttcttctactactcaacaacggaatagtctttgtttagtaaaccagagctcaggacgcccaatacgtaggagagcgctgtggagcatgtgtcattatggactggagcactcttaaatcactctgcgtgtgctaaacgatagatcataacatgtcctgagtaaattttcttgatacgtcgcaatataccgttattagttaaacgttctcatccgtcatgcgtgaaatacggctgtcgtgctcagatatactattagcgactcatctcgcctaacacgcacacgtataaactcggaatgactgccgctcttacatattagaaatacagactacaccacggaagcattgggtcattctcaaccgctgtataaaagatgattagtcttataataagattaccaaagaggcagaatcatgggtagtaaatctattattcaagtgattaccgtcgtgtaggcagggagtgaggacgagatggtactcaggacaaatattaaccggacgaagtggtttacgtcgtactttcactattagtagtaaatacaaggtaacaccggggaatagtactaaatataatgatatctatcttcgggagaacgagtcgtctattgctttgaacattctcaaggcgtaaaatgtgctgacttatagcatgatacaaccgattgttacttttgtctattcaaaagattgaatagttttttatacaaaagccgcatacttatgacggctagtatacagtttcatcccctagcatcaatgctatggacagtattgaacttataggaaattcttctaatagggcaaatccgtcgtgatgcctattttttttcagtcacatcctcaaatggcactagtattgtcgggatcccattaacaggctcaaccacgagctcacgcgaggacatgtagtccgtatctttaacgaagcgacagcgacagaactcccatggataaccaattataaggcccgtaatcctctagacatcgtttaccaataaatccgctttctccgtaatcatgttgaataccccagagtagtccagatgataaccgatgaaacacaagtctttctcaatgcacttacggtgaacttattaccgccaacgtagctcatcaaggttgcgacatctagttgtgtgtttgcgacgagcccagcgaacttcatcaactttcgtatattcaacgccttgtaattttactttaagacgcctggtgatgtagattcttagataatcagtttgttatcggctgtactttaccataatttcacaggtttcaggtcaagaagattatagctgtatatacagttccatgctcggtgcacagaaacgtgatcggataataatcaatcgcttatgtcgtctttaggcgtatccaatacatgccccgataccgcagtgtatttcgacatgtaggtataccgtcgcatttgagctcgagtcaggacgtcagctagattagattccttaatagaatataccgacctctagtccgaactaaactatagataacgccaacttcaggttaattgtctagtcgtctgtttgcagatgggattcttagatgagtgagtatcggccatattggttcgagcactttagtttttgatgcataggatatgcaatgtatagctgaaagtactttatctgtttcaaactcacattgattaaaccggtaaacctttaaagactacaagaaaatattcagtgagggcaattttgtcaatcacaatcttccagctagagatacttcacaatttgtcttgaggctacgcaacattagacggattttcgcgttttattgaaataatcgaggggcccaagagtatccatagttcattttgtaagatttctttacaggcttattacagcttcttcagactcctacatgcttacgagttatatgctagcatgtgaacaatagattaatatacaggaaaacgtacattgagagagatgaccctacacagcgcaaccgttgagtactttcattaaagggtaacgctctcgagacagcatccttaagatggccttattgtcaaatcatttgcagaagtacgcaagatccctaaccaacgtagaagaatccctacaaacacatgagacgcggtgaaaatagacagggtgttagtattcaatcttcggagtatcaatttcgccaatcttggtgagaaagcataccctttcttcagagaaagaagatcaatcataacactatctttaacgaggtacgcacgcgcatcattacctgcctccatggatctttaggatagcggaaagtattggcagcgtattgtgatttcgttcctactttatcaatttcacattcatatacatgtcttttatcaaaatcgccaataagataggatgagctatattagatgctagtagagttcgcgccaacatcatcgataggaatactcaggacagcgtgataggacttttcaatccctaatactctctataattataactctctcttaagtttggaggcagtaacgcgctctatataatcagtttgctgcaccattcttcagcctctgatacatacaaataaattccacagcagtaagagggtttaattgagacatcttgggaacttaggattttactctaacatcaccgaaacgattattggataccgtacctaaacgaactttctcaaggcagtaatataggacatccgcaataacacaaatgctgcctccccaggagttatgtcttcctggaggctatatcttacacccactcactataggcaaactaaagtttaaatgttgattgtctaaaaaaaagatagataagagttggccggcgtagcacatgcgaaagtgaatcgtaagctataattctctggacttgaagttctgtcctgttcctctgcaagaaacaaacttcctttaaagctatttacgacgcacatctcagcaagttataaacatgttggaagtttctagtcggaattcccaaagaacggatctatctaatgcattcctacatttttcctgtctgccgatggtgccatcctattcaaagaatttcttaaaagtagattaaatgggacttttaacaatgagtaaccttacgcctctaagggttcctcgagtgccatacaccagtcaggtccgagccacatacacggagaacattctaacatagcattctcaactcgatcatttgcaggttacttctttcctatcctagtgctaaaaatcatacttgcaatcccatagcacggattaagaacctaagaaacaattcagtaaaacatgttcgaattcttggtatgggaacatcattgcagctatggtctaacgcattaatgtttgggtacatcttccatcatataaacaggaagagtctgacgacagggagtgcttgcgatcatgtctatcattgtgaaatcaaattgtagctcacatgtcgtctatgagagcgtgtatccgataagatttagaaaaatagaagtcgtataagatctcactgaacttttgaatgaatgtgaagcatatatgatctgctttaataaaactttatccataggatacgtttccaaatcaattcaataattattagtcaaaatagataaggatgaacaacctgaaggccgatcggacgtagaaagtggtcccatcactttgagttgatattgttgaaccacacgttattatggttttcaaacagtctcaggatattgtatatacagataatccgataccagttgtctgacgcccctcttacgtaccccaccctttgtgacgtttaaagcagttgttcagtattttaaactaggcggcaactaatttggaaagaagcacagtggatatgtctaaattcttgttattcaggcctgaatttaatacaccgcatagttaacttcgcggtagagttgttcatcatgcctcctctaagctaccacttctatgatacaccaatagttgttctacggaatctgataattggccaagtcataaacttccgctgcgttcaacccccttgctcgaatatccaactcgaaaagacagccttttggtgtccggaacaaatcagttacttcttttctgatgttaattctctgtggtcagatacagaccaaaaactccgcggatttaccatcctccaagaacaaatttgcatcaacatagcattttggctacatattctaagtctcaatagtttaggttttcaactacattatcccaacattaggattggaggaataatagctgggtaagtccccttgcgtctacaatcgactattttttatgaatatgcttctgccgcacctatggttattaaaaaagtcatgactttgaagaaccctgaaaagatagatgaatcaggtgtaatggcagcagccaaagagcatataattagcaacactctaagaacattatagatatgatgatagcgatcgtcatgatgttatccggtcacaatagtagcttcatcagctaattcgttttgccagtggtgacttgcgctggaagaatcgttatacggtcccttccctcttgatacggtgggggcttattcaaccgcgtggattgggttgtcatacttgcattaaacgatgtaaaccatctagtagtcaactatactaaatcacaaaatagtgatcaatacatacccgcttcatggttttaaccatttaattgattaaagatattccgctaagaaccattatctacctaaactgatcgccgtatcctagtagtttgaaatttgatgtaccgtaatgatcaacgaagtaaaacgttatattgtatgtagaataataggtcttggagctaaatgatgtgattggtagtgaagacttacccttacaactttaccggtttctcggaagaatatactagagaatcaatgcatgggctacataagcactttagtctaatgagataaaaaatacacgagtcttccatcatgaattttttgtcgaaaaactcgaacctggtaatttaaaccatatatctttatgtcgtcaataactctcatatgttttatataacttcccaatcacgacttgtaactgcttgttcgactgagctgtttgagctatgaggccgggatccggttgagctacatctatttgctacaagaaaaatgaaagcacatttgttgggagttctggctacactcatagagaaataagtggcccgagtgggtgcggcctgcctccatattcaagtgtatcttaaaccaagtggttccaacgctcgcgctaaagaattaaagcctttatttcctccacggagtagcccgtaatccggttcgaaagagaccattgaagttaattttcatatccagtgaagtttaggcacaagcatgtgttctgccacatgcctcaaagcgctcttcaaccaagatatgattcatcctaacttcgatgaatgcgtctgtaacataaatatagaaggaatgattcggcgagttaattttcgccttctccaacatggcatccctacgttcgttataaggaccatacatgtaggttttaaaggtttgcggttaatcgatatttacatcatagaaattctatagtcaaatttacaagactctagatactcactcgttgcagccggctaggaagcgctttgtaccttacttcccttttcgttgcgtaatatgaatttcatatagtaagttcaaggcactcatacctccgtgaagagggtagatagactattaaagttgtttaatagtacgtattgatggaaatgacccgtaggagatttaccactcaatccacaagattcgctgctgtgcattatcaaaacagtgcatgtcgaaacatgggttgggtccttcaaacacgaatccaggtagagatacctttgcaatttttGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAAGGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCcttBtatcatatgctaKggNcataaaSatgtaaaDcDRtBggDtctttataattcBgtcgtactDtDagcctatttSVHtHttKtgtHMaSattgWaHKHttttagacatWatgtRgaaaNtactMcSMtYtcMgRtacttctWBacgaaatatagScDtttgaagacacatagtVgYgtcattHWtMMWcStgttaggKtSgaYaaccWStcgBttgcgaMttBYatcWtgacaYcagagtaBDtRacttttcWatMttDBcatWtatcttactaBgaYtcttgttttttttYaaScYaHgtgttNtSatcMtcVaaaStccRcctDaataataStcYtRDSaMtDttgttSagtRRcatttHatSttMtWgtcgtatSSagactYaaattcaMtWatttaSgYttaRgKaRtccactttattRggaMcDaWaWagttttgacatgttctacaaaRaatataataaMttcgDacgaSStacaStYRctVaNMtMgtaggcKatcttttattaaaaagVWaHKYagtttttatttaaccttacgtVtcVaattVMBcttaMtttaStgacttagattWWacVtgWYagWVRctDattBYtgtttaagaagattattgacVatMaacattVctgtBSgaVtgWWggaKHaatKWcBScSWaaccRVacacaaactaccScattRatatKVtactatatttHttaagtttSKtRtacaaagtRDttcaaaaWgcacatWaDgtDKacgaacaattacaRNWaatHtttStgttattaaMtgttgDcgtMgcatBtgcttcgcgaDWgagctgcgaggggVtaaScNatttacttaatgacagcccccacatYScaMgtaggtYaNgttctgaMaacNaMRaacaaacaKctacatagYWctgttWaaataaaataRattagHacacaagcgKatacBttRttaagtatttccgatctHSaatactcNttMaagtattMtgRtgaMgcataatHcMtaBSaRattagttgatHtMttaaKaggYtaaBataSaVatactWtataVWgKgttaaaacagtgcgRatatacatVtHRtVYataSaKtWaStVcNKHKttactatccctcatgWHatWaRcttactaggatctataDtDHBttataaaaHgtacVtagaYttYaKcctattcttcttaataNDaaggaaaDYgcggctaaWSctBaaNtgctggMBaKctaMVKagBaactaWaDaMaccYVtNtaHtVWtKgRtcaaNtYaNacggtttNattgVtttctgtBaWgtaattcaagtcaVWtactNggattctttaYtaaagccgctcttagHVggaYtgtNcDaVagctctctKgacgtatagYcctRYHDtgBattDaaDgccKtcHaaStttMcctagtattgcRgWBaVatHaaaataYtgtttagMDMRtaataaggatMtttctWgtNtgtgaaaaMaatatRtttMtDgHHtgtcattttcWattRSHcVagaagtacgggtaKVattKYagactNaatgtttgKMMgYNtcccgSKttctaStatatNVataYHgtNaBKRgNacaactgatttcctttaNcgatttctctataScaHtataRagtcRVttacDSDttaRtSatacHgtSKacYagttMHtWataggatgactNtatSaNctataVtttRNKtgRacctttYtatgttactttttcctttaaacatacaHactMacacggtWataMtBVacRaSaatccgtaBVttccagccBcttaRKtgtgcctttttRtgtcagcRttKtaaacKtaaatctcacaattgcaNtSBaaccgggttattaaBcKatDagttactcttcattVtttHaaggctKKgatacatcBggScagtVcacattttgaHaDSgHatRMaHWggtatatRgccDttcgtatcgaaacaHtaagttaRatgaVacttagattVKtaaYttaaatcaNatccRttRRaMScNaaaDgttVHWgtcHaaHgacVaWtgttScactaagSgttatcttagggDtaccagWattWtRtgttHWHacgattBtgVcaYatcggttgagKcWtKKcaVtgaYgWctgYggVctgtHgaNcVtaBtWaaYatcDRaaRtSctgaHaYRttagatMatgcatttNattaDttaattgttctaaccctcccctagaWBtttHtBccttagaVaatMcBHagaVcWcagBVttcBtaYMccagatgaaaaHctctaacgttagNWRtcggattNatcRaNHttcagtKttttgWatWttcSaNgggaWtactKKMaacatKatacNattgctWtatctaVgagctatgtRaHtYcWcttagccaatYttWttaWSSttaHcaaaaagVacVgtaVaRMgattaVcDactttcHHggHRtgNcctttYatcatKgctcctctatVcaaaaKaaaagtatatctgMtWtaaaacaStttMtcgactttaSatcgDataaactaaacaagtaaVctaggaSccaatMVtaaSKNVattttgHccatcacBVctgcaVatVttRtactgtVcaattHgtaaattaaattttYtatattaaRSgYtgBagaHSBDgtagcacRHtYcBgtcacttacactaYcgctWtattgSHtSatcataaatataHtcgtYaaMNgBaatttaRgaMaatatttBtttaaaHHKaatctgatWatYaacttMctcttttVctagctDaaagtaVaKaKRtaacBgtatccaaccactHHaagaagaaggaNaaatBWattccgStaMSaMatBttgcatgRSacgttVVtaaDMtcSgVatWcaSatcttttVatagttactttacgatcaccNtaDVgSRcgVcgtgaacgaNtaNatatagtHtMgtHcMtagaaattBgtataRaaaacaYKgtRccYtatgaagtaataKgtaaMttgaaRVatgcagaKStctHNaaatctBBtcttaYaBWHgtVtgacagcaRcataWctcaBcYacYgatDgtDHcctaaagacYRcaggattHaYgtKtaatgcVcaataMYacccatatcacgWDBtgaatcBaatacKcttRaRtgatgaBDacggtaattaaYtataStgVHDtDctgactcaaatKtacaatgcgYatBtRaDatHaactgtttatatDttttaaaKVccYcaaccNcBcgHaaVcattHctcgattaaatBtatgcaaaaatYMctSactHatacgaWacattacMBgHttcgaatVaaaacaBatatVtctgaaaaWtctRacgBMaatSgRgtgtcgactatcRtattaScctaStagKgaDcWgtYtDDWKRgRtHatRtggtcgaHgggcgtattaMgtcagccaBggWVcWctVaaattcgNaatcKWagcNaHtgaaaSaaagctcYctttRVtaaaatNtataaccKtaRgtttaMtgtKaBtRtNaggaSattHatatWactcagtgtactaKctatttgRYYatKatgtccgtRtttttatttaatatVgKtttgtatgtNtataRatWYNgtRtHggtaaKaYtKSDcatcKgtaaYatcSRctaVtSMWtVtRWHatttagataDtVggacagVcgKWagBgatBtaaagNcaRtagcataBggactaacacRctKgttaatcctHgDgttKHHagttgttaatgHBtatHcDaagtVaBaRccctVgtgDtacRHSctaagagcggWYaBtSaKtHBtaaactYacgNKBaVYgtaacttagtVttcttaatgtBtatMtMtttaattaatBWccatRtttcatagVgMMtagctStKctaMactacDNYgKYHgaWcgaHgagattacVgtttgtRaSttaWaVgataatgtgtYtaStattattMtNgWtgttKaccaatagNYttattcgtatHcWtctaaaNVYKKttWtggcDtcgaagtNcagatacgcattaagaccWctgcagcttggNSgaNcHggatgtVtcatNtRaaBNcHVagagaaBtaaSggDaatWaatRccaVgggStctDaacataKttKatttggacYtattcSatcttagcaatgaVBMcttDattctYaaRgatgcattttNgVHtKcYRaatRKctgtaaacRatVSagctgtWacBtKVatctgttttKcgtctaaDcaagtatcSataWVgcKKataWaYttcccSaatgaaaacccWgcRctWatNcWtBRttYaattataaNgacacaatagtttVNtataNaYtaatRaVWKtBatKagtaatataDaNaaaaataMtaagaaStccBcaatNgaataWtHaNactgtcDtRcYaaVaaaaaDgtttRatctatgHtgttKtgaaNSgatactttcgagWaaatctKaaDaRttgtggKKagcDgataaattgSaacWaVtaNMacKtcaDaaatttctRaaVcagNacaScRBatatctRatcctaNatWgRtcDcSaWSgttRtKaRtMtKaatgttBHcYaaBtgatSgaSWaScMgatNtctcctatttctYtatMatMtRRtSaattaMtagaaaaStcgVgRttSVaScagtgDtttatcatcatacRcatatDcttatcatVRtttataaHtattcYtcaaaatactttgVctagtaaYttagatagtSYacKaaacgaaKtaaatagataatSatatgaaatSgKtaatVtttatcctgKHaatHattagaaccgtYaaHactRcggSBNgtgctaaBagBttgtRttaaattYtVRaaaattgtaatVatttctcttcatgBcVgtgKgaHaaatattYatagWacNctgaaMcgaattStagWaSgtaaKagttttaagaDgatKcctgtaHtcatggKttVDatcaaggtYcgccagNgtgcVttttagagatgctaccacggggtNttttaSHaNtatNcctcatSaaVgtactgBHtagcaYggYVKNgtaKBcRttgaWatgaatVtagtcgattYgatgtaatttacDacSctgctaaaStttaWMagDaaatcaVYctccgggcgaVtaaWtStaKMgDtttcaaMtVgBaatccagNaaatcYRMBggttWtaaScKttMWtYataRaDBMaDataatHBcacDaaKDactaMgagttDattaHatHtaYatDtattDcRNStgaatattSDttggtattaaNSYacttcDMgYgBatWtaMagactVWttctttgYMaYaacRgHWaattgRtaagcattctMKVStatactacHVtatgatcBtVNataaBttYtSttacKgggWgYDtgaVtYgatDaacattYgatggtRDaVDttNactaSaMtgNttaacaaSaBStcDctaccacagacgcaHatMataWKYtaYattMcaMtgSttDagcHacgatcaHttYaKHggagttccgatYcaatgatRaVRcaagatcagtatggScctatattaNtagcgacgtgKaaWaactSgagtMYtcttccaKtStaacggMtaagNttattatcgtctaRcactctctDtaacWYtgaYaSaagaWtNtatttRacatgNaatgttattgWDDcNaHcctgaaHacSgaataaRaataMHttatMtgaSDSKatatHHaNtacagtccaYatWtcactaactatKDacSaStcggataHgYatagKtaatKagStaNgtatactatggRHacttgtattatgtDVagDVaRctacMYattDgtttYgtctatggtKaRSttRccRtaaccttagagRatagSaaMaacgcaNtatgaaatcaRaagataatagatactcHaaYKBctccaagaRaBaStNagataggcgaatgaMtagaatgtcaKttaaatgtaWcaBttaatRcggtgNcacaaKtttScRtWtgcatagtttWYaagBttDKgcctttatMggNttattBtctagVtacataaaYttacacaaRttcYtWttgHcaYYtaMgBaBatctNgcDtNttacgacDcgataaSatYaSttWtcctatKaatgcagHaVaacgctgcatDtgttaSataaaaYSNttatagtaNYtaDaaaNtggggacttaBggcHgcgtNtaaMcctggtVtaKcgNacNtatVaSWctWtgaWcggNaBagctctgaYataMgaagatBSttctatacttgtgtKtaattttRagtDtacatatatatgatNHVgBMtKtaKaNttDHaagatactHaccHtcatttaaagttVaMcNgHatatKtaNtgYMccttatcaaNagctggacStttcNtggcaVtattactHaSttatgNMVattMMDtMactattattgWMSgtHBttStStgatatRaDaagattttctatMtaaaaaggtactaaVttaSacNaatactgMttgacHaHRttgMacaaaatagttaatatWKRgacDgaRtatatttattatcYttaWtgtBRtWatgHaaattHataagtVaDtWaVaWtgStcgtMSgaSRgMKtaaataVacataatgtaSaatttagtcgaaHtaKaatgcacatcggRaggSKctDcagtcSttcccStYtccRtctctYtcaaKcgagtaMttttcRaYDttgttatctaatcataNctctgctatcaMatactataggDaHaaSttMtaDtcNatataattctMcStaaBYtaNagatgtaatHagagSttgWHVcttatKaYgDctcttggtgttMcRaVgSgggtagacaataaDtaattSaDaNaHaBctattgNtaccaaRgaVtKNtaaYggHtaKKgHcatctWtctDtttctttggSDtNtaStagttataaacaattgcaBaBWggHgcaaaBtYgctaatgaaatWcDcttHtcMtWWattBHatcatcaaatctKMagtDNatttWaBtHaaaNgMttaaStagttctctaatDtcRVaYttgttMtRtgtcaSaaYVgSWDRtaatagctcagDgcWWaaaBaaRaBctgVgggNgDWStNaNBKcBctaaKtttDcttBaaggBttgaccatgaaaNgttttttttatctatgttataccaaDRaaSagtaVtDtcaWatBtacattaWacttaSgtattggDgKaaatScaattacgWcagKHaaccaYcRcaRttaDttRtttHgaHVggcttBaRgtccctDatKaVtKtcRgYtaKttacgtatBtStaagcaattaagaRgBagSaattccSWYtttattVaataNctgHgttaaNBgcVYgtRtcccagWNaaaacaDNaBcaaaaRVtcWMgBagMtttattacgDacttBtactatcattggaaatVccggttRttcatagttVYcatYaSHaHcttaaagcNWaHataaaRWtctVtRYtagHtaaaYMataHYtNBctNtKaatattStgaMcBtRgctaKtgcScSttDgYatcVtggaaKtaagatWccHccgKYctaNNctacaWcttttgcRtgtVcgaKttcMRHgctaHtVaataaDtatgKDcttatBtDttggNtacttttMtgaacRattaaNagaactcaaaBBVtcDtcgaStaDctgaaaSgttMaDtcgttcaccaaaaggWtcKcgSMtcDtatgtttStaaBtatagDcatYatWtaaaBacaKgcaDatgRggaaYctaRtccagattDaWtttggacBaVcHtHtaacDacYgtaatataMagaatgHMatcttatacgtatttttatattacHactgttataMgStYaattYaccaattgagtcaaattaYtgtatcatgMcaDcgggtcttDtKgcatgWRtataatatRacacNRBttcHtBgcRttgtgcgtcatacMtttBctatctBaatcattMttMYgattaaVYatgDaatVagtattDacaacDMatcMtHcccataagatgBggaccattVWtRtSacatgctcaaggggYtttDtaaNgNtaaBatggaatgtctRtaBgBtcNYatatNRtagaacMgagSaSDDSaDcctRagtVWSHtVSRggaacaBVaccgtttaStagaacaMtactccagtttVctaaRaaHttNcttagcaatttattaatRtaaaatctaacDaBttggSagagctacHtaaRWgattcaaBtctRtSHaNtgtacattVcaHaNaagtataccacaWtaRtaaVKgMYaWgttaKggKMtKcgWatcaDatYtKSttgtacgaccNctSaattcDcatcttcaaaDKttacHtggttHggRRaRcaWacaMtBWVHSHgaaMcKattgtaRWttScNattBBatYtaNRgcggaagacHSaattRtttcYgaccBRccMacccKgatgaacttcgDgHcaaaaaRtatatDtatYVtttttHgSHaSaatagctNYtaHYaVYttattNtttgaaaYtaKttWtctaNtgagaaaNctNDctaaHgttagDcRttatagccBaacgcaRBtRctRtggtaMYYttWtgataatcgaataattattataVaaaaattacNRVYcaaMacNatRttcKatMctgaagactaattataaYgcKcaSYaatMNctcaacgtgatttttBacNtgatDccaattattKWWcattttatatatgatBcDtaaaagttgaaVtaHtaHHtBtataRBgtgDtaataMttRtDgDcttattNtggtctatctaaBcatctaRatgNacWtaatgaagtcMNaacNgHttatactaWgcNtaStaRgttaaHacccgaYStacaaaatWggaYaWgaattattcMaactcBKaaaRVNcaNRDcYcgaBctKaacaaaaaSgctccYBBHYaVagaatagaaaacagYtctVccaMtcgtttVatcaatttDRtgWctagtacRttMctgtDctttcKtWttttataaatgVttgBKtgtKWDaWagMtaaagaaattDVtaggttacatcatttatgtcgMHaVcttaBtVRtcgtaYgBRHatttHgaBcKaYWaatcNSctagtaaaaatttacaatcactSWacgtaatgKttWattagttttNaggtctcaagtcactattcttctaagKggaataMgtttcataagataaaaatagattatDgcBVHWgaBKttDgcatRHaagcaYcRaattattatgtMatatattgHDtcaDtcaaaHctStattaatHaccgacNattgatatattttgtgtDtRatagSacaMtcRtcattcccgacacSattgttKaWattNHcaacttccgtttSRtgtctgDcgctcaaMagVtBctBMcMcWtgtaacgactctcttRggRKSttgYtYatDccagttDgaKccacgVatWcataVaaagaataMgtgataaKYaaatcHDaacgataYctRtcYatcgcaMgtNttaBttttgatttaRtStgcaacaaaataccVgaaDgtVgDcStctatatttattaaaaRKDatagaaagaKaaYYcaYSgKStctccSttacagtcNactttDVttagaaagMHttRaNcSaRaMgBttattggtttaRMggatggcKDgWRtNaataataWKKacttcKWaaagNaBttaBatMHtccattaacttccccYtcBcYRtagattaagctaaYBDttaNtgaaaccHcaRMtKtaaHMcNBttaNaNcVcgVttWNtDaBatgataaVtcWKcttRggWatcattgaRagHgaattNtatttctctattaattaatgaDaaMatacgttgggcHaYVaaNaDDttHtcaaHtcVVDgBVagcMacgtgttaaBRNtatRtcagtaagaggtttaagacaVaaggttaWatctccgtVtaDtcDatttccVatgtacNtttccgtHttatKgScBatgtVgHtYcWagcaKtaMYaaHgtaattaSaHcgcagtWNaatNccNNYcacgVaagaRacttctcattcccRtgtgtaattagcSttaaStWaMtctNNcSMacattataaactaDgtatWgtagtttaagaaaattgtagtNagtcaataaatttgatMMYactaatatcggBWDtVcYttcDHtVttatacYaRgaMaacaStaatcRttttVtagaDtcacWatttWtgaaaagaaagNRacDtttStVatBaDNtaactatatcBSMcccaSttccggaMatgattaaWatKMaBaBatttgataNctgttKtVaagtcagScgaaaDggaWgtgttttKtWtatttHaatgtagttcactaaKMagttSYBtKtaYgaactcagagRtatagtVtatcaaaWYagcgNtaDagtacNSaaYDgatBgtcgataacYDtaaactacagWDcYKaagtttattagcatcgagttKcatDaattgattatDtcagRtWSKtcgNtMaaaaacaMttKcaWcaaSVMaaaccagMVtaMaDtMaHaBgaacataBBVtaatVYaNSWcSgNtDNaaKacacBtttatKtgtttcaaHaMctcagtaacgtcgYtactDcgcctaNgagagcYgatattttaaatttccattttacatttDaaRctattttWctttacgtDatYtttcagacgcaaVttagtaaKaaaRtgVtccataBggacttatttgtttaWNtgttVWtaWNVDaattgtatttBaagcBtaaBttaaVatcHcaVgacattccNggtcgacKttaaaRtagRtctWagaYggtgMtataatMtgaaRttattttgWcttNtDRRgMDKacagaaaaggaaaRStcccagtYccVattaNaaKStNWtgacaVtagaagcttSaaDtcacaacgDYacWDYtgtttKatcVtgcMaDaSKStVcgtagaaWaKaagtttcHaHgMgMtctataagBtKaaaKKcactggagRRttaagaBaaNatVVcgRcKSttDaactagtSttSattgttgaaRYatggttVttaataaHttccaagDtgatNWtaagHtgcYtaactRgcaatgMgtgtRaatRaNaacHKtagactactggaatttcgccataacgMctRgatgttaccctaHgtgWaYcactcacYaattcttaBtgacttaaacctgYgaWatgBttcttVttcgttWttMcNYgtaaaatctYgMgaaattacNgaHgaacDVVMtttggtHtctaaRgtacagacgHtVtaBMNBgattagcttaRcttacaHcRctgttcaaDBggttKaacatgKtttYataVaNattccgMcgcgtagtRaVVaattaKaatggttRgaMcagtatcWBttNtHagctaatctagaaNaaacaYBctatcgcVctBtgcaaagDgttVtgaHtactSNYtaaNccatgtgDacgaVtDcgKaRtacDcttgctaagggcagMDagggtBWRtttSgccttttttaacgtcHctaVtVDtagatcaNMaVtcVacatHctDWNaataRgcgtaVHaggtaaaaSgtttMtattDgBtctgatSgtRagagYtctSaKWaataMgattRKtaacatttYcgtaacacattRWtBtcggtaaatMtaaacBatttctKagtcDtttgcBtKYYBaKttctVttgttaDtgattttcttccacttgSaaacggaaaNDaattcYNNaWcgaaYattttMgcBtcatRtgtaaagatgaWtgaccaYBHgaatagataVVtHtttVgYBtMctaMtcctgaDcYttgtccaaaRNtacagcMctKaaaggatttacatgtttaaWSaYaKttBtagDacactagctMtttNaKtctttcNcSattNacttggaacaatDagtattRtgSHaataatgccVgacccgatactatccctgtRctttgagaSgatcatatcgDcagWaaHSgctYYWtatHttggttctttatVattatcgactaagtgtagcatVgtgHMtttgtttcgttaKattcMatttgtttWcaaStNatgtHcaaaDtaagBaKBtRgaBgDtSagtatMtaacYaatYtVcKatgtgcaacVaaaatactKcRgtaYtgtNgBBNcKtcttaccttKgaRaYcaNKtactttgagSBtgtRagaNgcaaaNcacagtVtttHWatgttaNatBgtttaatNgVtctgaatatcaRtattcttttttttRaaKcRStctcggDgKagattaMaaaKtcaHacttaataataKtaRgDtKVBttttcgtKaggHHcatgttagHggttNctcgtatKKagVagRaaaggaaBtNatttVKcRttaHctaHtcaaatgtaggHccaBataNaNaggttgcWaatctgatYcaaaHaatWtaVgaaBttagtaagaKKtaaaKtRHatMaDBtBctagcatWtatttgWttVaaaScMNattRactttgtYtttaaaagtaagtMtaMaSttMBtatgaBtttaKtgaatgagYgtNNacMtcNRacMMHcttWtgtRtctttaacaacattattcYaMagBaacYttMatcttKcRMtgMNccattaRttNatHaHNaSaaHMacacaVaatacaKaSttHatattMtVatWgattttttaYctttKttHgScWaacgHtttcaVaaMgaacagNatcgttaacaaaaagtacaHBNaattgttKtcttVttaaBtctgctacgBgcWtttcaggacacatMgacatcccagcggMgaVKaBattgacttaatgacacacaaaaaatRKaaBctacgtRaDcgtagcVBaacDSBHaaaaSacatatacagacRNatcttNaaVtaaaataHattagtaaaaSWccgtatWatggDttaactattgcccatcttHaSgYataBttBaactattBtcHtgatcaataSttaBtatKSHYttWggtcYtttBttaataccRgVatStaHaKagaatNtagRMNgtcttYaaSaactcagDSgagaaYtMttDtMRVgWKWtgMaKtKaDttttgactatacataatcNtatNaHattVagacgYgatatatttttgtStWaaatctWaMgagaRttRatacgStgattcttaagaDtaWccaaatRcagcagaaNKagtaaDggcgccBtYtagSBMtactaaataMataBSacRMgDgattMMgtcHtcaYDtRaDaacggttDaggcMtttatgttaNctaattaVacgaaMMtaatDccSgtattgaRtWWaccaccgagtactMcgVNgctDctaMScatagcgtcaactatacRacgHRttgctatttaatgaattataYKttgtaagWgtYttgcHgMtaMattWaWVtaRgcttgYgttBHtYataSccStBtgtagMgtDtggcVaaSBaatagDttgBgtctttctcattttaNagtHKtaMWcYactVcgcgtatMVtttRacVagDaatcttgctBBcRDgcaacKttgatSKtYtagBMagaRtcgBattHcBWcaactgatttaatttWDccatttatcgagSKaWttataHactaHMttaatHtggaHtHagaatgtKtaaRactgtttMatacgatcaagDgatKaDctataMggtHDtggHacctttRtatcttYattttgacttgaaSaataaatYcgBaaaaccgNatVBttMacHaKaataagtatKgtcaagactcttaHttcggaattgttDtctaaccHttttWaaatgaaatataaaWattccYDtKtaaaacggtgaggWVtctattagtgactattaagtMgtttaagcatttgSgaaatatccHaaggMaaaattttcWtatKctagDtYtMcctagagHcactttactatacaaacattaacttaHatcVMYattYgVgtMttaaRtgaaataaDatcaHgtHHatKcDYaatcttMtNcgatYatgSaMaNtcttKcWataScKggtatcttacgcttWaaagNatgMgHtctttNtaacVtgttcMaaRatccggggactcMtttaYMtcWRgNctgNccKatcttgYDcMgattNYaRagatHaaHgKctcataRDttacatBatccattgDWttatttaWgtcggagaaaaatacaatacSNtgggtttccttacSMaagBattacaMaNcactMttatgaRBacYcYtcaaaWtagctSaacttWgDMHgaggatgBVgcHaDtggaactttggtcNatNgtaKaBcccaNtaagttBaacagtatacDYttcctNgWgcgSMcacatStctHatgRcNcgtacacaatRttMggaNKKggataaaSaYcMVcMgtaMaHtgattYMatYcggtcttcctHtcDccgtgRatcattgcgccgatatMaaYaataaYSggatagcgcBtNtaaaScaKgttBgagVagttaKagagtatVaactaSacWactSaKatWccaKaaaatBKgaaKtDMattttgtaaatcRctMatcaaMagMttDgVatggMaaWgttcgaWatgaaatttgRtYtattaWHKcRgctacatKttctaccaaHttRatctaYattaaWatVNccatNgagtcKttKataStRaatatattcctRWatDctVagttYDgSBaatYgttttgtVaatttaatagcagMatRaacttBctattgtMagagattaaactaMatVtHtaaatctRgaaaaaaaatttWacaacaYccYDSaattMatgaccKtaBKWBattgtcaagcHKaagttMMtaatttcKcMagNaaKagattggMagaggtaatttYacatcWaaDgatMgKHacMacgcVaacaDtaDatatYggttBcgtatgWgaSatttgtagaHYRVacaRtctHaaRtatgaactaatatctSSBgggaaHMWtcaagatKgagtDaSatagttgattVRatNtctMtcSaagaSHaataNataataRaaRgattctttaataaagWaRHcYgcatgtWRcttgaaggaMcaataBRaaccagStaaacNtttcaatataYtaatatgHaDgcStcWttaacctaRgtYaRtataKtgMttttatgactaaaatttacYatcccRWtttHRtattaaatgtttatatttgttYaatMcaRcSVaaDatcgtaYMcatgtagacatgaaattgRtcaaYaaYtRBatKacttataccaNaaattVaBtctggacaagKaaYaaatatWtMtatcYaaVNtcgHaactBaagKcHgtctacaatWtaDtSgtaHcataHtactgataNctRgttMtDcDttatHtcgtacatcccaggSttaBgtcacacWtccNMcNatMVaVgtccDYStatMaccDatggYaRKaaagataRatttHKtSaaatDgataaacttaHgttgVBtcttVttHgDacgaKatgtatatNYataactctSatatatattgcHRRYttStggaactHgttttYtttaWtatMcttttctatctDtagVHYgMRBgtHttcctaatYRttKtaagatggaVRataKDctaMtKBNtMtHNtWtttYcVtattMcgRaacMcctNSctcatttaaagDcaHtYccSgatgcaatYaaaaDcttcgtaWtaattctcgttttScttggtaatctttYgtctaactKataHacctMctcttacHtKataacacagcNRatgKatttttSaaatRYcgDttaMRcgaaattactMtgcgtaagcgttatBtttttaattaagtNacatHgttcRgacKcBBtVgatKttcgaBaatactDRgtRtgaNacWtcacYttaaKcgttctHaKttaNaMgWgWaggtctRgaKgWttSttBtDcNtgtttacaaatYcDRtgVtgcctattcNtctaaaDMNttttNtggctgagaVctDaacVtWccaagtaacacaNctgaScattccDHcVBatcgatgtMtaatBgHaatDctMYgagaatgYWKcctaatNaStHaaaKccgHgcgtYaaYtattgtStgtgcaaRtattaKatattagaWVtcaMtBagttattagNaWHcVgcaattttDcMtgtaRHVYtHtctgtaaaaHVtMKacatcgNaatttMatatgttgttactagWYtaRacgataKagYNKcattataNaRtgaacKaYgcaaYYacaNccHatMatDcNgtHttRaWttagaaDcaaaaaatagggtKDtStaDaRtaVtHWKNtgtattVctSVgRgataDaRaWataBgaagaaKtaataaYgDcaStaNgtaDaaggtattHaRaWMYaYaWtggttHYgagVtgtgcttttcaaDKcagVcgttagacNaaWtagtaataDttctggttVcatcataaagtgKaaaNaMtaBBaattaatWaattgctHaVKaSgDaaVKaHtatatatHatcatSBagNgHtatcHYMHgttDgtaHtBttWatcgtttaRaattgStKgSKNWKatcagDtctcagatttctRtYtBatBgHHtKaWtgYBgacVVWaKtacKcDttKMaKaVcggtgttataagaataaHaatattagtataatMHgttYgaRttagtaRtcaaVatacggtcMcgagtaaRttacWgactKRYataaaagSattYaWgagatYagKagatgSaagKgttaatMggtataatgttWYttatgagaaacctNVataatHcccKtDctcctaatactggctHggaSaggRtKHaWaattcgSatMatttagaggcYtctaMcgctcataSatatgRagacNaaDaggaVBagaYttKtacNaKgtSYtagttggaWcatcWttaatctatgaVtcgtgtMtatcaYcgtRccaaYgDctgcMgtgtWgacWtgataacacgcgctBtgttaKtYDtatDcatcagKaVMctaatcttgVcaaRgcRMtDcgattaHttcaNatgaatMtactacVgtRgatggaWtttactaaKatgagSaaKggtaNtactVaYtaaKRagaacccacaMtaaMtKtatBcttgtaaWBtMctaataaVcDaaYtcRHBtcgttNtaaHatttBNgRStVDattBatVtaagttaYatVattaagaBcacggtSgtVtatttaRattgatgtaHDKgcaatattKtggcctatgaWDKRYcggattgRctatNgatacaatMNttctgtcRBYRaaaHctNYattcHtaWcaattctBtMKtVgYataatMgYtcagcttMDataVtggRtKtgaatgccNcRttcaMtRgattaacattRcagcctHtWMtgtDRagaKaBtgDttYaaaaKatKgatctVaaYaacWcgcatagBVtaNtRtYRaggBaaBtgKgttacataagagcatgtRattccacttaccatRaaatgWgDaMHaYVgVtaSctatcgKaatatattaDgacccYagtgtaYNaaatKcagtBRgagtccatgKgaaaccBgaagBtgSttWtacgatWHaYatcgatttRaaNRgcaNaKVacaNtDgattgHVaatcDaagcgtatgcNttaDataatcSataaKcaataaHWataBtttatBtcaKtKtatagttaDgSaYctacaRatNtaWctSaatatttYaKaKtaccWtatcRagacttaYttVcKgSDcgagaagatccHtaattctSttatggtKYgtMaHagVaBRatttctgtRgtctatgggtaHKgtHacHtSYacgtacacHatacKaaBaVaccaDtatcSaataaHaagagaatScagactataaRttagcaaVcaHataKgDacatWccccaagcaBgagWatctaYttgaaatctVNcYtttWagHcgcgcDcVaaatgttKcHtNtcaatagtgtNRaactttttcaatggWgBcgDtgVgtttctacMtaaataaaRggaaacWaHttaRtNtgctaaRRtVBctYtVtatDcattDtgaccYatagatYRKatNYKttNgcctagtaWtgaactaMVaacctgaStttctgaKVtaaVaRKDttVtVctaDNtataaaDtccccaagtWtcgatcactDgYaBcatcctMtVtacDaaBtYtMaKNatNtcaNacgDatYcatcgcaRatWBgaacWttKttagYtaattcggttgSWttttDWctttacYtatatWtcatDtMgtBttgRtVDggttaacYtacgtacatgaattgaaWcttMStaDgtatattgaDtcRBcattSgaaVBRgagccaaKtttcDgcgaSMtatgWattaKttWtgDBMaggBBttBaatWttRtgcNtHcgttttHtKtcWtagHStaacagttgatatBtaWSaWggtaataaMttaKacDaatactcBttcaatatHttcBaaSaaatYggtaRtatNtHcaatcaHtagVtgtattataNggaMtcttHtNagctaaaggtagaYctMattNaMVNtcKtactBKcaHHcBttaSagaKacataYgctaKaYgttYcgacWVttWtSagcaacatcccHaccKtcttaacgaKttcacKtNtacHtatatRtaaatacactaBtttgaHaRttggttWtatYagcatYDatcggagagcWBataagRtacctataRKgtBgatgaDatataSttagBaHtaatNtaDWcWtgtaattacagKttcNtMagtattaNgtctcgtcctcttBaHaKcKccgtRcaaYagSattaagtKataDatatatagtcDtaacaWHcaKttDgaaRcgtgYttgtcatatNtatttttatggccHtgDtYHtWgttatYaacaattcaWtatNgctcaaaSttRgctaatcaaatNatcgtttaBtNNVtgttataagcaaagattBacgtDatttNatttaaaDcBgtaSKgacgtagataatttcHMVNttgttBtDtgtaWKaaRMcKMtHtaVtagataWctccNNaSWtVaHatctcMgggDgtNHtDaDttatatVWttgttatttaacctttcacaaggaSaDcggttttttatatVtctgVtaacaStDVaKactaMtttaSNagtgaaattaNacttSKctattcctctaSagKcaVttaagNaVcttaVaaRNaHaaHttatgtHttgtgatMccaggtaDcgaccgtWgtWMtttaHcRtattgScctatttKtaaccaagtYagaHgtWcHaatgccKNRtttagtMYSgaDatctgtgaWDtccMNcgHgcaaacNDaaaRaStDWtcaaaaHKtaNBctagBtgtattaactaattttVctagaatggcWSatMacccttHttaSgSgtgMRcatRVKtatctgaaaccDNatYgaaVHNgatMgHRtacttaaaRtatStRtDtatDttYatattHggaBcttHgcgattgaKcKtttcRataMtcgaVttWacatNcatacctRataDDatVaWNcggttgaHtgtMacVtttaBHtgagVttMaataattatgttcttagtttgtgcDtSatttgBtcaacHattaaBagVWcgcaSYttMgcttacYKtVtatcaYaKctgBatgcgggcYcaaaaacgNtctagKBtattatctttKtaVttatagtaYtRagNtaYataaVtgaatatcHgcaaRataHtacacatgtaNtgtcgYatWMatttgaactacRctaWtWtatacaatctBatatgYtaagtatgtgtatSttactVatcttYtaBcKgRaSggRaaaaatgcagtaaaWgtaRgcgataatcBaataccgtatttttccatcNHtatWYgatHSaaaDHttgctgtccHtggggcctaataatttttctatattYWtcattBtgBRcVttaVMRSgctaatMagtYtttaaaaatBRtcBttcaaVtaacagctccSaaSttKNtHtKYcagcagaaaccccRtttttaaDcDtaStatccaagcgctHtatcttaDRYgatDHtWcaaaBcWgKWHttHataagHacgMNKttMKHccaYcatMVaacgttaKgYcaVaaBtacgcaactttMctaaHaatgtBatgagaSatgtatgSRgHgWaVWgataaatatttccKagVgataattWaHNcYggaaatgctHtKtaDtctaaagtMaatVDVactWtSaaWaaMtaHtaSKtcBRaNcttStggtBttacNagcatagRgtKtgcgaacaacBcgKaatgataagatgaaaattgtactgcgggtccHHWHaaNacaBttNKtKtcaaBatatgctaHNgtKcDWgtttatNgVDHgaccaacWctKaaggHttgaRgYaatHcaBacaatgagcaaattactgtaVaaYaDtagattgagNKggtggtgKtWKaatacagDRtatRaMRtgattDggtcaaYRtatttNtagaDtcacaaSDctDtataatcgtactaHttatacaatYaacaaHttHatHtgcgatRRttNgcatSVtacWWgaaggagtatVMaVaaattScDDKNcaYBYaDatHgtctatBagcaacaagaatgagaaRcataaKNaRtBDatcaaacgcattttttaaBtcSgtacaRggatgtMNaattggatatWtgagtattaaaVctgcaYMtatgatttttYgaHtgtcttaagWBttHttgtcttattDtcgtatWtataataSgctaHagcDVcNtaatcaagtaBDaWaDgtttagYctaNccDtaKtaHcttaataacccaRKtacaVaatNgcWRaMgaattatgaBaaagattVYaHMDcaDHtcRcgYtcttaaaWaaaVKgatacRtttRRKYgaatacaWVacVcRtatMacaBtactggMataaattttHggNagSctacHgtBagcgtcgtgattNtttgatSaaggMttctttcttNtYNagBtaaacaaatttMgaccttacataattgYtcgacBtVMctgStgMDtagtaRctHtatgttcatatVRNWataDKatWcgaaaaagttaaaagcacgHNacgtaatctttMRtgacttttDacctataaacgaaatatgattagaactccSYtaBctttaataacWgaaaYatagatgWttcatKtNgatttttcaagHtaYgaaRaDaagtaggagcttatVtagtctttcattaaaatcgKtattaRttacagVaDatgcatVgattgggtctttHVtagKaaRBtaHtaaggccccaaaaKatggtttaMWgtBtaaacttcactttKHtcgatctccctaYaBacMgtcttBaBaNgcgaaacaatctagtHccHtKttcRtRVttccVctttcatacYagMVtMcagaMaaacaataBctgYtaatRaaagattaaccatVRatHtaRagcgcaBcgDttStttttcVtttaDtKgcaaWaaaaatSccMcVatgtKgtaKgcgatatgtagtSaaaDttatacaaacatYaRRcVRHctKtcgacKttaaVctaDaatgttMggRcWaacttttHaDaKaDaBctgtaggcgtttaHBccatccattcNHtDaYtaataMttacggctNVaacDattgatattttacVttSaattacaaRtataNDgacVtgaacataVRttttaDtcaaacataYDBtttaatBaDtttYDaDaMccMttNBttatatgagaaMgaNtattHccNataattcaHagtgaaggDgatgtatatatgYatgaStcataaBStWacgtcccataRMaaDattggttaaattcMKtctMacaBSactcggaatDDgatDgcWctaacaccgggaVcacWKVacggtaNatatacctMtatgatagtgcaKagggVaDtgtaacttggagtcKatatcgMcttRaMagcattaBRaStctYSggaHYtacaactMBaagDcaBDRaaacMYacaHaattagcattaaaHgcgctaaggSccKtgaaKtNaBtatDDcKBSaVtgatVYaagVtctSgMctacgttaacWaaattctSgtDactaaStaaattgcagBBRVctaatatacctNttMcRggctttMttagacRaHcaBaacVKgaataHttttMgYgattcYaNRgttMgcVaaacaVVcDHaatttgKtMYgtatBtVVctWgVtatHtacaaHttcacgatagcagtaaNattBatatatttcVgaDagcggttMaagtcScHagaaatgcYNggcgtttttMtStggtRatctacttaaatVVtBacttHNttttaRcaaatcacagHgagagtMgatcSWaNRacagDtatactaaDKaSRtgattctccatSaaRttaaYctacacNtaRtaactggatgaccYtacactttaattaattgattYgttcagDtNKttagDttaaaaaaaBtttaaNaYWKMBaaaacVcBMtatWtgBatatgaacVtattMtYatMNYDKNcKgDttDaVtaaaatgggatttctgtaaatWtctcWgtVVagtcgRgacttcccctaDcacagcRcagagtgtWSatgtacatgttaaSttgtaaHcgatgggMagtgaacttatRtttaVcaccaWaMgtactaatSSaHtcMgaaYtatcgaaggYgggcgtgaNDtgttMNgaNDMtaattcgVttttaacatgVatgtWVMatatcaKgaaattcaBcctccWcttgaaWHtWgHtcgNWgaRgctcBgSgaattgcaaHtgattgtgNagtDttHHgBttaaWcaaWagcaSaHHtaaaVctRaaMagtaDaatHtDMtcVaWMtagSagcttHSattaacaaagtRacMtRtctgttagcMtcaBatVKtKtKacgagaSNatSactgtatatcBctgagVtYactgtaaattaaaggcYgDHgtaacatSRDatMMccHatKgttaacgactKtgKagtcttcaaHRVtccttKgtSataatttacaactggatDNgaacttcaRtVaagDcaWatcBctctHYatHaDaaatttagYatSatccaWtttagaaatVaacBatHcatcgtacaatatcgcNYRcaataYaRaYtgattVttgaatgaVaactcRcaNStgtgtattMtgaggtNttBaDRcgaaaagctNgBcWaWgtSaDcVtgVaatMKBtttcgtttctaaHctaaagYactgMtatBDtcStgaccgtSDattYaataHctgggaYYttcggttaWaatctggtRagWMaDagtaacBccactacgHWMKaatgatWatcctgHcaBaSctVtcMtgtDttacctaVgatYcWaDRaaaaRtagatcgaMagtggaRaWctctgMgcWttaagKBRtaaDaaWtctgtaagYMttactaHtaatcttcataacggcacBtSgcgttNHtgtHccatgttttaaagtatcgaKtMttVcataYBBaKtaMVaVgtattNDSataHcagtWMtaggtaSaaKgttgBtVtttgttatcatKcgHacacRtctHatNVagSBgatgHtgaRaSgttRcctaacaaattDNttgacctaaYtBgaaaatagttattactcttttgatgtNNtVtgtatMgtcttRttcatttgatgacacttcHSaaaccaWWDtWagtaRDDVNacVaRatgttBccttaatHtgtaaacStcVNtcacaSRttcYagacagaMMttttgMcNttBcgWBtactgVtaRttctccaaYHBtaaagaBattaYacgatttacatctgtaaMKaRYtttttactaaVatWgctBtttDVttctggcDaHaggDaagtcgaWcaagtagtWttHtgKtVataStccaMcWcaagataagatcactctHatgtcYgaKcatcagatactaagNSStHcctRRNtattgtccttagttagMVgtatagactaactctVcaatMctgtttgtgttgccttatWgtaBVtttctggMcaaKgDWtcgtaaYStgSactatttHgatctgKagtagBtVacRaagRtMctatgggcaaaKaaaatacttcHctaRtgtDcttDattaggaaatttcYHaRaaBttaatggcacKtgctHVcaDcaaaVDaaaVcgMttgtNagcgtaDWgtcgttaatDgKgagcSatatcSHtagtagttggtgtHaWtaHKtatagctgtVgattaBVaatgaataagtaatVatSttaHctttKtttgtagttaccttaatcgtagtcctgBcgactatttVcMacHaaaggaatgDatggKtaHtgStatattaaSagctWcctccRtataBaDYcgttgcNaagaggatRaaaYtaWgNtSMcaatttactaacatttaaWttHtatBattgtcgacaatNgattgcNgtMaaaKaBDattHacttggtRtttaYaacgVactBtaBaKtgBttatgVttgtVttcaatcWcNctDBaaBgaDHacBttattNtgtDtatttVSaaacaggatgcRatSgtaSaNtgBatagttcHBgcBBaaattaHgtDattatDaKaatBaaYaaMaataaataKtttYtagtBgMatNcatgtttgaNagtgttgtgKaNaSagtttgaSMaYBcaaaacDStagttVacaaaaactaaWttBaagtctgtgcgtMgtaattctcctacctcaNtttaaccaaaaVtBcacataacaccccBcWMtatVtggaatgaWtcaaWaaaaaaaaWtDtaatatRcctDWtcctaccMtVVatKttaWaaKaaatataaagScHBagaggBaSMtaWaVtatattactSaaaKNaactatNatccttgaYctattcaaaVgatttYHcRagattttaSataggttattcVtaaagaKgtattattKtRttNcggcRgtgtgtWYtaacHgKatKgatYtacYagDtWcHBDctctgRaYKaYagcactKcacSaRtBttttBHKcMtNtcBatttatttttgSatVgaaagaWtcDtagDatatgMacaacRgatatatgtttgtKtNRaatatNatgYcaHtgHataacKtgagtagtaacYttaNccaaatHcacaacaVDtagtaYtccagcattNtacKtBtactaaagaBatVtKaaHBctgStgtBgtatgaSNtgDataaccctgtagcaBgtgatcttaDataStgaMaccaSBBgWagtacKcgattgaDgNNaaaacacagtSatBacKDgcgtataBKcatacactaSaatYtYcDaactHttcatRtttaatcaattataRtttgtaagMcgNttcatcBtYBagtNWNMtSHcattcRctttttRWgaKacKttgggagBcgttcgcMaWHtaatactgtctctatttataVgtttaBScttttaBMaNaatMacactYtBMggtHacMagtaRtctgcatttaHtcaaaatttgagKtgNtactBacaHtcgtatttctMaSRagcagttaatgtNtaaattgagagWcKtaNttagVtacgatttgaatttcgRtgtWcVatcgttaaDVctgtttBWgaccagaaagtcSgtVtatagaBccttttcctaaattgHtatcggRattttcaaggcYSKaagWaWtRactaaaacccBatMtttBaatYtaagaactSttcgaaScaatagtattgaccaagtgttttctaacatgtttNVaatcaaagagaaaNattaaRttttaVaaaccgcaggNMtatattVctcaagaggaacgBgtttaacaagttcKcYaatatactaaccBaaaSggttcNtattctagttRtBacgScVctcaatttaatYtaaaaaaatgSaatgatagaMBRatgRcMcgttgaWHtcaVYgaatYtaatctttYttatRaWtctgBtDcgatNatcKaBaDgatgtaNatWKctccgatattaacattNaaacDatgBgttctgtDtaaaMggtgaBaSHataacgccSctaBtttaRBtcNHcDatcDcctagagtcRtaBgWttDRVHagattYatgtatcWtaHtttYcattWtaaagtctNgtStggRNcgcggagSSaaagaaaatYcHDtcgctttaatgYcKBVSgtattRaYBaDaaatBgtatgaHtaaRaRgcaSWNtagatHaacttNctBtcaccatctMcatattccaSatttgcgaDagDgtatYtaaaVDtaagtttWVaagtagYatRttaagDcNgacKBcScagHtattatcDaDactaaaaaYgHttBcgaDttggataaaKSRcBMaBcgaBSttcWtgNBatRaccgattcatttataacggHVtaattcacaagagVttaaRaatVVRKcgWtVgacctgDgYaaHaWtctttcacMagggatVgactagMaaataKaaNWagKatagNaaWtaaaatttgaattttatttgctaaVgaHatBatcaaBWcBgttcMatcgBaaNgttcgSNaggSaRtttgHtRtattaNttcDcatSaVttttcgaaaaattgHatctaRaggSaNatMDaaatDcacgattttagaHgHaWtYgattaatHNSttatMSgggNtcKtYatRggtttgtMWVtttaYtagcagBagHaYagttatatggtBacYcattaRSataBatMtttaaatctHcaaaSaaaagttNSaaWcWRccRtKaagtBWtcaaattSttMtattggaaaccttaacgttBtWatttatatWcDaatagattcctScacctaagggRaaYtaNaatgVtBcttaaBaacaMVaaattatStYgRcctgtactatcMcVKatttcgSgatRHMaaaHtagtaaHtVgcaaataatatcgKKtgccaatBNgaaWcVttgagttaKatagttcaggKDatDtattgaKaVcaKtaataDataataHSaHcattagttaatRVYcNaHtaRcaaggtNHcgtcaaccaBaaagYtHWaaaRcKgaYaaDttgcWYtataRgaatatgtYtgcKtaNttWacatYHctRaDtYtattcBttttatcSataYaYgttWaRagcacHMgtttHtYttYaatcggtatStttcgtRSattaaDaKMaatatactaNBaWgctacacYtgaYVgtgHtaaaRaaRgHtagtWattataaaSDaaWtgMattatcgaaaagtaYRSaWtSgNtBgagcRYaMDtactaacttaWgtatctagacaagNtattHggataatYttYatcataDcgHgttBttctttVttgccgaaWtaaaacgKgtatctaaaaaNtccDtaDatBMaMggaatNKtatBaaatVtccRaHtaSacataHattgtttKVYattcataVaattWtcgtgMttcttKtgtctaacVtatctatatBRataactcgKatStatattcatHHRttKtccaacgtgggtgRgtgaMtattattggctatcgtgacMtRcBDtcttgtactaatRHttttaagatcgVMDStattatYBtttDttgtBtNttgRcMtYtgBacHaWaBaatDKctaagtgaaactaatgRaaKgatccaagNaaaatattaggWNtaagtatacttttKcgtcggSYtcttgRctataYcttatataaagtatattaatttataVaacacaDHatctatttttKYVatHRactttaBHccaWagtactBtcacgaVgcgttRtttttttSVgtSagtBaaattctgaHgactcttgMcattttagVtaagaattHctHtcaDaaNtaacRggWatagttcgtSttgaDatcNgNagctagDgatcNttKgttgtaDtctttRaaYStRatDtgMggactSttaDtagSaVtBDttgtDgccatcacaMattaaaMtNacaVcgSWcVaaDatcaHaatgaattaMtatccVtctBtaattgtWattatBRcWcaatgNNtactWYtDaKttaaatcactcagtRaaRgatggtKgcgccaaHgaggatStattYcaNMtcaBttacttatgagDaNtaMgaaWtgtttcttctaHtMNgttatctaWWatMtBtaaatagDVatgtBYtatcggcttaagacMRtaHScgatatYgRDtcattatSDaHggaaataNgaWSRRaaaBaatagBattaDctttgHWNttacaataaaaaaatacggtttgHgVtaHtWMttNtBtctagtMcgKMgHgYtataHaNagWtcaacYattaataYRgtaWKgaBctataaccgatttaHaNBRaRaMtccggtNgacMtctcatttgcaattcWgMacttacaaDaaNtactWatVtttagccttMaatcagVaagtctVaaDaBtattaattaYtNaYtggattaKtaKctYaMtattYgatattataatKtVgDcttatatNBtcgttgtStttttMagaggttaHYSttcKgtcKtDNtataagttataagSgttatDtRttattgttttSNggRtcaaKMNatgaatattgtBWtaMacctgggYgaSgaagYataagattacgagaatBtggtRcVHtgYggaDgaYaKagWagctatagacgaaHgtWaNgacttHRatVaWacKYtgRVNgVcSgRWctacatcKSactctgWYtBggtataagcttNRttVtgRcaWaaatDMatYattaactttcgaagRatSctgccttgcRKaccHtttSNVagtagHagBagttagaccaRtataBccataatSHatRtcHagacBWatagcaMtacaRtgtgaaBatctKRtScttccaNaatcNgtaatatWtcaMgactctBtWtaaNactHaaaaRctcgcatggctMcaaNtcagaaaaacacagtggggWttRttagtaagaVctVMtcgaatcttcMaaaHcaHBttcgattatgtcaDagcYRtBtYcgacMgtDcagcgaNgttaataatagcagKYYtcgtaBtYctMaRtaRtDagaaaacacatgYaBttgattattcgaaNttBctSataaMataWRgaHtttccgtDgaYtatggtDgHKgMtatttVtMtVagttaRatMattRagataaccctKctMtSttgaHagtcStctatttccSagatgttccacgaggYNttHRacgattcDatatDcataaaatBBttatcgaHtNHaaatatDNaggctgaNcaaggagttBttMgRagVatBcRtaWgatgBtSgaKtcgHtttgaatcaaDaHttcSBgHcagtVaaSttDcagccgttNBtgttHagYtattctttRWaaVtSttcatatKaaRaaaNacaVtVctMtSDtDtRHRcgtaatgctcttaaatSacacaatcgHattcaWcttaaaatHaaatcNctWttaNMcMtaKctVtcctaagYgatgatcYaaaRactctaRDaYagtaacgtDgaggaaatctcaaacatcaScttcKttNtaccatNtaNatacatttHaaDHgcaDatMWaaBttcRggctMaagctVYcacgatcaDttatYtaatcKatWatcaatVYtNagatttgattgaYttttYgacttVtcKaRagaaaHVgDtaMatKYagagttNatWttaccNtYtcDWgSatgaRgtMatgKtcgacaagWtacttaagtcgKtgatccttNcttatagMatHVggtagcgHctatagccctYttggtaattKNaacgaaYatatVctaataMaaaYtgVtcKaYtaataacagaatHcacVagatYWHttagaaSMaatWtYtgtaaagNaaacaVgaWtcacNWgataNttcaSagctMDaRttgNactaccgataMaaatgtttattDtcaagacgctDHYYatggttcaagccNctccttcMctttagacBtaaWtaWVHggaaaaNatttaDtDtgctaaHHtMtatNtMtagtcatttgcaaaRatacagRHtatDNtgtDgaatVgtVNtcaaatYBMaaaagcaKgtgatgatMgWWMaHttttMgMagatDtataaattaaccaactMtacataaattgRataatacgBtKtaataattRgtatDagDtcRDacctatRcagagcSHatNtcaScNtttggacNtaaggaccgtgKNttgttNcttgaaRgYgRtNtcagttBcttttcHtKtgcttYaaNgYagtaaatgaatggWaMattBHtatctatSgtcYtgcHtaattHgaaMtHcagaaSatggtatgccaHBtYtcNattWtgtNgctttaggtttgtWatNtgHtgcDttactttttttgcNtactKtWRaVcttcatagtgSNKaNccgaataaBttataataYtSagctttaaatSttggctaaKSaatRccgWHgagDttaaatcatgagMtcgagtVtaDggaBtatttgDacataaacgtagYRagBWtgDStKDgatgaagttcattatttaKWcataaatWRgatataRgttRacaaNKttNtKagaaYaStaactScattattaacgatttaaatgDtaattagatHgaYataaactatggggatVHtgccgtNgatNYcaStRtagaccacWcaMtatRagHgVactYtWHtcttcatgatWgagaKggagtatgaWtDtVtNaNtcgYYgtaaactttaDtBactagtaDctatagtaatatttatatataacgHaaaRagKattSagttYtStagagagacgatgaaaattaatcgtcaatacgctggcgaacactgagggggacccaatgctcttctcggtctaaaaaggaatgtgtcagaaattggtcagttcaaaagtagaccggatctttgcggagaacaattcacggaacgtagcgttgggaaatatcctttctaccacacatcggattttcgccctctcccattatttattgtgttctcacatagaattattgtttagacatccctcgttgtatggagagttgcccgagcgtaaaggcataatccatataccgccgggtgagtgacctgaaattgtttttagttgggatttcgctatggattagcttacacgaagagattctaatggtactataggataattataatgctgcgtggcgcagtacaccgttacaaacgtcgttcgcatatgtggctaacacggtgaaaatacctacatcgtatttgcaatttcggtcgtttcatagagcgcattgaattactcaaaaattatatatgttgattatttgattagactgcgtggaaagaaggggtactcaagccatttgtaaaagctgcatctcgcttaagtttgagagcttacattagtctatttcagtcttctaggaaatgtctgtgtgagtggttgtcgtccataggtcactggcatatgcgattcatgacatgctaaactaagaaagtagattactattaccggcatgcctaatgcgattgcactgctatgaaggtgcggacgtcgcgcccatgtagccctgataataccaatacttacatttggtcagcaattctgacattatacctagcacccataaatttactcagacttgaggacaggctcttggagtcgatcttctgtttgtatgcatgtgatcatatagatgaataagcgatgcgactagttagggcatagtatagatctgtgtatacagttcagctgaacgtccgcgagtggaagtacagctgagatctatcctaaaatgcaaccatatcgttcacacatgatatgaacccagggggaaacattgagttcagttaaattggcagcgaatcccccaagaagaaggcggagtgacgttgaacgggcttatggtttttcagtacttcctccgtataagttgagcgaaatgtaaacagaataatcgttgtgttaacaacattaaaatcgcggaatatgatgagaatacacagtgtgagcatttcacttgtaaaatatctttggtagaacttactttgctttaaatatgttaaaccgatctaataatctacaaaacggtagattttgcctagcacattgcgtccttctctattcagatagaggcaatactcagaaggttttatccaaagcactgtgttgactaacctaagttttagtctaataatcatgattgattataggtgccgtggactacatgactcgtccacaaataatacttagcagatcagcaattggccaagcacccgacttttatttaatggttgtgcaatagtccagattcgtattcgggactctttcaaataatagtttcctggcatctaagtaagaaaagctcataaggaagcgatattatgacacgctcttccgccgctgttttgaaacttgagtattgctcgtccgaaattgagggtcacttcaaaatttactgagaagacgaagatcgactaaagttaaaatgctagtccacagttggtcaagttgaattcatccacgagttatatagctattttaatttatagtcgagtgtacaaaaaacatccacaataagatttatcttagaataacaacccccgtatcatcgaaatcctccgttatggcctgactcctcgagcttatagcatttgtgctggcgctcttgccaggaacttgctcgcgaggtggtgacgagtgagatgatcagtttcattatgatgatacgattttatcgcgactagttaatcatcatagcaagtaaaatttgaattatgtcattatcatgctccattaacaggttatttaattgatactgacgaaattttttcacaatgggttttctagaatttaatatcagtaattgaagccttcataggggtcctactagtatcctacacgacgcaggtccgcagtatcctggagggacgtgttactgattaaaagggtcaaaggaatgaaggctcacaatgttacctgcttcaccatagtgagccgatgagttttacattagtactaaatcccaaatcatactttacgatgaggcttgctagcgctaaagagaatacatacaccaccacatagaattgttagcgatgatatcaaatagactcctggaagtgtcagggggaaactgttcaatatttcgtccacaggactgaccaggcatggaaaagactgacgttggaaactataccatctcacgcccgacgcttcactaattgatgatccaaaaaatatagcccggattcctgattagcaaagggttcacagagaaagatattatcgacgtatatcccaaaaaacagacgtaatgtgcatcttcgaatcgggatgaatacttgtatcataaaaatgtgacctctagtatacaggttaatgttagtgatacacaatactcgtgggccatgggttctcaaataaaatgtaatattgcgtcgatcactcacccacgtatttggtctaattatgttttatttagtgacaatccaatagataaccggtcctattaagggctatatttttagcgaccacgcgtttaaacaaaggattgtatgtagatggtaccagtttaattgccagtgggcaatcctaagcaaaatgagattctatcctaaagtttgggcttgatataagatttcggatgtatgggttttataatcgttggagagctcaatcatgagctaatacatggatttcgctacctcaccgagagaccttgcatgaagaattctaaccaaaagtttaataggccggattggattgagttaattaagaccttgttcagtcatagtaaaaacccttaaattttaccgattgacaaagtgagcagtcgcaataccctatgcgaaacgcctcgatagtgactaggtatacaaggtttttgagttcctttgaaatagttaactaatttaaaattaattaacgacatggaaatcacagaacctaatgctttgtaggagttatttatgctgtttactgcctctacaaccctaataaagcagtcctaagaatgaaacgcatcttttagttcagaaagtggtatccagggtggtcaatttaataaattcaacatcgggtctcaggatattcggtcatataatttattaagggctcttcgagtcttactctgagtgaaattggaaacagtcatccttttcgttgtgaggcatcttacaccgctatcgatatacaatgcattccaccgcggtgtcccgtacacaaggaaacttgttaccttggggatataagaaaactcacacgtctcattattaaactgagtacaatttttgcacgagaaagtaatgcaatacaatatgatgaaagccagctaatgaaaagggatggaacgcacctcggatctgttgcactggattaaaatccgattatttttaaaaatattcagtgctagagcatatcaggtctacttttttatctggtatgtaaagcccacggagcgatagtgagatccttacgactcaacgaaaagttataacataactcccgttagccaaagcccaatcccgattactgccctaccctaacgtctgccatctaaatatcgaacttgttatgatcaatgtgactacctcccaccctttccccttcatttgttccactggggataagctagcgttttcagaatcaatgcaataagaatagccaattgtctcacttcatcagagctcttggcaattccaggcgctacgtggttctggaatatattcatttttcaaatagtaatacgtttagtgttgctattgtctacacgtttggatattacgttatgtgagcggacatcaatagttgtctaactctttagtaagccagagatagcactcttagcgaatggataccatcttccataagtttagttaatagtccgaaacaactgcttcgagcatatttgaacctccttgtaggcaaatagcctcttcaaagcaatcttactaatagatagagtttgttttaagggactactagaaatgggacaatcttaatagtatgacctaaactgacatttaaagatatatccaggtggcaagcataaagatcattgcgccacctccaccgtgggattacttatcagtcgatatcctatatgctaagtttgcgacggcagaatacaaactaagctgagttgatgctaaccttacctatgataccccattggaccggttaacagccctacttattccaaataaaagaacttttatgctgtagaagctattatagtgatgcctggtaacttcagtatattaaaatgacacacatacgccatatagagctcctggaactttgaataatgagcgaacttcgaagttgaagagcaagaaaccatatgtcacggttgcctaaagcccggtaaccagacatgtgctatcattgatcattatcgaggttttcataaccttgacccattatcggctgtgcgcggacaagtacttaaatcactagtttcttcacctgcttatcggtaagaaataaggttggcaaagaatcgcataagacggacgtagagccgcagcgttgtgcgagtccaggtgcatgcgcagcaataggattttaaattttgttccatttttaatttagccgtaaggatgtccgtaaatgattgaaaattggattcaatctttgggcctatgctactggaacctgatcgacaaaatttcaaacatacgttaactccgaaagaccgtatttttgcggctagaatagtcagtcgcttggagccatataccttaccacttaaacgacgtgctcctgtagttgaaatataaacagaacacaaagactaccgatcatatcaactgaagatctttgtaactttgaggcgaagcaccctcttcgagacaactaagagtaaagtaccgggcgccgcaaggagtcgattgggaccctaaatcttgacgaattgctaagaggctcagagctaccactgtaatttctctagagcccataataaatgaacgatacatccgtaggtagcacctaagggattataatggaagccaaatgcagttaataatattatatactggcgtacacgattcgacggatctctcacatagtgattcacgacccccccctttgattgacacagcgtcagcattttgcaagaacgatcttctgcatagggtgcgccaccgtaaggatgacgtcgaagctacaactgggtataatttaccatgcttccctgatgctgagtgcaatacactaagaatgagtttttaccccatatcaccagtatttgttctgttattgcgaagaaatggctatgctgagttggcgactaaagtcacccatcctttttattaggtaaccccctcccttaaactaactgatttgctggagctgccctgcatacatatactttatcatttatggacgtccgtgacgcttattatccaccatagtcgatatgctacacggattcattaatggatcgtaggagtttaagttatatttactaagatcggtctcggctactatcccgccttacccggcgctatttacggccatttttaatatattgacggtaattattcctatggtttcgaccgcacgtccttggacaagaaagaatggcaaaaaaaatgtaaaagaaaaaaaatattgagtccctaccatcatataaaaaatatgtgatgagtaacttgacgaaatgttagtggttattaaagactatctattacaccttttgttttctgtcgtagtatattaaagtctagaagccttacaggaaaatcagggttatacagccgatactccgcagcatgaatcatcgaggaggtgtcctaccatcgcgccttgtaatcttgtctgtgtatactgtatttagaccttttatacaaagtaaatatctcggctttatgtgattgggaggggcctactcaaacatgatgacttgacctaataatcactgtgcgggcgtcttatgactagctattccttgaaatccaccaccaaatggttaatatgtaaaaactttgacgatgaaacaaggtgaatgtgtagttactttgtgtaattagctgcgtcgagcattgcttgtaaaaccgtcaatcgcacacgttacttccataaaatttctacgaatacacccttcttaaaaaaaacgtaggaattcacgagtttaacaaacgataactgtataaagtggaagtccgaagaaagcagatgcccgaactactcgaagatgtttcgttttcttaaccataggggcttcttaatggcccactacgcacattttgttcaagcccgagagggacatccccattacgggagtattactaaaactgttccgtaatacgttcagcaagggatgaaaaaggccactgctcaagttattgacgtgggagtattacatcggaagcctgaatcccacactatgatggtctgtacaggcctagggactgcgtctagacggtattaccggcttctaatcatacgatcgtgagtcttaacgggaagtaaggctcacacctaccccaaaccatttatctatgtaagtataaaattgtgcgtaagtgttcaaagtggacaataaagacgtggcaaaaacccccgcacataagccgctttagatttcacaaataccaatgcggttaaaaacatccttgagtcgtacatacaccatactcgcgttaaacggatataacagaagataataaatccggatgtggagtcggtgtaactatagaaagccaagtgaaataatgcttaccagtcatttagctatacggctttcatttcatgtcaagagggtggagtttgacctgtacagttgatatatcaccgatacttagaactcacctaaagctaaaattgctcgcagcgtgtaatccgcatattacaaacaatagatgggattcattatacataagacacgatgatctgctttttcaggttgcgagatgttgcctatcgtcaatcgagtcctgccttacaccacttaaacaaaagtattgacagggaacctattttcgaggtattatatagtccagcttgaatatcaatttgacagttaacctagtgaaaatcagtaagaggaaatacgccacattctccagtgaaattctacgggttatcgtctagtccaactatcaattataactcacgagatataagtaaattctcgtacttggcctgatttttattatactttggatccttagtaaacaggaagggagaaaccttcaacgaaaaacactggattttgttttactctcaaagctcttatatgacggaaataccctgtcaagtcttaactttattactagactaatgaaatgggcttggggtggccagaatcatagtacaatttagcggatacactattcggactttcctatcggctgtctggttggataagtatggggactaataggctagacatacctatacttaaactatacaggcgtcatctatctctgcaactttggagttccctgatgttctcccgccctttgggttcacatcttctataccgacacccctaataacgattagtttgtgggttagagtaaattaatacggttaatattaatgtatcgttgaaaagctggtgtcgccaataaggtaaccggctaggcagagtatatgtcacgaagtataactaccctaatgataagctgtaggaataaaattaatgctgtctctaagcgaagagatatttccgactctgttttaatgacgaatctcattacttctgacttgcaaatgttcaatatggcacggtttcacggcacctttgtgacgcatataatgaacttagaagattataacgacggaactttatatgataatccgttacgattaaagaatctgttaaatatcataatggcattcagttctagaccgtgcatcatggtaaacttactttctctgcatggcgacatacatttcgctattcaaattcgcgtgtggttacacccactcgcacctttggaatattaagagaagatgatcagaaaatccattcgctcaatttttctgacgtacgtctaatttatcctaggagacaaatcgttttatgtctctcacatttttgaagaaaggttcgagagacaatactcaggtcctgaactgctagaagatactcggtggagcgtggcaacaatgaaaaactcgtgacataaatgaatgatacttttccaagttcagttaagtgaatatgtttaacatacccggcttttcgatcttaagctgacgctggacgtgcgagtaatgtcagtctcttacatacactagtgactccaagtttcgtcaaaaacgccccctcccttctcgagcccactcacgctatgtattgacgcgaacttgttcgggatcagacttttcaggagttcggtcgcgtgtccctatgtgctaatatataagttagatcgcattagatgctaatctgaatacttatagacgaccttcaacgagaacgggtaccaccttgaggctagagttaggtgtgaaacgacaggtagggacatataaaatttgagtgcggctttagttaagggtttaattacctactcaaacatcacgctcgcgcccttcgtacgtaatcgaccatctagaggctaaggggactgtactaggtagtgattaatgatatcctagacgcacgtgccttagatcttcagactctgatggtccgcgatcaccgtaattgtagtcctccaactcgatcactttgttggcgtcaaagaaattacgatatctaaatacttataatacaataaccaaggatgagaatgactcatcgcgttggagttatattgcttgaagttctatggaatgaaagcacgttatctgccgtcccaatatctccagtgagctaattcattggacggtccactttgatcaatccccgaggagatgttcggacactttagtctgtaacacttagcgttgagaccacgaacaattgattactcagtcttgaaggtgttttccaaagttcattttaaataagactacgataggcctttcctattgatataaactacccggctctgttgttcgtgtgagtcgtacttctctgtgtttttctgattatagcaagattcgattcttagtgtaaacagcgatttttatttgacccgtcaatgagaagcgcataggatctaagcaaaattatcaagttgtgccacaaggtaagatctttccagttattgcaggtaggatgtatcccacgttgatagtatgaggtctgacgtcaactgtctaggagagttgaccgcgtgcgggtacaccggatttgcatcgatgttgagaacgcagaactcccactgtcgtggcggcgttcctgatatttagcaagaggcgttgataaagccctcatcatctagatctcgacctcatctgccctcttgctccatcattttctacacagactactttcctatctacgttagtataattgctttctatcttagtatcatttagagcttctccgtcaacaggttcgtgctattaaagttagtacgaaagggacaacttgtagcaacgcatttaatcggttttcgactacttcgcacaaaatcagataaagaagtttgtcattctattagacattgaattgcgcaattgacttgtaccacttatgatcgaacactgaatcaagactgtgattaactaaaatagacaagccactatatcaactaataaaaacgcccctggtggtcgaacatagttgactacaggataattaattggactggagccattacattctctacaatcgtatcacttcccaagtagacaactttgaccttgtagtttcatgtacaaaaaaatgctttcgcaggagcacattggtagttcaatagtttcatgggaacctcttgagccgtcttctgtgggtgtgttcggatagtaggtactgataaagtcgtgtcgctttcgatgagagggaattcaccggaaaacaccttggttaacaggatagtctatgtaaacttcgagacatgtttaagagttaccagcttaatccacggtgctctactagtatcatcagctgtcttgcctcgcctagaaatatgcattctatcgttatcctatcaacggttgccgtactgagcagccttattgtggaagagtaatatataaatgtagtcttgtctttacgaagcagacgtaagtaataatgacttggaataccaaaactaaacatagtggattatcatactcaagaactctccagataaataacagtttttacgatacgtcaccaatgagcttaaagattaggatcctcaaaactgatacaaacgctaattcatttgttattggatccagtatcagttaaactgaatggagtgaagattgtagaatgttgttctggcctcgcatggggtctaggtgatatacaatttctcatacttacacggtagtggaaatctgattctagcttcgtagctgactatactcaaggaaccactgctcaaggtaggagactagttccgaccctacagtcaaagtggccgaagcttaaactatagactagttgttaaatgctgatttcaagatatcatctatatacagtttggacaattatgtgtgcgaaactaaaattcatgctattcagatggatttcacttatgccttagaaacagatattgcccgagctcaatcaacagttttagccggaaacaatcgaagcatagggacaatgtatcttttcctaaattgccatgtgcagatttctgagtgtcacgaagcgcataatagaatcttgtgttgcctcaactcgttgaaaagtttaaaacaatcgcagcagtctttttggggtctactgtgtgtttgcaaaataactgaaagaaacgcttgaacaactctgaagtagctcgagtactcattaaagtgtaacacattagtgaatatcggccaatgaaccaaacgcttcccggtacgctatctctctcatcgggaggcgatgtgcaggttatctacgaaagcatccctttacgttgagagtgtcgatgcatgaacctcattgtaacaatagcccagcaaattctcatacgtgcctcagggtccgggcgtactcctccatggaagggcgcgcatctagtgttataccaactcgctttttaactactatgctgtagttctacaggcatagtggccagtattttctaacttctctggatagatgctctcactcctcatccatcacggcttcagtttacgtcttacttgcttgttcagcaacggatggaggcattaagtatcttcactgttccctaaaattgctgttcaatatcaaagtaaggacgatacagggaaagctcaagcacactcattgaatactgccccagttgcaacctcacttaatctgacaaaaataatgactactctaagtgttgcggaagcagtctcttccacgagcttgtctgtatcacttcgtataggcatgtaactcgatagacacgaacaccgagtgagaaactatattcttgcttccgtgtgtgtgacaccaggtaattgatgcggatataagctggagatcactcacgcccacacaaggcgctgctacctctttattccaatgtgtaagaatttgctaacttcatttctagaccgcagctttgcggtcataatttcacggtacggacccttgggttagagacttgataacacacttcgcagtttccaccgcgcacatgttttagtggcttctaacatagaatttttgttgtgacataaagagtgcgtgggagacttgcccgaccgttaagccataatcaattgaaagccccgtgagtcacatctaattggttgtactgcgcatttagctatcctttagctgactcgaagagattcgattcctaatataggttaattagatggctgccgcgcgaagtaaaacgtgaaaaacgtagtgcgcagatctgcataactcgcgcttaattacttatgagtagttccaagttcgctacgttatgagagagattggaattaagcaaatatgttttatggtgattttgggatgagaaggactgctaagtacggctactaaacaaatttctaaaaccgccatctaccttatcttggagacatttaagttgtatatgtcactagtctagcttttgtctgtgggacgcgttctcggaatgagggaaatgcaagagccgattcatcaaatgcttatctaagaaagtagtggactattacaccaagcacgaatgccagggaactgctttcttgctcaggacctcgcgacaaggtaccccgcataagtcctagaattacatttggtcagcaatgctgacatttgaccgtgaaaacataattttaatcagaaggcagctcacccgcttgctctagatcttatctttgtatgaatgtcagaatttactgcaatatccgttccgaatagtgagggcttagtatagttctctgtatacaggtcacatcaaactccccctgtcctagtacagctctgagctttaattaattgcatacatttccttcaatcatcagatgaaaacaccgcgaatcatgctcttctcgtatagggcaagagaagcaacaaacaactagcccgactcacgttcatccgccgtatccttgttcagttcttactccgtattaggtcagcgaaatctaatcagaataatcggtcgcgtatcaaaattaaaatcccgcttgaggttgacaattaaaacgctgagcagttatcggctattagatagtggggtgaaagtaattggctggaattatgttaaaacgtgatattaagctaaaatacgctacttgttgccgacctaattcagtcattcgatattcagttagagccaagaataacaagcttgtataaattgaacggggtgcactaaacgatgtgttactctaatattcagcttggagtatacctgaaggcgaattcatgtatcggccaataataagacgttgaagatcacaatttggactagcaaaagaaggtgatttatgcgtggggattgagtccactgtacgagtacggtctctggaaaattataggttcagggaatataaggaagtaaagataattaccaagagatttttggtatcgctatgacccagaggtgttctaacgtctgttttgatccgcagaatttctgcctcaatgcatatttgacggacttgaactagagcctctaaagttaaatggcgacgcaactgttcctaaacttcaattattactactctttttttcctagggtattgtagaggccagtggacaaaataaatcaaatttaagatgtttcggacattaacatcccccgtagcatagaaatcatcagttatccaatctctcatcgagcttttacaatttctgctggcgctatggacagcatatgccgcgagacctccgcaagactcacttgatcactgtaagtatcttcattagaggttagagcctatagttaagctgctgacctagtaaaattggtattttctaattttattgctcaagttaaaggttagtgaagggataatgacgttatttttgaacaatgggttgtattcaattttatatcacgaatggaacccttcattcccggcataatactagacgacacgaacaagctccgatctatcagccaggcacgtgttaaggtttaattccggcaaaccaatgaagcatcaaaaggtgacctgatgcaacttagggtcacgatgagtttttcaggactacttattacctattaataagttaacatgagccttcataccccgtaagacaatacatactccaccaattagaattctgagccatcttatctttttgtatcatcgaagggtatggccgaataggttaattagttactcctaacgtctctacaggcatgcatttgacgcaccttcgaaaatagtcaatctctcgccacacgcgtctagtatgcagcatcaaaaatatagtccacggtttccggattaccaaacgcggcaaagagaaacattgtatcgacggagataacttaatacagaaggaaggggcatcttcgaatacggatgaataattctatctgtttattctgacatcttgttttcaggttaatcttacgcattcaaatgacgcctgccccatgcgtgcgcaattattttctaatattgacgagagcaatctcactccttttgggtctatttatgttttattgaggcacaagcctatacagaacaggtactattaaggccgtgagtgtgagactcaaaccgtggaaacaaaggatgggttgttcttggtacaagttttagtgcatgtgggcaatccttaccaaaatcagatgctatccttaactttgggctgcatttaagatggcggttggaggcctgtgagaatcctgcgtgtcatctttaatgaccgaattcatccatgtagattcagatcacacactcattccttgatgttgtctaaacaaaagttgttgtggacgcattggagggagttaagtaacaacttgggatcgcatacttataaaaattatatgttaaactttcacaaacgctgaagtccaaagtaactagcccaaacgcctcgagagtcactaggtattaatggtgtttgagttcctgtgaaatagtgttcgaaggtaaaatttatgtaccaaatcgaaagaacacttaataaggcttgcttgcacggaggtatgatgtttactgactctacaaccctaattttccagtacgtacattcattccaataggttagttctcaaagtgctatacaggctcctcaattgatgatatgcttcagccgctctatggatattagctcattttatttaggaagcccgcttagaggcttactatgagggaaatgccaaaatgtcatacttttcggtgtgtcccatatgacaccgctttacatagaatttgaattaaaacgcgctctcccgttcactaccatacttggtaccgtgcgcatattacatatagatataggatcattttttaaagctgtactaggtttgatcgacaatcttatgctatactatatgatgtaaccctcataatcaataccgatcgtacgatcctagcataggtggcaagcgattttatgccgattattgtgttaaatagtctgtgagtgtgattatcagggctacgttggtagaggggttgtatagacctcgcacacattgtgacatacttaacaatatacgaaaactgatataataaatccccttacccaaacaccaatcccgttgaatcaactaccataacgtctcccatataaattgcctacttgtttgcataaatctgaatacataacaccattgcaccttcttgtgttccaatcccgttaagattgccttgtcagatgatatgcaagaacaatagcatttgctagcaattattaacagctcttcgaattgcctccacataacgcgggagggtatattttaatttggcaaatactaagtactgttggcgtcatatgctattaacggttggatattaagttatgtcagccgtaagcaagagtgggcgaaatattttgttacccagtgagagcactcttagagtttggatacaataggccatatgttgacttaagaggacgtaactacgccgtacaccattgttcaaccgacttcttggcaaatagaatcgtattagcaatcttaagaatagagacacgttcgtgttagggtatactacaaatccgaaaatcttaagaggatcacctaaactgaaatttatacatatttcaacgtggatagatttaacataattcagccacctccaacctgggagtaattttcagtagatttactagatgattagtggcccaacgcacttgactatataagatctggggatcctaacctgacctatgagacaaaattggaaacgttaacagcccttatgtgtacaaagaaaagtaagttgttgctgttcaacagatgatagtcatgacgcgtaacttcactatagtaaattgaaacaaatacgcaatttagacagaatggtacggtcatgaatgacagtaattcgaagtgctagaccaacttaaaataggtaaacgtgcccgaaaccccccttaacagaaagctgctatcatggtgcagtatcgacgtgttcagaaacttgtaacttttgagcaggtccgagcacatggaagtatatcacgtgtttctgaaccggcttatccctaagatatatccgtcgcaaactttcgatttagtcccacgtagagcccaagcgttgtgcgactccacgtgcatgcccagaaatacgagtttaaatttggttacatggttaattttgaccgaagcatcgcactttatgattgataattggattcaatatgtcgccctatgcgaatgcaacatgatccacaatttggctataagacgtttaatccgtatcacactttgtttgcggctagtatagtaacgcccgtgcaccaagagtcagtaacaattataagtactccgcaggtacttcaaatataaaaactaatcaaacacgacccatatgatcatctgaagatatttggaactttctcgacaaccaccctcgtactcaatacttacactaatcgacaggcacacgcaacgtgtacagtcgcaccatattgagtcaagatttgcttagtggcgatgagcgtacacgcttatttctctagtcacaattagttatctacgagacatcacgagggagcaaataagcgatgttatggctacacataggcacgtatgaatatgatataagccagttaaacagtcgaaccatcgagcaaattctcatgcaccaacccacacgttgaggcacaaagagtaagctgtttgaatgtaacttcttctgctgagcgggccccaacgtaaggatcaactagaagagaaaactcggtattagtttaaatgcgtcacggagcatgagtgcatttcactaagaatgtctgtgtaaccaatataacatctatttgttatctgattgcctacttatggctttgcggtcgtggcgactaatgtctccaatccttttgaggtcggtaccaactccctttaaattacgctgtgcaggctcatgcactgcatacatatacggtagcaggtagggacctcacgcacccttattataatcaatagtagttatcagtcaacgaggcaggaatgctgaggtcgaggtgttggtatattttctatgtgccgtctaggcgactatcacgcattaccaggcgagatttaagccaattttgaatatagtcaacgtaatttttactatgggttccaccgaaacgccttgcacaactaagaatcccataaaatatcgatatcaaataaaagattgtgtcaataccttcatatatattttttcggttgactaacgtgaactaaggttaggggttttgtatgtctatataggaaacagtttcttttctgtcctactttagtaaagtcttcaagccttactccaaaatcacggtgattaagccgttactcagcagcatgattctgcctgctcgggtcctaaaatccagccttgtaagagtcgctgtgtattagctagggagacctttgttaaaaaggatatatcgcggcgggatgtgagtgcgtggcgcatactcaatcttcagctcgtgtcattataatatctctcccccacgcttttcactagatatgccgtgtaagcaaacaccttatgcttaatttcgaaaatattggtacttgaaaaaagctgtaggggtacttaatgtctggtaggagatcaggagagaattgagtgtaaaaccgtaaagccctcacctgacttcatgtaaatggcttagaagactccatgatttaataaatactacgaaggaaagactggatctaaagataactctagtaaggccaactcccttcaatgctgttgccagttataatccaagagctgtccttttctgaaccatagcggcttctgaagcgaactagaagcaaagttggttctagccagacagccacataccctgtacgggtgtattactaaaactggtccggtattagttcaccaagggaggaattaggcaaaggatctaggtatgcaagtcggagtattacatccctaccctgaatccatcaataggttcctctgtactggccttcgcaatgagtattcaaggttgtacagccgtataataataagatagtgactatgaacgggaagtaacccgctcaccttccccaaaacattgttatatctaagtattaaagtctgccgtagtgttaatactcgaaaataaacaactggcaaattacaccgcacttaagccgcttttgatttatatttttccaatgcgcttttaaaaataattcagtcctacatactaattaagacccttaaacggagatatcacaagttaagttttaaccatctcgactaggtggaactatagatacccaactcaatttatcattacctgtaatgttcctagaaggattgcatttcatgtcaagacggtggagtttcacagcgaaacttcagtgtgaacagattctgagaaatcacctaaacctattagtcagagcacccggttagaaccagttgtcaaaaaatagagcggttgcatgagacagaagtaacgatgagatccgttgtaacgttgagacatctggcctatcgtcaatacagtcctcccttaaaaatatttttaaatactaggcaaacccaacataggttagtcctatgtgatacgccacatggtatatcattttgtaacgttacctagggataatcaggaagtggaattacgcaaaagtagacagtgaaatgcttagggttatagtctagtccaaagataaaggataaagcacgtcagagaactatattagccgaatgggaatcattgttaggagactgtggatcatgtctaaaaagcaacgcagaaacagtcatcgaaaaaatctcgtttttgtttgaatctaaaagagctttgatgaccgatagtacctgtatactagttactgtattacgtgtctaatgatttcggattggggtccccagaatcagacgtcattgtagacgattcaagtttaccaatttaatttcccagctctccttggagaactatcgccaataattgcagtcactttccttttctgaaacgataaagccgtcagagttctctgcaacgttggacttacctgaggttctaacccactttcggttctaatagtagttaacgacacaacgaataacctttactgtggggctttcacgatattttttcgcttattattaatggttacgtcataagctggtgtccaaattaaggttaccggcttcgcagagtagttgtatccaagtataacttccctaatcataagatcgaggtagaaaattaatgctgtctctaaccgaacagatatgtcccactatgtggtatggacgttgctaattacttctgaagggaaattggtcattatggatacgtgtctaccatcaggtcggacgcagatatggttctgtcttcagttgatccaccgttctttataggataataactgacgattaaagattatggtaaatagattaagccaattctcttcttgtcagtgaagcatccttaactgacttgctctgcagcccctcatacatttagctattcaaagtaccggctcgtttcaaactctcccacctttggaagaggttgtcaacttgataagtatatcatttacagcattttttcggacgtacctctaatgtttcattgcagaaaattagttttttctatcgcacattttgcaagtaacgttagagacacaattatctgcgaatgaactgctagatctgacgaccgggagcctcgcaaatatcaaaaaagactgacatatatcaaggagtcgttgacaagtgctggtaagtcaattggtttatctgtcccggcgtttcgatcttaagctgaccatgcacggcagagtaatgtcactctcgttcttacaagtctgtctccaagggtcggcaaaaaagacccctccattctcgagcccactcacgatatgtagggacgacaacttgtgcggcttatgaattgtctggactgcgggcgagggtccatatctccgaagttagaagggacatacctttagatgataagatcaattcttattgacgaaattcatccacaacggggaacaacttcaccctagacttacgtctgaaaagacacctagcgtcttataaaaggtcagtgccccgtttcgtaaggctggaattacctacgcaaacttaaacctcgcgcccttccttacgtatcgacaagatagaggctatcgcgaatgtactacggaggcatgaatcatatactagaaccaagtgcctgtgatattaacaagatgatccgacgcgagcaccgtaattctaggcataaaactccagcaatttgggggccgaaaacaaatgacgttagctaattaattatatgacatgatcaaaggaggtcaatcacgcatcgagttcgacgtatattcattgaacttcgtgcgtttgaaagaaacttttatgaaggcaaaattgatcctgtctcctatttcatgcgtacctcctagttgataattccccgagcagtggttaggacacttttgtcggtatcaagttccggtctcaaaacgtaaaattctgtaatctgtatggatggtctgtgaattagttaatttttatgaagtcgtcgagacgcagttcctattgatttattctaaacggagatgtgcttcgtgggactcggaagtagatctgtgtttatgattattgctactttagatgctgactgttaactccgtgttgtttttcaaccgtatatcacaaccgaattggatagaacctatagtttcaagttctgccacaaggtatcatatttacagttagtgctggttgcttctttcaaacgtggtgagtttgtgctatcacgtcaacggtagagctcagtggaccgagtgcgcgttcaaccctgttccagagagggtgtgatagcacatataccacgctcgtcgaggcgttcatgatagtttgcaagagccggtgttaaacacatattattattgttatccaactaatcggacctatgcataaagcattgtctaaacagaataattgcctatatacggtagttttagtgatttatatcttagtatcagttagagcttcgaactcttcaggttcctcatatttaacgttcttcgaaagcgaaaacttctacaaacgaatgtaagcggttttccaagtagtacctataaatcacagaaagatctgtctcagtatagttgaaatggtattcagctagtgacgtgtaccaattatcatagttcactcaagcaagacgctcattaacgaatatagacaagacactatatcatataataaaaaagaacatggtgctcgaacatagttgaattcaccatattgaaggggaatgctgacatgtaattcgctactagacgatcaattccctacttgtcaaagttgaactggtacgttcttggaattaaatatgattgcgctggaccaaattgcgacttcttgagtttcagggcaaacgattgagccggaggatgtccgtctcttacctttcttgcttatgataaacgacggtccctgtacatcactgggaattctcagcaaaaataattgggtaaatcgagactcgatgtattcggccacaaaggtgttagacgttaaagattattcaacggggcgataataggatcataaccggtatgcaagcgcattgaaagagccatgagatccttatccgataaacgctgcacggtatgtgcagccttattgtcgatcacgaatttataaatgtagtctgggctgtaagttgaagacctaagttataatgaagtgcaataccaaatcgattcatagtggattatcagactcaagatatctcctgataaattacagttgttaagatacggataaaatgagatttaagattagcagcctctaatctgtttcaatcccgttggaatgtggtatgcgatcaaggttaagttaaaatcaagcctgtcttcagtcttgattcttgttctgccatcgcatgcggtctacgtgagttaatatgtagcttacgttctagcttgtgctaatctgagtatagattcgtagaggaatattatcaagcttccacgcctcaacgtacgtgtattggtcacacaagacactaaaagtggaagtagcgtaaactatagtctagttgttaaatgctcagttcttgttatattcgatatactcttggctaatttatgtctgagtatataaaattaatgatattaacttgcatttcacggatcccttagaaaaagattttgaccgagcgcattataaacggttacaccgaatcaatagaagcatacccaatagctttctttgaatttattgcctgcgcaacttggctgactctctagatccgaataattctatatggtcgtgacgaaactagttcattactgtttaaaatgccaacatgtcttttgggccgataatggctctttgcaaaattactcaatgatacgattgatcaaagcggtagttgctagtggtagcatgtaagtctatcaaatgtctgattatccgaaaatcttccaaaagagtccacgtaccatatctatctcatagcgacgcgaggggaaccttatctaactatcattccatttaccgggtgactctcgatgcaggatccgattgggataaattgcccagaaatggctcattcctgactaagggtaaggccgttctcagcaagggaaccccgcgaatctaggcttataccatctagattgttaactacttgcctgtagttctacagccatactggacagttgtttctaaatgatcgggattcatgctagcactcctctgaatgcaccgcgtaagtttaactattacgtccgtgggcagataaggatggaggctgtatgtatcttaactgttacctaatatggctggtaattatcaaagtaaggaccttaatgccatagcgctagcaatcgctttgtatactgaccatgtgccaacctctcttaatctgtaaaatataatgtcttagctaactgtggacgatcatgtctctgcctagagcttcgctgtatcaattcctatagccagcgtactagtgacacaacaacaccgtgtgagaaaagatattagtccttacgtctgtctctctacagcttattgatgaggattgaacatggacatatagctccccctcaaaagcagatgctacctctttattccattctcgaacatttgccgaacttaatttcgacaaacctgaggtcacgtcttaatttatcggtaacgtcacgtccctttgagactggataaatatattaccaggggccaacgagcaattgttggaggcgcttctataatacaaggtgtcttgtcaaagaaagacggcgtgcgtctcgtgcaactcacttaaccaatattaatgtgaaacccccctctctcacatcttatgcggtgtactgccctggtacatttcctgtacaggactccaacagtgtagattcctaagatagctgttggagttgcctcacgccagatcgaaaaactgaataaactagtgagctgagctgcagaaataccgcttaattacttatgactagttcaaagggacctacgtgatgtcagacattgcaaggaagaaattaggtttgtgcgtcattttggctggactagcactccttacttcccctactattcaaatgtcgtaaacagcatgagacaggatcgtgctgacatttaaggtctattgggaacgaggctacctttggtcgcgcgctcgcgttctccgaatgaccgaaatgcatgagcacagtatgcaattgcttatagatctaaggtctggtcgttgaaaccaagcacgtaggcctgggaaatcagttcttcctcagcaactacacaaaagcgtccaagcattagtacttgtagtaaatgtccgaacctatgcgctcatttgaaagtcaaaaaatatttttaagcagtaggcacctaacccgattcctctacttagtagctttctttgattctcagaattgactgcaatatcactgcacaattctgtgccattactagacttctctgtattaacgtctcatcttactaacactcgcctaggacacatctgagagtgaagtatttcaatacatttactgaaatcttcagttctaaaatccccgaataaggctcttatcggtttggccaacacaagaaaaaaacttcttgcaccactcaccttcatacgcaggagcctggggaacttagtaataactatttcggcagacaaagcttataacaagttgccggcgcgtataatatttaaaagaccccttgagctgctcaattaaaacgctcacctggtataggctattagatagtgccgtcttagtaaggggcgggaattatcggataaactgatattttgataaaataaccgacttgttcacgacataagtcactaaggagattttatctttctccaaagtatatcttccttggataatttcaaagcgctgcaatttaagttctgttactagtttatgctgctgggaggtgaccggaaggcgtagtaatctagaggcaaattataagaagttcatcatatcattttcgactacaaaaacaaggtgttgtatgccggcgcattgtgtaaactggacgagtaccctagatggaaaattatacgttaagccaagatttcgatgtaatgataattacctacacatttttgctatccataggaacaagagctgttctataggctcgtggcatacgaacatttgctgccgctatgaatattggaagctcttcaactacagactctattcttaattgccgtcgaaaatgggccgaatcggctattattaatactcggtttttccgaggggattgttgtcgacagtcgtaattattattaatattgatgttggtgaggtcatttaaatacaaccttgcagacaatgaataagggatccaatctctcatactccttttacaattgctcatgcccctatgcaaaccttatgccgccacacctccgcaactctctcttctgaactgtaagtagcttcattactggtttgagactatactgaagctgatgacattctaaaatggctattttcgaatgtgattcataatgtttatcgtttgggatggcagaatcacgttatttttgatatagcccgggtattctattgtatagaacgtatgctacaagtcattccccgaagaagactagaagtaaacaacatgcgaccatcgttaagccacgcaaggctgtagctttatttcccgataacctatcttccataaatagcggacagcaggatactgacgctcaacatcagtggttatggtctaatttttaacttttaataaggtaacttcagcaggcatacacagtaactctttaatttataatcaaattagaagtctgacacttcttatatttttctatcatccaacgcgatcgcccattagcttattgtgttactaataacgtatctaaaccaatccttttcaagctactgcctatattgtcaatatatacaaacaacaggatagtaggctgcttaaaaaatattgtcaaccgtgtacgctttacaatacccggaaatcacaaactttgtagacaacgagtgaaatttatacactacgaagggccagcgtacaagacccatgaattaggcgatatgtttattctgacatattggtttatccttaatctgtcgctgtaaaatgaagccgcccccatccctgcgaattttttttcgaagattcacgactgaaatataaatacgtttggctatatttatgttggagggaggcaatagcctttactgttaaccgaagatttagccagtgagtgtgacactaaaacactggaataaatgcaggcgttcttctgggtaaaaggtttagtcaatctcgcctataagttcatatagctctggatataattatctggcccatgcatttatcatggcgcttggtgccctgtgtgaagccggcctctcatattgaaggtccgaagtattccatgtacattaagatcactctctcattcatgcatcttggcttaacaaatctggttgtccaagctttccaggcacgtatggtacaaattcggatcgaatacttataaaaatgatatgttaaactgtctaaaacgctcatctacaaagtaaagtgcactaaccaatagagtctcaagaccgtgtaatgctggtgcactgaatgtgtaatacggttagaagggattagttatgttacaaatccattgaaaacttaagaagcattgcgtgctcggagggtgcatcttttatcaagagactaacattattttcaacgacgtacatgctttacaatagggtacttatcaaacgccgagaaacgcgcctatagtgatgttatgattatgacccgatatccattggaccgaattttatgtaggttcccagcgtactcgcgtaatatctcggtattgccataatgtaatacttgtcggtctctcccagatgaaaaagcgttacagagtatttcaatgaaaaacagcgcgcaacgtcaatacctttaggggtaacggccgctgatttcatatagatatacgataagttggtatagctctactaggtggcatccacaatcgttgcatttactatagctggttacaatcataatctataccgttccttacatactaccatagcgggatagcgtttttttgccgttgattgggtttaagaggatgtcagtctcattatatccgattcggtgggagagccgttgttttcaaatcgcacactttgtgacataatgtacaagataacaaaactgatataagatataaactgtcaatatcaccttgacacttgaatcaaagtaaattaactcgcaaatataatttgactaattgggtgcagatttctcaattaataaaaaaatggcaccggatgggcttacaagccccttatcattcacttgtatcatgatttccaagaacaatagaatttgctagcaagtatgaacagagattcgaattgcatccacagtacgccggagcgtttattttaatgtggatatgacgatgtactgttggcggcatttgctagtaaccggtccttatttacgtagcgcacacgtaagcatgtctgggagaaatatggtggtacaatctcagagaaagattacagtttggtttaaataggacttatcgggtcggaagtggaacttaataagcagtacacaattgggcaacagacgtcttgcctattacaataggattacaatgcgttagatttcagacacgttcgtgtttggctattcgtcaattccctaaatagttagacgatcaactattatcaaagtgattctttgttcatcctccattcatgtaacagatggcacactacgcataacgccgaggaattttaacgagatttaagagagcagttcgggcacaacccacttgactttataacagctcggcagcataaacggtaatatgtgacaaatttccaaacgttataagaacgtatgtgtacttagaaaactaagtggttcatgttcaacagatgtgacgcagcaagcctaacttatctattggttttgctataaaagaacaaagttacacagaatcctaagggcttgtttcacacttatgcctagtgcttcaccatcttaaaatagcgaaaccggcacgaatcaaaccttaaaacaatgcgcagatattggtgatggtgactccgggtatgataatggtaactgttgaccagcgcccacctcatcgaagtatagaaagtggttaggataaggatgagaccgaacttatttccggccataactttagattttctacctagtacacaacatcagggcggacacgaaaccgccatcacatcatataccaggtttaatttgcttaatgggggaagtgtcaacgaaccttcgaactttagcaggcatatggccattatatatggccccagagcagaatgctacagcagacaaaatttggatttatgtagtttaatacctatcaaacttggtgtgaccatacttgtctaacgacagtgcacaaagtgtaagttacaattattactactcagcagcttctgcaatgataaaatcttatcatacacgtcacatatgataatatctacttagggggaacgggctccacaacctacatagtactcaatacttacactattcgacaggcacaccaaacctgtacagtcccaaaagattgagtcaactttgcagtactgcagatcacagtaatagcttagttagcgagtcaaaattagttttctacgagactgcacgaccgtgcaaatttccgatgtgttggctacaaatagcaacgtatgaatttgtttgaagccacgtaaactgtacaaccttagagataagtctcaggctactaaaaacacgttgtggcactaacaggatcatggttgattcttacttattcggctgaccggcccaataagtaaccttcaactagaacagaataatcgggagtagtttaattcagtcaaggtgcaggtctcattgtaactaacaagctctgtgtaaccaagttaaaatcgttttcttagcggattccctacttatggatttgagctcgtccacaatattcgatacaagaagtttgtggtccgtaacaacgaaattttaattacgctgtgcagcctcatccaaggaattaatagaaggttgatggtaggctccgaacgctccatgattataatcaagtggactgtgcagtaaacgaggaaggtatcctgacgtcgtggtgttcgtttttgttatttgtgccctatacgagtagataaaccatgaacagcacagtgtgaacccatggttgattttaggctaccttatttttaatttccgttacacagaaacgaattccacaactaacatgccattaatttttcgatatcttataaaagatggtcgaaattcattcatttattttttttcggttctcgaaagtcaactaagctgtcgcgttttgtttctctttagaggtaaaagtggctttgatctcctacgtttggatactagtcaaccattactccatttgatccgtgagtatcacctgtctaacatccagcattatgactcctcggcgaagaaaagacacacttcttagagtcgatgtgtattagctagggacacagttgtttaatacgatagtgagcccagggagggcagtgcgtcccccagtagatttattcagctagtgtaagtataagatatctcacccacgaggttcaagtgatatgcagtcttagaataatacttatcctgaatttcgatattatgggtacttcaataatccgctagcgctactttatgtctcgttggacagcaggacacatggcagtcttaaacactaaagacatcacctgaatgaatgtaatgggattacaagaatcaatgaggtattatatacgacgtaggaaactctggatatatacagtaatctagttacgccatcgcacttcattcctctggaaacttagaagacatcagctgtacgtggaggaaccagacccccgtatgtagccaaatagaaccaaagttgcttatacaaacacacccaatgacaatggaccgctggagttcgtaaactcggaacgtagtactgcacaaacccagcatttagcaataggagctacgtatgcaactcccacgtggtaataccttcaagctatcaatatataggtgcctagctaatcgcattcgcaagcagtattcaagcttgtaaaccagtataataattacagaggctctatgaaacccaactttccagctaaaagtcccaattaaatggttatttcgtacttttaaagtcgcccgttctgttattacgcgaattgattctactccaaaattaaacacaaattatcaaccgtttcatttatatttgtcaatgcagctgtttaaaataaggctctactaaattataattaagacacttattaccagatttctctagttaagtttgaaccagctcgactaccgcgaaagatacattcccttctctatttttcagttcatctatgggtcagagaagcattgaatttattctattcaccctcgtcgttcacagcgaatcgtcagtgtgatcagtgtatgagaaatatcctaaaccgtttagtcagaccacacgcttagaacaagtggtctaaaaagactgccctggaaggagtaagaagtatacagctgatccggtgtatccttcagtcatctgccctatactaattacacgacgcaaggaaaaataggtttattttctaggcaaacccttcataggtgactccgatgtgttacgaatcatgcttgagaatgtgctatcgttaccgacggataataacgatctccaatgaaccaaatgtagaatgtctattgattacccttttactattcgacttagagataggagatagaacctcagtgtacttttttagccgaatgggaatctttgggaggtgaatggccataaggtcgtaaatccaaccctcttaaagtcttccatattatatcgttgttcgtggaatcgataacagatttgttgacccatagtaaatgtatactagtttatgttgtaagtgtagattgttttccgattgccgtccaaactttatgtcgtaattgtagaccagtaaagttgaccaaggtaagtgcccagcgatcctgcgagatcgatcgccaatttttccagtcactgtaagtgtaggtttagataaagccgtatgagttatatcataagggcctcggaaagcagcttcgaaccaaagttcccttataatagtagtttaactataaaagtatatactggtctgtcgccctttcacgatttgttttaccggtttatgaagcgttacgtcattagagcggctccaatttaaggttaacggcttccatgtgtagttgtatacaaggataacttaaagtatctgttcagcgagctagttaagttatcctcgatagaacacaactcagaggtcccaagatcgggtttgcaacttgctaatttattctcaaggcaaattgggaattatcgatacctgtataccataaggtcgctcgatgtgatgcttatgtcttctggtgatcctaccttagttagtgctgattaacggaacattaatgtttatcgttttgagatttagccaattctctgattctaactcaagatgccttatctgacgtgctatgcagcccctaagtattttacattgtaataggacacgctcctttaaaactcgccaaaaggtcgttgtggttctctactggttaactatataatttacagctttgttgagctagttcctctttggtttaagtcctcaatattagttggttcgagcgataagttggctagttaccttagtcactatattagatccgaatgttatgcttcatctgaagaccgccaccctccaaaatttcttttaagactcacttattgcaaggtgtaggtgaattcggctcgtttctcaagtggtgtatctgtacacgagtttccatattttcatcaacagccaccgcacacttatgtcactctaggtattaaaagtcgctctacaaggggacgcaattaagaaacagacatgctagtcaaaaataaacatagcgaggcaccactaattcggccgcttatcaatgggatgctctgcgcgagacgcgccagagctcagtagttagttcggacatacatttacttcagatgatcaattagttttctacaaatgcttactctaccccgaaaaaagtcaccagactcttacgtctctttagtatccttccgtcttatataaggtcagtcccccgtttcggtaccctggaatttactaagaataatgaaacagcccccaaggacgtacgtttacaaatgatagaccagatcgcctagcttattccgacgcatgttgcatagaattgaaccaacggaatgtgagagtaactagatgagccgaccacagcacccgtttgcgtcgcagaatacgcctgatagttcggccacgaaatcatatgtcctttgagtattaagtatttgtaatgatcaatcgagctcaagcaagcttacacttcctcggatattcagggaacttagtgcctttgaaagatacgttgatcaacgaaaaattgataatggctcatatggaatgcctacctcatagtgctgaattaacacagcactgcggacctaacttttcgaggtttcaagttcacgtctcaaaacctaataggctggaatatgtagggatcctcggtgaatttgtgattgggtttgttgtagtactgaccaagtgaatattctttttttctaaaagcagatctgctgccgggcactacgaaggagatctctgtgtatcattattgcttcttgacatgatgactcttaaatcactgtgggtgtgcaaaacgatagcacaacccaattcgatagtacatattgttgatacttcgcactaaaccgttcatatttaaaggttgtgctccttccttcgttaaatactggtgacttggtcctatctactattagctagacctctggggaaccacgcccccgtaaaacctgtgcaagagagggggtcatacatcttagacatcgcgcctccaccagggaagcattgggtgattgaccaggtgtgtaacaaatatgattattcttatactaatattagcaaagatgcataatgatttgtattaaatgtataattgaattgataagggtcttttagtcagtgatagagtagtataaggtagacattagaactcttaaccggacgcagatttttcggtcttagtaagccaattagtcgacaaaacaaggtaagagcggttactagtagtacctataatgcactgaatcttcggtcgaagtatagttctaatgctatgcagattgtgacggcgacaaatgttcagacttatatcatgaaacaagctcttgtaagtattgacaaatgaaaagattgaatatttttaaatacaaaatgcgcctacttattaggggaattaaccagattgaaggccaatcctcacatgtaatgagataatagacgataaatgaaattcttgtaatagttgaactgctacgtgatgggtattatatatgattgagatcctccaattgccgacgtcttgtcttgatgcccaaaagattgtcaacgaggagctccctcgcgtacctgtcgtccgtatcataaacgacgcgacatgtacagcactccgaagtataagcaataataatgcgggtaatccagactagatcttttcggactcaatgcggtttcacggtaaacatgattaataccggagagtagtcgagcttatcagcgatgcaagcgaattcattgtgccaggagatacgttgcagataaaaccggcaacgtatgtcaacaagttttggcgatctcgttgtttgtattcgacgaggcgcgggaacttcaagaactatcgtatattcaagtccattaccttttagtttcagactggtggagctgactaaagttatatcatcattttgtacactggtttagttaacgataatttcagatttaacatgaccagacgataatcgctgtatatccagttggaatgtggtttgccagaaaggttaacttataatcaagcctctcttcagtcttgattcgtcgtatcccatccattgcgctatacctcagtgtatttggagctgtagttataccgtgtgctaagatcagtagacatgacgagagcaatattatctaccttacaagcatcaacggacgtctagtcggaacaaaagactctaaaactcgaacttcaggttaatatactatagttctgtattcagcagttattcttatattcgatattatcttgcctattggatgtctgactttagtatattaatcatagtatctgccatgtaaaggtgccagtactaaatctgtttcacagtgcgaattataaacggttacaaccattaaagacaacaagaccctatagctttatttgaattttgtcaatgcgcaacttggagctcgcgatacatcccaattagtctatagggtcgggacgattctacggcatttctggttataatgacaacatggattgtggcccgagaatcgctctttcattaattaagcaatcattacagtcttataagcgctacttccgagtggtagcaggtaactcgatataaggtcgcatgagccgaatagcttaaaaaacaggccaccgaacattgatagagaataccgaccacagcgcaacctttgattactttcattaaattgtacggctcactcgacatcaagcttaagattgcgataatgtgaactcaaatggatcagtactgaagaaccgtaacccacttcgcagaaagcgtacccagagaagatacgctgttacaatatacagggtgaaattattgcctgttcttcgtaaccatttcgccaaacttggttagaaatgatagccattcatgatagaaataagctgaatgataccagtatctttaactatgtagtcagggggaagataacgatggtccatgtatgtttctgatatgtgacagtattggccgcgtaatttgctaacgaagctacttaatgcctttgagcttcatatagatttctttaatcaaaatcggcaaaaagatagtatgagctataatatatgctagtagagaactctggaccatcatctatatgaatactgattcgagcgtgcaattactttagcctgcgtactactgactctacaaaacactctgagataagtttgtagtcagtaagtcgctctctataaaccttttggatgaccattgtacagccacttatagatcccaataaatagcacaggagacagagtttttcaatgctcgatcatttgccgatagtattttcgtctaacctcagggcacctattatttgatacctaacctaacggccctttcacaatggagaaatatatgacatcgggacaaacacaaatggtgggtggccaggagatatgacatggtggcgtctctaagaaacacggactccctctaggcaaactcacgtaaccaattttaatgtcaaacaaaacgctcgaaaagattttgccgtgtaatgacctggtacattgactggtcaggaatacatcactgtagttgccgtagtgtcctgttggtgttccatcaagacacatcgtataacgcaatttacgacggacatcagatcaagttatacagattatttaagtatcacgtgtgcattgggacataagggatctcacacatgccttggaacatttttgctttgtgccgctttttcgctgcactaccaatccttacttaccagtatattcaaaggtcgttaacagaatgagaaaggttagggctctaagttatcgtcgattgggatagacgagacatttgcgagcgccctccacggatacgaatctcccatatcaatgtgaactggatgctatgcagtttagttcttacgtctcctagtggtaaaaatcaaagtagcactcgcatagcagttattcagaacctaatacacaaaaccgtcaaacattttctaattctaggtatgggccgatcataggagctaaggtgaaactcataaatgttttgttagatctagcatcctaaaaagatgcatatactgagtagctggcgtgcattctctcaattgtatcctttttaactgaactagtcggtcccatttcgtgactgagatctattaaccgataagattaataacactcgcattcgtatcagctcagagtgaagtttttcaataatttgactgatatattaacttctaaaataaccctttaagcctcggatccgtttcccaatcacatcaaaaattcttattccaactatctacggattaacaacgtgcatggggatcgtagtaagaacttgttccgatcactttgagtatatcaagttgacggcccggttattattgaatagaaacattcacctgctaaattaaataccgcacatcggatacccgatttcagagggccgtcttactaagggcaggctttgttcggtttaactgagatgttcattattttacagtatgcttcaactaatatgtaacgaaggacagtggatctgtctccatagtagatcttcagtcgtgaatttcataccgctcctatttaagttcgcgttcgagttgttgatcatggcacgtgaaagcaacccctagtattctagacgaaaattttttctagttcatctgataatttgccaattcaaaaacaaccgctggtttcccggcgcattctctaaaatggaagtcgaacctagagccattatttgtcggtaacccatgagttccttcttttcagaagttaatacactgtggtcctatacagaggaaaaacagcggttatatacgatcgtggcataacaacattggatcaagatagcaatttggctacctattctaattctcactagattcggtattccactacaatatcggcagattaggattggatgaataatcggtgtttaagtccggttgcgtctccaatctcctaatttttattaatattgatcttggtgacctattgtaaataaaaacttcaagactttgaataacggtgaaaagatagaagactcatttgaaaatggatcatccacagatccaaacattagcaagacactaatccccaactagctattctgatcgcgatcgtgctgcagtactcctgtcacaatagtctgttcatgatctaattctttttgggctttgttcgatggtgattcagaatctttatccggtcgcttccctgtagctactttgtggggatattgcccggggattatagggttgagatcgtttcctaaaagtatttaaaccaagtagacttcaactaaactacatcagaacatcgtgaagacaccatacgcggtacctttatttaccgataacatttcttcaagaaataccggtaagcagcataatgaccctaaacagctcggggtatcgtcgtagttttaaattttatttaggttactgctcaaggaataaaaactaactatttaatttataataatattacaaggctcacactgattagatttgtctataagacttcgcgatcccccattaccggattgtcttaagaataaactagataaaccatgcattttctagataaggcctttagtctaattagatacaaaaaacacgatagttgcatccttaatttattgtgtcaaacctggaaccttttaattacccgcaaatcactttatgtcgagactacctctgaaatttattatctacctaccgcatgaggacttgaaccatcttgtaggagttatgtttattagctaagattcgtttatcctgtagcggtccatgtatattcaacaagcaaaaagcactcagaattgtttttagttgagtcaagactgatatataaataagtttccctagttttttcgtggtgggacgatattgaattgaatcttaaccgaagagtttcccactctgtcgcacaataatacacgccaatatttccagccctgcttatgccttaatcggttactcaatctcccattgaagttcattttgatctgcatagaagtttcgggcccagccttttttctgccaccttcctccaagctctgtagacgcactctaagattgatgctcacatgtattaattctacattaacataaatatataagtcatgcatcttcgagtaaaatatctggttctccaacatgtcctggcacgtatcgttataatgcccatacatgtagtattaaaatgattgggttaactggatattaagatcatcgaaattgtaaagtcaaattaacaatactgtctcaagaccgtgtattcctcgtgctcggaagggctattacgcttacttccgttttggtatcttaatatgactttcaaaaattaagttgcagtgagtcctacctgcgtgcatcggttagcaagagtataaaagttgtttaaacgaactacttgctttacaataccggtcgtatatatcgccgtgaatccagaagattgtcttctttggattatcaaccgagatcctgtggaccgatgttttgggaccttcacagaggactccaggtagagctcgcttttgcattaatctaagaattgtacctctctaaaagatctaaaacagtgaatgtgtatttcatggaaaaacacagagaaacgtaaattactttaggccgaaaggcacatgagttattatacatatacgagatggtggtatacatcgaattcggggcatacactatagttgcattgtatttagctgctttaaataatatgatattaccttccttacataagacattaccggcataccctggttttcaacttgtggggctttttgacgatcgcactctcatttgatccgagtagggcggtgacccctgcttttcaaatacaaaaatttcgctatgaaggtaatagattacttttcgctgttatgatagaaacggtaaatttaaaattgaaacttctagaaaagtaaagtaacgagaaatgattttgtgaataatgcggtcatgattgcgcaagtaagaaaaaaaggcaaaaggatgcgcggaatagaaacttatcagtcacgggtatcttgatttcattcttcttgtcaattgccgacataggatgaaatcagattccaatgcaatacacagtaacccccacccttgattgtaatgtcgatttgaagttgtacgcgtcgacgaagtggatagtatacgggccttttgtacggtgcgatcaactatgaatctcggcgagttagatggtcgtacaatctcacacatagaggtcacttgcctgtaatgacgaattttcggctaggtactcgaactttattagaagtaaaaatgtgggcaaaagaaggattccattttacaagacgattacaatgagttacatgtctctcaacgtagtctttccctagtagtctttgaactatttaggtactccagaaaattttagcaaagggtttctgtgtgaatccgccattcatgtttatgatggaacaataagaataacgccctcgtatgttatcgacagtgaagtcagcagttcggccaaaaacatattcaatttagtacagatccccagaagttaagctaagtgctctaaaatggcctaaacggttatcaaagtaggtctaattactatactaacgggtgcatcgtaataactgctgtcgatgcaacactatatgatagtgtcgttttgctatatatgtacaatgtgacaaagaagccttagcgattcttgcaaacttaggacttcggattctcaatcttaaatgtccgaaaacgcaaagattcaaaaatttaatctatgagcagatatgcctgatggtgactacgcgtatgttaaggctaaatgttgacaaccgcacacataatcgaactattgatagtcgggagcataaccaggtgaacgtactttgttcacgacatttattgacatgttctaaatacgtctcaaaatcacggcgcactagaaaacgcaatcaaatcattgtcctggtttaagggccgtaatgccggtagtgtcaaacttcatgagaactttagctggcttttggccagtatttagggaccaagagcactagccttaagctgaatattttgccatttatctactgttataactttaaaacttggtggcaccagacttgtcgatacacacgcatcaatctgtaacgtaaaaggtttactaagaacaagcgtaggaattgagtttatattatatttaaactaaaagatgatattagcttctgagggcgatagggctccaaatcataaagaggaatatattattacacgattagaaacccacaacatacctcgaatcgcccaaaagtttgacgaaacttggcagtactccacatctcagtaatacagttgggagagtctcaaatgttgttttattactcaatgaaccaccctcataatttcactgctgttccattaaatttgcaaacgatcatttgctttgaagaaacgtaaaatcgacaaaattacagataagtagatgcataataaaaaaaactgctcgctataacacgatcatcgtgcattcttacttaggagcatcacccgcacaataacgtaccttaaactacaacactattagaccgagtactgtaattcacgaaagctcaagctcgcattgtaaagaacttgctctctcgtaaaatgtgataatagtttgcggagaggattcaattattttccattgcacctactccactagattcgataaaagaaggtggtcctcccttaaaaagaaatgttaagtaacatcggaaccataagcaaagcatgtaagtgaaccgtcatccttccctaagaaacataaaggtttttaataatgtcgactgtgaactataactgcatcctttcctgacctactccggttccttgttgttatttctgaacgagaccagtagataaacaatgtaaaccacagtgggtaccaatggtgcatgtgacgctaccgttgttttaagtgcccgtacaaacataagaagtcataatcttacttgaaattaattttgccttttattttttttcaggctcgaaattaatgatttgttttttttgaccttctagttacgctaatatgcggtcgcctgtggtttctattgagtcctataacgggatgggatctaatacgtttggttactagtaaacaaggtataaatttgataccggagtatcaactgtataacatcaagctttatgactcatacgcgaagtaatgacacaaggctttcaggagatcgcgagtacagagccactaaggggtgtattacgatagtgacaccaccgagcgcactcactccccaagtagatttatgatcctacgctaagtattagatatataaccaaagaggttctagtcagtgcaactcttagaataataattagccggttttgcctttttaggcctaatgcaatattcagctagcccttatgtatctcgcgttccacagcaccactcatggcacgcgtttaaactaatcaaatataatctatgaatgttatgccagtacttgaataaatcaggttttttataagtccttgcatactctcgttatatactgttagagtcttaccccatagaaattctttcatctgcaaacttagaagaattctcagctacggggagcataaagtccccaggatgttgacaaatacaacaaatgtggcttatacaaacactccatatgaaaatcgaaccctcgtggtagttttagccgaaccttgtacggataaatccctccattttccaatagcagatacctatcctactacctcgtggtattaaattaaagcttgaaatatagagctgcatagcttatccaattcccaagcacgagtctaccgtcgtaaccacgatttgatttacagacgctagagcaaacccatctttaaacatataagtaaaaattaaagggtgagtgcgtacgtgtttactagcaacttcgcttattaagacaattgtttataagccataattaaaaacatatgttcaacaggttcattgatatttgtaattgcacaggtttttaataaggatctacgtaagtataatgaacaaactttttaccagagttatattctgtactttgaaaatgctcctctaccgccttagagactttcaattagattttttgcagttaatctatgcgtaagtgaaccatgcaagggatgcgattcaaccgcctcgtgctaaccctatcgtctgtctcataactgtaggtctaatataattttcagttttcgaacacataaccctttgaaaatctgctatttaatgtctcacctgcatgcactatcttctatactgctcagaacggctatacgtcactatgctccaagtgacgatttaaacgaagcaaggaataataggtttattttagtgcaaaacaattaagtgcggactacgtgctctttacaataagccttgtgattgggctataggttaagtcccatattaacgatctccaatgtacaaaatcgacaatcgctttgcattacccggttactagtcgaattacagatagctgttagatactcactctaattttggacaacaatcccaatcttggggtcgtctatcgcctgaagctcgtaaatccttccatcttaaacgattacatattatagacttgttcggggtagagatatcacagttgtgcaaacattgtaaatcgatactagtttatgttggtagtctagttgcttttaccattccccgaaaaacttgatctactatttcgacaacagtaaacttgaactaggtaagtgaaaacagagaatgcctcatagtgccactatttgtccactatatgtaagtgtagctttacataatccactatgactgagatcattacggcctaggaaagcagcgtagaaaaaaagggcccggatattacgactgtaactataaaactagttactggtagcgcgccatgtatagatttgttttaccggttgtggttgcgttaacgaatttcagccgcgaaaattgatccgttaaccagtccatctcgacttctataaaacgataaagtaaagttgatgttcagcctccttcttatggttgcatcgagagtacactactcagtgggaaatagatcggggttcctacttcagattgtattatctaggcaattgccgattgtgccatacctggataaaataagctacctacatgtgatgcttatctattatcgtcatactaccttagggtgtcctgttgaacgctacattaatctttagccgtttgagatgttccaatggataggagtctaacgcatgatgaagtttaggaaggcagagcatcccactaagtatgtgacagtgtatttcgaaacgagacgttataaatagaaaaaaggtccttctggttctattctgctgaactattgaatggaaagattggttgacctacgtactatttgcttgaagtcatcaatttgacggggtgagagacatatggtgcatactttacggactctatattttagatcagaagcttagcagtcttctctacaccccctcacgacataattgcttttaagaatctatgtttgattcctctacgggaattcggatccgttcgcatgtgcggtttatctaaaccaggggacatatgttcagctaaagcatacgaacactttgctaactagacgtatgtatagtagctataaatcccgacgatatttacaaaaagaaatgagactcaaatatatacatagcgaccctacacttattcgcaccctgatctaggcgatcctagcacccacacccgaaagtgagcactagtgtcttccgtattaaatttactgcagttgagattttagttgtctactaaggattactctaacccgtaataaggatcaagactcggtactagctttactatcattccctatgtgttttcctaactcacaagggtacgtaccagcctatgtaattacaataatgataaagacacaaaggaagtaactttacaaatgagtctccagttacactagcttagtccctcccatcttgctttgaagtctaaatacgcaatctctgaggatatacagcagaagaacactcataacgttggagtccaagaattagactcatagggcccccaacatttaatatgtactgtgagtttgaaggtgttctattgttaattcctgctcttgatacatgacacgtactccgtgtttaaggcttcggactgactttctttcataagttgagcaacgaaaatttcagaatcgataagttggattcactaactaatacggctgattgaaaactccactccggacctatatggtcgacctttatacgtaaccgatataaaacttataggctggtatatcgagccttcctagcgcaatttcggatggggtttcttctactactcaacaacggaatagtctttgtttagtaaaccagagctcaggacgcccaatacgtaggagagcgctgtggagcatgtgtcattatggactggagcactcttaaatcactctgcgtgtgctaaacgatagatcataacatgtcctgagtaaattttcttgatacgtcgcaatataccgttattagttaaacgttctcatccgtcatgcgtgaaatacggctgtcgtgctcagatatactattagcgactcatctcgcctaacacgcacacgtataaactcggaatgactgccgctcttacatattagaaatacagactacaccacggaagcattgggtcattctcaaccgctgtataaaagatgattagtcttataataagattaccaaagaggcagaatcatgggtagtaaatctattattcaagtgattaccgtcgtgtaggcagggagtgaggacgagatggtactcaggacaaatattaaccggacgaagtggtttacgtcgtactttcactattagtagtaaatacaaggtaacaccggggaatagtactaaatataatgatatctatcttcgggagaacgagtcgtctattgctttgaacattctcaaggcgtaaaatgtgctgacttatagcatgatacaaccgattgttacttttgtctattcaaaagattgaatagttttttatacaaaagccgcatacttatgacggctagtatacagtttcatcccctagcatcaatgctatggacagtattgaacttataggaaattcttctaatagggcaaatccgtcgtgatgcctattttttttcagtcacatcctcaaatggcactagtattgtcgggatcccattaacaggctcaaccacgagctcacgcgaggacatgtagtccgtatctttaacgaagcgacagcgacagaactcccatggataaccaattataaggcccgtaatcctctagacatcgtttaccaataaatccgctttctccgtaatcatgttgaataccccagagtagtccagatgataaccgatgaaacacaagtctttctcaatgcacttacggtgaacttattaccgccaacgtagctcatcaaggttgcgacatctagttgtgtgtttgcgacgagcccagcgaacttcatcaactttcgtatattcaacgccttgtaattttactttaagacgcctggtgatgtagattcttagataatcagtttgttatcggctgtactttaccataatttcacaggtttcaggtcaagaagattatagctgtatatacagttccatgctcggtgcacagaaacgtgatcggataataatcaatcgcttatgtcgtctttaggcgtatccaatacatgccccgataccgcagtgtatttcgacatgtaggtataccgtcgcatttgagctcgagtcaggacgtcagctagattagattccttaatagaatataccgacctctagtccgaactaaactatagataacgccaacttcaggttaattgtctagtcgtctgtttgcagatgggattcttagatgagtgagtatcggccatattggttcgagcactttagtttttgatgcataggatatgcaatgtatagctgaaagtactttatctgtttcaaactcacattgattaaaccggtaaacctttaaagactacaagaaaatattcagtgagggcaattttgtcaatcacaatcttccagctagagatacttcacaatttgtcttgaggctacgcaacattagacggattttcgcgttttattgaaataatcgaggggcccaagagtatccatagttcattttgtaagatttctttacaggcttattacagcttcttcagactcctacatgcttacgagttatatgctagcatgtgaacaatagattaatatacaggaaaacgtacattgagagagatgaccctacacagcgcaaccgttgagtactttcattaaagggtaacgctctcgagacagcatccttaagatggccttattgtcaaatcatttgcagaagtacgcaagatccctaaccaacgtagaagaatccctacaaacacatgagacgcggtgaaaatagacagggtgttagtattcaatcttcggagtatcaatttcgccaatcttggtgagaaagcataccctttcttcagagaaagaagatcaatcataacactatctttaacgaggtacgcacgcgcatcattacctgcctccatggatctttaggatagcggaaagtattggcagcgtattgtgatttcgttcctactttatcaatttcacattcatatacatgtcttttatcaaaatcgccaataagataggatgagctatattagatgctagtagagttcgcgccaacatcatcgataggaatactcaggacagcgtgataggacttttcaatccctaatactctctataattataactctctcttaagtttggaggcagtaacgcgctctatataatcagtttgctgcaccattcttcagcctctgatacatacaaataaattccacagcagtaagagggtttaattgagacatcttgggaacttaggattttactctaacatcaccgaaacgattattggataccgtacctaaacgaactttctcaaggcagtaatataggacatccgcaataacacaaatgctgcctccccaggagttatgtcttcctggaggctatatcttacacccactcactataggcaaactaaagtttaaatgttgattgtctaaaaaaaagatagataagagttggccggcgtagcacatgcgaaagtgaatcgtaagctataattctctggacttgaagttctgtcctgttcctctgcaagaaacaaacttcctttaaagctatttacgacgcacatctcagcaagttataaacatgttggaagtttctagtcggaattcccaaagaacggatctatctaatgcattcctacatttttcctgtctgccgatggtgccatcctattcaaagaatttcttaaaagtagattaaatgggacttttaacaatgagtaaccttacgcctctaagggttcctcgagtgccatacaccagtcaggtccgagccacatacacggagaacattctaacatagcattctcaactcgatcatttgcaggttacttctttcctatcctagtgctaaaaatcatacttgcaatcccatagcacggattaagaacctaagaaacaattcagtaaaacatgttcgaattcttggtatgggaacatcattgcagctatggtctaacgcattaatgtttgggtacatcttccatcatataaacaggaagagtctgacgacagggagtgcttgcgatcatgtctatcattgtgaaatcaaattgtagctcacatgtcgtctatgagagcgtgtatccgataagatttagaaaaatagaagtcgtataagatctcactgaacttttgaatgaatgtgaagcatatatgatctgctttaataaaactttatccataggatacgtttccaaatcaattcaataattattagtcaaaatagataaggatgaacaacctgaaggccgatcggacgtagaaagtggtcccatcactttgagttgatattgttgaaccacacgttattatggttttcaaacagtctcaggatattgtatatacagataatccgataccagttgtctgacgcccctcttacgtaccccaccctttgtgacgtttaaagcagttgttcagtattttaaactaggcggcaactaatttggaaagaagcacagtggatatgtctaaattcttgttattcaggcctgaatttaatacaccgcatagttaacttcgcggtagagttgttcatcatgcctcctctaagctaccacttctatgatacaccaatagttgttctacggaatctgataattggccaagtcataaacttccgctgcgttcaacccccttgctcgaatatccaactcgaaaagacagccttttggtgtccggaacaaatcagttacttcttttctgatgttaattctctgtggtcagatacagaccaaaaactccgcggatttaccatcctccaagaacaaatttgcatcaacatagcattttggctacatattctaagtctcaatagtttaggttttcaactacattatcccaacattaggattggaggaataatagctgggtaagtccccttgcgtctacaatcgactattttttatgaatatgcttctgccgcacctatggttattaaaaaagtcatgactttgaagaaccctgaaaagatagatgaatcaggtgtaatggcagcagccaaagagcatataattagcaacactctaagaacattatagatatgatgatagcgatcgtcatgatgttatccggtcacaatagtagcttcatcagctaattcgttttgccagtggtgacttgcgctggaagaatcgttatacggtcccttccctcttgatacggtgggggcttattcaaccgcgtggattgggttgtcatacttgcattaaacgatgtaaaccatctagtagtcaactatactaaatcacaaaatagtgatcaatacatacccgcttcatggttttaaccatttaattgattaaagatattccgctaagaaccattatctacctaaactgatcgccgtatcctagtagtttgaaatttgatgtaccgtaatgatcaacgaagtaaaacgttatattgtatgtagaataataggtcttggagctaaatgatgtgattggtagtgaagacttacccttacaactttaccggtttctcggaagaatatactagagaatcaatgcatgggctacataagcactttagtctaatgagataaaaaatacacgagtcttccatcatgaattttttgtcgaaaaactcgaacctggtaatttaaaccatatatctttatgtcgtcaataactctcatatgttttatataacttcccaatcacgacttgtaactgcttgttcgactgagctgtttgagctatgaggccgggatccggttgagctacatctatttgctacaagaaaaatgaaagcacatttgttgggagttctggctacactcatagagaaataagtggcccgagtgggtgcggcctgcctccatattcaagtgtatcttaaaccaagtggttccaacgctcgcgctaaagaattaaagcctttatttcctccacggagtagcccgtaatccggttcgaaagagaccattgaagttaattttcatatccagtgaagtttaggcacaagcatgtgttctgccacatgcctcaaagcgctcttcaaccaagatatgattcatcctaacttcgatgaatgcgtctgtaacataaatatagaaggaatgattcggcgagttaattttcgccttctccaacatggcatccctacgttcgttataaggaccatacatgtaggttttaaaggtttgcggttaatcgatatttacatcatagaaattctatagtcaaatttacaagactctagatactcactcgttgcagccggctaggaagcgctttgtaccttacttcccttttcgttgcgtaatatgaatttcatatagtaagttcaaggcactcatacctccgtgaagagggtagatagactattaaagttgtttaatagtacgtattgatggaaatgacccgtaggagatttaccactcaatccacaagattcgctgctgtgcattatcaaaacagtgcatgtcgaaacatgggttgggtccttcaaacacgaatccaggtagagatacctttgcaattttt\";\n\
\n\
if (dnaOutputString != expectedDNAOutputString)\n\
    throw \"ERROR: bad result: expected \" + JSON.stringify(expectedDNAOutputString) + \" but got \" + JSON.stringify(dnaOutputString);\n\
\n\
if (dnaInput != expectedDNAInput)\n\
    throw \"ERROR: bad result: expected \" + JSON.stringify(expectedDNAInput) + \" but got \" + JSON.stringify(dnaInput);\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider string-base64</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>string-base64</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"string-base64 failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
/* ***** BEGIN LICENSE BLOCK *****\n\
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1\n\
 *\n\
 * The contents of this file are subject to the Mozilla Public License Version\n\
 * 1.1 (the \"License\"); you may not use this file except in compliance with\n\
 * the License. You may obtain a copy of the License at\n\
 * http://www.mozilla.org/MPL/\n\
 *\n\
 * Software distributed under the License is distributed on an \"AS IS\" basis,\n\
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License\n\
 * for the specific language governing rights and limitations under the\n\
 * License.\n\
 *\n\
 * The Original Code is Mozilla XML-RPC Client component.\n\
 *\n\
 * The Initial Developer of the Original Code is\n\
 * Digital Creations 2, Inc.\n\
 * Portions created by the Initial Developer are Copyright (C) 2000\n\
 * the Initial Developer. All Rights Reserved.\n\
 *\n\
 * Contributor(s):\n\
 *   Martijn Pieters <mj@digicool.com> (original author)\n\
 *   Samuel Sieb <samuel@sieb.net>\n\
 *\n\
 * Alternatively, the contents of this file may be used under the terms of\n\
 * either the GNU General Public License Version 2 or later (the \"GPL\"), or\n\
 * the GNU Lesser General Public License Version 2.1 or later (the \"LGPL\"),\n\
 * in which case the provisions of the GPL or the LGPL are applicable instead\n\
 * of those above. If you wish to allow use of your version of this file only\n\
 * under the terms of either the GPL or the LGPL, and not to allow others to\n\
 * use your version of this file under the terms of the MPL, indicate your\n\
 * decision by deleting the provisions above and replace them with the notice\n\
 * and other provisions required by the GPL or the LGPL. If you do not delete\n\
 * the provisions above, a recipient may use your version of this file under\n\
 * the terms of any one of the MPL, the GPL or the LGPL.\n\
 *\n\
 * ***** END LICENSE BLOCK ***** */\n\
\n\
// From: http://lxr.mozilla.org/mozilla/source/extensions/xml-rpc/src/nsXmlRpcClient.js#956\n\
\n\
/* Convert data (an array of integers) to a Base64 string. */\n\
var toBase64Table = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';\n\
var base64Pad = '=';\n\
\n\
function toBase64(data) {\n\
    var result = '';\n\
    var length = data.length;\n\
    var i;\n\
    // Convert every three bytes to 4 ascii characters.\n\
    for (i = 0; i < (length - 2); i += 3) {\n\
        result += toBase64Table[data.charCodeAt(i) >> 2];\n\
        result += toBase64Table[((data.charCodeAt(i) & 0x03) << 4) + (data.charCodeAt(i+1) >> 4)];\n\
        result += toBase64Table[((data.charCodeAt(i+1) & 0x0f) << 2) + (data.charCodeAt(i+2) >> 6)];\n\
        result += toBase64Table[data.charCodeAt(i+2) & 0x3f];\n\
    }\n\
\n\
    // Convert the remaining 1 or 2 bytes, pad out to 4 characters.\n\
    if (length%3) {\n\
        i = length - (length%3);\n\
        result += toBase64Table[data.charCodeAt(i) >> 2];\n\
        if ((length%3) == 2) {\n\
            result += toBase64Table[((data.charCodeAt(i) & 0x03) << 4) + (data.charCodeAt(i+1) >> 4)];\n\
            result += toBase64Table[(data.charCodeAt(i+1) & 0x0f) << 2];\n\
            result += base64Pad;\n\
        } else {\n\
            result += toBase64Table[(data.charCodeAt(i) & 0x03) << 4];\n\
            result += base64Pad + base64Pad;\n\
        }\n\
    }\n\
\n\
    return result;\n\
}\n\
\n\
/* Convert Base64 data to a string */\n\
var toBinaryTable = [\n\
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,\n\
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,\n\
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,\n\
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1,\n\
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,\n\
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,\n\
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,\n\
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1\n\
];\n\
\n\
function base64ToString(data) {\n\
    var result = '';\n\
    var leftbits = 0; // number of bits decoded, but yet to be appended\n\
    var leftdata = 0; // bits decoded, but yet to be appended\n\
\n\
    // Convert one by one.\n\
    for (var i = 0; i < data.length; i++) {\n\
        var c = toBinaryTable[data.charCodeAt(i) & 0x7f];\n\
        var padding = (data.charCodeAt(i) == base64Pad.charCodeAt(0));\n\
        // Skip illegal characters and whitespace\n\
        if (c == -1) continue;\n\
        \n\
        // Collect data into leftdata, update bitcount\n\
        leftdata = (leftdata << 6) | c;\n\
        leftbits += 6;\n\
\n\
        // If we have 8 or more bits, append 8 bits to the result\n\
        if (leftbits >= 8) {\n\
            leftbits -= 8;\n\
            // Append if not padding.\n\
            if (!padding)\n\
                result += String.fromCharCode((leftdata >> leftbits) & 0xff);\n\
            leftdata &= (1 << leftbits) - 1;\n\
        }\n\
    }\n\
\n\
    // If there are any bits left, the base64 string was corrupted\n\
    if (leftbits)\n\
        throw Components.Exception('Corrupted base64 string');\n\
\n\
    return result;\n\
}\n\
\n\
var str = \"\";\n\
\n\
for ( var i = 0; i < 8192; i++ )\n\
        str += String.fromCharCode( (25 * Math.random()) + 97 );\n\
\n\
for ( var i = 8192; i <= 16384; i *= 2 ) {\n\
\n\
    var base64;\n\
\n\
    base64 = toBase64(str);\n\
    var encoded = base64ToString(base64);\n\
    if (encoded != str)\n\
        throw \"ERROR: bad result: expected \" + str + \" but got \" + encoded;\n\
\n\
    // Double the string\n\
    str += str;\n\
}\n\
\n\
toBinaryTable = null;\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider string-fasta</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>string-fasta</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"string-fasta failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// The Great Computer Language Shootout\n\
//  http://shootout.alioth.debian.org\n\
//\n\
//  Contributed by Ian Osgood\n\
\n\
var last = 42, A = 3877, C = 29573, M = 139968;\n\
\n\
function rand(max) {\n\
  last = (last * A + C) % M;\n\
  return max * last / M;\n\
}\n\
\n\
var ALU =\n\
  \"GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG\" +\n\
  \"GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA\" +\n\
  \"CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAAT\" +\n\
  \"ACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA\" +\n\
  \"GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGG\" +\n\
  \"AGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC\" +\n\
  \"AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA\";\n\
\n\
var IUB = {\n\
  a:0.27, c:0.12, g:0.12, t:0.27,\n\
  B:0.02, D:0.02, H:0.02, K:0.02,\n\
  M:0.02, N:0.02, R:0.02, S:0.02,\n\
  V:0.02, W:0.02, Y:0.02\n\
}\n\
\n\
var HomoSap = {\n\
  a: 0.3029549426680,\n\
  c: 0.1979883004921,\n\
  g: 0.1975473066391,\n\
  t: 0.3015094502008\n\
}\n\
\n\
function makeCumulative(table) {\n\
  var last = null;\n\
  for (var c in table) {\n\
    if (last) table[c] += table[last];\n\
    last = c;\n\
  }\n\
}\n\
\n\
function fastaRepeat(n, seq) {\n\
  var seqi = 0, lenOut = 60;\n\
  while (n>0) {\n\
    if (n<lenOut) lenOut = n;\n\
    if (seqi + lenOut < seq.length) {\n\
      ret += seq.substring(seqi, seqi+lenOut).length;\n\
      seqi += lenOut;\n\
    } else {\n\
      var s = seq.substring(seqi);\n\
      seqi = lenOut - s.length;\n\
      ret += (s + seq.substring(0, seqi)).length;\n\
    }\n\
    n -= lenOut;\n\
  }\n\
}\n\
\n\
function fastaRandom(n, table) {\n\
  var line = new Array(60);\n\
  makeCumulative(table);\n\
  while (n>0) {\n\
    if (n<line.length) line = new Array(n);\n\
    for (var i=0; i<line.length; i++) {\n\
      var r = rand(1);\n\
      for (var c in table) {\n\
        if (r < table[c]) {\n\
          line[i] = c;\n\
          break;\n\
        }\n\
      }\n\
    }\n\
    ret += line.join('').length;\n\
    n -= line.length;\n\
  }\n\
}\n\
\n\
var ret = 0;\n\
\n\
var count = 7;\n\
fastaRepeat(2*count*100000, ALU);\n\
fastaRandom(3*count*1000, IUB);\n\
fastaRandom(5*count*1000, HomoSap);\n\
\n\
var expected = 1456000;\n\
\n\
if (ret != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + ret;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider string-tagcloud</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>string-tagcloud</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"string-tagcloud failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
\n\
/*\n\
 * Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
 *\n\
 * Redistribution and use in source and binary forms, with or without\n\
 * modification, are permitted provided that the following conditions\n\
 * are met:\n\
 * 1. Redistributions of source code must retain the above copyright\n\
 *    notice, this list of conditions and the following disclaimer.\n\
 * 2. Redistributions in binary form must reproduce the above copyright\n\
 *    notice, this list of conditions and the following disclaimer in the\n\
 *    documentation and/or other materials provided with the distribution.\n\
 *\n\
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
 */\n\
\n\
/*\n\
    Portions from:\n\
    json.js\n\
    2007-10-10\n\
\n\
    Public Domain\n\
*/\n\
\n\
// This test parses a JSON string giving tag names and popularity, and\n\
// generates html markup for a \"tagcloud\" view.\n\
\n\
if (!Object.prototype.toJSONString) {\n\
\n\
    Array.prototype.toJSONString = function (w) {\n\
        var a = [],     // The array holding the partial texts.\n\
            i,          // Loop counter.\n\
            l = this.length,\n\
            v;          // The value to be stringified.\n\
\n\
        for (i = 0; i < l; i += 1) {\n\
            v = this[i];\n\
            switch (typeof v) {\n\
            case 'object':\n\
\n\
                if (v && typeof v.toJSONString === 'function') {\n\
                    a.push(v.toJSONString(w));\n\
                } else {\n\
                    a.push('null');\n\
                }\n\
                break;\n\
\n\
            case 'string':\n\
            case 'number':\n\
            case 'boolean':\n\
                a.push(v.toJSONString());\n\
                break;\n\
            default:\n\
                a.push('null');\n\
            }\n\
        }\n\
\n\
        return '[' + a.join(',') + ']';\n\
    };\n\
\n\
\n\
    Boolean.prototype.toJSONString = function () {\n\
        return String(this);\n\
    };\n\
\n\
\n\
    Date.prototype.toJSONString = function () {\n\
\n\
        function f(n) {\n\
\n\
            return n < 10 ? '0' + n : n;\n\
        }\n\
\n\
        return '\"' + this.getUTCFullYear()   + '-' +\n\
                   f(this.getUTCMonth() + 1) + '-' +\n\
                   f(this.getUTCDate())      + 'T' +\n\
                   f(this.getUTCHours())     + ':' +\n\
                   f(this.getUTCMinutes())   + ':' +\n\
                   f(this.getUTCSeconds())   + 'Z\"';\n\
    };\n\
\n\
\n\
    Number.prototype.toJSONString = function () {\n\
\n\
        return isFinite(this) ? String(this) : 'null';\n\
    };\n\
\n\
\n\
    Object.prototype.toJSONString = function (w) {\n\
        var a = [],     // The array holding the partial texts.\n\
            k,          // The current key.\n\
            i,          // The loop counter.\n\
            v;          // The current value.\n\
\n\
        if (w) {\n\
            for (i = 0; i < w.length; i += 1) {\n\
                k = w[i];\n\
                if (typeof k === 'string') {\n\
                    v = this[k];\n\
                    switch (typeof v) {\n\
                    case 'object':\n\
\n\
                        if (v) {\n\
                            if (typeof v.toJSONString === 'function') {\n\
                                a.push(k.toJSONString() + ':' +\n\
                                       v.toJSONString(w));\n\
                            }\n\
                        } else {\n\
                            a.push(k.toJSONString() + ':null');\n\
                        }\n\
                        break;\n\
\n\
                    case 'string':\n\
                    case 'number':\n\
                    case 'boolean':\n\
                        a.push(k.toJSONString() + ':' + v.toJSONString());\n\
\n\
                    }\n\
                }\n\
            }\n\
        } else {\n\
\n\
            for (k in this) {\n\
                if (typeof k === 'string' &&\n\
                        Object.prototype.hasOwnProperty.apply(this, [k])) {\n\
                    v = this[k];\n\
                    switch (typeof v) {\n\
                    case 'object':\n\
\n\
                        if (v) {\n\
                            if (typeof v.toJSONString === 'function') {\n\
                                a.push(k.toJSONString() + ':' +\n\
                                       v.toJSONString());\n\
                            }\n\
                        } else {\n\
                            a.push(k.toJSONString() + ':null');\n\
                        }\n\
                        break;\n\
\n\
                    case 'string':\n\
                    case 'number':\n\
                    case 'boolean':\n\
                        a.push(k.toJSONString() + ':' + v.toJSONString());\n\
\n\
                    }\n\
                }\n\
            }\n\
        }\n\
\n\
        return '{' + a.join(',') + '}';\n\
    };\n\
\n\
\n\
    (function (s) {\n\
\n\
        var m = {\n\
            '\\b': '\\\\b',\n\
            '\\t': '\\\\t',\n\
            '\\n': '\\\\n',\n\
            '\\f': '\\\\f',\n\
            '\\r': '\\\\r',\n\
            '\"' : '\\\\\"',\n\
            '\\\\': '\\\\\\\\'\n\
        };\n\
\n\
\n\
        s.parseJSON = function (filter) {\n\
            var j;\n\
\n\
            function walk(k, v) {\n\
                var i, n;\n\
                if (v && typeof v === 'object') {\n\
                    for (i in v) {\n\
                        if (Object.prototype.hasOwnProperty.apply(v, [i])) {\n\
                            n = walk(i, v[i]);\n\
                            if (n !== undefined) {\n\
                                v[i] = n;\n\
                            }\n\
                        }\n\
                    }\n\
                }\n\
                return filter(k, v);\n\
            }\n\
\n\
            if (/^[\\],:{}\\s]*$/.test(this.replace(/\\\\./g, '@').\n\
                    replace(/\"[^\"\\\\\\n\\r]*\"|true|false|null|-?\\d+(?:\\.\\d*)?(:?[eE][+\\-]?\\d+)?/g, ']').\n\
                    replace(/(?:^|:|,)(?:\\s*\\[)+/g, ''))) {\n\
\n\
                j = eval('(' + this + ')');\n\
\n\
                return typeof filter === 'function' ? walk('', j) : j;\n\
            }\n\
\n\
            throw new SyntaxError('parseJSON');\n\
        };\n\
\n\
\n\
        s.toJSONString = function () {\n\
\n\
            if (/[\"\\\\\\x00-\\x1f]/.test(this)) {\n\
                return '\"' + this.replace(/[\\x00-\\x1f\\\\\"]/g, function (a) {\n\
                    var c = m[a];\n\
                    if (c) {\n\
                        return c;\n\
                    }\n\
                    c = a.charCodeAt();\n\
                    return '\\\\u00' + Math.floor(c / 16).toString(16) +\n\
                                               (c % 16).toString(16);\n\
                }) + '\"';\n\
            }\n\
            return '\"' + this + '\"';\n\
        };\n\
    })(String.prototype);\n\
}\n\
\n\
var tagInfoJSON = '[\\n  {\\n    \\\"tag\\\": \"titillation\",\\n    \\\"popularity\\\": 4294967296\\n  },\\n  {\\n    \\\"tag\\\": \"foamless\",\\n    \\\"popularity\\\": 1257718401\\n  },\\n  {\\n    \\\"tag\\\": \"snarler\",\\n    \\\"popularity\\\": 613166183\\n  },\\n  {\\n    \\\"tag\\\": \"multangularness\",\\n    \\\"popularity\\\": 368304452\\n  },\\n  {\\n    \\\"tag\\\": \"Fesapo unventurous\",\\n    \\\"popularity\\\": 248026512\\n  },\\n  {\\n    \\\"tag\\\": \"esthesioblast\",\\n    \\\"popularity\\\": 179556755\\n  },\\n  {\\n    \\\"tag\\\": \"echeneidoid\",\\n    \\\"popularity\\\": 136641578\\n  },\\n  {\\n    \\\"tag\\\": \"embryoctony\",\\n    \\\"popularity\\\": 107852576\\n  },\\n  {\\n    \\\"tag\\\": \"undilatory\",\\n    \\\"popularity\\\": 87537981\\n  },\\n  {\\n    \\\"tag\\\": \"predisregard\",\\n    \\\"popularity\\\": 72630939\\n  },\\n  {\\n    \\\"tag\\\": \"allergenic\",\\n    \\\"popularity\\\": 61345190\\n  },\\n  {\\n    \\\"tag\\\": \"uncloudy\",\\n    \\\"popularity\\\": 52580571\\n  },\\n  {\\n    \\\"tag\\\": \"unforeseeably\",\\n    \\\"popularity\\\": 45628109\\n  },\\n  {\\n    \\\"tag\\\": \"sturniform\",\\n    \\\"popularity\\\": 40013489\\n  },\\n  {\\n    \\\"tag\\\": \"anesthetize\",\\n    \\\"popularity\\\": 35409226\\n  },\\n  {\\n    \\\"tag\\\": \"ametabolia\",\\n    \\\"popularity\\\": 31583050\\n  },\\n  {\\n    \\\"tag\\\": \"angiopathy\",\\n    \\\"popularity\\\": 28366350\\n  },\\n  {\\n    \\\"tag\\\": \"sultanaship\",\\n    \\\"popularity\\\": 25634218\\n  },\\n  {\\n    \\\"tag\\\": \"Frenchwise\",\\n    \\\"popularity\\\": 23292461\\n  },\\n  {\\n    \\\"tag\\\": \"cerviconasal\",\\n    \\\"popularity\\\": 21268909\\n  },\\n  {\\n    \\\"tag\\\": \"mercurialness\",\\n    \\\"popularity\\\": 19507481\\n  },\\n  {\\n    \\\"tag\\\": \"glutelin venditate\",\\n    \\\"popularity\\\": 17964042\\n  },\\n  {\\n    \\\"tag\\\": \"acred overblack\",\\n    \\\"popularity\\\": 16603454\\n  },\\n  {\\n    \\\"tag\\\": \"Atik\",\\n    \\\"popularity\\\": 15397451\\n  },\\n  {\\n    \\\"tag\\\": \"puncturer\",\\n    \\\"popularity\\\": 14323077\\n  },\\n  {\\n    \\\"tag\\\": \"pukatea\",\\n    \\\"popularity\\\": 13361525\\n  },\\n  {\\n    \\\"tag\\\": \"suberize\",\\n    \\\"popularity\\\": 12497261\\n  },\\n  {\\n    \\\"tag\\\": \"Godfrey\",\\n    \\\"popularity\\\": 11717365\\n  },\\n  {\\n    \\\"tag\\\": \"tetraptote\",\\n    \\\"popularity\\\": 11011011\\n  },\\n  {\\n    \\\"tag\\\": \"lucidness\",\\n    \\\"popularity\\\": 10369074\\n  },\\n  {\\n    \\\"tag\\\": \"tartness\",\\n    \\\"popularity\\\": 9783815\\n  },\\n  {\\n    \\\"tag\\\": \"axfetch\",\\n    \\\"popularity\\\": 9248634\\n  },\\n  {\\n    \\\"tag\\\": \"preacquittal\",\\n    \\\"popularity\\\": 8757877\\n  },\\n  {\\n    \\\"tag\\\": \"matris\",\\n    \\\"popularity\\\": 8306671\\n  },\\n  {\\n    \\\"tag\\\": \"hyphenate\",\\n    \\\"popularity\\\": 7890801\\n  },\\n  {\\n    \\\"tag\\\": \"semifabulous\",\\n    \\\"popularity\\\": 7506606\\n  },\\n  {\\n    \\\"tag\\\": \"oppressiveness\",\\n    \\\"popularity\\\": 7150890\\n  },\\n  {\\n    \\\"tag\\\": \"Protococcales\",\\n    \\\"popularity\\\": 6820856\\n  },\\n  {\\n    \\\"tag\\\": \"unpreventive\",\\n    \\\"popularity\\\": 6514045\\n  },\\n  {\\n    \\\"tag\\\": \"Cordia\",\\n    \\\"popularity\\\": 6228289\\n  },\\n  {\\n    \\\"tag\\\": \"Wakamba leaflike\",\\n    \\\"popularity\\\": 5961668\\n  },\\n  {\\n    \\\"tag\\\": \"dacryoma\",\\n    \\\"popularity\\\": 5712480\\n  },\\n  {\\n    \\\"tag\\\": \"inguinal\",\\n    \\\"popularity\\\": 5479211\\n  },\\n  {\\n    \\\"tag\\\": \"responseless\",\\n    \\\"popularity\\\": 5260507\\n  },\\n  {\\n    \\\"tag\\\": \"supplementarily\",\\n    \\\"popularity\\\": 5055158\\n  },\\n  {\\n    \\\"tag\\\": \"emu\",\\n    \\\"popularity\\\": 4862079\\n  },\\n  {\\n    \\\"tag\\\": \"countermeet\",\\n    \\\"popularity\\\": 4680292\\n  },\\n  {\\n    \\\"tag\\\": \"purrer\",\\n    \\\"popularity\\\": 4508918\\n  },\\n  {\\n    \\\"tag\\\": \"Corallinaceae\",\\n    \\\"popularity\\\": 4347162\\n  },\\n  {\\n    \\\"tag\\\": \"speculum\",\\n    \\\"popularity\\\": 4194304\\n  },\\n  {\\n    \\\"tag\\\": \"crimpness\",\\n    \\\"popularity\\\": 4049690\\n  },\\n  {\\n    \\\"tag\\\": \"antidetonant\",\\n    \\\"popularity\\\": 3912727\\n  },\\n  {\\n    \\\"tag\\\": \"topeewallah\",\\n    \\\"popularity\\\": 3782875\\n  },\\n  {\\n    \\\"tag\\\": \"fidalgo ballant\",\\n    \\\"popularity\\\": 3659640\\n  },\\n  {\\n    \\\"tag\\\": \"utriculose\",\\n    \\\"popularity\\\": 3542572\\n  },\\n  {\\n    \\\"tag\\\": \"testata\",\\n    \\\"popularity\\\": 3431259\\n  },\\n  {\\n    \\\"tag\\\": \"beltmaking\",\\n    \\\"popularity\\\": 3325322\\n  },\\n  {\\n    \\\"tag\\\": \"necrotype\",\\n    \\\"popularity\\\": 3224413\\n  },\\n  {\\n    \\\"tag\\\": \"ovistic\",\\n    \\\"popularity\\\": 3128215\\n  },\\n  {\\n    \\\"tag\\\": \"swindlership\",\\n    \\\"popularity\\\": 3036431\\n  },\\n  {\\n    \\\"tag\\\": \"augustal\",\\n    \\\"popularity\\\": 2948792\\n  },\\n  {\\n    \\\"tag\\\": \"Titoist\",\\n    \\\"popularity\\\": 2865047\\n  },\\n  {\\n    \\\"tag\\\": \"trisoctahedral\",\\n    \\\"popularity\\\": 2784963\\n  },\\n  {\\n    \\\"tag\\\": \"sequestrator\",\\n    \\\"popularity\\\": 2708327\\n  },\\n  {\\n    \\\"tag\\\": \"sideburns\",\\n    \\\"popularity\\\": 2634939\\n  },\\n  {\\n    \\\"tag\\\": \"paraphrasia\",\\n    \\\"popularity\\\": 2564616\\n  },\\n  {\\n    \\\"tag\\\": \"graminology unbay\",\\n    \\\"popularity\\\": 2497185\\n  },\\n  {\\n    \\\"tag\\\": \"acaridomatium emargination\",\\n    \\\"popularity\\\": 2432487\\n  },\\n  {\\n    \\\"tag\\\": \"roofward\",\\n    \\\"popularity\\\": 2370373\\n  },\\n  {\\n    \\\"tag\\\": \"lauder\",\\n    \\\"popularity\\\": 2310705\\n  },\\n  {\\n    \\\"tag\\\": \"subjunctive\",\\n    \\\"popularity\\\": 2253354\\n  },\\n  {\\n    \\\"tag\\\": \"subelongate\",\\n    \\\"popularity\\\": 2198199\\n  },\\n  {\\n    \\\"tag\\\": \"guacimo\",\\n    \\\"popularity\\\": 2145128\\n  },\\n  {\\n    \\\"tag\\\": \"cockade\",\\n    \\\"popularity\\\": 2094033\\n  },\\n  {\\n    \\\"tag\\\": \"misgauge\",\\n    \\\"popularity\\\": 2044818\\n  },\\n  {\\n    \\\"tag\\\": \"unexpensive\",\\n    \\\"popularity\\\": 1997388\\n  },\\n  {\\n    \\\"tag\\\": \"chebel\",\\n    \\\"popularity\\\": 1951657\\n  },\\n  {\\n    \\\"tag\\\": \"unpursuing\",\\n    \\\"popularity\\\": 1907543\\n  },\\n  {\\n    \\\"tag\\\": \"kilobar\",\\n    \\\"popularity\\\": 1864969\\n  },\\n  {\\n    \\\"tag\\\": \"obsecration\",\\n    \\\"popularity\\\": 1823863\\n  },\\n  {\\n    \\\"tag\\\": \"nacarine\",\\n    \\\"popularity\\\": 1784157\\n  },\\n  {\\n    \\\"tag\\\": \"spirituosity\",\\n    \\\"popularity\\\": 1745787\\n  },\\n  {\\n    \\\"tag\\\": \"movableness deity\",\\n    \\\"popularity\\\": 1708692\\n  },\\n  {\\n    \\\"tag\\\": \"exostracism\",\\n    \\\"popularity\\\": 1672816\\n  },\\n  {\\n    \\\"tag\\\": \"archipterygium\",\\n    \\\"popularity\\\": 1638104\\n  },\\n  {\\n    \\\"tag\\\": \"monostrophic\",\\n    \\\"popularity\\\": 1604506\\n  },\\n  {\\n    \\\"tag\\\": \"gynecide\",\\n    \\\"popularity\\\": 1571974\\n  },\\n  {\\n    \\\"tag\\\": \"gladden\",\\n    \\\"popularity\\\": 1540462\\n  },\\n  {\\n    \\\"tag\\\": \"throughbred\",\\n    \\\"popularity\\\": 1509927\\n  },\\n  {\\n    \\\"tag\\\": \"groper\",\\n    \\\"popularity\\\": 1480329\\n  },\\n  {\\n    \\\"tag\\\": \"Xenosaurus\",\\n    \\\"popularity\\\": 1451628\\n  },\\n  {\\n    \\\"tag\\\": \"photoetcher\",\\n    \\\"popularity\\\": 1423788\\n  },\\n  {\\n    \\\"tag\\\": \"glucosid\",\\n    \\\"popularity\\\": 1396775\\n  },\\n  {\\n    \\\"tag\\\": \"Galtonian\",\\n    \\\"popularity\\\": 1370555\\n  },\\n  {\\n    \\\"tag\\\": \"mesosporic\",\\n    \\\"popularity\\\": 1345097\\n  },\\n  {\\n    \\\"tag\\\": \"theody\",\\n    \\\"popularity\\\": 1320370\\n  },\\n  {\\n    \\\"tag\\\": \"zaffer\",\\n    \\\"popularity\\\": 1296348\\n  },\\n  {\\n    \\\"tag\\\": \"probiology\",\\n    \\\"popularity\\\": 1273003\\n  },\\n  {\\n    \\\"tag\\\": \"rhizomic\",\\n    \\\"popularity\\\": 1250308\\n  },\\n  {\\n    \\\"tag\\\": \"superphosphate\",\\n    \\\"popularity\\\": 1228240\\n  },\\n  {\\n    \\\"tag\\\": \"Hippolytan\",\\n    \\\"popularity\\\": 1206776\\n  },\\n  {\\n    \\\"tag\\\": \"garget\",\\n    \\\"popularity\\\": 1185892\\n  },\\n  {\\n    \\\"tag\\\": \"diploplacula\",\\n    \\\"popularity\\\": 1165568\\n  },\\n  {\\n    \\\"tag\\\": \"orohydrographical\",\\n    \\\"popularity\\\": 1145785\\n  },\\n  {\\n    \\\"tag\\\": \"enhypostatize\",\\n    \\\"popularity\\\": 1126521\\n  },\\n  {\\n    \\\"tag\\\": \"polisman\",\\n    \\\"popularity\\\": 1107759\\n  },\\n  {\\n    \\\"tag\\\": \"acetometer\",\\n    \\\"popularity\\\": 1089482\\n  },\\n  {\\n    \\\"tag\\\": \"unsnatched\",\\n    \\\"popularity\\\": 1071672\\n  },\\n  {\\n    \\\"tag\\\": \"yabber\",\\n    \\\"popularity\\\": 1054313\\n  },\\n  {\\n    \\\"tag\\\": \"demiwolf\",\\n    \\\"popularity\\\": 1037390\\n  },\\n  {\\n    \\\"tag\\\": \"chromascope\",\\n    \\\"popularity\\\": 1020888\\n  },\\n  {\\n    \\\"tag\\\": \"seamanship\",\\n    \\\"popularity\\\": 1004794\\n  },\\n  {\\n    \\\"tag\\\": \"nonfenestrated\",\\n    \\\"popularity\\\": 989092\\n  },\\n  {\\n    \\\"tag\\\": \"hydrophytism\",\\n    \\\"popularity\\\": 973771\\n  },\\n  {\\n    \\\"tag\\\": \"dotter\",\\n    \\\"popularity\\\": 958819\\n  },\\n  {\\n    \\\"tag\\\": \"thermoperiodism\",\\n    \\\"popularity\\\": 944222\\n  },\\n  {\\n    \\\"tag\\\": \"unlawyerlike\",\\n    \\\"popularity\\\": 929970\\n  },\\n  {\\n    \\\"tag\\\": \"enantiomeride citywards\",\\n    \\\"popularity\\\": 916052\\n  },\\n  {\\n    \\\"tag\\\": \"unmetallurgical\",\\n    \\\"popularity\\\": 902456\\n  },\\n  {\\n    \\\"tag\\\": \"prickled\",\\n    \\\"popularity\\\": 889174\\n  },\\n  {\\n    \\\"tag\\\": \"strangerwise manioc\",\\n    \\\"popularity\\\": 876195\\n  },\\n  {\\n    \\\"tag\\\": \"incisorial\",\\n    \\\"popularity\\\": 863510\\n  },\\n  {\\n    \\\"tag\\\": \"irrationalize\",\\n    \\\"popularity\\\": 851110\\n  },\\n  {\\n    \\\"tag\\\": \"nasology\",\\n    \\\"popularity\\\": 838987\\n  },\\n  {\\n    \\\"tag\\\": \"fatuism\",\\n    \\\"popularity\\\": 827131\\n  },\\n  {\\n    \\\"tag\\\": \"Huk\",\\n    \\\"popularity\\\": 815535\\n  },\\n  {\\n    \\\"tag\\\": \"properispomenon\",\\n    \\\"popularity\\\": 804192\\n  },\\n  {\\n    \\\"tag\\\": \"unpummelled\",\\n    \\\"popularity\\\": 793094\\n  },\\n  {\\n    \\\"tag\\\": \"technographically\",\\n    \\\"popularity\\\": 782233\\n  },\\n  {\\n    \\\"tag\\\": \"underfurnish\",\\n    \\\"popularity\\\": 771603\\n  },\\n  {\\n    \\\"tag\\\": \"sinter\",\\n    \\\"popularity\\\": 761198\\n  },\\n  {\\n    \\\"tag\\\": \"lateroanterior\",\\n    \\\"popularity\\\": 751010\\n  },\\n  {\\n    \\\"tag\\\": \"nonpersonification\",\\n    \\\"popularity\\\": 741034\\n  },\\n  {\\n    \\\"tag\\\": \"Sitophilus\",\\n    \\\"popularity\\\": 731264\\n  },\\n  {\\n    \\\"tag\\\": \"unstudded overexerted\",\\n    \\\"popularity\\\": 721694\\n  },\\n  {\\n    \\\"tag\\\": \"tracheation\",\\n    \\\"popularity\\\": 712318\\n  },\\n  {\\n    \\\"tag\\\": \"thirteenth begloze\",\\n    \\\"popularity\\\": 703131\\n  },\\n  {\\n    \\\"tag\\\": \"bespice\",\\n    \\\"popularity\\\": 694129\\n  },\\n  {\\n    \\\"tag\\\": \"doppia\",\\n    \\\"popularity\\\": 685305\\n  },\\n  {\\n    \\\"tag\\\": \"unadorned\",\\n    \\\"popularity\\\": 676656\\n  },\\n  {\\n    \\\"tag\\\": \"dovelet engraff\",\\n    \\\"popularity\\\": 668176\\n  },\\n  {\\n    \\\"tag\\\": \"diphyozooid\",\\n    \\\"popularity\\\": 659862\\n  },\\n  {\\n    \\\"tag\\\": \"mure\",\\n    \\\"popularity\\\": 651708\\n  },\\n  {\\n    \\\"tag\\\": \"Tripitaka\",\\n    \\\"popularity\\\": 643710\\n  },\\n  {\\n    \\\"tag\\\": \"Billjim\",\\n    \\\"popularity\\\": 635865\\n  },\\n  {\\n    \\\"tag\\\": \"pyramidical\",\\n    \\\"popularity\\\": 628169\\n  },\\n  {\\n    \\\"tag\\\": \"circumlocutionist\",\\n    \\\"popularity\\\": 620617\\n  },\\n  {\\n    \\\"tag\\\": \"slapstick\",\\n    \\\"popularity\\\": 613207\\n  },\\n  {\\n    \\\"tag\\\": \"preobedience\",\\n    \\\"popularity\\\": 605934\\n  },\\n  {\\n    \\\"tag\\\": \"unfriarlike\",\\n    \\\"popularity\\\": 598795\\n  },\\n  {\\n    \\\"tag\\\": \"microchromosome\",\\n    \\\"popularity\\\": 591786\\n  },\\n  {\\n    \\\"tag\\\": \"Orphicism\",\\n    \\\"popularity\\\": 584905\\n  },\\n  {\\n    \\\"tag\\\": \"peel\",\\n    \\\"popularity\\\": 578149\\n  },\\n  {\\n    \\\"tag\\\": \"obediential\",\\n    \\\"popularity\\\": 571514\\n  },\\n  {\\n    \\\"tag\\\": \"Peripatidea\",\\n    \\\"popularity\\\": 564997\\n  },\\n  {\\n    \\\"tag\\\": \"undoubtful\",\\n    \\\"popularity\\\": 558596\\n  },\\n  {\\n    \\\"tag\\\": \"lodgeable\",\\n    \\\"popularity\\\": 552307\\n  },\\n  {\\n    \\\"tag\\\": \"pustulated woodchat\",\\n    \\\"popularity\\\": 546129\\n  },\\n  {\\n    \\\"tag\\\": \"antepast\",\\n    \\\"popularity\\\": 540057\\n  },\\n  {\\n    \\\"tag\\\": \"sagittoid matrimoniously\",\\n    \\\"popularity\\\": 534091\\n  },\\n  {\\n    \\\"tag\\\": \"Albizzia\",\\n    \\\"popularity\\\": 528228\\n  },\\n  {\\n    \\\"tag\\\": \"Elateridae unnewness\",\\n    \\\"popularity\\\": 522464\\n  },\\n  {\\n    \\\"tag\\\": \"convertingness\",\\n    \\\"popularity\\\": 516798\\n  },\\n  {\\n    \\\"tag\\\": \"Pelew\",\\n    \\\"popularity\\\": 511228\\n  },\\n  {\\n    \\\"tag\\\": \"recapitulation\",\\n    \\\"popularity\\\": 505751\\n  },\\n  {\\n    \\\"tag\\\": \"shack\",\\n    \\\"popularity\\\": 500365\\n  },\\n  {\\n    \\\"tag\\\": \"unmellowed\",\\n    \\\"popularity\\\": 495069\\n  },\\n  {\\n    \\\"tag\\\": \"pavis capering\",\\n    \\\"popularity\\\": 489859\\n  },\\n  {\\n    \\\"tag\\\": \"fanfare\",\\n    \\\"popularity\\\": 484735\\n  },\\n  {\\n    \\\"tag\\\": \"sole\",\\n    \\\"popularity\\\": 479695\\n  },\\n  {\\n    \\\"tag\\\": \"subarcuate\",\\n    \\\"popularity\\\": 474735\\n  },\\n  {\\n    \\\"tag\\\": \"multivious\",\\n    \\\"popularity\\\": 469856\\n  },\\n  {\\n    \\\"tag\\\": \"squandermania\",\\n    \\\"popularity\\\": 465054\\n  },\\n  {\\n    \\\"tag\\\": \"scintle\",\\n    \\\"popularity\\\": 460329\\n  },\\n  {\\n    \\\"tag\\\": \"hash chirognomic\",\\n    \\\"popularity\\\": 455679\\n  },\\n  {\\n    \\\"tag\\\": \"linseed\",\\n    \\\"popularity\\\": 451101\\n  },\\n  {\\n    \\\"tag\\\": \"redoubtable\",\\n    \\\"popularity\\\": 446596\\n  },\\n  {\\n    \\\"tag\\\": \"poachy reimpact\",\\n    \\\"popularity\\\": 442160\\n  },\\n  {\\n    \\\"tag\\\": \"limestone\",\\n    \\\"popularity\\\": 437792\\n  },\\n  {\\n    \\\"tag\\\": \"serranid\",\\n    \\\"popularity\\\": 433492\\n  },\\n  {\\n    \\\"tag\\\": \"pohna\",\\n    \\\"popularity\\\": 429258\\n  },\\n  {\\n    \\\"tag\\\": \"warwolf\",\\n    \\\"popularity\\\": 425088\\n  },\\n  {\\n    \\\"tag\\\": \"ruthenous\",\\n    \\\"popularity\\\": 420981\\n  },\\n  {\\n    \\\"tag\\\": \"dover\",\\n    \\\"popularity\\\": 416935\\n  },\\n  {\\n    \\\"tag\\\": \"deuteroalbumose\",\\n    \\\"popularity\\\": 412950\\n  },\\n  {\\n    \\\"tag\\\": \"pseudoprophetic\",\\n    \\\"popularity\\\": 409025\\n  },\\n  {\\n    \\\"tag\\\": \"dissoluteness\",\\n    \\\"popularity\\\": 405157\\n  },\\n  {\\n    \\\"tag\\\": \"preinvention\",\\n    \\\"popularity\\\": 401347\\n  },\\n  {\\n    \\\"tag\\\": \"swagbellied\",\\n    \\\"popularity\\\": 397592\\n  },\\n  {\\n    \\\"tag\\\": \"Ophidia\",\\n    \\\"popularity\\\": 393892\\n  },\\n  {\\n    \\\"tag\\\": \"equanimity\",\\n    \\\"popularity\\\": 390245\\n  },\\n  {\\n    \\\"tag\\\": \"troutful\",\\n    \\\"popularity\\\": 386651\\n  },\\n  {\\n    \\\"tag\\\": \"uke\",\\n    \\\"popularity\\\": 383108\\n  },\\n  {\\n    \\\"tag\\\": \"preacquaint\",\\n    \\\"popularity\\\": 379616\\n  },\\n  {\\n    \\\"tag\\\": \"shoq\",\\n    \\\"popularity\\\": 376174\\n  },\\n  {\\n    \\\"tag\\\": \"yox\",\\n    \\\"popularity\\\": 372780\\n  },\\n  {\\n    \\\"tag\\\": \"unelemental\",\\n    \\\"popularity\\\": 369434\\n  },\\n  {\\n    \\\"tag\\\": \"Yavapai\",\\n    \\\"popularity\\\": 366134\\n  },\\n  {\\n    \\\"tag\\\": \"joulean\",\\n    \\\"popularity\\\": 362880\\n  },\\n  {\\n    \\\"tag\\\": \"dracontine\",\\n    \\\"popularity\\\": 359672\\n  },\\n  {\\n    \\\"tag\\\": \"hardmouth\",\\n    \\\"popularity\\\": 356507\\n  },\\n  {\\n    \\\"tag\\\": \"sylvanize\",\\n    \\\"popularity\\\": 353386\\n  },\\n  {\\n    \\\"tag\\\": \"intraparenchymatous meadowbur\",\\n    \\\"popularity\\\": 350308\\n  },\\n  {\\n    \\\"tag\\\": \"uncharily\",\\n    \\\"popularity\\\": 347271\\n  },\\n  {\\n    \\\"tag\\\": \"redtab flexibly\",\\n    \\\"popularity\\\": 344275\\n  },\\n  {\\n    \\\"tag\\\": \"centervelic\",\\n    \\\"popularity\\\": 341319\\n  },\\n  {\\n    \\\"tag\\\": \"unravellable\",\\n    \\\"popularity\\\": 338403\\n  },\\n  {\\n    \\\"tag\\\": \"infortunately\",\\n    \\\"popularity\\\": 335526\\n  },\\n  {\\n    \\\"tag\\\": \"cannel\",\\n    \\\"popularity\\\": 332687\\n  },\\n  {\\n    \\\"tag\\\": \"oxyblepsia\",\\n    \\\"popularity\\\": 329885\\n  },\\n  {\\n    \\\"tag\\\": \"Damon\",\\n    \\\"popularity\\\": 327120\\n  },\\n  {\\n    \\\"tag\\\": \"etherin\",\\n    \\\"popularity\\\": 324391\\n  },\\n  {\\n    \\\"tag\\\": \"luminal\",\\n    \\\"popularity\\\": 321697\\n  },\\n  {\\n    \\\"tag\\\": \"interrogatorily presbyte\",\\n    \\\"popularity\\\": 319038\\n  },\\n  {\\n    \\\"tag\\\": \"hemiclastic\",\\n    \\\"popularity\\\": 316414\\n  },\\n  {\\n    \\\"tag\\\": \"poh flush\",\\n    \\\"popularity\\\": 313823\\n  },\\n  {\\n    \\\"tag\\\": \"Psoroptes\",\\n    \\\"popularity\\\": 311265\\n  },\\n  {\\n    \\\"tag\\\": \"dispirit\",\\n    \\\"popularity\\\": 308740\\n  },\\n  {\\n    \\\"tag\\\": \"nashgab\",\\n    \\\"popularity\\\": 306246\\n  },\\n  {\\n    \\\"tag\\\": \"Aphidiinae\",\\n    \\\"popularity\\\": 303784\\n  },\\n  {\\n    \\\"tag\\\": \"rhapsody nonconstruction\",\\n    \\\"popularity\\\": 301353\\n  },\\n  {\\n    \\\"tag\\\": \"Osmond\",\\n    \\\"popularity\\\": 298952\\n  },\\n  {\\n    \\\"tag\\\": \"Leonis\",\\n    \\\"popularity\\\": 296581\\n  },\\n  {\\n    \\\"tag\\\": \"Lemnian\",\\n    \\\"popularity\\\": 294239\\n  },\\n  {\\n    \\\"tag\\\": \"acetonic gnathonic\",\\n    \\\"popularity\\\": 291926\\n  },\\n  {\\n    \\\"tag\\\": \"surculus\",\\n    \\\"popularity\\\": 289641\\n  },\\n  {\\n    \\\"tag\\\": \"diagonally\",\\n    \\\"popularity\\\": 287384\\n  },\\n  {\\n    \\\"tag\\\": \"counterpenalty\",\\n    \\\"popularity\\\": 285154\\n  },\\n  {\\n    \\\"tag\\\": \"Eugenie\",\\n    \\\"popularity\\\": 282952\\n  },\\n  {\\n    \\\"tag\\\": \"hornbook\",\\n    \\\"popularity\\\": 280776\\n  },\\n  {\\n    \\\"tag\\\": \"miscoin\",\\n    \\\"popularity\\\": 278626\\n  },\\n  {\\n    \\\"tag\\\": \"admi\",\\n    \\\"popularity\\\": 276501\\n  },\\n  {\\n    \\\"tag\\\": \"Tarmac\",\\n    \\\"popularity\\\": 274402\\n  },\\n  {\\n    \\\"tag\\\": \"inexplicable\",\\n    \\\"popularity\\\": 272328\\n  },\\n  {\\n    \\\"tag\\\": \"rascallion\",\\n    \\\"popularity\\\": 270278\\n  },\\n  {\\n    \\\"tag\\\": \"dusterman\",\\n    \\\"popularity\\\": 268252\\n  },\\n  {\\n    \\\"tag\\\": \"osteostomous unhoroscopic\",\\n    \\\"popularity\\\": 266250\\n  },\\n  {\\n    \\\"tag\\\": \"spinibulbar\",\\n    \\\"popularity\\\": 264271\\n  },\\n  {\\n    \\\"tag\\\": \"phototelegraphically\",\\n    \\\"popularity\\\": 262315\\n  },\\n  {\\n    \\\"tag\\\": \"Manihot\",\\n    \\\"popularity\\\": 260381\\n  },\\n  {\\n    \\\"tag\\\": \"neighborhood\",\\n    \\\"popularity\\\": 258470\\n  },\\n  {\\n    \\\"tag\\\": \"Vincetoxicum\",\\n    \\\"popularity\\\": 256581\\n  },\\n  {\\n    \\\"tag\\\": \"khirka\",\\n    \\\"popularity\\\": 254713\\n  },\\n  {\\n    \\\"tag\\\": \"conscriptive\",\\n    \\\"popularity\\\": 252866\\n  },\\n  {\\n    \\\"tag\\\": \"synechthran\",\\n    \\\"popularity\\\": 251040\\n  },\\n  {\\n    \\\"tag\\\": \"Guttiferales\",\\n    \\\"popularity\\\": 249235\\n  },\\n  {\\n    \\\"tag\\\": \"roomful\",\\n    \\\"popularity\\\": 247450\\n  },\\n  {\\n    \\\"tag\\\": \"germinal\",\\n    \\\"popularity\\\": 245685\\n  },\\n  {\\n    \\\"tag\\\": \"untraitorous\",\\n    \\\"popularity\\\": 243939\\n  },\\n  {\\n    \\\"tag\\\": \"nondissenting\",\\n    \\\"popularity\\\": 242213\\n  },\\n  {\\n    \\\"tag\\\": \"amotion\",\\n    \\\"popularity\\\": 240506\\n  },\\n  {\\n    \\\"tag\\\": \"badious\",\\n    \\\"popularity\\\": 238817\\n  },\\n  {\\n    \\\"tag\\\": \"sumpit\",\\n    \\\"popularity\\\": 237147\\n  },\\n  {\\n    \\\"tag\\\": \"ectozoic\",\\n    \\\"popularity\\\": 235496\\n  },\\n  {\\n    \\\"tag\\\": \"elvet\",\\n    \\\"popularity\\\": 233862\\n  },\\n  {\\n    \\\"tag\\\": \"underclerk\",\\n    \\\"popularity\\\": 232246\\n  },\\n  {\\n    \\\"tag\\\": \"reticency\",\\n    \\\"popularity\\\": 230647\\n  },\\n  {\\n    \\\"tag\\\": \"neutroclusion\",\\n    \\\"popularity\\\": 229065\\n  },\\n  {\\n    \\\"tag\\\": \"unbelieving\",\\n    \\\"popularity\\\": 227500\\n  },\\n  {\\n    \\\"tag\\\": \"histogenetic\",\\n    \\\"popularity\\\": 225952\\n  },\\n  {\\n    \\\"tag\\\": \"dermamyiasis\",\\n    \\\"popularity\\\": 224421\\n  },\\n  {\\n    \\\"tag\\\": \"telenergy\",\\n    \\\"popularity\\\": 222905\\n  },\\n  {\\n    \\\"tag\\\": \"axiomatic\",\\n    \\\"popularity\\\": 221406\\n  },\\n  {\\n    \\\"tag\\\": \"undominoed\",\\n    \\\"popularity\\\": 219922\\n  },\\n  {\\n    \\\"tag\\\": \"periosteoma\",\\n    \\\"popularity\\\": 218454\\n  },\\n  {\\n    \\\"tag\\\": \"justiciaryship\",\\n    \\\"popularity\\\": 217001\\n  },\\n  {\\n    \\\"tag\\\": \"autoluminescence\",\\n    \\\"popularity\\\": 215563\\n  },\\n  {\\n    \\\"tag\\\": \"osmous\",\\n    \\\"popularity\\\": 214140\\n  },\\n  {\\n    \\\"tag\\\": \"borgh\",\\n    \\\"popularity\\\": 212731\\n  },\\n  {\\n    \\\"tag\\\": \"bedebt\",\\n    \\\"popularity\\\": 211337\\n  },\\n  {\\n    \\\"tag\\\": \"considerableness adenoidism\",\\n    \\\"popularity\\\": 209957\\n  },\\n  {\\n    \\\"tag\\\": \"sailorizing\",\\n    \\\"popularity\\\": 208592\\n  },\\n  {\\n    \\\"tag\\\": \"Montauk\",\\n    \\\"popularity\\\": 207240\\n  },\\n  {\\n    \\\"tag\\\": \"Bridget\",\\n    \\\"popularity\\\": 205901\\n  },\\n  {\\n    \\\"tag\\\": \"Gekkota\",\\n    \\\"popularity\\\": 204577\\n  },\\n  {\\n    \\\"tag\\\": \"subcorymbose\",\\n    \\\"popularity\\\": 203265\\n  },\\n  {\\n    \\\"tag\\\": \"undersap\",\\n    \\\"popularity\\\": 201967\\n  },\\n  {\\n    \\\"tag\\\": \"poikilothermic\",\\n    \\\"popularity\\\": 200681\\n  },\\n  {\\n    \\\"tag\\\": \"enneatical\",\\n    \\\"popularity\\\": 199409\\n  },\\n  {\\n    \\\"tag\\\": \"martinetism\",\\n    \\\"popularity\\\": 198148\\n  },\\n  {\\n    \\\"tag\\\": \"sustanedly\",\\n    \\\"popularity\\\": 196901\\n  },\\n  {\\n    \\\"tag\\\": \"declaration\",\\n    \\\"popularity\\\": 195665\\n  },\\n  {\\n    \\\"tag\\\": \"myringoplasty\",\\n    \\\"popularity\\\": 194442\\n  },\\n  {\\n    \\\"tag\\\": \"Ginkgo\",\\n    \\\"popularity\\\": 193230\\n  },\\n  {\\n    \\\"tag\\\": \"unrecurrent\",\\n    \\\"popularity\\\": 192031\\n  },\\n  {\\n    \\\"tag\\\": \"proprecedent\",\\n    \\\"popularity\\\": 190843\\n  },\\n  {\\n    \\\"tag\\\": \"roadman\",\\n    \\\"popularity\\\": 189666\\n  },\\n  {\\n    \\\"tag\\\": \"elemin\",\\n    \\\"popularity\\\": 188501\\n  },\\n  {\\n    \\\"tag\\\": \"maggot\",\\n    \\\"popularity\\\": 187347\\n  },\\n  {\\n    \\\"tag\\\": \"alitrunk\",\\n    \\\"popularity\\\": 186204\\n  },\\n  {\\n    \\\"tag\\\": \"introspection\",\\n    \\\"popularity\\\": 185071\\n  },\\n  {\\n    \\\"tag\\\": \"batiker\",\\n    \\\"popularity\\\": 183950\\n  },\\n  {\\n    \\\"tag\\\": \"backhatch oversettle\",\\n    \\\"popularity\\\": 182839\\n  },\\n  {\\n    \\\"tag\\\": \"thresherman\",\\n    \\\"popularity\\\": 181738\\n  },\\n  {\\n    \\\"tag\\\": \"protemperance\",\\n    \\\"popularity\\\": 180648\\n  },\\n  {\\n    \\\"tag\\\": \"undern\",\\n    \\\"popularity\\\": 179568\\n  },\\n  {\\n    \\\"tag\\\": \"tweeg\",\\n    \\\"popularity\\\": 178498\\n  },\\n  {\\n    \\\"tag\\\": \"crosspath\",\\n    \\\"popularity\\\": 177438\\n  },\\n  {\\n    \\\"tag\\\": \"Tangaridae\",\\n    \\\"popularity\\\": 176388\\n  },\\n  {\\n    \\\"tag\\\": \"scrutation\",\\n    \\\"popularity\\\": 175348\\n  },\\n  {\\n    \\\"tag\\\": \"piecemaker\",\\n    \\\"popularity\\\": 174317\\n  },\\n  {\\n    \\\"tag\\\": \"paster\",\\n    \\\"popularity\\\": 173296\\n  },\\n  {\\n    \\\"tag\\\": \"unpretendingness\",\\n    \\\"popularity\\\": 172284\\n  },\\n  {\\n    \\\"tag\\\": \"inframundane\",\\n    \\\"popularity\\\": 171281\\n  },\\n  {\\n    \\\"tag\\\": \"kiblah\",\\n    \\\"popularity\\\": 170287\\n  },\\n  {\\n    \\\"tag\\\": \"playwrighting\",\\n    \\\"popularity\\\": 169302\\n  },\\n  {\\n    \\\"tag\\\": \"gonepoiesis snowslip\",\\n    \\\"popularity\\\": 168326\\n  },\\n  {\\n    \\\"tag\\\": \"hoodwise\",\\n    \\\"popularity\\\": 167359\\n  },\\n  {\\n    \\\"tag\\\": \"postseason\",\\n    \\\"popularity\\\": 166401\\n  },\\n  {\\n    \\\"tag\\\": \"equivocality\",\\n    \\\"popularity\\\": 165451\\n  },\\n  {\\n    \\\"tag\\\": \"Opiliaceae nuclease\",\\n    \\\"popularity\\\": 164509\\n  },\\n  {\\n    \\\"tag\\\": \"sextipara\",\\n    \\\"popularity\\\": 163576\\n  },\\n  {\\n    \\\"tag\\\": \"weeper\",\\n    \\\"popularity\\\": 162651\\n  },\\n  {\\n    \\\"tag\\\": \"frambesia\",\\n    \\\"popularity\\\": 161735\\n  },\\n  {\\n    \\\"tag\\\": \"answerable\",\\n    \\\"popularity\\\": 160826\\n  },\\n  {\\n    \\\"tag\\\": \"Trichosporum\",\\n    \\\"popularity\\\": 159925\\n  },\\n  {\\n    \\\"tag\\\": \"cajuputol\",\\n    \\\"popularity\\\": 159033\\n  },\\n  {\\n    \\\"tag\\\": \"pleomorphous\",\\n    \\\"popularity\\\": 158148\\n  },\\n  {\\n    \\\"tag\\\": \"aculeolate\",\\n    \\\"popularity\\\": 157270\\n  },\\n  {\\n    \\\"tag\\\": \"wherever\",\\n    \\\"popularity\\\": 156400\\n  },\\n  {\\n    \\\"tag\\\": \"collapse\",\\n    \\\"popularity\\\": 155538\\n  },\\n  {\\n    \\\"tag\\\": \"porky\",\\n    \\\"popularity\\\": 154683\\n  },\\n  {\\n    \\\"tag\\\": \"perule\",\\n    \\\"popularity\\\": 153836\\n  },\\n  {\\n    \\\"tag\\\": \"Nevada\",\\n    \\\"popularity\\\": 152996\\n  },\\n  {\\n    \\\"tag\\\": \"conalbumin\",\\n    \\\"popularity\\\": 152162\\n  },\\n  {\\n    \\\"tag\\\": \"tsunami\",\\n    \\\"popularity\\\": 151336\\n  },\\n  {\\n    \\\"tag\\\": \"Gulf\",\\n    \\\"popularity\\\": 150517\\n  },\\n  {\\n    \\\"tag\\\": \"hertz\",\\n    \\\"popularity\\\": 149705\\n  },\\n  {\\n    \\\"tag\\\": \"limmock\",\\n    \\\"popularity\\\": 148900\\n  },\\n  {\\n    \\\"tag\\\": \"Tartarize\",\\n    \\\"popularity\\\": 148101\\n  },\\n  {\\n    \\\"tag\\\": \"entosphenoid\",\\n    \\\"popularity\\\": 147310\\n  },\\n  {\\n    \\\"tag\\\": \"ibis\",\\n    \\\"popularity\\\": 146524\\n  },\\n  {\\n    \\\"tag\\\": \"unyeaned\",\\n    \\\"popularity\\\": 145746\\n  },\\n  {\\n    \\\"tag\\\": \"tritural\",\\n    \\\"popularity\\\": 144973\\n  },\\n  {\\n    \\\"tag\\\": \"hundredary\",\\n    \\\"popularity\\\": 144207\\n  },\\n  {\\n    \\\"tag\\\": \"stolonlike\",\\n    \\\"popularity\\\": 143448\\n  },\\n  {\\n    \\\"tag\\\": \"chorister\",\\n    \\\"popularity\\\": 142694\\n  },\\n  {\\n    \\\"tag\\\": \"mismove\",\\n    \\\"popularity\\\": 141947\\n  },\\n  {\\n    \\\"tag\\\": \"Andine\",\\n    \\\"popularity\\\": 141206\\n  },\\n  {\\n    \\\"tag\\\": \"Annette proneur escribe\",\\n    \\\"popularity\\\": 140471\\n  },\\n  {\\n    \\\"tag\\\": \"exoperidium\",\\n    \\\"popularity\\\": 139742\\n  },\\n  {\\n    \\\"tag\\\": \"disedge\",\\n    \\\"popularity\\\": 139019\\n  },\\n  {\\n    \\\"tag\\\": \"hypochloruria\",\\n    \\\"popularity\\\": 138302\\n  },\\n  {\\n    \\\"tag\\\": \"prepupa\",\\n    \\\"popularity\\\": 137590\\n  },\\n  {\\n    \\\"tag\\\": \"assent\",\\n    \\\"popularity\\\": 136884\\n  },\\n  {\\n    \\\"tag\\\": \"hydrazobenzene\",\\n    \\\"popularity\\\": 136184\\n  },\\n  {\\n    \\\"tag\\\": \"emballonurid\",\\n    \\\"popularity\\\": 135489\\n  },\\n  {\\n    \\\"tag\\\": \"roselle\",\\n    \\\"popularity\\\": 134800\\n  },\\n  {\\n    \\\"tag\\\": \"unifiedly\",\\n    \\\"popularity\\\": 134117\\n  },\\n  {\\n    \\\"tag\\\": \"clang\",\\n    \\\"popularity\\\": 133439\\n  },\\n  {\\n    \\\"tag\\\": \"acetolytic\",\\n    \\\"popularity\\\": 132766\\n  },\\n  {\\n    \\\"tag\\\": \"cladodont\",\\n    \\\"popularity\\\": 132098\\n  },\\n  {\\n    \\\"tag\\\": \"recoast\",\\n    \\\"popularity\\\": 131436\\n  },\\n  {\\n    \\\"tag\\\": \"celebrated tydie Eocarboniferous\",\\n    \\\"popularity\\\": 130779\\n  },\\n  {\\n    \\\"tag\\\": \"superconsciousness\",\\n    \\\"popularity\\\": 130127\\n  },\\n  {\\n    \\\"tag\\\": \"soberness\",\\n    \\\"popularity\\\": 129480\\n  },\\n  {\\n    \\\"tag\\\": \"panoramist\",\\n    \\\"popularity\\\": 128838\\n  },\\n  {\\n    \\\"tag\\\": \"Orbitolina\",\\n    \\\"popularity\\\": 128201\\n  },\\n  {\\n    \\\"tag\\\": \"overlewd\",\\n    \\\"popularity\\\": 127569\\n  },\\n  {\\n    \\\"tag\\\": \"demiquaver\",\\n    \\\"popularity\\\": 126942\\n  },\\n  {\\n    \\\"tag\\\": \"kamelaukion\",\\n    \\\"popularity\\\": 126319\\n  },\\n  {\\n    \\\"tag\\\": \"flancard\",\\n    \\\"popularity\\\": 125702\\n  },\\n  {\\n    \\\"tag\\\": \"tricuspid\",\\n    \\\"popularity\\\": 125089\\n  },\\n  {\\n    \\\"tag\\\": \"bepelt\",\\n    \\\"popularity\\\": 124480\\n  },\\n  {\\n    \\\"tag\\\": \"decuplet\",\\n    \\\"popularity\\\": 123877\\n  },\\n  {\\n    \\\"tag\\\": \"Rockies\",\\n    \\\"popularity\\\": 123278\\n  },\\n  {\\n    \\\"tag\\\": \"unforgeability\",\\n    \\\"popularity\\\": 122683\\n  },\\n  {\\n    \\\"tag\\\": \"mocha\",\\n    \\\"popularity\\\": 122093\\n  },\\n  {\\n    \\\"tag\\\": \"scrunge\",\\n    \\\"popularity\\\": 121507\\n  },\\n  {\\n    \\\"tag\\\": \"delighter\",\\n    \\\"popularity\\\": 120926\\n  },\\n  {\\n    \\\"tag\\\": \"willey Microtinae\",\\n    \\\"popularity\\\": 120349\\n  },\\n  {\\n    \\\"tag\\\": \"unhuntable\",\\n    \\\"popularity\\\": 119777\\n  },\\n  {\\n    \\\"tag\\\": \"historically\",\\n    \\\"popularity\\\": 119208\\n  },\\n  {\\n    \\\"tag\\\": \"vicegerentship\",\\n    \\\"popularity\\\": 118644\\n  },\\n  {\\n    \\\"tag\\\": \"hemangiosarcoma\",\\n    \\\"popularity\\\": 118084\\n  },\\n  {\\n    \\\"tag\\\": \"harpago\",\\n    \\\"popularity\\\": 117528\\n  },\\n  {\\n    \\\"tag\\\": \"unionoid\",\\n    \\\"popularity\\\": 116976\\n  },\\n  {\\n    \\\"tag\\\": \"wiseman\",\\n    \\\"popularity\\\": 116429\\n  },\\n  {\\n    \\\"tag\\\": \"diclinism\",\\n    \\\"popularity\\\": 115885\\n  },\\n  {\\n    \\\"tag\\\": \"Maud\",\\n    \\\"popularity\\\": 115345\\n  },\\n  {\\n    \\\"tag\\\": \"scaphocephalism\",\\n    \\\"popularity\\\": 114809\\n  },\\n  {\\n    \\\"tag\\\": \"obtenebration\",\\n    \\\"popularity\\\": 114277\\n  },\\n  {\\n    \\\"tag\\\": \"cymar predreadnought\",\\n    \\\"popularity\\\": 113749\\n  },\\n  {\\n    \\\"tag\\\": \"discommend\",\\n    \\\"popularity\\\": 113225\\n  },\\n  {\\n    \\\"tag\\\": \"crude\",\\n    \\\"popularity\\\": 112704\\n  },\\n  {\\n    \\\"tag\\\": \"upflash\",\\n    \\\"popularity\\\": 112187\\n  },\\n  {\\n    \\\"tag\\\": \"saltimbank\",\\n    \\\"popularity\\\": 111674\\n  },\\n  {\\n    \\\"tag\\\": \"posthysterical\",\\n    \\\"popularity\\\": 111165\\n  },\\n  {\\n    \\\"tag\\\": \"trample\",\\n    \\\"popularity\\\": 110659\\n  },\\n  {\\n    \\\"tag\\\": \"ungirthed\",\\n    \\\"popularity\\\": 110157\\n  },\\n  {\\n    \\\"tag\\\": \"unshakable\",\\n    \\\"popularity\\\": 109658\\n  },\\n  {\\n    \\\"tag\\\": \"hepatocystic\",\\n    \\\"popularity\\\": 109163\\n  },\\n  {\\n    \\\"tag\\\": \"psammophyte\",\\n    \\\"popularity\\\": 108671\\n  },\\n  {\\n    \\\"tag\\\": \"millionfold\",\\n    \\\"popularity\\\": 108183\\n  },\\n  {\\n    \\\"tag\\\": \"outtaste\",\\n    \\\"popularity\\\": 107698\\n  },\\n  {\\n    \\\"tag\\\": \"poppycockish\",\\n    \\\"popularity\\\": 107217\\n  },\\n  {\\n    \\\"tag\\\": \"viduine\",\\n    \\\"popularity\\\": 106739\\n  },\\n  {\\n    \\\"tag\\\": \"pleasureman\",\\n    \\\"popularity\\\": 106264\\n  },\\n  {\\n    \\\"tag\\\": \"cholesterolemia\",\\n    \\\"popularity\\\": 105792\\n  },\\n  {\\n    \\\"tag\\\": \"hostlerwife\",\\n    \\\"popularity\\\": 105324\\n  },\\n  {\\n    \\\"tag\\\": \"figure undergrass\",\\n    \\\"popularity\\\": 104859\\n  },\\n  {\\n    \\\"tag\\\": \"bedrape\",\\n    \\\"popularity\\\": 104398\\n  },\\n  {\\n    \\\"tag\\\": \"nuttishness\",\\n    \\\"popularity\\\": 103939\\n  },\\n  {\\n    \\\"tag\\\": \"fow\",\\n    \\\"popularity\\\": 103484\\n  },\\n  {\\n    \\\"tag\\\": \"rachianesthesia\",\\n    \\\"popularity\\\": 103031\\n  },\\n  {\\n    \\\"tag\\\": \"recruitable\",\\n    \\\"popularity\\\": 102582\\n  },\\n  {\\n    \\\"tag\\\": \"semianatomical Oenotheraceae\",\\n    \\\"popularity\\\": 102136\\n  },\\n  {\\n    \\\"tag\\\": \"extracapsular\",\\n    \\\"popularity\\\": 101693\\n  },\\n  {\\n    \\\"tag\\\": \"unsigneted\",\\n    \\\"popularity\\\": 101253\\n  },\\n  {\\n    \\\"tag\\\": \"fissural\",\\n    \\\"popularity\\\": 100816\\n  },\\n  {\\n    \\\"tag\\\": \"ayous\",\\n    \\\"popularity\\\": 100381\\n  },\\n  {\\n    \\\"tag\\\": \"crestfallenness odontograph\",\\n    \\\"popularity\\\": 99950\\n  },\\n  {\\n    \\\"tag\\\": \"monopodium\",\\n    \\\"popularity\\\": 99522\\n  },\\n  {\\n    \\\"tag\\\": \"germfree\",\\n    \\\"popularity\\\": 99096\\n  },\\n  {\\n    \\\"tag\\\": \"dauphin\",\\n    \\\"popularity\\\": 98673\\n  },\\n  {\\n    \\\"tag\\\": \"nonagesimal\",\\n    \\\"popularity\\\": 98254\\n  },\\n  {\\n    \\\"tag\\\": \"waterchat\",\\n    \\\"popularity\\\": 97836\\n  },\\n  {\\n    \\\"tag\\\": \"Entelodon\",\\n    \\\"popularity\\\": 97422\\n  },\\n  {\\n    \\\"tag\\\": \"semischolastic\",\\n    \\\"popularity\\\": 97010\\n  },\\n  {\\n    \\\"tag\\\": \"somata\",\\n    \\\"popularity\\\": 96602\\n  },\\n  {\\n    \\\"tag\\\": \"expositorily\",\\n    \\\"popularity\\\": 96195\\n  },\\n  {\\n    \\\"tag\\\": \"bass\",\\n    \\\"popularity\\\": 95792\\n  },\\n  {\\n    \\\"tag\\\": \"calorimetry\",\\n    \\\"popularity\\\": 95391\\n  },\\n  {\\n    \\\"tag\\\": \"entireness\",\\n    \\\"popularity\\\": 94993\\n  },\\n  {\\n    \\\"tag\\\": \"ratline soppiness\",\\n    \\\"popularity\\\": 94597\\n  },\\n  {\\n    \\\"tag\\\": \"shor\",\\n    \\\"popularity\\\": 94204\\n  },\\n  {\\n    \\\"tag\\\": \"coprecipitation\",\\n    \\\"popularity\\\": 93813\\n  },\\n  {\\n    \\\"tag\\\": \"unblushingly\",\\n    \\\"popularity\\\": 93425\\n  },\\n  {\\n    \\\"tag\\\": \"macarize\",\\n    \\\"popularity\\\": 93040\\n  },\\n  {\\n    \\\"tag\\\": \"scruplesomeness\",\\n    \\\"popularity\\\": 92657\\n  },\\n  {\\n    \\\"tag\\\": \"offsaddle\",\\n    \\\"popularity\\\": 92276\\n  },\\n  {\\n    \\\"tag\\\": \"hypertragical\",\\n    \\\"popularity\\\": 91898\\n  },\\n  {\\n    \\\"tag\\\": \"uncassock loined\",\\n    \\\"popularity\\\": 91522\\n  },\\n  {\\n    \\\"tag\\\": \"interlobate\",\\n    \\\"popularity\\\": 91149\\n  },\\n  {\\n    \\\"tag\\\": \"releasor orrisroot stoloniferously\",\\n    \\\"popularity\\\": 90778\\n  },\\n  {\\n    \\\"tag\\\": \"elementoid\",\\n    \\\"popularity\\\": 90410\\n  },\\n  {\\n    \\\"tag\\\": \"Lentilla\",\\n    \\\"popularity\\\": 90043\\n  },\\n  {\\n    \\\"tag\\\": \"distressing\",\\n    \\\"popularity\\\": 89679\\n  },\\n  {\\n    \\\"tag\\\": \"hydrodrome\",\\n    \\\"popularity\\\": 89318\\n  },\\n  {\\n    \\\"tag\\\": \"Jeannette\",\\n    \\\"popularity\\\": 88958\\n  },\\n  {\\n    \\\"tag\\\": \"Kuli\",\\n    \\\"popularity\\\": 88601\\n  },\\n  {\\n    \\\"tag\\\": \"taxinomist\",\\n    \\\"popularity\\\": 88246\\n  },\\n  {\\n    \\\"tag\\\": \"southwestwardly\",\\n    \\\"popularity\\\": 87894\\n  },\\n  {\\n    \\\"tag\\\": \"polyparia\",\\n    \\\"popularity\\\": 87543\\n  },\\n  {\\n    \\\"tag\\\": \"exmeridian\",\\n    \\\"popularity\\\": 87195\\n  },\\n  {\\n    \\\"tag\\\": \"splenius regimentaled\",\\n    \\\"popularity\\\": 86849\\n  },\\n  {\\n    \\\"tag\\\": \"Sphaeropsidaceae\",\\n    \\\"popularity\\\": 86505\\n  },\\n  {\\n    \\\"tag\\\": \"unbegun\",\\n    \\\"popularity\\\": 86163\\n  },\\n  {\\n    \\\"tag\\\": \"something\",\\n    \\\"popularity\\\": 85823\\n  },\\n  {\\n    \\\"tag\\\": \"contaminable nonexpulsion\",\\n    \\\"popularity\\\": 85486\\n  },\\n  {\\n    \\\"tag\\\": \"douser\",\\n    \\\"popularity\\\": 85150\\n  },\\n  {\\n    \\\"tag\\\": \"prostrike\",\\n    \\\"popularity\\\": 84817\\n  },\\n  {\\n    \\\"tag\\\": \"worky\",\\n    \\\"popularity\\\": 84485\\n  },\\n  {\\n    \\\"tag\\\": \"folliful\",\\n    \\\"popularity\\\": 84156\\n  },\\n  {\\n    \\\"tag\\\": \"prioracy\",\\n    \\\"popularity\\\": 83828\\n  },\\n  {\\n    \\\"tag\\\": \"undermentioned\",\\n    \\\"popularity\\\": 83503\\n  },\\n  {\\n    \\\"tag\\\": \"Judaica\",\\n    \\\"popularity\\\": 83179\\n  },\\n  {\\n    \\\"tag\\\": \"multifarious\",\\n    \\\"popularity\\\": 82858\\n  },\\n  {\\n    \\\"tag\\\": \"poogye\",\\n    \\\"popularity\\\": 82538\\n  },\\n  {\\n    \\\"tag\\\": \"Sparganium\",\\n    \\\"popularity\\\": 82221\\n  },\\n  {\\n    \\\"tag\\\": \"thurrock\",\\n    \\\"popularity\\\": 81905\\n  },\\n  {\\n    \\\"tag\\\": \"outblush\",\\n    \\\"popularity\\\": 81591\\n  },\\n  {\\n    \\\"tag\\\": \"Strophanthus supraordination\",\\n    \\\"popularity\\\": 81279\\n  },\\n  {\\n    \\\"tag\\\": \"gingerroot\",\\n    \\\"popularity\\\": 80969\\n  },\\n  {\\n    \\\"tag\\\": \"unconscient\",\\n    \\\"popularity\\\": 80661\\n  },\\n  {\\n    \\\"tag\\\": \"unconstitutionally\",\\n    \\\"popularity\\\": 80354\\n  },\\n  {\\n    \\\"tag\\\": \"plaguily\",\\n    \\\"popularity\\\": 80050\\n  },\\n  {\\n    \\\"tag\\\": \"waterily equatorwards\",\\n    \\\"popularity\\\": 79747\\n  },\\n  {\\n    \\\"tag\\\": \"nondeposition\",\\n    \\\"popularity\\\": 79446\\n  },\\n  {\\n    \\\"tag\\\": \"dronishly\",\\n    \\\"popularity\\\": 79147\\n  },\\n  {\\n    \\\"tag\\\": \"gateado\",\\n    \\\"popularity\\\": 78849\\n  },\\n  {\\n    \\\"tag\\\": \"dislink\",\\n    \\\"popularity\\\": 78553\\n  },\\n  {\\n    \\\"tag\\\": \"Joceline\",\\n    \\\"popularity\\\": 78259\\n  },\\n  {\\n    \\\"tag\\\": \"amphiboliferous\",\\n    \\\"popularity\\\": 77967\\n  },\\n  {\\n    \\\"tag\\\": \"bushrope\",\\n    \\\"popularity\\\": 77676\\n  },\\n  {\\n    \\\"tag\\\": \"plumicorn sulphosalicylic\",\\n    \\\"popularity\\\": 77387\\n  },\\n  {\\n    \\\"tag\\\": \"nonefficiency\",\\n    \\\"popularity\\\": 77100\\n  },\\n  {\\n    \\\"tag\\\": \"hieroscopy\",\\n    \\\"popularity\\\": 76815\\n  },\\n  {\\n    \\\"tag\\\": \"causativeness\",\\n    \\\"popularity\\\": 76531\\n  },\\n  {\\n    \\\"tag\\\": \"swird paleoeremology\",\\n    \\\"popularity\\\": 76249\\n  },\\n  {\\n    \\\"tag\\\": \"camphoric\",\\n    \\\"popularity\\\": 75968\\n  },\\n  {\\n    \\\"tag\\\": \"retaining\",\\n    \\\"popularity\\\": 75689\\n  },\\n  {\\n    \\\"tag\\\": \"thyreoprotein\",\\n    \\\"popularity\\\": 75411\\n  },\\n  {\\n    \\\"tag\\\": \"carbona\",\\n    \\\"popularity\\\": 75136\\n  },\\n  {\\n    \\\"tag\\\": \"protectively\",\\n    \\\"popularity\\\": 74861\\n  },\\n  {\\n    \\\"tag\\\": \"mosasaur\",\\n    \\\"popularity\\\": 74589\\n  },\\n  {\\n    \\\"tag\\\": \"reciprocator\",\\n    \\\"popularity\\\": 74317\\n  },\\n  {\\n    \\\"tag\\\": \"detentive\",\\n    \\\"popularity\\\": 74048\\n  },\\n  {\\n    \\\"tag\\\": \"supravital\",\\n    \\\"popularity\\\": 73780\\n  },\\n  {\\n    \\\"tag\\\": \"Vespertilionidae\",\\n    \\\"popularity\\\": 73513\\n  },\\n  {\\n    \\\"tag\\\": \"parka\",\\n    \\\"popularity\\\": 73248\\n  },\\n  {\\n    \\\"tag\\\": \"pickaway\",\\n    \\\"popularity\\\": 72984\\n  },\\n  {\\n    \\\"tag\\\": \"oleaceous\",\\n    \\\"popularity\\\": 72722\\n  },\\n  {\\n    \\\"tag\\\": \"anticogitative\",\\n    \\\"popularity\\\": 72462\\n  },\\n  {\\n    \\\"tag\\\": \"woe\",\\n    \\\"popularity\\\": 72203\\n  },\\n  {\\n    \\\"tag\\\": \"skeuomorph\",\\n    \\\"popularity\\\": 71945\\n  },\\n  {\\n    \\\"tag\\\": \"helpmeet\",\\n    \\\"popularity\\\": 71689\\n  },\\n  {\\n    \\\"tag\\\": \"Hexactinellida brickmaking\",\\n    \\\"popularity\\\": 71434\\n  },\\n  {\\n    \\\"tag\\\": \"resink\",\\n    \\\"popularity\\\": 71180\\n  },\\n  {\\n    \\\"tag\\\": \"diluter\",\\n    \\\"popularity\\\": 70928\\n  },\\n  {\\n    \\\"tag\\\": \"micromicron\",\\n    \\\"popularity\\\": 70677\\n  },\\n  {\\n    \\\"tag\\\": \"parentage\",\\n    \\\"popularity\\\": 70428\\n  },\\n  {\\n    \\\"tag\\\": \"galactorrhoea\",\\n    \\\"popularity\\\": 70180\\n  },\\n  {\\n    \\\"tag\\\": \"gey\",\\n    \\\"popularity\\\": 69934\\n  },\\n  {\\n    \\\"tag\\\": \"gesticulatory\",\\n    \\\"popularity\\\": 69689\\n  },\\n  {\\n    \\\"tag\\\": \"wergil\",\\n    \\\"popularity\\\": 69445\\n  },\\n  {\\n    \\\"tag\\\": \"Lecanora\",\\n    \\\"popularity\\\": 69202\\n  },\\n  {\\n    \\\"tag\\\": \"malanders karst\",\\n    \\\"popularity\\\": 68961\\n  },\\n  {\\n    \\\"tag\\\": \"vibetoite\",\\n    \\\"popularity\\\": 68721\\n  },\\n  {\\n    \\\"tag\\\": \"unrequitedness\",\\n    \\\"popularity\\\": 68483\\n  },\\n  {\\n    \\\"tag\\\": \"outwash\",\\n    \\\"popularity\\\": 68245\\n  },\\n  {\\n    \\\"tag\\\": \"unsacred\",\\n    \\\"popularity\\\": 68009\\n  },\\n  {\\n    \\\"tag\\\": \"unabetted dividend\",\\n    \\\"popularity\\\": 67775\\n  },\\n  {\\n    \\\"tag\\\": \"untraveling\",\\n    \\\"popularity\\\": 67541\\n  },\\n  {\\n    \\\"tag\\\": \"thermobattery\",\\n    \\\"popularity\\\": 67309\\n  },\\n  {\\n    \\\"tag\\\": \"polypragmist\",\\n    \\\"popularity\\\": 67078\\n  },\\n  {\\n    \\\"tag\\\": \"irrefutableness\",\\n    \\\"popularity\\\": 66848\\n  },\\n  {\\n    \\\"tag\\\": \"remiges\",\\n    \\\"popularity\\\": 66620\\n  },\\n  {\\n    \\\"tag\\\": \"implode\",\\n    \\\"popularity\\\": 66393\\n  },\\n  {\\n    \\\"tag\\\": \"superfluousness\",\\n    \\\"popularity\\\": 66166\\n  },\\n  {\\n    \\\"tag\\\": \"croakily unalleviated\",\\n    \\\"popularity\\\": 65942\\n  },\\n  {\\n    \\\"tag\\\": \"edicule\",\\n    \\\"popularity\\\": 65718\\n  },\\n  {\\n    \\\"tag\\\": \"entophytous\",\\n    \\\"popularity\\\": 65495\\n  },\\n  {\\n    \\\"tag\\\": \"benefactorship Toryish\",\\n    \\\"popularity\\\": 65274\\n  },\\n  {\\n    \\\"tag\\\": \"pseudoamateurish\",\\n    \\\"popularity\\\": 65054\\n  },\\n  {\\n    \\\"tag\\\": \"flueless Iguanodontoidea snipnose\",\\n    \\\"popularity\\\": 64835\\n  },\\n  {\\n    \\\"tag\\\": \"zealotical Zamicrus interpole\",\\n    \\\"popularity\\\": 64617\\n  },\\n  {\\n    \\\"tag\\\": \"whereabout\",\\n    \\\"popularity\\\": 64401\\n  },\\n  {\\n    \\\"tag\\\": \"benzazide\",\\n    \\\"popularity\\\": 64185\\n  },\\n  {\\n    \\\"tag\\\": \"pokeweed\",\\n    \\\"popularity\\\": 63971\\n  },\\n  {\\n    \\\"tag\\\": \"calamitoid\",\\n    \\\"popularity\\\": 63757\\n  },\\n  {\\n    \\\"tag\\\": \"sporozoal\",\\n    \\\"popularity\\\": 63545\\n  },\\n  {\\n    \\\"tag\\\": \"physcioid Welshwoman\",\\n    \\\"popularity\\\": 63334\\n  },\\n  {\\n    \\\"tag\\\": \"wanting\",\\n    \\\"popularity\\\": 63124\\n  },\\n  {\\n    \\\"tag\\\": \"unencumbering\",\\n    \\\"popularity\\\": 62915\\n  },\\n  {\\n    \\\"tag\\\": \"Tupi\",\\n    \\\"popularity\\\": 62707\\n  },\\n  {\\n    \\\"tag\\\": \"potbank\",\\n    \\\"popularity\\\": 62501\\n  },\\n  {\\n    \\\"tag\\\": \"bulked\",\\n    \\\"popularity\\\": 62295\\n  },\\n  {\\n    \\\"tag\\\": \"uparise\",\\n    \\\"popularity\\\": 62090\\n  },\\n  {\\n    \\\"tag\\\": \"Sudra\",\\n    \\\"popularity\\\": 61887\\n  },\\n  {\\n    \\\"tag\\\": \"hyperscrupulosity\",\\n    \\\"popularity\\\": 61684\\n  },\\n  {\\n    \\\"tag\\\": \"subterraneously unmaid\",\\n    \\\"popularity\\\": 61483\\n  },\\n  {\\n    \\\"tag\\\": \"poisonousness\",\\n    \\\"popularity\\\": 61282\\n  },\\n  {\\n    \\\"tag\\\": \"phare\",\\n    \\\"popularity\\\": 61083\\n  },\\n  {\\n    \\\"tag\\\": \"dicynodont\",\\n    \\\"popularity\\\": 60884\\n  },\\n  {\\n    \\\"tag\\\": \"chewer\",\\n    \\\"popularity\\\": 60687\\n  },\\n  {\\n    \\\"tag\\\": \"uliginous\",\\n    \\\"popularity\\\": 60490\\n  },\\n  {\\n    \\\"tag\\\": \"tinman\",\\n    \\\"popularity\\\": 60295\\n  },\\n  {\\n    \\\"tag\\\": \"coconut\",\\n    \\\"popularity\\\": 60100\\n  },\\n  {\\n    \\\"tag\\\": \"phryganeoid\",\\n    \\\"popularity\\\": 59907\\n  },\\n  {\\n    \\\"tag\\\": \"bismillah\",\\n    \\\"popularity\\\": 59714\\n  },\\n  {\\n    \\\"tag\\\": \"tautomeric\",\\n    \\\"popularity\\\": 59523\\n  },\\n  {\\n    \\\"tag\\\": \"jerquer\",\\n    \\\"popularity\\\": 59332\\n  },\\n  {\\n    \\\"tag\\\": \"Dryopithecinae\",\\n    \\\"popularity\\\": 59143\\n  },\\n  {\\n    \\\"tag\\\": \"ghizite\",\\n    \\\"popularity\\\": 58954\\n  },\\n  {\\n    \\\"tag\\\": \"unliveable\",\\n    \\\"popularity\\\": 58766\\n  },\\n  {\\n    \\\"tag\\\": \"craftsmaster\",\\n    \\\"popularity\\\": 58579\\n  },\\n  {\\n    \\\"tag\\\": \"semiscenic\",\\n    \\\"popularity\\\": 58394\\n  },\\n  {\\n    \\\"tag\\\": \"danaid\",\\n    \\\"popularity\\\": 58209\\n  },\\n  {\\n    \\\"tag\\\": \"flawful\",\\n    \\\"popularity\\\": 58025\\n  },\\n  {\\n    \\\"tag\\\": \"risibleness\",\\n    \\\"popularity\\\": 57841\\n  },\\n  {\\n    \\\"tag\\\": \"Muscovite\",\\n    \\\"popularity\\\": 57659\\n  },\\n  {\\n    \\\"tag\\\": \"snaringly\",\\n    \\\"popularity\\\": 57478\\n  },\\n  {\\n    \\\"tag\\\": \"brilliantwise\",\\n    \\\"popularity\\\": 57297\\n  },\\n  {\\n    \\\"tag\\\": \"plebeity\",\\n    \\\"popularity\\\": 57118\\n  },\\n  {\\n    \\\"tag\\\": \"historicalness\",\\n    \\\"popularity\\\": 56939\\n  },\\n  {\\n    \\\"tag\\\": \"piecemeal\",\\n    \\\"popularity\\\": 56761\\n  },\\n  {\\n    \\\"tag\\\": \"maxillipedary\",\\n    \\\"popularity\\\": 56584\\n  },\\n  {\\n    \\\"tag\\\": \"Hypenantron\",\\n    \\\"popularity\\\": 56408\\n  },\\n  {\\n    \\\"tag\\\": \"quaintness avigate\",\\n    \\\"popularity\\\": 56233\\n  },\\n  {\\n    \\\"tag\\\": \"ave\",\\n    \\\"popularity\\\": 56059\\n  },\\n  {\\n    \\\"tag\\\": \"mediaevally\",\\n    \\\"popularity\\\": 55885\\n  },\\n  {\\n    \\\"tag\\\": \"brucite\",\\n    \\\"popularity\\\": 55712\\n  },\\n  {\\n    \\\"tag\\\": \"Schwendenerian\",\\n    \\\"popularity\\\": 55541\\n  },\\n  {\\n    \\\"tag\\\": \"julole\",\\n    \\\"popularity\\\": 55370\\n  },\\n  {\\n    \\\"tag\\\": \"palaeolith\",\\n    \\\"popularity\\\": 55199\\n  },\\n  {\\n    \\\"tag\\\": \"cotyledonary\",\\n    \\\"popularity\\\": 55030\\n  },\\n  {\\n    \\\"tag\\\": \"rond\",\\n    \\\"popularity\\\": 54861\\n  },\\n  {\\n    \\\"tag\\\": \"boomster tassoo\",\\n    \\\"popularity\\\": 54694\\n  },\\n  {\\n    \\\"tag\\\": \"cattishly\",\\n    \\\"popularity\\\": 54527\\n  },\\n  {\\n    \\\"tag\\\": \"tonguefence\",\\n    \\\"popularity\\\": 54360\\n  },\\n  {\\n    \\\"tag\\\": \"hexastylar triskele\",\\n    \\\"popularity\\\": 54195\\n  },\\n  {\\n    \\\"tag\\\": \"ariot\",\\n    \\\"popularity\\\": 54030\\n  },\\n  {\\n    \\\"tag\\\": \"intarsist\",\\n    \\\"popularity\\\": 53867\\n  },\\n  {\\n    \\\"tag\\\": \"Oscines\",\\n    \\\"popularity\\\": 53704\\n  },\\n  {\\n    \\\"tag\\\": \"Spaniolize\",\\n    \\\"popularity\\\": 53541\\n  },\\n  {\\n    \\\"tag\\\": \"smellfungus\",\\n    \\\"popularity\\\": 53380\\n  },\\n  {\\n    \\\"tag\\\": \"redisplay\",\\n    \\\"popularity\\\": 53219\\n  },\\n  {\\n    \\\"tag\\\": \"phosphene\",\\n    \\\"popularity\\\": 53059\\n  },\\n  {\\n    \\\"tag\\\": \"phycomycete\",\\n    \\\"popularity\\\": 52900\\n  },\\n  {\\n    \\\"tag\\\": \"prophetic\",\\n    \\\"popularity\\\": 52741\\n  },\\n  {\\n    \\\"tag\\\": \"overtrustful\",\\n    \\\"popularity\\\": 52584\\n  },\\n  {\\n    \\\"tag\\\": \"pinitol\",\\n    \\\"popularity\\\": 52427\\n  },\\n  {\\n    \\\"tag\\\": \"asthmatic\",\\n    \\\"popularity\\\": 52270\\n  },\\n  {\\n    \\\"tag\\\": \"convulsive\",\\n    \\\"popularity\\\": 52115\\n  },\\n  {\\n    \\\"tag\\\": \"draughtswoman\",\\n    \\\"popularity\\\": 51960\\n  },\\n  {\\n    \\\"tag\\\": \"unetymologizable\",\\n    \\\"popularity\\\": 51806\\n  },\\n  {\\n    \\\"tag\\\": \"centrarchoid\",\\n    \\\"popularity\\\": 51652\\n  },\\n  {\\n    \\\"tag\\\": \"mesioincisal\",\\n    \\\"popularity\\\": 51500\\n  },\\n  {\\n    \\\"tag\\\": \"transbaikal\",\\n    \\\"popularity\\\": 51348\\n  },\\n  {\\n    \\\"tag\\\": \"silveriness\",\\n    \\\"popularity\\\": 51196\\n  },\\n  {\\n    \\\"tag\\\": \"costotomy\",\\n    \\\"popularity\\\": 51046\\n  },\\n  {\\n    \\\"tag\\\": \"caracore\",\\n    \\\"popularity\\\": 50896\\n  },\\n  {\\n    \\\"tag\\\": \"depotentiation\",\\n    \\\"popularity\\\": 50747\\n  },\\n  {\\n    \\\"tag\\\": \"glossoepiglottidean\",\\n    \\\"popularity\\\": 50598\\n  },\\n  {\\n    \\\"tag\\\": \"upswell\",\\n    \\\"popularity\\\": 50450\\n  },\\n  {\\n    \\\"tag\\\": \"flecnodal\",\\n    \\\"popularity\\\": 50303\\n  },\\n  {\\n    \\\"tag\\\": \"coventrate\",\\n    \\\"popularity\\\": 50157\\n  },\\n  {\\n    \\\"tag\\\": \"duchesse\",\\n    \\\"popularity\\\": 50011\\n  },\\n  {\\n    \\\"tag\\\": \"excisemanship trophied\",\\n    \\\"popularity\\\": 49866\\n  },\\n  {\\n    \\\"tag\\\": \"cytinaceous\",\\n    \\\"popularity\\\": 49721\\n  },\\n  {\\n    \\\"tag\\\": \"assuringly\",\\n    \\\"popularity\\\": 49577\\n  },\\n  {\\n    \\\"tag\\\": \"unconducted upliftitis\",\\n    \\\"popularity\\\": 49434\\n  },\\n  {\\n    \\\"tag\\\": \"rachicentesis\",\\n    \\\"popularity\\\": 49292\\n  },\\n  {\\n    \\\"tag\\\": \"antiangular\",\\n    \\\"popularity\\\": 49150\\n  },\\n  {\\n    \\\"tag\\\": \"advisal\",\\n    \\\"popularity\\\": 49008\\n  },\\n  {\\n    \\\"tag\\\": \"birdcatcher\",\\n    \\\"popularity\\\": 48868\\n  },\\n  {\\n    \\\"tag\\\": \"secularistic\",\\n    \\\"popularity\\\": 48728\\n  },\\n  {\\n    \\\"tag\\\": \"grandeeism superinformal\",\\n    \\\"popularity\\\": 48588\\n  },\\n  {\\n    \\\"tag\\\": \"unapprehension\",\\n    \\\"popularity\\\": 48449\\n  },\\n  {\\n    \\\"tag\\\": \"excipulum\",\\n    \\\"popularity\\\": 48311\\n  },\\n  {\\n    \\\"tag\\\": \"decimole\",\\n    \\\"popularity\\\": 48174\\n  },\\n  {\\n    \\\"tag\\\": \"semidrachm\",\\n    \\\"popularity\\\": 48037\\n  },\\n  {\\n    \\\"tag\\\": \"uvulotome\",\\n    \\\"popularity\\\": 47901\\n  },\\n  {\\n    \\\"tag\\\": \"Lemaneaceae\",\\n    \\\"popularity\\\": 47765\\n  },\\n  {\\n    \\\"tag\\\": \"corrade\",\\n    \\\"popularity\\\": 47630\\n  },\\n  {\\n    \\\"tag\\\": \"Kuroshio\",\\n    \\\"popularity\\\": 47495\\n  },\\n  {\\n    \\\"tag\\\": \"Araliophyllum\",\\n    \\\"popularity\\\": 47361\\n  },\\n  {\\n    \\\"tag\\\": \"victoriousness cardiosphygmograph\",\\n    \\\"popularity\\\": 47228\\n  },\\n  {\\n    \\\"tag\\\": \"reinvent\",\\n    \\\"popularity\\\": 47095\\n  },\\n  {\\n    \\\"tag\\\": \"Macrotolagus\",\\n    \\\"popularity\\\": 46963\\n  },\\n  {\\n    \\\"tag\\\": \"strenuousness\",\\n    \\\"popularity\\\": 46831\\n  },\\n  {\\n    \\\"tag\\\": \"deviability\",\\n    \\\"popularity\\\": 46700\\n  },\\n  {\\n    \\\"tag\\\": \"phyllospondylous\",\\n    \\\"popularity\\\": 46570\\n  },\\n  {\\n    \\\"tag\\\": \"bisect rudderhole\",\\n    \\\"popularity\\\": 46440\\n  },\\n  {\\n    \\\"tag\\\": \"crownwork\",\\n    \\\"popularity\\\": 46311\\n  },\\n  {\\n    \\\"tag\\\": \"Ascalabota\",\\n    \\\"popularity\\\": 46182\\n  },\\n  {\\n    \\\"tag\\\": \"prostatomyomectomy\",\\n    \\\"popularity\\\": 46054\\n  },\\n  {\\n    \\\"tag\\\": \"neurosyphilis\",\\n    \\\"popularity\\\": 45926\\n  },\\n  {\\n    \\\"tag\\\": \"tabloid scraplet\",\\n    \\\"popularity\\\": 45799\\n  },\\n  {\\n    \\\"tag\\\": \"nonmedullated servility\",\\n    \\\"popularity\\\": 45673\\n  },\\n  {\\n    \\\"tag\\\": \"melopoeic practicalization\",\\n    \\\"popularity\\\": 45547\\n  },\\n  {\\n    \\\"tag\\\": \"nonrhythmic\",\\n    \\\"popularity\\\": 45421\\n  },\\n  {\\n    \\\"tag\\\": \"deplorer\",\\n    \\\"popularity\\\": 45296\\n  },\\n  {\\n    \\\"tag\\\": \"Ophion\",\\n    \\\"popularity\\\": 45172\\n  },\\n  {\\n    \\\"tag\\\": \"subprioress\",\\n    \\\"popularity\\\": 45048\\n  },\\n  {\\n    \\\"tag\\\": \"semiregular\",\\n    \\\"popularity\\\": 44925\\n  },\\n  {\\n    \\\"tag\\\": \"praelection\",\\n    \\\"popularity\\\": 44802\\n  },\\n  {\\n    \\\"tag\\\": \"discinct\",\\n    \\\"popularity\\\": 44680\\n  },\\n  {\\n    \\\"tag\\\": \"preplace\",\\n    \\\"popularity\\\": 44558\\n  },\\n  {\\n    \\\"tag\\\": \"paternoster\",\\n    \\\"popularity\\\": 44437\\n  },\\n  {\\n    \\\"tag\\\": \"suboccipital\",\\n    \\\"popularity\\\": 44316\\n  },\\n  {\\n    \\\"tag\\\": \"Teutophil\",\\n    \\\"popularity\\\": 44196\\n  },\\n  {\\n    \\\"tag\\\": \"tracheole\",\\n    \\\"popularity\\\": 44076\\n  },\\n  {\\n    \\\"tag\\\": \"subsmile\",\\n    \\\"popularity\\\": 43957\\n  },\\n  {\\n    \\\"tag\\\": \"nonapostatizing\",\\n    \\\"popularity\\\": 43839\\n  },\\n  {\\n    \\\"tag\\\": \"cleidotomy\",\\n    \\\"popularity\\\": 43720\\n  },\\n  {\\n    \\\"tag\\\": \"hingle\",\\n    \\\"popularity\\\": 43603\\n  },\\n  {\\n    \\\"tag\\\": \"jocoque\",\\n    \\\"popularity\\\": 43486\\n  },\\n  {\\n    \\\"tag\\\": \"trundler notidanian\",\\n    \\\"popularity\\\": 43369\\n  },\\n  {\\n    \\\"tag\\\": \"strangling misdaub\",\\n    \\\"popularity\\\": 43253\\n  },\\n  {\\n    \\\"tag\\\": \"noncancellable\",\\n    \\\"popularity\\\": 43137\\n  },\\n  {\\n    \\\"tag\\\": \"lavabo\",\\n    \\\"popularity\\\": 43022\\n  },\\n  {\\n    \\\"tag\\\": \"lanterloo\",\\n    \\\"popularity\\\": 42907\\n  },\\n  {\\n    \\\"tag\\\": \"uncitizenly\",\\n    \\\"popularity\\\": 42793\\n  },\\n  {\\n    \\\"tag\\\": \"autoturning\",\\n    \\\"popularity\\\": 42679\\n  },\\n  {\\n    \\\"tag\\\": \"Haganah\",\\n    \\\"popularity\\\": 42566\\n  },\\n  {\\n    \\\"tag\\\": \"Glecoma\",\\n    \\\"popularity\\\": 42453\\n  },\\n  {\\n    \\\"tag\\\": \"membered\",\\n    \\\"popularity\\\": 42341\\n  },\\n  {\\n    \\\"tag\\\": \"consuetudinal\",\\n    \\\"popularity\\\": 42229\\n  },\\n  {\\n    \\\"tag\\\": \"gatehouse\",\\n    \\\"popularity\\\": 42117\\n  },\\n  {\\n    \\\"tag\\\": \"tetherball\",\\n    \\\"popularity\\\": 42006\\n  },\\n  {\\n    \\\"tag\\\": \"counterrevolutionist numismatical\",\\n    \\\"popularity\\\": 41896\\n  },\\n  {\\n    \\\"tag\\\": \"pagehood plateiasmus\",\\n    \\\"popularity\\\": 41786\\n  },\\n  {\\n    \\\"tag\\\": \"pelterer\",\\n    \\\"popularity\\\": 41676\\n  },\\n  {\\n    \\\"tag\\\": \"splenemphraxis\",\\n    \\\"popularity\\\": 41567\\n  },\\n  {\\n    \\\"tag\\\": \"Crypturidae\",\\n    \\\"popularity\\\": 41458\\n  },\\n  {\\n    \\\"tag\\\": \"caboodle\",\\n    \\\"popularity\\\": 41350\\n  },\\n  {\\n    \\\"tag\\\": \"Filaria\",\\n    \\\"popularity\\\": 41242\\n  },\\n  {\\n    \\\"tag\\\": \"noninvincibility\",\\n    \\\"popularity\\\": 41135\\n  },\\n  {\\n    \\\"tag\\\": \"preadvertisement\",\\n    \\\"popularity\\\": 41028\\n  },\\n  {\\n    \\\"tag\\\": \"bathrobe\",\\n    \\\"popularity\\\": 40921\\n  },\\n  {\\n    \\\"tag\\\": \"nitrifier\",\\n    \\\"popularity\\\": 40815\\n  },\\n  {\\n    \\\"tag\\\": \"furthermore\",\\n    \\\"popularity\\\": 40709\\n  },\\n  {\\n    \\\"tag\\\": \"recrate\",\\n    \\\"popularity\\\": 40604\\n  },\\n  {\\n    \\\"tag\\\": \"inexist\",\\n    \\\"popularity\\\": 40499\\n  },\\n  {\\n    \\\"tag\\\": \"Mocoan\",\\n    \\\"popularity\\\": 40395\\n  },\\n  {\\n    \\\"tag\\\": \"forint\",\\n    \\\"popularity\\\": 40291\\n  },\\n  {\\n    \\\"tag\\\": \"cardiomyoliposis\",\\n    \\\"popularity\\\": 40187\\n  },\\n  {\\n    \\\"tag\\\": \"channeling\",\\n    \\\"popularity\\\": 40084\\n  },\\n  {\\n    \\\"tag\\\": \"quebrachine\",\\n    \\\"popularity\\\": 39981\\n  },\\n  {\\n    \\\"tag\\\": \"magistery\",\\n    \\\"popularity\\\": 39879\\n  },\\n  {\\n    \\\"tag\\\": \"koko\",\\n    \\\"popularity\\\": 39777\\n  },\\n  {\\n    \\\"tag\\\": \"nobilify\",\\n    \\\"popularity\\\": 39676\\n  },\\n  {\\n    \\\"tag\\\": \"articulate taprooted\",\\n    \\\"popularity\\\": 39575\\n  },\\n  {\\n    \\\"tag\\\": \"cardiotonic Nicaragua\",\\n    \\\"popularity\\\": 39474\\n  },\\n  {\\n    \\\"tag\\\": \"assertiveness\",\\n    \\\"popularity\\\": 39374\\n  },\\n  {\\n    \\\"tag\\\": \"springtail\",\\n    \\\"popularity\\\": 39274\\n  },\\n  {\\n    \\\"tag\\\": \"spontoon\",\\n    \\\"popularity\\\": 39174\\n  },\\n  {\\n    \\\"tag\\\": \"plesiobiosis\",\\n    \\\"popularity\\\": 39075\\n  },\\n  {\\n    \\\"tag\\\": \"rooinek\",\\n    \\\"popularity\\\": 38976\\n  },\\n  {\\n    \\\"tag\\\": \"hairif falsehood\",\\n    \\\"popularity\\\": 38878\\n  },\\n  {\\n    \\\"tag\\\": \"synodally\",\\n    \\\"popularity\\\": 38780\\n  },\\n  {\\n    \\\"tag\\\": \"biodynamics\",\\n    \\\"popularity\\\": 38683\\n  },\\n  {\\n    \\\"tag\\\": \"trickling\",\\n    \\\"popularity\\\": 38585\\n  },\\n  {\\n    \\\"tag\\\": \"oxfly daystar\",\\n    \\\"popularity\\\": 38489\\n  },\\n  {\\n    \\\"tag\\\": \"epicycloidal\",\\n    \\\"popularity\\\": 38392\\n  },\\n  {\\n    \\\"tag\\\": \"shorthand\",\\n    \\\"popularity\\\": 38296\\n  },\\n  {\\n    \\\"tag\\\": \"herpolhode\",\\n    \\\"popularity\\\": 38201\\n  },\\n  {\\n    \\\"tag\\\": \"polysynthesism\",\\n    \\\"popularity\\\": 38105\\n  },\\n  {\\n    \\\"tag\\\": \"cany\",\\n    \\\"popularity\\\": 38010\\n  },\\n  {\\n    \\\"tag\\\": \"sideage\",\\n    \\\"popularity\\\": 37916\\n  },\\n  {\\n    \\\"tag\\\": \"strainableness\",\\n    \\\"popularity\\\": 37822\\n  },\\n  {\\n    \\\"tag\\\": \"superformidable\",\\n    \\\"popularity\\\": 37728\\n  },\\n  {\\n    \\\"tag\\\": \"slendang\",\\n    \\\"popularity\\\": 37634\\n  },\\n  {\\n    \\\"tag\\\": \"impropriation\",\\n    \\\"popularity\\\": 37541\\n  },\\n  {\\n    \\\"tag\\\": \"ficklehearted\",\\n    \\\"popularity\\\": 37449\\n  },\\n  {\\n    \\\"tag\\\": \"wintrify\",\\n    \\\"popularity\\\": 37356\\n  },\\n  {\\n    \\\"tag\\\": \"geomorphogenist\",\\n    \\\"popularity\\\": 37264\\n  },\\n  {\\n    \\\"tag\\\": \"smuggleable\",\\n    \\\"popularity\\\": 37173\\n  },\\n  {\\n    \\\"tag\\\": \"delapsion\",\\n    \\\"popularity\\\": 37081\\n  },\\n  {\\n    \\\"tag\\\": \"projective\",\\n    \\\"popularity\\\": 36990\\n  },\\n  {\\n    \\\"tag\\\": \"unglue exfoliation\",\\n    \\\"popularity\\\": 36900\\n  },\\n  {\\n    \\\"tag\\\": \"Acerae\",\\n    \\\"popularity\\\": 36810\\n  },\\n  {\\n    \\\"tag\\\": \"unstaged\",\\n    \\\"popularity\\\": 36720\\n  },\\n  {\\n    \\\"tag\\\": \"ranal\",\\n    \\\"popularity\\\": 36630\\n  },\\n  {\\n    \\\"tag\\\": \"worrier\",\\n    \\\"popularity\\\": 36541\\n  },\\n  {\\n    \\\"tag\\\": \"unhid\",\\n    \\\"popularity\\\": 36452\\n  },\\n  {\\n    \\\"tag\\\": \"adequation\",\\n    \\\"popularity\\\": 36363\\n  },\\n  {\\n    \\\"tag\\\": \"strongylid Sokotri\",\\n    \\\"popularity\\\": 36275\\n  },\\n  {\\n    \\\"tag\\\": \"fumingly\",\\n    \\\"popularity\\\": 36187\\n  },\\n  {\\n    \\\"tag\\\": \"gynosporangium phaenogenetic\",\\n    \\\"popularity\\\": 36100\\n  },\\n  {\\n    \\\"tag\\\": \"uniunguiculate\",\\n    \\\"popularity\\\": 36012\\n  },\\n  {\\n    \\\"tag\\\": \"prudelike\",\\n    \\\"popularity\\\": 35926\\n  },\\n  {\\n    \\\"tag\\\": \"seminomata\",\\n    \\\"popularity\\\": 35839\\n  },\\n  {\\n    \\\"tag\\\": \"trinklet\",\\n    \\\"popularity\\\": 35753\\n  },\\n  {\\n    \\\"tag\\\": \"risorial\",\\n    \\\"popularity\\\": 35667\\n  },\\n  {\\n    \\\"tag\\\": \"pericardiocentesis\",\\n    \\\"popularity\\\": 35581\\n  },\\n  {\\n    \\\"tag\\\": \"filmist\",\\n    \\\"popularity\\\": 35496\\n  },\\n  {\\n    \\\"tag\\\": \"Nana\",\\n    \\\"popularity\\\": 35411\\n  },\\n  {\\n    \\\"tag\\\": \"cynipoid\",\\n    \\\"popularity\\\": 35326\\n  },\\n  {\\n    \\\"tag\\\": \"cteniform\",\\n    \\\"popularity\\\": 35242\\n  },\\n  {\\n    \\\"tag\\\": \"semiflex\",\\n    \\\"popularity\\\": 35158\\n  },\\n  {\\n    \\\"tag\\\": \"solstitially\",\\n    \\\"popularity\\\": 35074\\n  },\\n  {\\n    \\\"tag\\\": \"Algarsife\",\\n    \\\"popularity\\\": 34991\\n  },\\n  {\\n    \\\"tag\\\": \"noncriminal\",\\n    \\\"popularity\\\": 34908\\n  },\\n  {\\n    \\\"tag\\\": \"compassion\",\\n    \\\"popularity\\\": 34825\\n  },\\n  {\\n    \\\"tag\\\": \"Buddhic\",\\n    \\\"popularity\\\": 34743\\n  },\\n  {\\n    \\\"tag\\\": \"vellicative dactylically hotfoot\",\\n    \\\"popularity\\\": 34661\\n  },\\n  {\\n    \\\"tag\\\": \"chicory\",\\n    \\\"popularity\\\": 34579\\n  },\\n  {\\n    \\\"tag\\\": \"transperitoneally\",\\n    \\\"popularity\\\": 34497\\n  },\\n  {\\n    \\\"tag\\\": \"pennae\",\\n    \\\"popularity\\\": 34416\\n  },\\n  {\\n    \\\"tag\\\": \"Flamandize\",\\n    \\\"popularity\\\": 34335\\n  },\\n  {\\n    \\\"tag\\\": \"underviewer\",\\n    \\\"popularity\\\": 34254\\n  },\\n  {\\n    \\\"tag\\\": \"assoil\",\\n    \\\"popularity\\\": 34174\\n  },\\n  {\\n    \\\"tag\\\": \"saccharobacillus\",\\n    \\\"popularity\\\": 34094\\n  },\\n  {\\n    \\\"tag\\\": \"biacetylene\",\\n    \\\"popularity\\\": 34014\\n  },\\n  {\\n    \\\"tag\\\": \"mouchardism\",\\n    \\\"popularity\\\": 33935\\n  },\\n  {\\n    \\\"tag\\\": \"anisomeric\",\\n    \\\"popularity\\\": 33856\\n  },\\n  {\\n    \\\"tag\\\": \"digestive\",\\n    \\\"popularity\\\": 33777\\n  },\\n  {\\n    \\\"tag\\\": \"darlingly\",\\n    \\\"popularity\\\": 33698\\n  },\\n  {\\n    \\\"tag\\\": \"liman\",\\n    \\\"popularity\\\": 33620\\n  },\\n  {\\n    \\\"tag\\\": \"soldanrie\",\\n    \\\"popularity\\\": 33542\\n  },\\n  {\\n    \\\"tag\\\": \"sully\",\\n    \\\"popularity\\\": 33464\\n  },\\n  {\\n    \\\"tag\\\": \"brightsmith\",\\n    \\\"popularity\\\": 33387\\n  },\\n  {\\n    \\\"tag\\\": \"inwrap antiliturgist ureterocervical\",\\n    \\\"popularity\\\": 33309\\n  },\\n  {\\n    \\\"tag\\\": \"discommodity\",\\n    \\\"popularity\\\": 33232\\n  },\\n  {\\n    \\\"tag\\\": \"typical aggrandizer\",\\n    \\\"popularity\\\": 33156\\n  },\\n  {\\n    \\\"tag\\\": \"xenogeny\",\\n    \\\"popularity\\\": 33079\\n  },\\n  {\\n    \\\"tag\\\": \"uncountrified\",\\n    \\\"popularity\\\": 33003\\n  },\\n  {\\n    \\\"tag\\\": \"Podarge\",\\n    \\\"popularity\\\": 32928\\n  },\\n  {\\n    \\\"tag\\\": \"uninterviewed\",\\n    \\\"popularity\\\": 32852\\n  },\\n  {\\n    \\\"tag\\\": \"underprior\",\\n    \\\"popularity\\\": 32777\\n  },\\n  {\\n    \\\"tag\\\": \"leiomyomatous\",\\n    \\\"popularity\\\": 32702\\n  },\\n  {\\n    \\\"tag\\\": \"postdysenteric\",\\n    \\\"popularity\\\": 32627\\n  },\\n  {\\n    \\\"tag\\\": \"Fusicladium\",\\n    \\\"popularity\\\": 32553\\n  },\\n  {\\n    \\\"tag\\\": \"Dulcinea\",\\n    \\\"popularity\\\": 32478\\n  },\\n  {\\n    \\\"tag\\\": \"interspersion\",\\n    \\\"popularity\\\": 32404\\n  },\\n  {\\n    \\\"tag\\\": \"preobligate\",\\n    \\\"popularity\\\": 32331\\n  },\\n  {\\n    \\\"tag\\\": \"subaggregate\",\\n    \\\"popularity\\\": 32257\\n  },\\n  {\\n    \\\"tag\\\": \"grammarianism\",\\n    \\\"popularity\\\": 32184\\n  },\\n  {\\n    \\\"tag\\\": \"palikar\",\\n    \\\"popularity\\\": 32111\\n  },\\n  {\\n    \\\"tag\\\": \"facileness\",\\n    \\\"popularity\\\": 32039\\n  },\\n  {\\n    \\\"tag\\\": \"deuterofibrinose\",\\n    \\\"popularity\\\": 31966\\n  },\\n  {\\n    \\\"tag\\\": \"pseudesthesia\",\\n    \\\"popularity\\\": 31894\\n  },\\n  {\\n    \\\"tag\\\": \"sedimentary\",\\n    \\\"popularity\\\": 31822\\n  },\\n  {\\n    \\\"tag\\\": \"typewrite\",\\n    \\\"popularity\\\": 31751\\n  },\\n  {\\n    \\\"tag\\\": \"immemorable\",\\n    \\\"popularity\\\": 31679\\n  },\\n  {\\n    \\\"tag\\\": \"Myrtus\",\\n    \\\"popularity\\\": 31608\\n  },\\n  {\\n    \\\"tag\\\": \"hauchecornite\",\\n    \\\"popularity\\\": 31537\\n  },\\n  {\\n    \\\"tag\\\": \"galleylike\",\\n    \\\"popularity\\\": 31467\\n  },\\n  {\\n    \\\"tag\\\": \"thimber\",\\n    \\\"popularity\\\": 31396\\n  },\\n  {\\n    \\\"tag\\\": \"Hegelianism\",\\n    \\\"popularity\\\": 31326\\n  },\\n  {\\n    \\\"tag\\\": \"strig\",\\n    \\\"popularity\\\": 31256\\n  },\\n  {\\n    \\\"tag\\\": \"skyre\",\\n    \\\"popularity\\\": 31187\\n  },\\n  {\\n    \\\"tag\\\": \"eupepticism\",\\n    \\\"popularity\\\": 31117\\n  },\\n  {\\n    \\\"tag\\\": \"eponymism\",\\n    \\\"popularity\\\": 31048\\n  },\\n  {\\n    \\\"tag\\\": \"flunkeyhood\",\\n    \\\"popularity\\\": 30979\\n  },\\n  {\\n    \\\"tag\\\": \"Abama\",\\n    \\\"popularity\\\": 30911\\n  },\\n  {\\n    \\\"tag\\\": \"adiadochokinesis\",\\n    \\\"popularity\\\": 30842\\n  },\\n  {\\n    \\\"tag\\\": \"spendthrifty\",\\n    \\\"popularity\\\": 30774\\n  },\\n  {\\n    \\\"tag\\\": \"chalcedony\",\\n    \\\"popularity\\\": 30706\\n  },\\n  {\\n    \\\"tag\\\": \"authorism\",\\n    \\\"popularity\\\": 30638\\n  },\\n  {\\n    \\\"tag\\\": \"nasturtium\",\\n    \\\"popularity\\\": 30571\\n  },\\n  {\\n    \\\"tag\\\": \"Acanthocereus\",\\n    \\\"popularity\\\": 30504\\n  },\\n  {\\n    \\\"tag\\\": \"uncollapsible\",\\n    \\\"popularity\\\": 30437\\n  },\\n  {\\n    \\\"tag\\\": \"excursionist\",\\n    \\\"popularity\\\": 30370\\n  },\\n  {\\n    \\\"tag\\\": \"fogbow\",\\n    \\\"popularity\\\": 30303\\n  },\\n  {\\n    \\\"tag\\\": \"overlie\",\\n    \\\"popularity\\\": 30237\\n  },\\n  {\\n    \\\"tag\\\": \"velours\",\\n    \\\"popularity\\\": 30171\\n  },\\n  {\\n    \\\"tag\\\": \"zoodendria madrigal stagbush\",\\n    \\\"popularity\\\": 30105\\n  },\\n  {\\n    \\\"tag\\\": \"imi\",\\n    \\\"popularity\\\": 30039\\n  },\\n  {\\n    \\\"tag\\\": \"cojudge\",\\n    \\\"popularity\\\": 29974\\n  },\\n  {\\n    \\\"tag\\\": \"depurate argal\",\\n    \\\"popularity\\\": 29909\\n  },\\n  {\\n    \\\"tag\\\": \"unrecognition\",\\n    \\\"popularity\\\": 29844\\n  },\\n  {\\n    \\\"tag\\\": \"paunchful\",\\n    \\\"popularity\\\": 29779\\n  },\\n  {\\n    \\\"tag\\\": \"invalued\",\\n    \\\"popularity\\\": 29714\\n  },\\n  {\\n    \\\"tag\\\": \"probang\",\\n    \\\"popularity\\\": 29650\\n  },\\n  {\\n    \\\"tag\\\": \"chetvert\",\\n    \\\"popularity\\\": 29586\\n  },\\n  {\\n    \\\"tag\\\": \"enactable\",\\n    \\\"popularity\\\": 29522\\n  },\\n  {\\n    \\\"tag\\\": \"detoxicate adhibit\",\\n    \\\"popularity\\\": 29458\\n  },\\n  {\\n    \\\"tag\\\": \"kullaite\",\\n    \\\"popularity\\\": 29395\\n  },\\n  {\\n    \\\"tag\\\": \"undazzling\",\\n    \\\"popularity\\\": 29332\\n  },\\n  {\\n    \\\"tag\\\": \"excalation\",\\n    \\\"popularity\\\": 29269\\n  },\\n  {\\n    \\\"tag\\\": \"sievings\",\\n    \\\"popularity\\\": 29206\\n  },\\n  {\\n    \\\"tag\\\": \"disenthral\",\\n    \\\"popularity\\\": 29143\\n  },\\n  {\\n    \\\"tag\\\": \"disinterestedly\",\\n    \\\"popularity\\\": 29081\\n  },\\n  {\\n    \\\"tag\\\": \"stanner\",\\n    \\\"popularity\\\": 29018\\n  },\\n  {\\n    \\\"tag\\\": \"recapitulative\",\\n    \\\"popularity\\\": 28956\\n  },\\n  {\\n    \\\"tag\\\": \"objectivist\",\\n    \\\"popularity\\\": 28895\\n  },\\n  {\\n    \\\"tag\\\": \"hypermetropia\",\\n    \\\"popularity\\\": 28833\\n  },\\n  {\\n    \\\"tag\\\": \"incumbency\",\\n    \\\"popularity\\\": 28772\\n  },\\n  {\\n    \\\"tag\\\": \"protegee\",\\n    \\\"popularity\\\": 28711\\n  },\\n  {\\n    \\\"tag\\\": \"zealotic\",\\n    \\\"popularity\\\": 28650\\n  },\\n  {\\n    \\\"tag\\\": \"predebit\",\\n    \\\"popularity\\\": 28589\\n  },\\n  {\\n    \\\"tag\\\": \"cupolar\",\\n    \\\"popularity\\\": 28528\\n  },\\n  {\\n    \\\"tag\\\": \"unattributed\",\\n    \\\"popularity\\\": 28468\\n  },\\n  {\\n    \\\"tag\\\": \"louisine\",\\n    \\\"popularity\\\": 28408\\n  },\\n  {\\n    \\\"tag\\\": \"illustrate\",\\n    \\\"popularity\\\": 28348\\n  },\\n  {\\n    \\\"tag\\\": \"inofficiousness\",\\n    \\\"popularity\\\": 28288\\n  },\\n  {\\n    \\\"tag\\\": \"Americawards\",\\n    \\\"popularity\\\": 28228\\n  },\\n  {\\n    \\\"tag\\\": \"foreflap\",\\n    \\\"popularity\\\": 28169\\n  },\\n  {\\n    \\\"tag\\\": \"eruditeness\",\\n    \\\"popularity\\\": 28110\\n  },\\n  {\\n    \\\"tag\\\": \"copiopsia\",\\n    \\\"popularity\\\": 28051\\n  },\\n  {\\n    \\\"tag\\\": \"sporuliferous\",\\n    \\\"popularity\\\": 27992\\n  },\\n  {\\n    \\\"tag\\\": \"muttering\",\\n    \\\"popularity\\\": 27934\\n  },\\n  {\\n    \\\"tag\\\": \"prepsychology adrip\",\\n    \\\"popularity\\\": 27875\\n  },\\n  {\\n    \\\"tag\\\": \"unfriendly\",\\n    \\\"popularity\\\": 27817\\n  },\\n  {\\n    \\\"tag\\\": \"sulphanilic\",\\n    \\\"popularity\\\": 27759\\n  },\\n  {\\n    \\\"tag\\\": \"Coelococcus\",\\n    \\\"popularity\\\": 27701\\n  },\\n  {\\n    \\\"tag\\\": \"undoubtfulness\",\\n    \\\"popularity\\\": 27643\\n  },\\n  {\\n    \\\"tag\\\": \"flaringly\",\\n    \\\"popularity\\\": 27586\\n  },\\n  {\\n    \\\"tag\\\": \"unordain\",\\n    \\\"popularity\\\": 27529\\n  },\\n  {\\n    \\\"tag\\\": \"fratchety\",\\n    \\\"popularity\\\": 27472\\n  },\\n  {\\n    \\\"tag\\\": \"decadentism dolefully\",\\n    \\\"popularity\\\": 27415\\n  },\\n  {\\n    \\\"tag\\\": \"synthronus\",\\n    \\\"popularity\\\": 27358\\n  },\\n  {\\n    \\\"tag\\\": \"maiid\",\\n    \\\"popularity\\\": 27301\\n  },\\n  {\\n    \\\"tag\\\": \"rhinobyon\",\\n    \\\"popularity\\\": 27245\\n  },\\n  {\\n    \\\"tag\\\": \"Didynamia\",\\n    \\\"popularity\\\": 27189\\n  },\\n  {\\n    \\\"tag\\\": \"millionairedom\",\\n    \\\"popularity\\\": 27133\\n  },\\n  {\\n    \\\"tag\\\": \"mulierine\",\\n    \\\"popularity\\\": 27077\\n  },\\n  {\\n    \\\"tag\\\": \"Mayo\",\\n    \\\"popularity\\\": 27021\\n  },\\n  {\\n    \\\"tag\\\": \"perceivedness\",\\n    \\\"popularity\\\": 26966\\n  },\\n  {\\n    \\\"tag\\\": \"unadoration\",\\n    \\\"popularity\\\": 26911\\n  },\\n  {\\n    \\\"tag\\\": \"regraft\",\\n    \\\"popularity\\\": 26856\\n  },\\n  {\\n    \\\"tag\\\": \"witch\",\\n    \\\"popularity\\\": 26801\\n  },\\n  {\\n    \\\"tag\\\": \"ungrow\",\\n    \\\"popularity\\\": 26746\\n  },\\n  {\\n    \\\"tag\\\": \"glossopharyngeus\",\\n    \\\"popularity\\\": 26691\\n  },\\n  {\\n    \\\"tag\\\": \"unstirrable\",\\n    \\\"popularity\\\": 26637\\n  },\\n  {\\n    \\\"tag\\\": \"synodsman\",\\n    \\\"popularity\\\": 26583\\n  },\\n  {\\n    \\\"tag\\\": \"placentalian\",\\n    \\\"popularity\\\": 26529\\n  },\\n  {\\n    \\\"tag\\\": \"corpulently\",\\n    \\\"popularity\\\": 26475\\n  },\\n  {\\n    \\\"tag\\\": \"photochromoscope\",\\n    \\\"popularity\\\": 26421\\n  },\\n  {\\n    \\\"tag\\\": \"indusiate retinasphaltum chokestrap\",\\n    \\\"popularity\\\": 26368\\n  },\\n  {\\n    \\\"tag\\\": \"murdrum\",\\n    \\\"popularity\\\": 26314\\n  },\\n  {\\n    \\\"tag\\\": \"belatedness\",\\n    \\\"popularity\\\": 26261\\n  },\\n  {\\n    \\\"tag\\\": \"Cochin\",\\n    \\\"popularity\\\": 26208\\n  },\\n  {\\n    \\\"tag\\\": \"Leonist\",\\n    \\\"popularity\\\": 26155\\n  },\\n  {\\n    \\\"tag\\\": \"keeker confined\",\\n    \\\"popularity\\\": 26102\\n  },\\n  {\\n    \\\"tag\\\": \"unintellectual\",\\n    \\\"popularity\\\": 26050\\n  },\\n  {\\n    \\\"tag\\\": \"nymphaline bait\",\\n    \\\"popularity\\\": 25997\\n  },\\n  {\\n    \\\"tag\\\": \"sarcosporidiosis\",\\n    \\\"popularity\\\": 25945\\n  },\\n  {\\n    \\\"tag\\\": \"catawamptiously\",\\n    \\\"popularity\\\": 25893\\n  },\\n  {\\n    \\\"tag\\\": \"outshame\",\\n    \\\"popularity\\\": 25841\\n  },\\n  {\\n    \\\"tag\\\": \"animalism\",\\n    \\\"popularity\\\": 25790\\n  },\\n  {\\n    \\\"tag\\\": \"epithalamial\",\\n    \\\"popularity\\\": 25738\\n  },\\n  {\\n    \\\"tag\\\": \"ganner\",\\n    \\\"popularity\\\": 25687\\n  },\\n  {\\n    \\\"tag\\\": \"desilicify\",\\n    \\\"popularity\\\": 25635\\n  },\\n  {\\n    \\\"tag\\\": \"dandyism\",\\n    \\\"popularity\\\": 25584\\n  },\\n  {\\n    \\\"tag\\\": \"hyleg\",\\n    \\\"popularity\\\": 25533\\n  },\\n  {\\n    \\\"tag\\\": \"photophysical\",\\n    \\\"popularity\\\": 25483\\n  },\\n  {\\n    \\\"tag\\\": \"underload\",\\n    \\\"popularity\\\": 25432\\n  },\\n  {\\n    \\\"tag\\\": \"unintrusive\",\\n    \\\"popularity\\\": 25382\\n  },\\n  {\\n    \\\"tag\\\": \"succinamic\",\\n    \\\"popularity\\\": 25331\\n  },\\n  {\\n    \\\"tag\\\": \"matchy\",\\n    \\\"popularity\\\": 25281\\n  },\\n  {\\n    \\\"tag\\\": \"concordal\",\\n    \\\"popularity\\\": 25231\\n  },\\n  {\\n    \\\"tag\\\": \"exteriority\",\\n    \\\"popularity\\\": 25181\\n  },\\n  {\\n    \\\"tag\\\": \"sterculiad\",\\n    \\\"popularity\\\": 25132\\n  },\\n  {\\n    \\\"tag\\\": \"sulfoxylic\",\\n    \\\"popularity\\\": 25082\\n  },\\n  {\\n    \\\"tag\\\": \"oversubscription\",\\n    \\\"popularity\\\": 25033\\n  },\\n  {\\n    \\\"tag\\\": \"chiasmic\",\\n    \\\"popularity\\\": 24984\\n  },\\n  {\\n    \\\"tag\\\": \"pseudoparthenogenesis\",\\n    \\\"popularity\\\": 24935\\n  },\\n  {\\n    \\\"tag\\\": \"indorse\",\\n    \\\"popularity\\\": 24886\\n  },\\n  {\\n    \\\"tag\\\": \"Krishnaite\",\\n    \\\"popularity\\\": 24837\\n  },\\n  {\\n    \\\"tag\\\": \"calcinize\",\\n    \\\"popularity\\\": 24788\\n  },\\n  {\\n    \\\"tag\\\": \"rhodium\",\\n    \\\"popularity\\\": 24740\\n  },\\n  {\\n    \\\"tag\\\": \"tragopan\",\\n    \\\"popularity\\\": 24692\\n  },\\n  {\\n    \\\"tag\\\": \"overwhelmingly\",\\n    \\\"popularity\\\": 24643\\n  },\\n  {\\n    \\\"tag\\\": \"procidence accorporate\",\\n    \\\"popularity\\\": 24595\\n  },\\n  {\\n    \\\"tag\\\": \"polemize speelless\",\\n    \\\"popularity\\\": 24548\\n  },\\n  {\\n    \\\"tag\\\": \"radiocarpal goran\",\\n    \\\"popularity\\\": 24500\\n  },\\n  {\\n    \\\"tag\\\": \"counteroffer Pelodytes\",\\n    \\\"popularity\\\": 24452\\n  },\\n  {\\n    \\\"tag\\\": \"lionhearted\",\\n    \\\"popularity\\\": 24405\\n  },\\n  {\\n    \\\"tag\\\": \"paramastoid\",\\n    \\\"popularity\\\": 24358\\n  },\\n  {\\n    \\\"tag\\\": \"murine\",\\n    \\\"popularity\\\": 24310\\n  },\\n  {\\n    \\\"tag\\\": \"woodbined\",\\n    \\\"popularity\\\": 24263\\n  },\\n  {\\n    \\\"tag\\\": \"packthread\",\\n    \\\"popularity\\\": 24217\\n  },\\n  {\\n    \\\"tag\\\": \"citreous\",\\n    \\\"popularity\\\": 24170\\n  },\\n  {\\n    \\\"tag\\\": \"unfallaciously\",\\n    \\\"popularity\\\": 24123\\n  },\\n  {\\n    \\\"tag\\\": \"tentwork reincarnadine\",\\n    \\\"popularity\\\": 24077\\n  },\\n  {\\n    \\\"tag\\\": \"verminousness\",\\n    \\\"popularity\\\": 24030\\n  },\\n  {\\n    \\\"tag\\\": \"sillometer\",\\n    \\\"popularity\\\": 23984\\n  },\\n  {\\n    \\\"tag\\\": \"jointy\",\\n    \\\"popularity\\\": 23938\\n  },\\n  {\\n    \\\"tag\\\": \"streptolysin\",\\n    \\\"popularity\\\": 23892\\n  },\\n  {\\n    \\\"tag\\\": \"Florentinism\",\\n    \\\"popularity\\\": 23847\\n  },\\n  {\\n    \\\"tag\\\": \"monosomatous\",\\n    \\\"popularity\\\": 23801\\n  },\\n  {\\n    \\\"tag\\\": \"capsulociliary\",\\n    \\\"popularity\\\": 23756\\n  },\\n  {\\n    \\\"tag\\\": \"organum\",\\n    \\\"popularity\\\": 23710\\n  },\\n  {\\n    \\\"tag\\\": \"overtly\",\\n    \\\"popularity\\\": 23665\\n  },\\n  {\\n    \\\"tag\\\": \"ophthalmoscopical\",\\n    \\\"popularity\\\": 23620\\n  },\\n  {\\n    \\\"tag\\\": \"supposititiously\",\\n    \\\"popularity\\\": 23575\\n  },\\n  {\\n    \\\"tag\\\": \"radiochemistry\",\\n    \\\"popularity\\\": 23530\\n  },\\n  {\\n    \\\"tag\\\": \"flaxtail\",\\n    \\\"popularity\\\": 23486\\n  },\\n  {\\n    \\\"tag\\\": \"pretympanic\",\\n    \\\"popularity\\\": 23441\\n  },\\n  {\\n    \\\"tag\\\": \"auscultation\",\\n    \\\"popularity\\\": 23397\\n  },\\n  {\\n    \\\"tag\\\": \"hairdresser\",\\n    \\\"popularity\\\": 23352\\n  },\\n  {\\n    \\\"tag\\\": \"chaffless\",\\n    \\\"popularity\\\": 23308\\n  },\\n  {\\n    \\\"tag\\\": \"polioencephalitis\",\\n    \\\"popularity\\\": 23264\\n  },\\n  {\\n    \\\"tag\\\": \"axolotl\",\\n    \\\"popularity\\\": 23220\\n  },\\n  {\\n    \\\"tag\\\": \"smous\",\\n    \\\"popularity\\\": 23177\\n  },\\n  {\\n    \\\"tag\\\": \"morgen disenamour toothed\",\\n    \\\"popularity\\\": 23133\\n  },\\n  {\\n    \\\"tag\\\": \"chaiseless\",\\n    \\\"popularity\\\": 23089\\n  },\\n  {\\n    \\\"tag\\\": \"frugally\",\\n    \\\"popularity\\\": 23046\\n  },\\n  {\\n    \\\"tag\\\": \"combustive antievolutionist cinenegative\",\\n    \\\"popularity\\\": 23003\\n  },\\n  {\\n    \\\"tag\\\": \"malacolite\",\\n    \\\"popularity\\\": 22960\\n  },\\n  {\\n    \\\"tag\\\": \"borne\",\\n    \\\"popularity\\\": 22917\\n  },\\n  {\\n    \\\"tag\\\": \"mercaptole\",\\n    \\\"popularity\\\": 22874\\n  },\\n  {\\n    \\\"tag\\\": \"judicatory\",\\n    \\\"popularity\\\": 22831\\n  },\\n  {\\n    \\\"tag\\\": \"noctivagation\",\\n    \\\"popularity\\\": 22789\\n  },\\n  {\\n    \\\"tag\\\": \"synthete\",\\n    \\\"popularity\\\": 22746\\n  },\\n  {\\n    \\\"tag\\\": \"tomboyism\",\\n    \\\"popularity\\\": 22704\\n  },\\n  {\\n    \\\"tag\\\": \"serranoid\",\\n    \\\"popularity\\\": 22661\\n  },\\n  {\\n    \\\"tag\\\": \"impostorism\",\\n    \\\"popularity\\\": 22619\\n  },\\n  {\\n    \\\"tag\\\": \"flagellosis Talitha\",\\n    \\\"popularity\\\": 22577\\n  },\\n  {\\n    \\\"tag\\\": \"pseudoviscous\",\\n    \\\"popularity\\\": 22535\\n  },\\n  {\\n    \\\"tag\\\": \"Galleriidae\",\\n    \\\"popularity\\\": 22494\\n  },\\n  {\\n    \\\"tag\\\": \"undulation didelph Comintern\",\\n    \\\"popularity\\\": 22452\\n  },\\n  {\\n    \\\"tag\\\": \"triangulopyramidal\",\\n    \\\"popularity\\\": 22411\\n  },\\n  {\\n    \\\"tag\\\": \"middlings\",\\n    \\\"popularity\\\": 22369\\n  },\\n  {\\n    \\\"tag\\\": \"piperazin\",\\n    \\\"popularity\\\": 22328\\n  },\\n  {\\n    \\\"tag\\\": \"endostitis\",\\n    \\\"popularity\\\": 22287\\n  },\\n  {\\n    \\\"tag\\\": \"swordlike\",\\n    \\\"popularity\\\": 22246\\n  },\\n  {\\n    \\\"tag\\\": \"forthwith\",\\n    \\\"popularity\\\": 22205\\n  },\\n  {\\n    \\\"tag\\\": \"menaceful\",\\n    \\\"popularity\\\": 22164\\n  },\\n  {\\n    \\\"tag\\\": \"explantation defective\",\\n    \\\"popularity\\\": 22123\\n  },\\n  {\\n    \\\"tag\\\": \"arrear\",\\n    \\\"popularity\\\": 22083\\n  },\\n  {\\n    \\\"tag\\\": \"engraft\",\\n    \\\"popularity\\\": 22042\\n  },\\n  {\\n    \\\"tag\\\": \"revolunteer\",\\n    \\\"popularity\\\": 22002\\n  },\\n  {\\n    \\\"tag\\\": \"foliaceous\",\\n    \\\"popularity\\\": 21962\\n  },\\n  {\\n    \\\"tag\\\": \"pseudograph\",\\n    \\\"popularity\\\": 21922\\n  },\\n  {\\n    \\\"tag\\\": \"maenaite\",\\n    \\\"popularity\\\": 21882\\n  },\\n  {\\n    \\\"tag\\\": \"interfinger\",\\n    \\\"popularity\\\": 21842\\n  },\\n  {\\n    \\\"tag\\\": \"macroscopically\",\\n    \\\"popularity\\\": 21802\\n  },\\n  {\\n    \\\"tag\\\": \"bluewood\",\\n    \\\"popularity\\\": 21762\\n  },\\n  {\\n    \\\"tag\\\": \"chikara\",\\n    \\\"popularity\\\": 21723\\n  },\\n  {\\n    \\\"tag\\\": \"reprehension diazeuxis nickelous\",\\n    \\\"popularity\\\": 21683\\n  },\\n  {\\n    \\\"tag\\\": \"vacuation\",\\n    \\\"popularity\\\": 21644\\n  },\\n  {\\n    \\\"tag\\\": \"Sartish\",\\n    \\\"popularity\\\": 21605\\n  },\\n  {\\n    \\\"tag\\\": \"pseudogyny\",\\n    \\\"popularity\\\": 21566\\n  },\\n  {\\n    \\\"tag\\\": \"friedcake\",\\n    \\\"popularity\\\": 21527\\n  },\\n  {\\n    \\\"tag\\\": \"thraw\",\\n    \\\"popularity\\\": 21488\\n  },\\n  {\\n    \\\"tag\\\": \"bifid\",\\n    \\\"popularity\\\": 21449\\n  },\\n  {\\n    \\\"tag\\\": \"truthlessly\",\\n    \\\"popularity\\\": 21411\\n  },\\n  {\\n    \\\"tag\\\": \"lungy\",\\n    \\\"popularity\\\": 21372\\n  },\\n  {\\n    \\\"tag\\\": \"fluoborite\",\\n    \\\"popularity\\\": 21334\\n  },\\n  {\\n    \\\"tag\\\": \"anthropolithic\",\\n    \\\"popularity\\\": 21295\\n  },\\n  {\\n    \\\"tag\\\": \"coachee straw\",\\n    \\\"popularity\\\": 21257\\n  },\\n  {\\n    \\\"tag\\\": \"dehorner Grecize\",\\n    \\\"popularity\\\": 21219\\n  },\\n  {\\n    \\\"tag\\\": \"spondylopyosis\",\\n    \\\"popularity\\\": 21181\\n  },\\n  {\\n    \\\"tag\\\": \"institutionary\",\\n    \\\"popularity\\\": 21143\\n  },\\n  {\\n    \\\"tag\\\": \"agentry\",\\n    \\\"popularity\\\": 21105\\n  },\\n  {\\n    \\\"tag\\\": \"musing bietle\",\\n    \\\"popularity\\\": 21068\\n  },\\n  {\\n    \\\"tag\\\": \"cormophyte\",\\n    \\\"popularity\\\": 21030\\n  },\\n  {\\n    \\\"tag\\\": \"semielliptic\",\\n    \\\"popularity\\\": 20993\\n  },\\n  {\\n    \\\"tag\\\": \"ependytes\",\\n    \\\"popularity\\\": 20955\\n  },\\n  {\\n    \\\"tag\\\": \"coachmaster\",\\n    \\\"popularity\\\": 20918\\n  },\\n  {\\n    \\\"tag\\\": \"overexuberant\",\\n    \\\"popularity\\\": 20881\\n  },\\n  {\\n    \\\"tag\\\": \"selectable\",\\n    \\\"popularity\\\": 20844\\n  },\\n  {\\n    \\\"tag\\\": \"saclike\",\\n    \\\"popularity\\\": 20807\\n  },\\n  {\\n    \\\"tag\\\": \"mullion\",\\n    \\\"popularity\\\": 20770\\n  },\\n  {\\n    \\\"tag\\\": \"pantheonize prevalency\",\\n    \\\"popularity\\\": 20733\\n  },\\n  {\\n    \\\"tag\\\": \"trophosperm\",\\n    \\\"popularity\\\": 20697\\n  },\\n  {\\n    \\\"tag\\\": \"paraphrasist\",\\n    \\\"popularity\\\": 20660\\n  },\\n  {\\n    \\\"tag\\\": \"undercarry\",\\n    \\\"popularity\\\": 20624\\n  },\\n  {\\n    \\\"tag\\\": \"thallogenic\",\\n    \\\"popularity\\\": 20587\\n  },\\n  {\\n    \\\"tag\\\": \"bulgy forbid\",\\n    \\\"popularity\\\": 20551\\n  },\\n  {\\n    \\\"tag\\\": \"proliquor gratulatory\",\\n    \\\"popularity\\\": 20515\\n  },\\n  {\\n    \\\"tag\\\": \"booker\",\\n    \\\"popularity\\\": 20479\\n  },\\n  {\\n    \\\"tag\\\": \"wizen\",\\n    \\\"popularity\\\": 20443\\n  },\\n  {\\n    \\\"tag\\\": \"synchondrosially\",\\n    \\\"popularity\\\": 20407\\n  },\\n  {\\n    \\\"tag\\\": \"herbless\",\\n    \\\"popularity\\\": 20371\\n  },\\n  {\\n    \\\"tag\\\": \"arfvedsonite\",\\n    \\\"popularity\\\": 20336\\n  },\\n  {\\n    \\\"tag\\\": \"Neuroptera\",\\n    \\\"popularity\\\": 20300\\n  },\\n  {\\n    \\\"tag\\\": \"fingerstone\",\\n    \\\"popularity\\\": 20265\\n  },\\n  {\\n    \\\"tag\\\": \"Odontoglossae\",\\n    \\\"popularity\\\": 20229\\n  },\\n  {\\n    \\\"tag\\\": \"transmigrator\",\\n    \\\"popularity\\\": 20194\\n  },\\n  {\\n    \\\"tag\\\": \"Dehaites\",\\n    \\\"popularity\\\": 20159\\n  },\\n  {\\n    \\\"tag\\\": \"Molinist\",\\n    \\\"popularity\\\": 20124\\n  },\\n  {\\n    \\\"tag\\\": \"novelistic\",\\n    \\\"popularity\\\": 20089\\n  },\\n  {\\n    \\\"tag\\\": \"astelic\",\\n    \\\"popularity\\\": 20054\\n  },\\n  {\\n    \\\"tag\\\": \"pyelometry\",\\n    \\\"popularity\\\": 20019\\n  },\\n  {\\n    \\\"tag\\\": \"pigmentation\",\\n    \\\"popularity\\\": 19984\\n  },\\n  {\\n    \\\"tag\\\": \"epinaos\",\\n    \\\"popularity\\\": 19950\\n  },\\n  {\\n    \\\"tag\\\": \"outdare\",\\n    \\\"popularity\\\": 19915\\n  },\\n  {\\n    \\\"tag\\\": \"Funje philaristocracy\",\\n    \\\"popularity\\\": 19881\\n  },\\n  {\\n    \\\"tag\\\": \"keddah\",\\n    \\\"popularity\\\": 19846\\n  },\\n  {\\n    \\\"tag\\\": \"axoidean\",\\n    \\\"popularity\\\": 19812\\n  },\\n  {\\n    \\\"tag\\\": \"ovule\",\\n    \\\"popularity\\\": 19778\\n  },\\n  {\\n    \\\"tag\\\": \"solidify\",\\n    \\\"popularity\\\": 19744\\n  },\\n  {\\n    \\\"tag\\\": \"noncelestial\",\\n    \\\"popularity\\\": 19710\\n  },\\n  {\\n    \\\"tag\\\": \"overmultiplication\",\\n    \\\"popularity\\\": 19676\\n  },\\n  {\\n    \\\"tag\\\": \"hexatetrahedron\",\\n    \\\"popularity\\\": 19642\\n  },\\n  {\\n    \\\"tag\\\": \"pliciform\",\\n    \\\"popularity\\\": 19609\\n  },\\n  {\\n    \\\"tag\\\": \"zimbalon\",\\n    \\\"popularity\\\": 19575\\n  },\\n  {\\n    \\\"tag\\\": \"annexational\",\\n    \\\"popularity\\\": 19542\\n  },\\n  {\\n    \\\"tag\\\": \"eurhodol\",\\n    \\\"popularity\\\": 19508\\n  },\\n  {\\n    \\\"tag\\\": \"yark\",\\n    \\\"popularity\\\": 19475\\n  },\\n  {\\n    \\\"tag\\\": \"illegality nitroalizarin\",\\n    \\\"popularity\\\": 19442\\n  },\\n  {\\n    \\\"tag\\\": \"quadratum\",\\n    \\\"popularity\\\": 19409\\n  },\\n  {\\n    \\\"tag\\\": \"saccharine\",\\n    \\\"popularity\\\": 19376\\n  },\\n  {\\n    \\\"tag\\\": \"unemploy\",\\n    \\\"popularity\\\": 19343\\n  },\\n  {\\n    \\\"tag\\\": \"uniclinal unipotent\",\\n    \\\"popularity\\\": 19310\\n  },\\n  {\\n    \\\"tag\\\": \"turbo\",\\n    \\\"popularity\\\": 19277\\n  },\\n  {\\n    \\\"tag\\\": \"sybarism\",\\n    \\\"popularity\\\": 19244\\n  },\\n  {\\n    \\\"tag\\\": \"motacilline\",\\n    \\\"popularity\\\": 19212\\n  },\\n  {\\n    \\\"tag\\\": \"weaselly\",\\n    \\\"popularity\\\": 19179\\n  },\\n  {\\n    \\\"tag\\\": \"plastid\",\\n    \\\"popularity\\\": 19147\\n  },\\n  {\\n    \\\"tag\\\": \"wasting\",\\n    \\\"popularity\\\": 19114\\n  },\\n  {\\n    \\\"tag\\\": \"begrime fluting\",\\n    \\\"popularity\\\": 19082\\n  },\\n  {\\n    \\\"tag\\\": \"Nephilinae\",\\n    \\\"popularity\\\": 19050\\n  },\\n  {\\n    \\\"tag\\\": \"disregardance\",\\n    \\\"popularity\\\": 19018\\n  },\\n  {\\n    \\\"tag\\\": \"Shakerlike\",\\n    \\\"popularity\\\": 18986\\n  },\\n  {\\n    \\\"tag\\\": \"uniped\",\\n    \\\"popularity\\\": 18954\\n  },\\n  {\\n    \\\"tag\\\": \"knap\",\\n    \\\"popularity\\\": 18922\\n  },\\n  {\\n    \\\"tag\\\": \"electivism undergardener\",\\n    \\\"popularity\\\": 18890\\n  },\\n  {\\n    \\\"tag\\\": \"hulverheaded\",\\n    \\\"popularity\\\": 18858\\n  },\\n  {\\n    \\\"tag\\\": \"unruptured\",\\n    \\\"popularity\\\": 18827\\n  },\\n  {\\n    \\\"tag\\\": \"solemnize credently\",\\n    \\\"popularity\\\": 18795\\n  },\\n  {\\n    \\\"tag\\\": \"pentastomoid possessingly\",\\n    \\\"popularity\\\": 18764\\n  },\\n  {\\n    \\\"tag\\\": \"octose\",\\n    \\\"popularity\\\": 18733\\n  },\\n  {\\n    \\\"tag\\\": \"psithurism indefensibility\",\\n    \\\"popularity\\\": 18701\\n  },\\n  {\\n    \\\"tag\\\": \"torrentuous cyanometer subcrenate\",\\n    \\\"popularity\\\": 18670\\n  },\\n  {\\n    \\\"tag\\\": \"photoplaywright tapaculo\",\\n    \\\"popularity\\\": 18639\\n  },\\n  {\\n    \\\"tag\\\": \"univalence\",\\n    \\\"popularity\\\": 18608\\n  },\\n  {\\n    \\\"tag\\\": \"Porthetria\",\\n    \\\"popularity\\\": 18577\\n  },\\n  {\\n    \\\"tag\\\": \"funambulo\",\\n    \\\"popularity\\\": 18546\\n  },\\n  {\\n    \\\"tag\\\": \"pedion\",\\n    \\\"popularity\\\": 18515\\n  },\\n  {\\n    \\\"tag\\\": \"horticulturally\",\\n    \\\"popularity\\\": 18485\\n  },\\n  {\\n    \\\"tag\\\": \"marennin\",\\n    \\\"popularity\\\": 18454\\n  },\\n  {\\n    \\\"tag\\\": \"horselaugh\",\\n    \\\"popularity\\\": 18423\\n  },\\n  {\\n    \\\"tag\\\": \"semiexecutive\",\\n    \\\"popularity\\\": 18393\\n  },\\n  {\\n    \\\"tag\\\": \"Monopteridae\",\\n    \\\"popularity\\\": 18363\\n  },\\n  {\\n    \\\"tag\\\": \"commonable\",\\n    \\\"popularity\\\": 18332\\n  },\\n  {\\n    \\\"tag\\\": \"dreariment\",\\n    \\\"popularity\\\": 18302\\n  },\\n  {\\n    \\\"tag\\\": \"disbud\",\\n    \\\"popularity\\\": 18272\\n  },\\n  {\\n    \\\"tag\\\": \"monocled\",\\n    \\\"popularity\\\": 18242\\n  },\\n  {\\n    \\\"tag\\\": \"hurlbarrow\",\\n    \\\"popularity\\\": 18212\\n  },\\n  {\\n    \\\"tag\\\": \"opiateproof\",\\n    \\\"popularity\\\": 18182\\n  },\\n  {\\n    \\\"tag\\\": \"Fahrenheit\",\\n    \\\"popularity\\\": 18152\\n  },\\n  {\\n    \\\"tag\\\": \"writhed\",\\n    \\\"popularity\\\": 18122\\n  },\\n  {\\n    \\\"tag\\\": \"Volstead\",\\n    \\\"popularity\\\": 18093\\n  },\\n  {\\n    \\\"tag\\\": \"yesternight\",\\n    \\\"popularity\\\": 18063\\n  },\\n  {\\n    \\\"tag\\\": \"readmittance\",\\n    \\\"popularity\\\": 18033\\n  },\\n  {\\n    \\\"tag\\\": \"reiterable\",\\n    \\\"popularity\\\": 18004\\n  },\\n  {\\n    \\\"tag\\\": \"triquetral\",\\n    \\\"popularity\\\": 17975\\n  },\\n  {\\n    \\\"tag\\\": \"guillotinement\",\\n    \\\"popularity\\\": 17945\\n  },\\n  {\\n    \\\"tag\\\": \"repermission\",\\n    \\\"popularity\\\": 17916\\n  },\\n  {\\n    \\\"tag\\\": \"assishly\",\\n    \\\"popularity\\\": 17887\\n  },\\n  {\\n    \\\"tag\\\": \"daidle\",\\n    \\\"popularity\\\": 17858\\n  },\\n  {\\n    \\\"tag\\\": \"prismatoid\",\\n    \\\"popularity\\\": 17829\\n  },\\n  {\\n    \\\"tag\\\": \"irreptitious\",\\n    \\\"popularity\\\": 17800\\n  },\\n  {\\n    \\\"tag\\\": \"sourdeline\",\\n    \\\"popularity\\\": 17771\\n  },\\n  {\\n    \\\"tag\\\": \"Austrian\",\\n    \\\"popularity\\\": 17742\\n  },\\n  {\\n    \\\"tag\\\": \"psychorrhagic\",\\n    \\\"popularity\\\": 17713\\n  },\\n  {\\n    \\\"tag\\\": \"Monumbo\",\\n    \\\"popularity\\\": 17685\\n  },\\n  {\\n    \\\"tag\\\": \"cloiochoanitic\",\\n    \\\"popularity\\\": 17656\\n  },\\n  {\\n    \\\"tag\\\": \"hant\",\\n    \\\"popularity\\\": 17628\\n  },\\n  {\\n    \\\"tag\\\": \"roily pulldown\",\\n    \\\"popularity\\\": 17599\\n  },\\n  {\\n    \\\"tag\\\": \"recongratulation\",\\n    \\\"popularity\\\": 17571\\n  },\\n  {\\n    \\\"tag\\\": \"Peking\",\\n    \\\"popularity\\\": 17543\\n  },\\n  {\\n    \\\"tag\\\": \"erdvark\",\\n    \\\"popularity\\\": 17514\\n  },\\n  {\\n    \\\"tag\\\": \"antimnemonic\",\\n    \\\"popularity\\\": 17486\\n  },\\n  {\\n    \\\"tag\\\": \"noncapillarity\",\\n    \\\"popularity\\\": 17458\\n  },\\n  {\\n    \\\"tag\\\": \"irrepressive\",\\n    \\\"popularity\\\": 17430\\n  },\\n  {\\n    \\\"tag\\\": \"Petromyzontes\",\\n    \\\"popularity\\\": 17402\\n  },\\n  {\\n    \\\"tag\\\": \"piscatorially\",\\n    \\\"popularity\\\": 17374\\n  },\\n  {\\n    \\\"tag\\\": \"cholesterosis\",\\n    \\\"popularity\\\": 17346\\n  },\\n  {\\n    \\\"tag\\\": \"denunciate\",\\n    \\\"popularity\\\": 17319\\n  },\\n  {\\n    \\\"tag\\\": \"unmetalled\",\\n    \\\"popularity\\\": 17291\\n  },\\n  {\\n    \\\"tag\\\": \"Tigris enruin\",\\n    \\\"popularity\\\": 17263\\n  },\\n  {\\n    \\\"tag\\\": \"anaspalin\",\\n    \\\"popularity\\\": 17236\\n  },\\n  {\\n    \\\"tag\\\": \"monodromy\",\\n    \\\"popularity\\\": 17208\\n  },\\n  {\\n    \\\"tag\\\": \"Canichanan\",\\n    \\\"popularity\\\": 17181\\n  },\\n  {\\n    \\\"tag\\\": \"mesolabe\",\\n    \\\"popularity\\\": 17154\\n  },\\n  {\\n    \\\"tag\\\": \"trichothallic overcunningness\",\\n    \\\"popularity\\\": 17127\\n  },\\n  {\\n    \\\"tag\\\": \"spinsterishly\",\\n    \\\"popularity\\\": 17099\\n  },\\n  {\\n    \\\"tag\\\": \"sensilla\",\\n    \\\"popularity\\\": 17072\\n  },\\n  {\\n    \\\"tag\\\": \"wifelkin\",\\n    \\\"popularity\\\": 17045\\n  },\\n  {\\n    \\\"tag\\\": \"suppositionless\",\\n    \\\"popularity\\\": 17018\\n  },\\n  {\\n    \\\"tag\\\": \"irksomeness\",\\n    \\\"popularity\\\": 16991\\n  },\\n  {\\n    \\\"tag\\\": \"sanbenito\",\\n    \\\"popularity\\\": 16964\\n  },\\n  {\\n    \\\"tag\\\": \"nonstatement\",\\n    \\\"popularity\\\": 16938\\n  },\\n  {\\n    \\\"tag\\\": \"phenoloid\",\\n    \\\"popularity\\\": 16911\\n  },\\n  {\\n    \\\"tag\\\": \"Steinberger\",\\n    \\\"popularity\\\": 16884\\n  },\\n  {\\n    \\\"tag\\\": \"replicated boom\",\\n    \\\"popularity\\\": 16858\\n  },\\n  {\\n    \\\"tag\\\": \"sciomachiology\",\\n    \\\"popularity\\\": 16831\\n  },\\n  {\\n    \\\"tag\\\": \"starwise\",\\n    \\\"popularity\\\": 16805\\n  },\\n  {\\n    \\\"tag\\\": \"prerich\",\\n    \\\"popularity\\\": 16778\\n  },\\n  {\\n    \\\"tag\\\": \"unspawned\",\\n    \\\"popularity\\\": 16752\\n  },\\n  {\\n    \\\"tag\\\": \"unindentable\",\\n    \\\"popularity\\\": 16726\\n  },\\n  {\\n    \\\"tag\\\": \"stromatic\",\\n    \\\"popularity\\\": 16700\\n  },\\n  {\\n    \\\"tag\\\": \"fetishize\",\\n    \\\"popularity\\\": 16673\\n  },\\n  {\\n    \\\"tag\\\": \"dihydroxy\",\\n    \\\"popularity\\\": 16647\\n  },\\n  {\\n    \\\"tag\\\": \"precaudal\",\\n    \\\"popularity\\\": 16621\\n  },\\n  {\\n    \\\"tag\\\": \"Madagascar\",\\n    \\\"popularity\\\": 16595\\n  },\\n  {\\n    \\\"tag\\\": \"repinement\",\\n    \\\"popularity\\\": 16570\\n  },\\n  {\\n    \\\"tag\\\": \"noncathedral wenzel\",\\n    \\\"popularity\\\": 16544\\n  },\\n  {\\n    \\\"tag\\\": \"corollike\",\\n    \\\"popularity\\\": 16518\\n  },\\n  {\\n    \\\"tag\\\": \"pubes unamortization\",\\n    \\\"popularity\\\": 16492\\n  },\\n  {\\n    \\\"tag\\\": \"brickcroft\",\\n    \\\"popularity\\\": 16467\\n  },\\n  {\\n    \\\"tag\\\": \"intertrabecular\",\\n    \\\"popularity\\\": 16441\\n  },\\n  {\\n    \\\"tag\\\": \"formulaic\",\\n    \\\"popularity\\\": 16416\\n  },\\n  {\\n    \\\"tag\\\": \"arienzo\",\\n    \\\"popularity\\\": 16390\\n  },\\n  {\\n    \\\"tag\\\": \"Mazzinian\",\\n    \\\"popularity\\\": 16365\\n  },\\n  {\\n    \\\"tag\\\": \"wallowishly\",\\n    \\\"popularity\\\": 16339\\n  },\\n  {\\n    \\\"tag\\\": \"sysselman\",\\n    \\\"popularity\\\": 16314\\n  },\\n  {\\n    \\\"tag\\\": \"seligmannite\",\\n    \\\"popularity\\\": 16289\\n  },\\n  {\\n    \\\"tag\\\": \"harlequinery\",\\n    \\\"popularity\\\": 16264\\n  },\\n  {\\n    \\\"tag\\\": \"zucchetto\",\\n    \\\"popularity\\\": 16239\\n  },\\n  {\\n    \\\"tag\\\": \"malonyl\",\\n    \\\"popularity\\\": 16214\\n  },\\n  {\\n    \\\"tag\\\": \"patwari\",\\n    \\\"popularity\\\": 16189\\n  },\\n  {\\n    \\\"tag\\\": \"neoholmia venturesomeness\",\\n    \\\"popularity\\\": 16164\\n  },\\n  {\\n    \\\"tag\\\": \"Dehwar\",\\n    \\\"popularity\\\": 16139\\n  },\\n  {\\n    \\\"tag\\\": \"fetiferous\",\\n    \\\"popularity\\\": 16114\\n  },\\n  {\\n    \\\"tag\\\": \"chromatophore\",\\n    \\\"popularity\\\": 16090\\n  },\\n  {\\n    \\\"tag\\\": \"reregistration\",\\n    \\\"popularity\\\": 16065\\n  },\\n  {\\n    \\\"tag\\\": \"alienor\",\\n    \\\"popularity\\\": 16040\\n  },\\n  {\\n    \\\"tag\\\": \"Hexagynia\",\\n    \\\"popularity\\\": 16016\\n  },\\n  {\\n    \\\"tag\\\": \"cerebrotonia\",\\n    \\\"popularity\\\": 15991\\n  },\\n  {\\n    \\\"tag\\\": \"deedbox\",\\n    \\\"popularity\\\": 15967\\n  },\\n  {\\n    \\\"tag\\\": \"staab\",\\n    \\\"popularity\\\": 15943\\n  },\\n  {\\n    \\\"tag\\\": \"uratemia\",\\n    \\\"popularity\\\": 15918\\n  },\\n  {\\n    \\\"tag\\\": \"flaunt\",\\n    \\\"popularity\\\": 15894\\n  },\\n  {\\n    \\\"tag\\\": \"bogy\",\\n    \\\"popularity\\\": 15870\\n  },\\n  {\\n    \\\"tag\\\": \"subcartilaginous\",\\n    \\\"popularity\\\": 15846\\n  },\\n  {\\n    \\\"tag\\\": \"protonephridial\",\\n    \\\"popularity\\\": 15822\\n  },\\n  {\\n    \\\"tag\\\": \"Boswellia\",\\n    \\\"popularity\\\": 15798\\n  },\\n  {\\n    \\\"tag\\\": \"relaxant untiaraed protoepiphyte\",\\n    \\\"popularity\\\": 15774\\n  },\\n  {\\n    \\\"tag\\\": \"nesslerization\",\\n    \\\"popularity\\\": 15750\\n  },\\n  {\\n    \\\"tag\\\": \"precession\",\\n    \\\"popularity\\\": 15726\\n  },\\n  {\\n    \\\"tag\\\": \"peat\",\\n    \\\"popularity\\\": 15702\\n  },\\n  {\\n    \\\"tag\\\": \"unbit\",\\n    \\\"popularity\\\": 15678\\n  },\\n  {\\n    \\\"tag\\\": \"snailish\",\\n    \\\"popularity\\\": 15655\\n  },\\n  {\\n    \\\"tag\\\": \"porismatical\",\\n    \\\"popularity\\\": 15631\\n  },\\n  {\\n    \\\"tag\\\": \"hooflike\",\\n    \\\"popularity\\\": 15608\\n  },\\n  {\\n    \\\"tag\\\": \"resuppose phene cranic\",\\n    \\\"popularity\\\": 15584\\n  },\\n  {\\n    \\\"tag\\\": \"peptonization kipskin\",\\n    \\\"popularity\\\": 15561\\n  },\\n  {\\n    \\\"tag\\\": \"birdstone\",\\n    \\\"popularity\\\": 15537\\n  },\\n  {\\n    \\\"tag\\\": \"empty inferoanterior\",\\n    \\\"popularity\\\": 15514\\n  },\\n  {\\n    \\\"tag\\\": \"androtauric\",\\n    \\\"popularity\\\": 15491\\n  },\\n  {\\n    \\\"tag\\\": \"triamide\",\\n    \\\"popularity\\\": 15467\\n  },\\n  {\\n    \\\"tag\\\": \"showmanry\",\\n    \\\"popularity\\\": 15444\\n  },\\n  {\\n    \\\"tag\\\": \"doing\",\\n    \\\"popularity\\\": 15421\\n  },\\n  {\\n    \\\"tag\\\": \"bouchaleen\",\\n    \\\"popularity\\\": 15398\\n  },\\n  {\\n    \\\"tag\\\": \"precollude\",\\n    \\\"popularity\\\": 15375\\n  },\\n  {\\n    \\\"tag\\\": \"finger\",\\n    \\\"popularity\\\": 15352\\n  },\\n  {\\n    \\\"tag\\\": \"limnetic intermessenger\",\\n    \\\"popularity\\\": 15329\\n  },\\n  {\\n    \\\"tag\\\": \"uncharitable picrotoxic\",\\n    \\\"popularity\\\": 15306\\n  },\\n  {\\n    \\\"tag\\\": \"nationalizer Phasmidae\",\\n    \\\"popularity\\\": 15283\\n  },\\n  {\\n    \\\"tag\\\": \"laughingstock\",\\n    \\\"popularity\\\": 15261\\n  },\\n  {\\n    \\\"tag\\\": \"nondeferential\",\\n    \\\"popularity\\\": 15238\\n  },\\n  {\\n    \\\"tag\\\": \"uproariously\",\\n    \\\"popularity\\\": 15215\\n  },\\n  {\\n    \\\"tag\\\": \"manzanilla\",\\n    \\\"popularity\\\": 15193\\n  },\\n  {\\n    \\\"tag\\\": \"khahoon\",\\n    \\\"popularity\\\": 15170\\n  },\\n  {\\n    \\\"tag\\\": \"olericulturally longshanks\",\\n    \\\"popularity\\\": 15148\\n  },\\n  {\\n    \\\"tag\\\": \"enthusiastically methionic\",\\n    \\\"popularity\\\": 15125\\n  },\\n  {\\n    \\\"tag\\\": \"pobs\",\\n    \\\"popularity\\\": 15103\\n  },\\n  {\\n    \\\"tag\\\": \"tricarpellate\",\\n    \\\"popularity\\\": 15081\\n  },\\n  {\\n    \\\"tag\\\": \"souterrain\",\\n    \\\"popularity\\\": 15058\\n  },\\n  {\\n    \\\"tag\\\": \"tethelin\",\\n    \\\"popularity\\\": 15036\\n  },\\n  {\\n    \\\"tag\\\": \"tartle\",\\n    \\\"popularity\\\": 15014\\n  },\\n  {\\n    \\\"tag\\\": \"tidelike\",\\n    \\\"popularity\\\": 14992\\n  },\\n  {\\n    \\\"tag\\\": \"cosmoramic\",\\n    \\\"popularity\\\": 14970\\n  },\\n  {\\n    \\\"tag\\\": \"pretardiness\",\\n    \\\"popularity\\\": 14948\\n  },\\n  {\\n    \\\"tag\\\": \"insoul\",\\n    \\\"popularity\\\": 14926\\n  },\\n  {\\n    \\\"tag\\\": \"anthroxan\",\\n    \\\"popularity\\\": 14904\\n  },\\n  {\\n    \\\"tag\\\": \"jilter\",\\n    \\\"popularity\\\": 14882\\n  },\\n  {\\n    \\\"tag\\\": \"pectinibranchian trematode\",\\n    \\\"popularity\\\": 14860\\n  },\\n  {\\n    \\\"tag\\\": \"Renaissancist\",\\n    \\\"popularity\\\": 14838\\n  },\\n  {\\n    \\\"tag\\\": \"imaginant\",\\n    \\\"popularity\\\": 14817\\n  },\\n  {\\n    \\\"tag\\\": \"supercensure\",\\n    \\\"popularity\\\": 14795\\n  },\\n  {\\n    \\\"tag\\\": \"festilogy\",\\n    \\\"popularity\\\": 14773\\n  },\\n  {\\n    \\\"tag\\\": \"regression\",\\n    \\\"popularity\\\": 14752\\n  },\\n  {\\n    \\\"tag\\\": \"mesobregmate languorously\",\\n    \\\"popularity\\\": 14730\\n  },\\n  {\\n    \\\"tag\\\": \"unsupernaturalized\",\\n    \\\"popularity\\\": 14709\\n  },\\n  {\\n    \\\"tag\\\": \"boobyish\",\\n    \\\"popularity\\\": 14687\\n  },\\n  {\\n    \\\"tag\\\": \"scopolamine\",\\n    \\\"popularity\\\": 14666\\n  },\\n  {\\n    \\\"tag\\\": \"reamputation unchristianly\",\\n    \\\"popularity\\\": 14645\\n  },\\n  {\\n    \\\"tag\\\": \"cuneatic\",\\n    \\\"popularity\\\": 14623\\n  },\\n  {\\n    \\\"tag\\\": \"heathberry\",\\n    \\\"popularity\\\": 14602\\n  },\\n  {\\n    \\\"tag\\\": \"hate\",\\n    \\\"popularity\\\": 14581\\n  },\\n  {\\n    \\\"tag\\\": \"redeemableness\",\\n    \\\"popularity\\\": 14560\\n  },\\n  {\\n    \\\"tag\\\": \"damasse\",\\n    \\\"popularity\\\": 14539\\n  },\\n  {\\n    \\\"tag\\\": \"thrillsome\",\\n    \\\"popularity\\\": 14518\\n  },\\n  {\\n    \\\"tag\\\": \"disseverment\",\\n    \\\"popularity\\\": 14497\\n  },\\n  {\\n    \\\"tag\\\": \"underbishopric Ostyak\",\\n    \\\"popularity\\\": 14476\\n  },\\n  {\\n    \\\"tag\\\": \"Exoascales\",\\n    \\\"popularity\\\": 14455\\n  },\\n  {\\n    \\\"tag\\\": \"soiled\",\\n    \\\"popularity\\\": 14434\\n  },\\n  {\\n    \\\"tag\\\": \"Cain\",\\n    \\\"popularity\\\": 14413\\n  },\\n  {\\n    \\\"tag\\\": \"mismanageable arenae\",\\n    \\\"popularity\\\": 14392\\n  },\\n  {\\n    \\\"tag\\\": \"manducate unhinderably\",\\n    \\\"popularity\\\": 14372\\n  },\\n  {\\n    \\\"tag\\\": \"peregrin\",\\n    \\\"popularity\\\": 14351\\n  },\\n  {\\n    \\\"tag\\\": \"musicianly\",\\n    \\\"popularity\\\": 14330\\n  },\\n  {\\n    \\\"tag\\\": \"aln\",\\n    \\\"popularity\\\": 14310\\n  },\\n  {\\n    \\\"tag\\\": \"intercentrum\",\\n    \\\"popularity\\\": 14289\\n  },\\n  {\\n    \\\"tag\\\": \"roothold\",\\n    \\\"popularity\\\": 14269\\n  },\\n  {\\n    \\\"tag\\\": \"jane aneurism\",\\n    \\\"popularity\\\": 14248\\n  },\\n  {\\n    \\\"tag\\\": \"insinuatively forefeel phytolatrous\",\\n    \\\"popularity\\\": 14228\\n  },\\n  {\\n    \\\"tag\\\": \"kanchil\",\\n    \\\"popularity\\\": 14208\\n  },\\n  {\\n    \\\"tag\\\": \"Austrophile\",\\n    \\\"popularity\\\": 14187\\n  },\\n  {\\n    \\\"tag\\\": \"unterrorized\",\\n    \\\"popularity\\\": 14167\\n  },\\n  {\\n    \\\"tag\\\": \"admeasure\",\\n    \\\"popularity\\\": 14147\\n  },\\n  {\\n    \\\"tag\\\": \"electrodissolution\",\\n    \\\"popularity\\\": 14127\\n  },\\n  {\\n    \\\"tag\\\": \"unweddedly\",\\n    \\\"popularity\\\": 14107\\n  },\\n  {\\n    \\\"tag\\\": \"unannoying\",\\n    \\\"popularity\\\": 14087\\n  },\\n  {\\n    \\\"tag\\\": \"uningenuous\",\\n    \\\"popularity\\\": 14067\\n  },\\n  {\\n    \\\"tag\\\": \"omnibenevolent\",\\n    \\\"popularity\\\": 14047\\n  },\\n  {\\n    \\\"tag\\\": \"commissure\",\\n    \\\"popularity\\\": 14027\\n  },\\n  {\\n    \\\"tag\\\": \"tellureted\",\\n    \\\"popularity\\\": 14007\\n  },\\n  {\\n    \\\"tag\\\": \"suffragan\",\\n    \\\"popularity\\\": 13987\\n  },\\n  {\\n    \\\"tag\\\": \"sphaeriaceous\",\\n    \\\"popularity\\\": 13967\\n  },\\n  {\\n    \\\"tag\\\": \"unfearing\",\\n    \\\"popularity\\\": 13947\\n  },\\n  {\\n    \\\"tag\\\": \"stentoriousness precounsellor\",\\n    \\\"popularity\\\": 13928\\n  },\\n  {\\n    \\\"tag\\\": \"haemaspectroscope\",\\n    \\\"popularity\\\": 13908\\n  },\\n  {\\n    \\\"tag\\\": \"teras\",\\n    \\\"popularity\\\": 13888\\n  },\\n  {\\n    \\\"tag\\\": \"pulicine\",\\n    \\\"popularity\\\": 13869\\n  },\\n  {\\n    \\\"tag\\\": \"colicystopyelitis\",\\n    \\\"popularity\\\": 13849\\n  },\\n  {\\n    \\\"tag\\\": \"Physalia\",\\n    \\\"popularity\\\": 13830\\n  },\\n  {\\n    \\\"tag\\\": \"Saxicolidae\",\\n    \\\"popularity\\\": 13810\\n  },\\n  {\\n    \\\"tag\\\": \"peritonital\",\\n    \\\"popularity\\\": 13791\\n  },\\n  {\\n    \\\"tag\\\": \"dysphotic\",\\n    \\\"popularity\\\": 13771\\n  },\\n  {\\n    \\\"tag\\\": \"unabandoned\",\\n    \\\"popularity\\\": 13752\\n  },\\n  {\\n    \\\"tag\\\": \"rashful\",\\n    \\\"popularity\\\": 13733\\n  },\\n  {\\n    \\\"tag\\\": \"goodyness Manobo\",\\n    \\\"popularity\\\": 13714\\n  },\\n  {\\n    \\\"tag\\\": \"glaring\",\\n    \\\"popularity\\\": 13694\\n  },\\n  {\\n    \\\"tag\\\": \"horrorful\",\\n    \\\"popularity\\\": 13675\\n  },\\n  {\\n    \\\"tag\\\": \"intercepting\",\\n    \\\"popularity\\\": 13656\\n  },\\n  {\\n    \\\"tag\\\": \"semifine\",\\n    \\\"popularity\\\": 13637\\n  },\\n  {\\n    \\\"tag\\\": \"Gaypoo\",\\n    \\\"popularity\\\": 13618\\n  },\\n  {\\n    \\\"tag\\\": \"Metrosideros\",\\n    \\\"popularity\\\": 13599\\n  },\\n  {\\n    \\\"tag\\\": \"thoracicolumbar\",\\n    \\\"popularity\\\": 13580\\n  },\\n  {\\n    \\\"tag\\\": \"unserried\",\\n    \\\"popularity\\\": 13561\\n  },\\n  {\\n    \\\"tag\\\": \"keeperess cauterization\",\\n    \\\"popularity\\\": 13542\\n  },\\n  {\\n    \\\"tag\\\": \"administrant\",\\n    \\\"popularity\\\": 13523\\n  },\\n  {\\n    \\\"tag\\\": \"unpropitiatedness\",\\n    \\\"popularity\\\": 13505\\n  },\\n  {\\n    \\\"tag\\\": \"pensileness\",\\n    \\\"popularity\\\": 13486\\n  },\\n  {\\n    \\\"tag\\\": \"quinaldic unreceivable\",\\n    \\\"popularity\\\": 13467\\n  },\\n  {\\n    \\\"tag\\\": \"Carnaria\",\\n    \\\"popularity\\\": 13448\\n  },\\n  {\\n    \\\"tag\\\": \"azothionium wurrus\",\\n    \\\"popularity\\\": 13430\\n  },\\n  {\\n    \\\"tag\\\": \"mistresshood\",\\n    \\\"popularity\\\": 13411\\n  },\\n  {\\n    \\\"tag\\\": \"Savara\",\\n    \\\"popularity\\\": 13393\\n  },\\n  {\\n    \\\"tag\\\": \"dasyurine\",\\n    \\\"popularity\\\": 13374\\n  },\\n  {\\n    \\\"tag\\\": \"superideal\",\\n    \\\"popularity\\\": 13356\\n  },\\n  {\\n    \\\"tag\\\": \"Parisianize\",\\n    \\\"popularity\\\": 13337\\n  },\\n  {\\n    \\\"tag\\\": \"underearth\",\\n    \\\"popularity\\\": 13319\\n  },\\n  {\\n    \\\"tag\\\": \"athrogenic\",\\n    \\\"popularity\\\": 13301\\n  },\\n  {\\n    \\\"tag\\\": \"communicate\",\\n    \\\"popularity\\\": 13282\\n  },\\n  {\\n    \\\"tag\\\": \"denervation enworthed\",\\n    \\\"popularity\\\": 13264\\n  },\\n  {\\n    \\\"tag\\\": \"subbromide\",\\n    \\\"popularity\\\": 13246\\n  },\\n  {\\n    \\\"tag\\\": \"stenocoriasis\",\\n    \\\"popularity\\\": 13228\\n  },\\n  {\\n    \\\"tag\\\": \"facetiousness\",\\n    \\\"popularity\\\": 13209\\n  },\\n  {\\n    \\\"tag\\\": \"twaddling\",\\n    \\\"popularity\\\": 13191\\n  },\\n  {\\n    \\\"tag\\\": \"tetartoconid\",\\n    \\\"popularity\\\": 13173\\n  },\\n  {\\n    \\\"tag\\\": \"audiophile\",\\n    \\\"popularity\\\": 13155\\n  },\\n  {\\n    \\\"tag\\\": \"fustigate\",\\n    \\\"popularity\\\": 13137\\n  },\\n  {\\n    \\\"tag\\\": \"Sorbian cacophonia\",\\n    \\\"popularity\\\": 13119\\n  },\\n  {\\n    \\\"tag\\\": \"fondish\",\\n    \\\"popularity\\\": 13101\\n  },\\n  {\\n    \\\"tag\\\": \"endomastoiditis\",\\n    \\\"popularity\\\": 13084\\n  },\\n  {\\n    \\\"tag\\\": \"sniptious\",\\n    \\\"popularity\\\": 13066\\n  },\\n  {\\n    \\\"tag\\\": \"glochidiate\",\\n    \\\"popularity\\\": 13048\\n  },\\n  {\\n    \\\"tag\\\": \"polycarboxylic\",\\n    \\\"popularity\\\": 13030\\n  },\\n  {\\n    \\\"tag\\\": \"stamp\",\\n    \\\"popularity\\\": 13012\\n  },\\n  {\\n    \\\"tag\\\": \"tritonymph endotoxoid\",\\n    \\\"popularity\\\": 12995\\n  },\\n  {\\n    \\\"tag\\\": \"wolfskin\",\\n    \\\"popularity\\\": 12977\\n  },\\n  {\\n    \\\"tag\\\": \"oncosimeter\",\\n    \\\"popularity\\\": 12959\\n  },\\n  {\\n    \\\"tag\\\": \"outward\",\\n    \\\"popularity\\\": 12942\\n  },\\n  {\\n    \\\"tag\\\": \"circumscribed\",\\n    \\\"popularity\\\": 12924\\n  },\\n  {\\n    \\\"tag\\\": \"autohemolytic\",\\n    \\\"popularity\\\": 12907\\n  },\\n  {\\n    \\\"tag\\\": \"isorhamnose\",\\n    \\\"popularity\\\": 12889\\n  },\\n  {\\n    \\\"tag\\\": \"monarchomachic\",\\n    \\\"popularity\\\": 12872\\n  },\\n  {\\n    \\\"tag\\\": \"phaenomenon\",\\n    \\\"popularity\\\": 12855\\n  },\\n  {\\n    \\\"tag\\\": \"angiopressure\",\\n    \\\"popularity\\\": 12837\\n  },\\n  {\\n    \\\"tag\\\": \"similarize\",\\n    \\\"popularity\\\": 12820\\n  },\\n  {\\n    \\\"tag\\\": \"unseeable\",\\n    \\\"popularity\\\": 12803\\n  },\\n  {\\n    \\\"tag\\\": \"Toryize\",\\n    \\\"popularity\\\": 12785\\n  },\\n  {\\n    \\\"tag\\\": \"fruitling\",\\n    \\\"popularity\\\": 12768\\n  },\\n  {\\n    \\\"tag\\\": \"axle\",\\n    \\\"popularity\\\": 12751\\n  },\\n  {\\n    \\\"tag\\\": \"priestal cocked\",\\n    \\\"popularity\\\": 12734\\n  },\\n  {\\n    \\\"tag\\\": \"serotoxin\",\\n    \\\"popularity\\\": 12717\\n  },\\n  {\\n    \\\"tag\\\": \"unmovably\",\\n    \\\"popularity\\\": 12700\\n  },\\n  {\\n    \\\"tag\\\": \"darbha\",\\n    \\\"popularity\\\": 12683\\n  },\\n  {\\n    \\\"tag\\\": \"Mongolize\",\\n    \\\"popularity\\\": 12666\\n  },\\n  {\\n    \\\"tag\\\": \"clusteringly\",\\n    \\\"popularity\\\": 12649\\n  },\\n  {\\n    \\\"tag\\\": \"tendence\",\\n    \\\"popularity\\\": 12632\\n  },\\n  {\\n    \\\"tag\\\": \"foziness\",\\n    \\\"popularity\\\": 12615\\n  },\\n  {\\n    \\\"tag\\\": \"brickkiln lithify\",\\n    \\\"popularity\\\": 12598\\n  },\\n  {\\n    \\\"tag\\\": \"unpriest\",\\n    \\\"popularity\\\": 12581\\n  },\\n  {\\n    \\\"tag\\\": \"convincer\",\\n    \\\"popularity\\\": 12564\\n  },\\n  {\\n    \\\"tag\\\": \"mornlike\",\\n    \\\"popularity\\\": 12548\\n  },\\n  {\\n    \\\"tag\\\": \"overaddiction ostentatiousness\",\\n    \\\"popularity\\\": 12531\\n  },\\n  {\\n    \\\"tag\\\": \"diffusively moccasin pendom\",\\n    \\\"popularity\\\": 12514\\n  },\\n  {\\n    \\\"tag\\\": \"boose\",\\n    \\\"popularity\\\": 12498\\n  },\\n  {\\n    \\\"tag\\\": \"myonosus\",\\n    \\\"popularity\\\": 12481\\n  },\\n  {\\n    \\\"tag\\\": \"handsome\",\\n    \\\"popularity\\\": 12464\\n  },\\n  {\\n    \\\"tag\\\": \"paroxysmic\",\\n    \\\"popularity\\\": 12448\\n  },\\n  {\\n    \\\"tag\\\": \"Ulidian\",\\n    \\\"popularity\\\": 12431\\n  },\\n  {\\n    \\\"tag\\\": \"heartache\",\\n    \\\"popularity\\\": 12415\\n  },\\n  {\\n    \\\"tag\\\": \"torporize\",\\n    \\\"popularity\\\": 12398\\n  },\\n  {\\n    \\\"tag\\\": \"hippish\",\\n    \\\"popularity\\\": 12382\\n  },\\n  {\\n    \\\"tag\\\": \"stigmal militation\",\\n    \\\"popularity\\\": 12366\\n  },\\n  {\\n    \\\"tag\\\": \"matmaker\",\\n    \\\"popularity\\\": 12349\\n  },\\n  {\\n    \\\"tag\\\": \"marantaceous bivoluminous\",\\n    \\\"popularity\\\": 12333\\n  },\\n  {\\n    \\\"tag\\\": \"Uraniidae\",\\n    \\\"popularity\\\": 12317\\n  },\\n  {\\n    \\\"tag\\\": \"risper\",\\n    \\\"popularity\\\": 12301\\n  },\\n  {\\n    \\\"tag\\\": \"tintinnabulation\",\\n    \\\"popularity\\\": 12284\\n  },\\n  {\\n    \\\"tag\\\": \"tributorian\",\\n    \\\"popularity\\\": 12268\\n  },\\n  {\\n    \\\"tag\\\": \"ashamedly\",\\n    \\\"popularity\\\": 12252\\n  },\\n  {\\n    \\\"tag\\\": \"Macrourus\",\\n    \\\"popularity\\\": 12236\\n  },\\n  {\\n    \\\"tag\\\": \"Chora\",\\n    \\\"popularity\\\": 12220\\n  },\\n  {\\n    \\\"tag\\\": \"caul\",\\n    \\\"popularity\\\": 12204\\n  },\\n  {\\n    \\\"tag\\\": \"exsector\",\\n    \\\"popularity\\\": 12188\\n  },\\n  {\\n    \\\"tag\\\": \"acutish\",\\n    \\\"popularity\\\": 12172\\n  },\\n  {\\n    \\\"tag\\\": \"amphichrome\",\\n    \\\"popularity\\\": 12156\\n  },\\n  {\\n    \\\"tag\\\": \"guarder\",\\n    \\\"popularity\\\": 12140\\n  },\\n  {\\n    \\\"tag\\\": \"sculpturally\",\\n    \\\"popularity\\\": 12124\\n  },\\n  {\\n    \\\"tag\\\": \"benightmare\",\\n    \\\"popularity\\\": 12108\\n  },\\n  {\\n    \\\"tag\\\": \"chucky\",\\n    \\\"popularity\\\": 12093\\n  },\\n  {\\n    \\\"tag\\\": \"Venetian\",\\n    \\\"popularity\\\": 12077\\n  },\\n  {\\n    \\\"tag\\\": \"autotheater\",\\n    \\\"popularity\\\": 12061\\n  },\\n  {\\n    \\\"tag\\\": \"planarioid\",\\n    \\\"popularity\\\": 12045\\n  },\\n  {\\n    \\\"tag\\\": \"handkerchiefful\",\\n    \\\"popularity\\\": 12030\\n  },\\n  {\\n    \\\"tag\\\": \"fuliginousness potentize\",\\n    \\\"popularity\\\": 12014\\n  },\\n  {\\n    \\\"tag\\\": \"pantheum\",\\n    \\\"popularity\\\": 11998\\n  },\\n  {\\n    \\\"tag\\\": \"heavyweight\",\\n    \\\"popularity\\\": 11983\\n  },\\n  {\\n    \\\"tag\\\": \"unbrick\",\\n    \\\"popularity\\\": 11967\\n  },\\n  {\\n    \\\"tag\\\": \"duomachy\",\\n    \\\"popularity\\\": 11952\\n  },\\n  {\\n    \\\"tag\\\": \"polyphyodont\",\\n    \\\"popularity\\\": 11936\\n  },\\n  {\\n    \\\"tag\\\": \"hibernacle\",\\n    \\\"popularity\\\": 11921\\n  },\\n  {\\n    \\\"tag\\\": \"undistend\",\\n    \\\"popularity\\\": 11905\\n  },\\n  {\\n    \\\"tag\\\": \"hystericky\",\\n    \\\"popularity\\\": 11890\\n  },\\n  {\\n    \\\"tag\\\": \"paleolimnology\",\\n    \\\"popularity\\\": 11875\\n  },\\n  {\\n    \\\"tag\\\": \"cedarware\",\\n    \\\"popularity\\\": 11859\\n  },\\n  {\\n    \\\"tag\\\": \"overwrested\",\\n    \\\"popularity\\\": 11844\\n  },\\n  {\\n    \\\"tag\\\": \"Syriacism\",\\n    \\\"popularity\\\": 11829\\n  },\\n  {\\n    \\\"tag\\\": \"pretan\",\\n    \\\"popularity\\\": 11813\\n  },\\n  {\\n    \\\"tag\\\": \"formant\",\\n    \\\"popularity\\\": 11798\\n  },\\n  {\\n    \\\"tag\\\": \"pharmacopoeist Fedia\",\\n    \\\"popularity\\\": 11783\\n  },\\n  {\\n    \\\"tag\\\": \"exorcist eerisome\",\\n    \\\"popularity\\\": 11768\\n  },\\n  {\\n    \\\"tag\\\": \"separation\",\\n    \\\"popularity\\\": 11753\\n  },\\n  {\\n    \\\"tag\\\": \"infancy\",\\n    \\\"popularity\\\": 11738\\n  },\\n  {\\n    \\\"tag\\\": \"ecrasite\",\\n    \\\"popularity\\\": 11723\\n  },\\n  {\\n    \\\"tag\\\": \"propolize\",\\n    \\\"popularity\\\": 11708\\n  },\\n  {\\n    \\\"tag\\\": \"uncram phyllin\",\\n    \\\"popularity\\\": 11693\\n  },\\n  {\\n    \\\"tag\\\": \"thymopathy\",\\n    \\\"popularity\\\": 11678\\n  },\\n  {\\n    \\\"tag\\\": \"omniscient\",\\n    \\\"popularity\\\": 11663\\n  },\\n  {\\n    \\\"tag\\\": \"coussinet hazer\",\\n    \\\"popularity\\\": 11648\\n  },\\n  {\\n    \\\"tag\\\": \"contributiveness\",\\n    \\\"popularity\\\": 11633\\n  },\\n  {\\n    \\\"tag\\\": \"septifluous\",\\n    \\\"popularity\\\": 11618\\n  },\\n  {\\n    \\\"tag\\\": \"halfness\",\\n    \\\"popularity\\\": 11603\\n  },\\n  {\\n    \\\"tag\\\": \"tocher\",\\n    \\\"popularity\\\": 11589\\n  },\\n  {\\n    \\\"tag\\\": \"monotonist\",\\n    \\\"popularity\\\": 11574\\n  },\\n  {\\n    \\\"tag\\\": \"headchair\",\\n    \\\"popularity\\\": 11559\\n  },\\n  {\\n    \\\"tag\\\": \"everywhence\",\\n    \\\"popularity\\\": 11544\\n  },\\n  {\\n    \\\"tag\\\": \"gerate\",\\n    \\\"popularity\\\": 11530\\n  },\\n  {\\n    \\\"tag\\\": \"unrepellent\",\\n    \\\"popularity\\\": 11515\\n  },\\n  {\\n    \\\"tag\\\": \"inidoneous\",\\n    \\\"popularity\\\": 11500\\n  },\\n  {\\n    \\\"tag\\\": \"Rifi\",\\n    \\\"popularity\\\": 11486\\n  },\\n  {\\n    \\\"tag\\\": \"unstop\",\\n    \\\"popularity\\\": 11471\\n  },\\n  {\\n    \\\"tag\\\": \"conformer\",\\n    \\\"popularity\\\": 11457\\n  },\\n  {\\n    \\\"tag\\\": \"vivisectionally\",\\n    \\\"popularity\\\": 11442\\n  },\\n  {\\n    \\\"tag\\\": \"nonfinishing\",\\n    \\\"popularity\\\": 11428\\n  },\\n  {\\n    \\\"tag\\\": \"tyranness\",\\n    \\\"popularity\\\": 11413\\n  },\\n  {\\n    \\\"tag\\\": \"shepherdage havoc\",\\n    \\\"popularity\\\": 11399\\n  },\\n  {\\n    \\\"tag\\\": \"coronale\",\\n    \\\"popularity\\\": 11385\\n  },\\n  {\\n    \\\"tag\\\": \"airmarker\",\\n    \\\"popularity\\\": 11370\\n  },\\n  {\\n    \\\"tag\\\": \"subpanel\",\\n    \\\"popularity\\\": 11356\\n  },\\n  {\\n    \\\"tag\\\": \"conciliation\",\\n    \\\"popularity\\\": 11342\\n  },\\n  {\\n    \\\"tag\\\": \"supergun\",\\n    \\\"popularity\\\": 11327\\n  },\\n  {\\n    \\\"tag\\\": \"photoheliography\",\\n    \\\"popularity\\\": 11313\\n  },\\n  {\\n    \\\"tag\\\": \"cacosmia\",\\n    \\\"popularity\\\": 11299\\n  },\\n  {\\n    \\\"tag\\\": \"caressant\",\\n    \\\"popularity\\\": 11285\\n  },\\n  {\\n    \\\"tag\\\": \"swivet\",\\n    \\\"popularity\\\": 11270\\n  },\\n  {\\n    \\\"tag\\\": \"coddler\",\\n    \\\"popularity\\\": 11256\\n  },\\n  {\\n    \\\"tag\\\": \"rakehellish\",\\n    \\\"popularity\\\": 11242\\n  },\\n  {\\n    \\\"tag\\\": \"recohabitation\",\\n    \\\"popularity\\\": 11228\\n  },\\n  {\\n    \\\"tag\\\": \"postillator\",\\n    \\\"popularity\\\": 11214\\n  },\\n  {\\n    \\\"tag\\\": \"receipt\",\\n    \\\"popularity\\\": 11200\\n  },\\n  {\\n    \\\"tag\\\": \"nonconformistical\",\\n    \\\"popularity\\\": 11186\\n  },\\n  {\\n    \\\"tag\\\": \"unglorified\",\\n    \\\"popularity\\\": 11172\\n  },\\n  {\\n    \\\"tag\\\": \"unordinariness\",\\n    \\\"popularity\\\": 11158\\n  },\\n  {\\n    \\\"tag\\\": \"tetrahydroxy\",\\n    \\\"popularity\\\": 11144\\n  },\\n  {\\n    \\\"tag\\\": \"haploperistomic corporeity\",\\n    \\\"popularity\\\": 11130\\n  },\\n  {\\n    \\\"tag\\\": \"varical\",\\n    \\\"popularity\\\": 11117\\n  },\\n  {\\n    \\\"tag\\\": \"pilferment\",\\n    \\\"popularity\\\": 11103\\n  },\\n  {\\n    \\\"tag\\\": \"reverentially playcraft\",\\n    \\\"popularity\\\": 11089\\n  },\\n  {\\n    \\\"tag\\\": \"unretentive\",\\n    \\\"popularity\\\": 11075\\n  },\\n  {\\n    \\\"tag\\\": \"readiness\",\\n    \\\"popularity\\\": 11061\\n  },\\n  {\\n    \\\"tag\\\": \"thermomagnetism\",\\n    \\\"popularity\\\": 11048\\n  },\\n  {\\n    \\\"tag\\\": \"spotless\",\\n    \\\"popularity\\\": 11034\\n  },\\n  {\\n    \\\"tag\\\": \"semishrubby\",\\n    \\\"popularity\\\": 11020\\n  },\\n  {\\n    \\\"tag\\\": \"metrotomy\",\\n    \\\"popularity\\\": 11007\\n  },\\n  {\\n    \\\"tag\\\": \"hocker\",\\n    \\\"popularity\\\": 10993\\n  },\\n  {\\n    \\\"tag\\\": \"anecdotal\",\\n    \\\"popularity\\\": 10979\\n  },\\n  {\\n    \\\"tag\\\": \"tetrabelodont\",\\n    \\\"popularity\\\": 10966\\n  },\\n  {\\n    \\\"tag\\\": \"Ramillied\",\\n    \\\"popularity\\\": 10952\\n  },\\n  {\\n    \\\"tag\\\": \"sympatheticism\",\\n    \\\"popularity\\\": 10939\\n  },\\n  {\\n    \\\"tag\\\": \"kiskatom\",\\n    \\\"popularity\\\": 10925\\n  },\\n  {\\n    \\\"tag\\\": \"concyclically\",\\n    \\\"popularity\\\": 10912\\n  },\\n  {\\n    \\\"tag\\\": \"tunicless\",\\n    \\\"popularity\\\": 10899\\n  },\\n  {\\n    \\\"tag\\\": \"formalistic\",\\n    \\\"popularity\\\": 10885\\n  },\\n  {\\n    \\\"tag\\\": \"thermacogenesis\",\\n    \\\"popularity\\\": 10872\\n  },\\n  {\\n    \\\"tag\\\": \"multimotored\",\\n    \\\"popularity\\\": 10858\\n  },\\n  {\\n    \\\"tag\\\": \"inversive\",\\n    \\\"popularity\\\": 10845\\n  },\\n  {\\n    \\\"tag\\\": \"Jatki\",\\n    \\\"popularity\\\": 10832\\n  },\\n  {\\n    \\\"tag\\\": \"highest\",\\n    \\\"popularity\\\": 10818\\n  },\\n  {\\n    \\\"tag\\\": \"rubidic\",\\n    \\\"popularity\\\": 10805\\n  },\\n  {\\n    \\\"tag\\\": \"acranial\",\\n    \\\"popularity\\\": 10792\\n  },\\n  {\\n    \\\"tag\\\": \"pulvinulus\",\\n    \\\"popularity\\\": 10779\\n  },\\n  {\\n    \\\"tag\\\": \"nattiness\",\\n    \\\"popularity\\\": 10766\\n  },\\n  {\\n    \\\"tag\\\": \"antisimoniacal\",\\n    \\\"popularity\\\": 10752\\n  },\\n  {\\n    \\\"tag\\\": \"tetanize\",\\n    \\\"popularity\\\": 10739\\n  },\\n  {\\n    \\\"tag\\\": \"spectrophobia\",\\n    \\\"popularity\\\": 10726\\n  },\\n  {\\n    \\\"tag\\\": \"monopolitical\",\\n    \\\"popularity\\\": 10713\\n  },\\n  {\\n    \\\"tag\\\": \"teallite\",\\n    \\\"popularity\\\": 10700\\n  },\\n  {\\n    \\\"tag\\\": \"alicyclic interpellator\",\\n    \\\"popularity\\\": 10687\\n  },\\n  {\\n    \\\"tag\\\": \"nonsynthesized\",\\n    \\\"popularity\\\": 10674\\n  },\\n  {\\n    \\\"tag\\\": \"wheelwrighting\",\\n    \\\"popularity\\\": 10661\\n  },\\n  {\\n    \\\"tag\\\": \"pelliculate\",\\n    \\\"popularity\\\": 10648\\n  },\\n  {\\n    \\\"tag\\\": \"Euphyllopoda\",\\n    \\\"popularity\\\": 10635\\n  },\\n  {\\n    \\\"tag\\\": \"graver\",\\n    \\\"popularity\\\": 10622\\n  },\\n  {\\n    \\\"tag\\\": \"automorph\",\\n    \\\"popularity\\\": 10609\\n  },\\n  {\\n    \\\"tag\\\": \"underhanded\",\\n    \\\"popularity\\\": 10597\\n  },\\n  {\\n    \\\"tag\\\": \"causal\",\\n    \\\"popularity\\\": 10584\\n  },\\n  {\\n    \\\"tag\\\": \"odoom\",\\n    \\\"popularity\\\": 10571\\n  },\\n  {\\n    \\\"tag\\\": \"apodictical\",\\n    \\\"popularity\\\": 10558\\n  },\\n  {\\n    \\\"tag\\\": \"foundery\",\\n    \\\"popularity\\\": 10545\\n  },\\n  {\\n    \\\"tag\\\": \"unneighbored\",\\n    \\\"popularity\\\": 10533\\n  },\\n  {\\n    \\\"tag\\\": \"woolshearing\",\\n    \\\"popularity\\\": 10520\\n  },\\n  {\\n    \\\"tag\\\": \"boschveld\",\\n    \\\"popularity\\\": 10507\\n  },\\n  {\\n    \\\"tag\\\": \"unhardened lipopod\",\\n    \\\"popularity\\\": 10495\\n  },\\n  {\\n    \\\"tag\\\": \"unenriching\",\\n    \\\"popularity\\\": 10482\\n  },\\n  {\\n    \\\"tag\\\": \"spak\",\\n    \\\"popularity\\\": 10469\\n  },\\n  {\\n    \\\"tag\\\": \"yogasana\",\\n    \\\"popularity\\\": 10457\\n  },\\n  {\\n    \\\"tag\\\": \"depoetize\",\\n    \\\"popularity\\\": 10444\\n  },\\n  {\\n    \\\"tag\\\": \"parousiamania\",\\n    \\\"popularity\\\": 10432\\n  },\\n  {\\n    \\\"tag\\\": \"longlegs\",\\n    \\\"popularity\\\": 10419\\n  },\\n  {\\n    \\\"tag\\\": \"gelatinizability\",\\n    \\\"popularity\\\": 10407\\n  },\\n  {\\n    \\\"tag\\\": \"edeology\",\\n    \\\"popularity\\\": 10394\\n  },\\n  {\\n    \\\"tag\\\": \"sodwork\",\\n    \\\"popularity\\\": 10382\\n  },\\n  {\\n    \\\"tag\\\": \"somnambule\",\\n    \\\"popularity\\\": 10369\\n  },\\n  {\\n    \\\"tag\\\": \"antiquing\",\\n    \\\"popularity\\\": 10357\\n  },\\n  {\\n    \\\"tag\\\": \"intaker\",\\n    \\\"popularity\\\": 10344\\n  },\\n  {\\n    \\\"tag\\\": \"Gerberia\",\\n    \\\"popularity\\\": 10332\\n  },\\n  {\\n    \\\"tag\\\": \"preadmit\",\\n    \\\"popularity\\\": 10320\\n  },\\n  {\\n    \\\"tag\\\": \"bullhorn\",\\n    \\\"popularity\\\": 10307\\n  },\\n  {\\n    \\\"tag\\\": \"sororal\",\\n    \\\"popularity\\\": 10295\\n  },\\n  {\\n    \\\"tag\\\": \"phaeophyceous\",\\n    \\\"popularity\\\": 10283\\n  },\\n  {\\n    \\\"tag\\\": \"omphalopsychite\",\\n    \\\"popularity\\\": 10271\\n  },\\n  {\\n    \\\"tag\\\": \"substantious\",\\n    \\\"popularity\\\": 10258\\n  },\\n  {\\n    \\\"tag\\\": \"undemonstratively\",\\n    \\\"popularity\\\": 10246\\n  },\\n  {\\n    \\\"tag\\\": \"corallike blackit\",\\n    \\\"popularity\\\": 10234\\n  },\\n  {\\n    \\\"tag\\\": \"amoebous\",\\n    \\\"popularity\\\": 10222\\n  },\\n  {\\n    \\\"tag\\\": \"Polypodium\",\\n    \\\"popularity\\\": 10210\\n  },\\n  {\\n    \\\"tag\\\": \"blodite\",\\n    \\\"popularity\\\": 10198\\n  },\\n  {\\n    \\\"tag\\\": \"hordarian\",\\n    \\\"popularity\\\": 10186\\n  },\\n  {\\n    \\\"tag\\\": \"nonmoral\",\\n    \\\"popularity\\\": 10174\\n  },\\n  {\\n    \\\"tag\\\": \"dredgeful\",\\n    \\\"popularity\\\": 10162\\n  },\\n  {\\n    \\\"tag\\\": \"nourishingly\",\\n    \\\"popularity\\\": 10150\\n  },\\n  {\\n    \\\"tag\\\": \"seamy\",\\n    \\\"popularity\\\": 10138\\n  },\\n  {\\n    \\\"tag\\\": \"vara\",\\n    \\\"popularity\\\": 10126\\n  },\\n  {\\n    \\\"tag\\\": \"incorruptibleness\",\\n    \\\"popularity\\\": 10114\\n  },\\n  {\\n    \\\"tag\\\": \"manipulator\",\\n    \\\"popularity\\\": 10102\\n  },\\n  {\\n    \\\"tag\\\": \"chromodiascope uncountably\",\\n    \\\"popularity\\\": 10090\\n  },\\n  {\\n    \\\"tag\\\": \"typhemia\",\\n    \\\"popularity\\\": 10078\\n  },\\n  {\\n    \\\"tag\\\": \"Smalcaldic\",\\n    \\\"popularity\\\": 10066\\n  },\\n  {\\n    \\\"tag\\\": \"precontrive\",\\n    \\\"popularity\\\": 10054\\n  },\\n  {\\n    \\\"tag\\\": \"sowarry\",\\n    \\\"popularity\\\": 10042\\n  },\\n  {\\n    \\\"tag\\\": \"monopodic\",\\n    \\\"popularity\\\": 10031\\n  },\\n  {\\n    \\\"tag\\\": \"recodify\",\\n    \\\"popularity\\\": 10019\\n  },\\n  {\\n    \\\"tag\\\": \"phosphowolframic rimple\",\\n    \\\"popularity\\\": 10007\\n  },\\n  {\\n    \\\"tag\\\": \"triconch\",\\n    \\\"popularity\\\": 9995\\n  },\\n  {\\n    \\\"tag\\\": \"pycnodontoid\",\\n    \\\"popularity\\\": 9984\\n  },\\n  {\\n    \\\"tag\\\": \"bradyspermatism\",\\n    \\\"popularity\\\": 9972\\n  },\\n  {\\n    \\\"tag\\\": \"extensionist\",\\n    \\\"popularity\\\": 9960\\n  },\\n  {\\n    \\\"tag\\\": \"characterize\",\\n    \\\"popularity\\\": 9949\\n  },\\n  {\\n    \\\"tag\\\": \"anatreptic proteolytic\",\\n    \\\"popularity\\\": 9937\\n  },\\n  {\\n    \\\"tag\\\": \"waterboard\",\\n    \\\"popularity\\\": 9925\\n  },\\n  {\\n    \\\"tag\\\": \"allopathically\",\\n    \\\"popularity\\\": 9914\\n  },\\n  {\\n    \\\"tag\\\": \"arithmetician\",\\n    \\\"popularity\\\": 9902\\n  },\\n  {\\n    \\\"tag\\\": \"subsist\",\\n    \\\"popularity\\\": 9891\\n  },\\n  {\\n    \\\"tag\\\": \"Islamitish\",\\n    \\\"popularity\\\": 9879\\n  },\\n  {\\n    \\\"tag\\\": \"biddy\",\\n    \\\"popularity\\\": 9868\\n  },\\n  {\\n    \\\"tag\\\": \"reverberation\",\\n    \\\"popularity\\\": 9856\\n  },\\n  {\\n    \\\"tag\\\": \"Zaporogue\",\\n    \\\"popularity\\\": 9845\\n  },\\n  {\\n    \\\"tag\\\": \"soapberry\",\\n    \\\"popularity\\\": 9833\\n  },\\n  {\\n    \\\"tag\\\": \"physiognomics\",\\n    \\\"popularity\\\": 9822\\n  },\\n  {\\n    \\\"tag\\\": \"hospitalization\",\\n    \\\"popularity\\\": 9810\\n  },\\n  {\\n    \\\"tag\\\": \"dissembler\",\\n    \\\"popularity\\\": 9799\\n  },\\n  {\\n    \\\"tag\\\": \"festinate\",\\n    \\\"popularity\\\": 9788\\n  },\\n  {\\n    \\\"tag\\\": \"angiectopia\",\\n    \\\"popularity\\\": 9776\\n  },\\n  {\\n    \\\"tag\\\": \"Pulicidae\",\\n    \\\"popularity\\\": 9765\\n  },\\n  {\\n    \\\"tag\\\": \"beslimer\",\\n    \\\"popularity\\\": 9754\\n  },\\n  {\\n    \\\"tag\\\": \"nontreaty\",\\n    \\\"popularity\\\": 9743\\n  },\\n  {\\n    \\\"tag\\\": \"unhaggled\",\\n    \\\"popularity\\\": 9731\\n  },\\n  {\\n    \\\"tag\\\": \"catfall\",\\n    \\\"popularity\\\": 9720\\n  },\\n  {\\n    \\\"tag\\\": \"stola\",\\n    \\\"popularity\\\": 9709\\n  },\\n  {\\n    \\\"tag\\\": \"pataco\",\\n    \\\"popularity\\\": 9698\\n  },\\n  {\\n    \\\"tag\\\": \"ontologistic\",\\n    \\\"popularity\\\": 9686\\n  },\\n  {\\n    \\\"tag\\\": \"aerosphere\",\\n    \\\"popularity\\\": 9675\\n  },\\n  {\\n    \\\"tag\\\": \"deobstruent\",\\n    \\\"popularity\\\": 9664\\n  },\\n  {\\n    \\\"tag\\\": \"threepence\",\\n    \\\"popularity\\\": 9653\\n  },\\n  {\\n    \\\"tag\\\": \"cyprinoid\",\\n    \\\"popularity\\\": 9642\\n  },\\n  {\\n    \\\"tag\\\": \"overbank\",\\n    \\\"popularity\\\": 9631\\n  },\\n  {\\n    \\\"tag\\\": \"prostyle\",\\n    \\\"popularity\\\": 9620\\n  },\\n  {\\n    \\\"tag\\\": \"photoactivation\",\\n    \\\"popularity\\\": 9609\\n  },\\n  {\\n    \\\"tag\\\": \"homothetic\",\\n    \\\"popularity\\\": 9598\\n  },\\n  {\\n    \\\"tag\\\": \"roguedom\",\\n    \\\"popularity\\\": 9587\\n  },\\n  {\\n    \\\"tag\\\": \"underschool\",\\n    \\\"popularity\\\": 9576\\n  },\\n  {\\n    \\\"tag\\\": \"tractility\",\\n    \\\"popularity\\\": 9565\\n  },\\n  {\\n    \\\"tag\\\": \"gardenin\",\\n    \\\"popularity\\\": 9554\\n  },\\n  {\\n    \\\"tag\\\": \"Micromastictora\",\\n    \\\"popularity\\\": 9543\\n  },\\n  {\\n    \\\"tag\\\": \"gossypine\",\\n    \\\"popularity\\\": 9532\\n  },\\n  {\\n    \\\"tag\\\": \"amylodyspepsia\",\\n    \\\"popularity\\\": 9521\\n  },\\n  {\\n    \\\"tag\\\": \"Luciana\",\\n    \\\"popularity\\\": 9510\\n  },\\n  {\\n    \\\"tag\\\": \"meetly nonfisherman\",\\n    \\\"popularity\\\": 9500\\n  },\\n  {\\n    \\\"tag\\\": \"backhanded\",\\n    \\\"popularity\\\": 9489\\n  },\\n  {\\n    \\\"tag\\\": \"decrustation\",\\n    \\\"popularity\\\": 9478\\n  },\\n  {\\n    \\\"tag\\\": \"pinrail\",\\n    \\\"popularity\\\": 9467\\n  },\\n  {\\n    \\\"tag\\\": \"Mahori\",\\n    \\\"popularity\\\": 9456\\n  },\\n  {\\n    \\\"tag\\\": \"unsizable\",\\n    \\\"popularity\\\": 9446\\n  },\\n  {\\n    \\\"tag\\\": \"disawa\",\\n    \\\"popularity\\\": 9435\\n  },\\n  {\\n    \\\"tag\\\": \"launderability inconsidered\",\\n    \\\"popularity\\\": 9424\\n  },\\n  {\\n    \\\"tag\\\": \"unclassical\",\\n    \\\"popularity\\\": 9414\\n  },\\n  {\\n    \\\"tag\\\": \"inobtrusiveness\",\\n    \\\"popularity\\\": 9403\\n  },\\n  {\\n    \\\"tag\\\": \"sialogenous\",\\n    \\\"popularity\\\": 9392\\n  },\\n  {\\n    \\\"tag\\\": \"sulphonamide\",\\n    \\\"popularity\\\": 9382\\n  },\\n  {\\n    \\\"tag\\\": \"diluvion\",\\n    \\\"popularity\\\": 9371\\n  },\\n  {\\n    \\\"tag\\\": \"deuteranope\",\\n    \\\"popularity\\\": 9361\\n  },\\n  {\\n    \\\"tag\\\": \"addition\",\\n    \\\"popularity\\\": 9350\\n  },\\n  {\\n    \\\"tag\\\": \"bockeret\",\\n    \\\"popularity\\\": 9339\\n  },\\n  {\\n    \\\"tag\\\": \"unidentified\",\\n    \\\"popularity\\\": 9329\\n  },\\n  {\\n    \\\"tag\\\": \"caryatic\",\\n    \\\"popularity\\\": 9318\\n  },\\n  {\\n    \\\"tag\\\": \"misattribution\",\\n    \\\"popularity\\\": 9308\\n  },\\n  {\\n    \\\"tag\\\": \"outray\",\\n    \\\"popularity\\\": 9297\\n  },\\n  {\\n    \\\"tag\\\": \"areometrical\",\\n    \\\"popularity\\\": 9287\\n  },\\n  {\\n    \\\"tag\\\": \"antilogism\",\\n    \\\"popularity\\\": 9277\\n  },\\n  {\\n    \\\"tag\\\": \"inadjustable\",\\n    \\\"popularity\\\": 9266\\n  },\\n  {\\n    \\\"tag\\\": \"byssus\",\\n    \\\"popularity\\\": 9256\\n  },\\n  {\\n    \\\"tag\\\": \"trun\",\\n    \\\"popularity\\\": 9245\\n  },\\n  {\\n    \\\"tag\\\": \"thereology\",\\n    \\\"popularity\\\": 9235\\n  },\\n  {\\n    \\\"tag\\\": \"extort\",\\n    \\\"popularity\\\": 9225\\n  },\\n  {\\n    \\\"tag\\\": \"bumpkin\",\\n    \\\"popularity\\\": 9214\\n  },\\n  {\\n    \\\"tag\\\": \"sulphobenzide\",\\n    \\\"popularity\\\": 9204\\n  },\\n  {\\n    \\\"tag\\\": \"hydrogeology\",\\n    \\\"popularity\\\": 9194\\n  },\\n  {\\n    \\\"tag\\\": \"nidulariaceous\",\\n    \\\"popularity\\\": 9183\\n  },\\n  {\\n    \\\"tag\\\": \"propodiale\",\\n    \\\"popularity\\\": 9173\\n  },\\n  {\\n    \\\"tag\\\": \"fierily\",\\n    \\\"popularity\\\": 9163\\n  },\\n  {\\n    \\\"tag\\\": \"aerotonometry\",\\n    \\\"popularity\\\": 9153\\n  },\\n  {\\n    \\\"tag\\\": \"pelobatid oversuperstitious\",\\n    \\\"popularity\\\": 9142\\n  },\\n  {\\n    \\\"tag\\\": \"restringent\",\\n    \\\"popularity\\\": 9132\\n  },\\n  {\\n    \\\"tag\\\": \"tetrapodic\",\\n    \\\"popularity\\\": 9122\\n  },\\n  {\\n    \\\"tag\\\": \"heroicness Vendidad\",\\n    \\\"popularity\\\": 9112\\n  },\\n  {\\n    \\\"tag\\\": \"Sphingurus\",\\n    \\\"popularity\\\": 9102\\n  },\\n  {\\n    \\\"tag\\\": \"sclerote\",\\n    \\\"popularity\\\": 9092\\n  },\\n  {\\n    \\\"tag\\\": \"unkeyed\",\\n    \\\"popularity\\\": 9082\\n  },\\n  {\\n    \\\"tag\\\": \"superparliamentary\",\\n    \\\"popularity\\\": 9072\\n  },\\n  {\\n    \\\"tag\\\": \"hetericism\",\\n    \\\"popularity\\\": 9061\\n  },\\n  {\\n    \\\"tag\\\": \"hucklebone\",\\n    \\\"popularity\\\": 9051\\n  },\\n  {\\n    \\\"tag\\\": \"yojan\",\\n    \\\"popularity\\\": 9041\\n  },\\n  {\\n    \\\"tag\\\": \"bossed\",\\n    \\\"popularity\\\": 9031\\n  },\\n  {\\n    \\\"tag\\\": \"spiderwork\",\\n    \\\"popularity\\\": 9021\\n  },\\n  {\\n    \\\"tag\\\": \"millfeed dullery\",\\n    \\\"popularity\\\": 9011\\n  },\\n  {\\n    \\\"tag\\\": \"adnoun\",\\n    \\\"popularity\\\": 9001\\n  },\\n  {\\n    \\\"tag\\\": \"mesometric\",\\n    \\\"popularity\\\": 8992\\n  },\\n  {\\n    \\\"tag\\\": \"doublehandedness\",\\n    \\\"popularity\\\": 8982\\n  },\\n  {\\n    \\\"tag\\\": \"suppurant\",\\n    \\\"popularity\\\": 8972\\n  },\\n  {\\n    \\\"tag\\\": \"Berlinize\",\\n    \\\"popularity\\\": 8962\\n  },\\n  {\\n    \\\"tag\\\": \"sontag\",\\n    \\\"popularity\\\": 8952\\n  },\\n  {\\n    \\\"tag\\\": \"biplane\",\\n    \\\"popularity\\\": 8942\\n  },\\n  {\\n    \\\"tag\\\": \"insula\",\\n    \\\"popularity\\\": 8932\\n  },\\n  {\\n    \\\"tag\\\": \"unbrand\",\\n    \\\"popularity\\\": 8922\\n  },\\n  {\\n    \\\"tag\\\": \"Basilosaurus\",\\n    \\\"popularity\\\": 8913\\n  },\\n  {\\n    \\\"tag\\\": \"prenomination\",\\n    \\\"popularity\\\": 8903\\n  },\\n  {\\n    \\\"tag\\\": \"untextual\",\\n    \\\"popularity\\\": 8893\\n  },\\n  {\\n    \\\"tag\\\": \"coleslaw\",\\n    \\\"popularity\\\": 8883\\n  },\\n  {\\n    \\\"tag\\\": \"langsyne\",\\n    \\\"popularity\\\": 8874\\n  },\\n  {\\n    \\\"tag\\\": \"impede\",\\n    \\\"popularity\\\": 8864\\n  },\\n  {\\n    \\\"tag\\\": \"irrigator\",\\n    \\\"popularity\\\": 8854\\n  },\\n  {\\n    \\\"tag\\\": \"deflocculation\",\\n    \\\"popularity\\\": 8844\\n  },\\n  {\\n    \\\"tag\\\": \"narghile\",\\n    \\\"popularity\\\": 8835\\n  },\\n  {\\n    \\\"tag\\\": \"unguardedly ebenaceous\",\\n    \\\"popularity\\\": 8825\\n  },\\n  {\\n    \\\"tag\\\": \"conversantly subocular\",\\n    \\\"popularity\\\": 8815\\n  },\\n  {\\n    \\\"tag\\\": \"hydroponic\",\\n    \\\"popularity\\\": 8806\\n  },\\n  {\\n    \\\"tag\\\": \"anthropopsychism\",\\n    \\\"popularity\\\": 8796\\n  },\\n  {\\n    \\\"tag\\\": \"panoptic\",\\n    \\\"popularity\\\": 8787\\n  },\\n  {\\n    \\\"tag\\\": \"insufferable\",\\n    \\\"popularity\\\": 8777\\n  },\\n  {\\n    \\\"tag\\\": \"salema\",\\n    \\\"popularity\\\": 8768\\n  },\\n  {\\n    \\\"tag\\\": \"Myriapoda\",\\n    \\\"popularity\\\": 8758\\n  },\\n  {\\n    \\\"tag\\\": \"regarrison\",\\n    \\\"popularity\\\": 8748\\n  },\\n  {\\n    \\\"tag\\\": \"overlearned\",\\n    \\\"popularity\\\": 8739\\n  },\\n  {\\n    \\\"tag\\\": \"ultraroyalist conventical bureaucratical\",\\n    \\\"popularity\\\": 8729\\n  },\\n  {\\n    \\\"tag\\\": \"epicaridan\",\\n    \\\"popularity\\\": 8720\\n  },\\n  {\\n    \\\"tag\\\": \"poetastress\",\\n    \\\"popularity\\\": 8711\\n  },\\n  {\\n    \\\"tag\\\": \"monophthalmus\",\\n    \\\"popularity\\\": 8701\\n  },\\n  {\\n    \\\"tag\\\": \"simnel\",\\n    \\\"popularity\\\": 8692\\n  },\\n  {\\n    \\\"tag\\\": \"compotor\",\\n    \\\"popularity\\\": 8682\\n  },\\n  {\\n    \\\"tag\\\": \"hydrolase\",\\n    \\\"popularity\\\": 8673\\n  },\\n  {\\n    \\\"tag\\\": \"attemptless\",\\n    \\\"popularity\\\": 8663\\n  },\\n  {\\n    \\\"tag\\\": \"visceroptosis\",\\n    \\\"popularity\\\": 8654\\n  },\\n  {\\n    \\\"tag\\\": \"unpreparedly\",\\n    \\\"popularity\\\": 8645\\n  },\\n  {\\n    \\\"tag\\\": \"mastage\",\\n    \\\"popularity\\\": 8635\\n  },\\n  {\\n    \\\"tag\\\": \"preinfluence\",\\n    \\\"popularity\\\": 8626\\n  },\\n  {\\n    \\\"tag\\\": \"Siwan\",\\n    \\\"popularity\\\": 8617\\n  },\\n  {\\n    \\\"tag\\\": \"ceratotheca belvedere\",\\n    \\\"popularity\\\": 8607\\n  },\\n  {\\n    \\\"tag\\\": \"disenablement\",\\n    \\\"popularity\\\": 8598\\n  },\\n  {\\n    \\\"tag\\\": \"nine\",\\n    \\\"popularity\\\": 8589\\n  },\\n  {\\n    \\\"tag\\\": \"spellingdown abridgment\",\\n    \\\"popularity\\\": 8580\\n  },\\n  {\\n    \\\"tag\\\": \"twilightless\",\\n    \\\"popularity\\\": 8571\\n  },\\n  {\\n    \\\"tag\\\": \"overflow\",\\n    \\\"popularity\\\": 8561\\n  },\\n  {\\n    \\\"tag\\\": \"mismeasurement\",\\n    \\\"popularity\\\": 8552\\n  },\\n  {\\n    \\\"tag\\\": \"nawabship\",\\n    \\\"popularity\\\": 8543\\n  },\\n  {\\n    \\\"tag\\\": \"Phrynosoma\",\\n    \\\"popularity\\\": 8534\\n  },\\n  {\\n    \\\"tag\\\": \"unanticipatingly\",\\n    \\\"popularity\\\": 8525\\n  },\\n  {\\n    \\\"tag\\\": \"blankite\",\\n    \\\"popularity\\\": 8516\\n  },\\n  {\\n    \\\"tag\\\": \"role\",\\n    \\\"popularity\\\": 8506\\n  },\\n  {\\n    \\\"tag\\\": \"peperine edelweiss\",\\n    \\\"popularity\\\": 8497\\n  },\\n  {\\n    \\\"tag\\\": \"unhysterical\",\\n    \\\"popularity\\\": 8488\\n  },\\n  {\\n    \\\"tag\\\": \"attentiveness\",\\n    \\\"popularity\\\": 8479\\n  },\\n  {\\n    \\\"tag\\\": \"scintillant\",\\n    \\\"popularity\\\": 8470\\n  },\\n  {\\n    \\\"tag\\\": \"stenostomatous\",\\n    \\\"popularity\\\": 8461\\n  },\\n  {\\n    \\\"tag\\\": \"pectinite\",\\n    \\\"popularity\\\": 8452\\n  },\\n  {\\n    \\\"tag\\\": \"herring\",\\n    \\\"popularity\\\": 8443\\n  },\\n  {\\n    \\\"tag\\\": \"interroom\",\\n    \\\"popularity\\\": 8434\\n  },\\n  {\\n    \\\"tag\\\": \"laccol\",\\n    \\\"popularity\\\": 8425\\n  },\\n  {\\n    \\\"tag\\\": \"unpartably kylite\",\\n    \\\"popularity\\\": 8416\\n  },\\n  {\\n    \\\"tag\\\": \"spirivalve\",\\n    \\\"popularity\\\": 8407\\n  },\\n  {\\n    \\\"tag\\\": \"hoosegow\",\\n    \\\"popularity\\\": 8398\\n  },\\n  {\\n    \\\"tag\\\": \"doat\",\\n    \\\"popularity\\\": 8389\\n  },\\n  {\\n    \\\"tag\\\": \"amphibian\",\\n    \\\"popularity\\\": 8380\\n  },\\n  {\\n    \\\"tag\\\": \"exposit\",\\n    \\\"popularity\\\": 8371\\n  },\\n  {\\n    \\\"tag\\\": \"canopy\",\\n    \\\"popularity\\\": 8363\\n  },\\n  {\\n    \\\"tag\\\": \"houndlike\",\\n    \\\"popularity\\\": 8354\\n  },\\n  {\\n    \\\"tag\\\": \"spikebill\",\\n    \\\"popularity\\\": 8345\\n  },\\n  {\\n    \\\"tag\\\": \"wiseacre pyrotechnic\",\\n    \\\"popularity\\\": 8336\\n  },\\n  {\\n    \\\"tag\\\": \"confessingly woodman\",\\n    \\\"popularity\\\": 8327\\n  },\\n  {\\n    \\\"tag\\\": \"overside\",\\n    \\\"popularity\\\": 8318\\n  },\\n  {\\n    \\\"tag\\\": \"oftwhiles\",\\n    \\\"popularity\\\": 8310\\n  },\\n  {\\n    \\\"tag\\\": \"Musophagidae\",\\n    \\\"popularity\\\": 8301\\n  },\\n  {\\n    \\\"tag\\\": \"slumberer\",\\n    \\\"popularity\\\": 8292\\n  },\\n  {\\n    \\\"tag\\\": \"leiotrichy\",\\n    \\\"popularity\\\": 8283\\n  },\\n  {\\n    \\\"tag\\\": \"Mantispidae\",\\n    \\\"popularity\\\": 8275\\n  },\\n  {\\n    \\\"tag\\\": \"perceptually\",\\n    \\\"popularity\\\": 8266\\n  },\\n  {\\n    \\\"tag\\\": \"biller\",\\n    \\\"popularity\\\": 8257\\n  },\\n  {\\n    \\\"tag\\\": \"eudaemonical\",\\n    \\\"popularity\\\": 8249\\n  },\\n  {\\n    \\\"tag\\\": \"underfiend\",\\n    \\\"popularity\\\": 8240\\n  },\\n  {\\n    \\\"tag\\\": \"impartible\",\\n    \\\"popularity\\\": 8231\\n  },\\n  {\\n    \\\"tag\\\": \"saxicavous\",\\n    \\\"popularity\\\": 8223\\n  },\\n  {\\n    \\\"tag\\\": \"yapster\",\\n    \\\"popularity\\\": 8214\\n  },\\n  {\\n    \\\"tag\\\": \"aliseptal\",\\n    \\\"popularity\\\": 8205\\n  },\\n  {\\n    \\\"tag\\\": \"omniparient\",\\n    \\\"popularity\\\": 8197\\n  },\\n  {\\n    \\\"tag\\\": \"nishiki\",\\n    \\\"popularity\\\": 8188\\n  },\\n  {\\n    \\\"tag\\\": \"yuzluk\",\\n    \\\"popularity\\\": 8180\\n  },\\n  {\\n    \\\"tag\\\": \"solderer\",\\n    \\\"popularity\\\": 8171\\n  },\\n  {\\n    \\\"tag\\\": \"Pinna\",\\n    \\\"popularity\\\": 8162\\n  },\\n  {\\n    \\\"tag\\\": \"reinterfere\",\\n    \\\"popularity\\\": 8154\\n  },\\n  {\\n    \\\"tag\\\": \"superepic\",\\n    \\\"popularity\\\": 8145\\n  },\\n  {\\n    \\\"tag\\\": \"ronquil\",\\n    \\\"popularity\\\": 8137\\n  },\\n  {\\n    \\\"tag\\\": \"bratstvo\",\\n    \\\"popularity\\\": 8128\\n  },\\n  {\\n    \\\"tag\\\": \"Thea\",\\n    \\\"popularity\\\": 8120\\n  },\\n  {\\n    \\\"tag\\\": \"hermaphroditical\",\\n    \\\"popularity\\\": 8111\\n  },\\n  {\\n    \\\"tag\\\": \"enlief\",\\n    \\\"popularity\\\": 8103\\n  },\\n  {\\n    \\\"tag\\\": \"Jesuate\",\\n    \\\"popularity\\\": 8095\\n  },\\n  {\\n    \\\"tag\\\": \"gaysome\",\\n    \\\"popularity\\\": 8086\\n  },\\n  {\\n    \\\"tag\\\": \"iliohypogastric\",\\n    \\\"popularity\\\": 8078\\n  },\\n  {\\n    \\\"tag\\\": \"regardance\",\\n    \\\"popularity\\\": 8069\\n  },\\n  {\\n    \\\"tag\\\": \"cumulately\",\\n    \\\"popularity\\\": 8061\\n  },\\n  {\\n    \\\"tag\\\": \"haustorial nucleolocentrosome\",\\n    \\\"popularity\\\": 8053\\n  },\\n  {\\n    \\\"tag\\\": \"cosmocrat\",\\n    \\\"popularity\\\": 8044\\n  },\\n  {\\n    \\\"tag\\\": \"onyxitis\",\\n    \\\"popularity\\\": 8036\\n  },\\n  {\\n    \\\"tag\\\": \"Cabinda\",\\n    \\\"popularity\\\": 8028\\n  },\\n  {\\n    \\\"tag\\\": \"coresort\",\\n    \\\"popularity\\\": 8019\\n  },\\n  {\\n    \\\"tag\\\": \"drusy preformant\",\\n    \\\"popularity\\\": 8011\\n  },\\n  {\\n    \\\"tag\\\": \"piningly\",\\n    \\\"popularity\\\": 8003\\n  },\\n  {\\n    \\\"tag\\\": \"bootlessly\",\\n    \\\"popularity\\\": 7994\\n  },\\n  {\\n    \\\"tag\\\": \"talari\",\\n    \\\"popularity\\\": 7986\\n  },\\n  {\\n    \\\"tag\\\": \"amidoacetal\",\\n    \\\"popularity\\\": 7978\\n  },\\n  {\\n    \\\"tag\\\": \"pschent\",\\n    \\\"popularity\\\": 7970\\n  },\\n  {\\n    \\\"tag\\\": \"consumptional scarer titivate\",\\n    \\\"popularity\\\": 7962\\n  },\\n  {\\n    \\\"tag\\\": \"Anserinae\",\\n    \\\"popularity\\\": 7953\\n  },\\n  {\\n    \\\"tag\\\": \"flaunter\",\\n    \\\"popularity\\\": 7945\\n  },\\n  {\\n    \\\"tag\\\": \"reindeer\",\\n    \\\"popularity\\\": 7937\\n  },\\n  {\\n    \\\"tag\\\": \"disparage\",\\n    \\\"popularity\\\": 7929\\n  },\\n  {\\n    \\\"tag\\\": \"superheat\",\\n    \\\"popularity\\\": 7921\\n  },\\n  {\\n    \\\"tag\\\": \"Chromatium\",\\n    \\\"popularity\\\": 7912\\n  },\\n  {\\n    \\\"tag\\\": \"Tina\",\\n    \\\"popularity\\\": 7904\\n  },\\n  {\\n    \\\"tag\\\": \"rededicatory\",\\n    \\\"popularity\\\": 7896\\n  },\\n  {\\n    \\\"tag\\\": \"nontransient\",\\n    \\\"popularity\\\": 7888\\n  },\\n  {\\n    \\\"tag\\\": \"Phocaean brinkless\",\\n    \\\"popularity\\\": 7880\\n  },\\n  {\\n    \\\"tag\\\": \"ventriculose\",\\n    \\\"popularity\\\": 7872\\n  },\\n  {\\n    \\\"tag\\\": \"upplough\",\\n    \\\"popularity\\\": 7864\\n  },\\n  {\\n    \\\"tag\\\": \"succorless\",\\n    \\\"popularity\\\": 7856\\n  },\\n  {\\n    \\\"tag\\\": \"hayrake\",\\n    \\\"popularity\\\": 7848\\n  },\\n  {\\n    \\\"tag\\\": \"merriness amorphia\",\\n    \\\"popularity\\\": 7840\\n  },\\n  {\\n    \\\"tag\\\": \"merycism\",\\n    \\\"popularity\\\": 7832\\n  },\\n  {\\n    \\\"tag\\\": \"checkrow\",\\n    \\\"popularity\\\": 7824\\n  },\\n  {\\n    \\\"tag\\\": \"scry\",\\n    \\\"popularity\\\": 7816\\n  },\\n  {\\n    \\\"tag\\\": \"obvolve\",\\n    \\\"popularity\\\": 7808\\n  },\\n  {\\n    \\\"tag\\\": \"orchard\",\\n    \\\"popularity\\\": 7800\\n  },\\n  {\\n    \\\"tag\\\": \"isomerize\",\\n    \\\"popularity\\\": 7792\\n  },\\n  {\\n    \\\"tag\\\": \"competitrix\",\\n    \\\"popularity\\\": 7784\\n  },\\n  {\\n    \\\"tag\\\": \"unbannered\",\\n    \\\"popularity\\\": 7776\\n  },\\n  {\\n    \\\"tag\\\": \"undoctrined\",\\n    \\\"popularity\\\": 7768\\n  },\\n  {\\n    \\\"tag\\\": \"theologian\",\\n    \\\"popularity\\\": 7760\\n  },\\n  {\\n    \\\"tag\\\": \"nebby\",\\n    \\\"popularity\\\": 7752\\n  },\\n  {\\n    \\\"tag\\\": \"Cardiazol\",\\n    \\\"popularity\\\": 7745\\n  },\\n  {\\n    \\\"tag\\\": \"phagedenic\",\\n    \\\"popularity\\\": 7737\\n  },\\n  {\\n    \\\"tag\\\": \"nostalgic\",\\n    \\\"popularity\\\": 7729\\n  },\\n  {\\n    \\\"tag\\\": \"orthodoxy\",\\n    \\\"popularity\\\": 7721\\n  },\\n  {\\n    \\\"tag\\\": \"oversanguine\",\\n    \\\"popularity\\\": 7713\\n  },\\n  {\\n    \\\"tag\\\": \"lish\",\\n    \\\"popularity\\\": 7705\\n  },\\n  {\\n    \\\"tag\\\": \"ketogenic\",\\n    \\\"popularity\\\": 7698\\n  },\\n  {\\n    \\\"tag\\\": \"syndicalize\",\\n    \\\"popularity\\\": 7690\\n  },\\n  {\\n    \\\"tag\\\": \"leeftail\",\\n    \\\"popularity\\\": 7682\\n  },\\n  {\\n    \\\"tag\\\": \"bulbomedullary\",\\n    \\\"popularity\\\": 7674\\n  },\\n  {\\n    \\\"tag\\\": \"reletter\",\\n    \\\"popularity\\\": 7667\\n  },\\n  {\\n    \\\"tag\\\": \"bitterly\",\\n    \\\"popularity\\\": 7659\\n  },\\n  {\\n    \\\"tag\\\": \"participatory\",\\n    \\\"popularity\\\": 7651\\n  },\\n  {\\n    \\\"tag\\\": \"baldberry\",\\n    \\\"popularity\\\": 7643\\n  },\\n  {\\n    \\\"tag\\\": \"prowaterpower\",\\n    \\\"popularity\\\": 7636\\n  },\\n  {\\n    \\\"tag\\\": \"lexicographical\",\\n    \\\"popularity\\\": 7628\\n  },\\n  {\\n    \\\"tag\\\": \"Anisodactyli\",\\n    \\\"popularity\\\": 7620\\n  },\\n  {\\n    \\\"tag\\\": \"amphipodous\",\\n    \\\"popularity\\\": 7613\\n  },\\n  {\\n    \\\"tag\\\": \"triglandular\",\\n    \\\"popularity\\\": 7605\\n  },\\n  {\\n    \\\"tag\\\": \"xanthopsin\",\\n    \\\"popularity\\\": 7597\\n  },\\n  {\\n    \\\"tag\\\": \"indefinitude\",\\n    \\\"popularity\\\": 7590\\n  },\\n  {\\n    \\\"tag\\\": \"bookworm\",\\n    \\\"popularity\\\": 7582\\n  },\\n  {\\n    \\\"tag\\\": \"suffocative\",\\n    \\\"popularity\\\": 7574\\n  },\\n  {\\n    \\\"tag\\\": \"uncongested tyrant\",\\n    \\\"popularity\\\": 7567\\n  },\\n  {\\n    \\\"tag\\\": \"alow harmoniously Pamir\",\\n    \\\"popularity\\\": 7559\\n  },\\n  {\\n    \\\"tag\\\": \"monander\",\\n    \\\"popularity\\\": 7552\\n  },\\n  {\\n    \\\"tag\\\": \"bagatelle\",\\n    \\\"popularity\\\": 7544\\n  },\\n  {\\n    \\\"tag\\\": \"membranology\",\\n    \\\"popularity\\\": 7537\\n  },\\n  {\\n    \\\"tag\\\": \"parturifacient\",\\n    \\\"popularity\\\": 7529\\n  },\\n  {\\n    \\\"tag\\\": \"excitovascular\",\\n    \\\"popularity\\\": 7522\\n  },\\n  {\\n    \\\"tag\\\": \"homopolar\",\\n    \\\"popularity\\\": 7514\\n  },\\n  {\\n    \\\"tag\\\": \"phobiac\",\\n    \\\"popularity\\\": 7507\\n  },\\n  {\\n    \\\"tag\\\": \"clype\",\\n    \\\"popularity\\\": 7499\\n  },\\n  {\\n    \\\"tag\\\": \"unsubversive\",\\n    \\\"popularity\\\": 7492\\n  },\\n  {\\n    \\\"tag\\\": \"bostrychoidal scorpionwort\",\\n    \\\"popularity\\\": 7484\\n  },\\n  {\\n    \\\"tag\\\": \"biliteralism\",\\n    \\\"popularity\\\": 7477\\n  },\\n  {\\n    \\\"tag\\\": \"dentatocostate\",\\n    \\\"popularity\\\": 7469\\n  },\\n  {\\n    \\\"tag\\\": \"Pici\",\\n    \\\"popularity\\\": 7462\\n  },\\n  {\\n    \\\"tag\\\": \"sideritic\",\\n    \\\"popularity\\\": 7454\\n  },\\n  {\\n    \\\"tag\\\": \"syntaxis\",\\n    \\\"popularity\\\": 7447\\n  },\\n  {\\n    \\\"tag\\\": \"ingest\",\\n    \\\"popularity\\\": 7440\\n  },\\n  {\\n    \\\"tag\\\": \"rigmarolish\",\\n    \\\"popularity\\\": 7432\\n  },\\n  {\\n    \\\"tag\\\": \"ocreaceous\",\\n    \\\"popularity\\\": 7425\\n  },\\n  {\\n    \\\"tag\\\": \"hyperbrachyskelic\",\\n    \\\"popularity\\\": 7418\\n  },\\n  {\\n    \\\"tag\\\": \"basophobia\",\\n    \\\"popularity\\\": 7410\\n  },\\n  {\\n    \\\"tag\\\": \"substantialness\",\\n    \\\"popularity\\\": 7403\\n  },\\n  {\\n    \\\"tag\\\": \"agglutinoid\",\\n    \\\"popularity\\\": 7396\\n  },\\n  {\\n    \\\"tag\\\": \"longleaf\",\\n    \\\"popularity\\\": 7388\\n  },\\n  {\\n    \\\"tag\\\": \"electroengraving\",\\n    \\\"popularity\\\": 7381\\n  },\\n  {\\n    \\\"tag\\\": \"laparoenterotomy\",\\n    \\\"popularity\\\": 7374\\n  },\\n  {\\n    \\\"tag\\\": \"oxalylurea\",\\n    \\\"popularity\\\": 7366\\n  },\\n  {\\n    \\\"tag\\\": \"unattaintedly\",\\n    \\\"popularity\\\": 7359\\n  },\\n  {\\n    \\\"tag\\\": \"pennystone\",\\n    \\\"popularity\\\": 7352\\n  },\\n  {\\n    \\\"tag\\\": \"Plumbaginaceae\",\\n    \\\"popularity\\\": 7345\\n  },\\n  {\\n    \\\"tag\\\": \"horntip\",\\n    \\\"popularity\\\": 7337\\n  },\\n  {\\n    \\\"tag\\\": \"begrudge\",\\n    \\\"popularity\\\": 7330\\n  },\\n  {\\n    \\\"tag\\\": \"bechignoned\",\\n    \\\"popularity\\\": 7323\\n  },\\n  {\\n    \\\"tag\\\": \"hologonidium\",\\n    \\\"popularity\\\": 7316\\n  },\\n  {\\n    \\\"tag\\\": \"Pulian\",\\n    \\\"popularity\\\": 7309\\n  },\\n  {\\n    \\\"tag\\\": \"gratulation\",\\n    \\\"popularity\\\": 7301\\n  },\\n  {\\n    \\\"tag\\\": \"Sebright\",\\n    \\\"popularity\\\": 7294\\n  },\\n  {\\n    \\\"tag\\\": \"coinstantaneous emotionally\",\\n    \\\"popularity\\\": 7287\\n  },\\n  {\\n    \\\"tag\\\": \"thoracostracan\",\\n    \\\"popularity\\\": 7280\\n  },\\n  {\\n    \\\"tag\\\": \"saurodont\",\\n    \\\"popularity\\\": 7273\\n  },\\n  {\\n    \\\"tag\\\": \"coseat\",\\n    \\\"popularity\\\": 7266\\n  },\\n  {\\n    \\\"tag\\\": \"irascibility\",\\n    \\\"popularity\\\": 7259\\n  },\\n  {\\n    \\\"tag\\\": \"occlude\",\\n    \\\"popularity\\\": 7251\\n  },\\n  {\\n    \\\"tag\\\": \"metallurgist\",\\n    \\\"popularity\\\": 7244\\n  },\\n  {\\n    \\\"tag\\\": \"extraviolet\",\\n    \\\"popularity\\\": 7237\\n  },\\n  {\\n    \\\"tag\\\": \"clinic\",\\n    \\\"popularity\\\": 7230\\n  },\\n  {\\n    \\\"tag\\\": \"skater\",\\n    \\\"popularity\\\": 7223\\n  },\\n  {\\n    \\\"tag\\\": \"linguistic\",\\n    \\\"popularity\\\": 7216\\n  },\\n  {\\n    \\\"tag\\\": \"attacheship\",\\n    \\\"popularity\\\": 7209\\n  },\\n  {\\n    \\\"tag\\\": \"Rachianectes\",\\n    \\\"popularity\\\": 7202\\n  },\\n  {\\n    \\\"tag\\\": \"foliolose\",\\n    \\\"popularity\\\": 7195\\n  },\\n  {\\n    \\\"tag\\\": \"claudetite\",\\n    \\\"popularity\\\": 7188\\n  },\\n  {\\n    \\\"tag\\\": \"aphidian scratching\",\\n    \\\"popularity\\\": 7181\\n  },\\n  {\\n    \\\"tag\\\": \"Carida\",\\n    \\\"popularity\\\": 7174\\n  },\\n  {\\n    \\\"tag\\\": \"tiepin polymicroscope\",\\n    \\\"popularity\\\": 7167\\n  },\\n  {\\n    \\\"tag\\\": \"telpherage\",\\n    \\\"popularity\\\": 7160\\n  },\\n  {\\n    \\\"tag\\\": \"meek\",\\n    \\\"popularity\\\": 7153\\n  },\\n  {\\n    \\\"tag\\\": \"swiftness\",\\n    \\\"popularity\\\": 7146\\n  },\\n  {\\n    \\\"tag\\\": \"gentes\",\\n    \\\"popularity\\\": 7139\\n  },\\n  {\\n    \\\"tag\\\": \"uncommemorated\",\\n    \\\"popularity\\\": 7132\\n  },\\n  {\\n    \\\"tag\\\": \"Lazarus\",\\n    \\\"popularity\\\": 7125\\n  },\\n  {\\n    \\\"tag\\\": \"redivive\",\\n    \\\"popularity\\\": 7119\\n  },\\n  {\\n    \\\"tag\\\": \"nonfebrile\",\\n    \\\"popularity\\\": 7112\\n  },\\n  {\\n    \\\"tag\\\": \"nymphet\",\\n    \\\"popularity\\\": 7105\\n  },\\n  {\\n    \\\"tag\\\": \"areologically\",\\n    \\\"popularity\\\": 7098\\n  },\\n  {\\n    \\\"tag\\\": \"undonkey\",\\n    \\\"popularity\\\": 7091\\n  },\\n  {\\n    \\\"tag\\\": \"projecting\",\\n    \\\"popularity\\\": 7084\\n  },\\n  {\\n    \\\"tag\\\": \"pinnigrade\",\\n    \\\"popularity\\\": 7077\\n  },\\n  {\\n    \\\"tag\\\": \"butylation\",\\n    \\\"popularity\\\": 7071\\n  },\\n  {\\n    \\\"tag\\\": \"philologistic lenticle\",\\n    \\\"popularity\\\": 7064\\n  },\\n  {\\n    \\\"tag\\\": \"nooky\",\\n    \\\"popularity\\\": 7057\\n  },\\n  {\\n    \\\"tag\\\": \"incestuousness\",\\n    \\\"popularity\\\": 7050\\n  },\\n  {\\n    \\\"tag\\\": \"palingenetically\",\\n    \\\"popularity\\\": 7043\\n  },\\n  {\\n    \\\"tag\\\": \"mitochondria\",\\n    \\\"popularity\\\": 7037\\n  },\\n  {\\n    \\\"tag\\\": \"truthify\",\\n    \\\"popularity\\\": 7030\\n  },\\n  {\\n    \\\"tag\\\": \"titanyl\",\\n    \\\"popularity\\\": 7023\\n  },\\n  {\\n    \\\"tag\\\": \"bestride\",\\n    \\\"popularity\\\": 7016\\n  },\\n  {\\n    \\\"tag\\\": \"chende\",\\n    \\\"popularity\\\": 7010\\n  },\\n  {\\n    \\\"tag\\\": \"Chaucerian monophote\",\\n    \\\"popularity\\\": 7003\\n  },\\n  {\\n    \\\"tag\\\": \"cutback\",\\n    \\\"popularity\\\": 6996\\n  },\\n  {\\n    \\\"tag\\\": \"unpatiently\",\\n    \\\"popularity\\\": 6989\\n  },\\n  {\\n    \\\"tag\\\": \"subvitreous\",\\n    \\\"popularity\\\": 6983\\n  },\\n  {\\n    \\\"tag\\\": \"organizable\",\\n    \\\"popularity\\\": 6976\\n  },\\n  {\\n    \\\"tag\\\": \"anniverse uncomprehensible\",\\n    \\\"popularity\\\": 6969\\n  },\\n  {\\n    \\\"tag\\\": \"hyalescence\",\\n    \\\"popularity\\\": 6963\\n  },\\n  {\\n    \\\"tag\\\": \"amniochorial\",\\n    \\\"popularity\\\": 6956\\n  },\\n  {\\n    \\\"tag\\\": \"Corybantian\",\\n    \\\"popularity\\\": 6949\\n  },\\n  {\\n    \\\"tag\\\": \"genocide Scaphitidae\",\\n    \\\"popularity\\\": 6943\\n  },\\n  {\\n    \\\"tag\\\": \"accordionist\",\\n    \\\"popularity\\\": 6936\\n  },\\n  {\\n    \\\"tag\\\": \"becheck\",\\n    \\\"popularity\\\": 6930\\n  },\\n  {\\n    \\\"tag\\\": \"overproduce\",\\n    \\\"popularity\\\": 6923\\n  },\\n  {\\n    \\\"tag\\\": \"unmaniac frijolillo\",\\n    \\\"popularity\\\": 6916\\n  },\\n  {\\n    \\\"tag\\\": \"multisulcated\",\\n    \\\"popularity\\\": 6910\\n  },\\n  {\\n    \\\"tag\\\": \"wennebergite\",\\n    \\\"popularity\\\": 6903\\n  },\\n  {\\n    \\\"tag\\\": \"tautousious mowth\",\\n    \\\"popularity\\\": 6897\\n  },\\n  {\\n    \\\"tag\\\": \"marigold\",\\n    \\\"popularity\\\": 6890\\n  },\\n  {\\n    \\\"tag\\\": \"affray\",\\n    \\\"popularity\\\": 6884\\n  },\\n  {\\n    \\\"tag\\\": \"nonidolatrous\",\\n    \\\"popularity\\\": 6877\\n  },\\n  {\\n    \\\"tag\\\": \"aphrasia\",\\n    \\\"popularity\\\": 6871\\n  },\\n  {\\n    \\\"tag\\\": \"muddlingly\",\\n    \\\"popularity\\\": 6864\\n  },\\n  {\\n    \\\"tag\\\": \"clear\",\\n    \\\"popularity\\\": 6858\\n  },\\n  {\\n    \\\"tag\\\": \"Clitoria\",\\n    \\\"popularity\\\": 6851\\n  },\\n  {\\n    \\\"tag\\\": \"apportionment underwaist\",\\n    \\\"popularity\\\": 6845\\n  },\\n  {\\n    \\\"tag\\\": \"kodakist\",\\n    \\\"popularity\\\": 6838\\n  },\\n  {\\n    \\\"tag\\\": \"Momotidae\",\\n    \\\"popularity\\\": 6832\\n  },\\n  {\\n    \\\"tag\\\": \"cryptovalency\",\\n    \\\"popularity\\\": 6825\\n  },\\n  {\\n    \\\"tag\\\": \"floe\",\\n    \\\"popularity\\\": 6819\\n  },\\n  {\\n    \\\"tag\\\": \"aphagia\",\\n    \\\"popularity\\\": 6812\\n  },\\n  {\\n    \\\"tag\\\": \"brontograph\",\\n    \\\"popularity\\\": 6806\\n  },\\n  {\\n    \\\"tag\\\": \"tubulous\",\\n    \\\"popularity\\\": 6799\\n  },\\n  {\\n    \\\"tag\\\": \"unhorse\",\\n    \\\"popularity\\\": 6793\\n  },\\n  {\\n    \\\"tag\\\": \"chlordane\",\\n    \\\"popularity\\\": 6787\\n  },\\n  {\\n    \\\"tag\\\": \"colloquy brochan\",\\n    \\\"popularity\\\": 6780\\n  },\\n  {\\n    \\\"tag\\\": \"sloosh\",\\n    \\\"popularity\\\": 6774\\n  },\\n  {\\n    \\\"tag\\\": \"battered\",\\n    \\\"popularity\\\": 6767\\n  },\\n  {\\n    \\\"tag\\\": \"monocularity pluriguttulate\",\\n    \\\"popularity\\\": 6761\\n  },\\n  {\\n    \\\"tag\\\": \"chiastoneury\",\\n    \\\"popularity\\\": 6755\\n  },\\n  {\\n    \\\"tag\\\": \"Sanguinaria\",\\n    \\\"popularity\\\": 6748\\n  },\\n  {\\n    \\\"tag\\\": \"confessionary\",\\n    \\\"popularity\\\": 6742\\n  },\\n  {\\n    \\\"tag\\\": \"enzymic\",\\n    \\\"popularity\\\": 6736\\n  },\\n  {\\n    \\\"tag\\\": \"cord\",\\n    \\\"popularity\\\": 6729\\n  },\\n  {\\n    \\\"tag\\\": \"oviducal\",\\n    \\\"popularity\\\": 6723\\n  },\\n  {\\n    \\\"tag\\\": \"crozzle outsea\",\\n    \\\"popularity\\\": 6717\\n  },\\n  {\\n    \\\"tag\\\": \"balladical\",\\n    \\\"popularity\\\": 6710\\n  },\\n  {\\n    \\\"tag\\\": \"uncollectibleness\",\\n    \\\"popularity\\\": 6704\\n  },\\n  {\\n    \\\"tag\\\": \"predorsal\",\\n    \\\"popularity\\\": 6698\\n  },\\n  {\\n    \\\"tag\\\": \"reauthenticate\",\\n    \\\"popularity\\\": 6692\\n  },\\n  {\\n    \\\"tag\\\": \"ravissant\",\\n    \\\"popularity\\\": 6685\\n  },\\n  {\\n    \\\"tag\\\": \"advantageousness\",\\n    \\\"popularity\\\": 6679\\n  },\\n  {\\n    \\\"tag\\\": \"rung\",\\n    \\\"popularity\\\": 6673\\n  },\\n  {\\n    \\\"tag\\\": \"duncedom\",\\n    \\\"popularity\\\": 6667\\n  },\\n  {\\n    \\\"tag\\\": \"hematolite\",\\n    \\\"popularity\\\": 6660\\n  },\\n  {\\n    \\\"tag\\\": \"thisness\",\\n    \\\"popularity\\\": 6654\\n  },\\n  {\\n    \\\"tag\\\": \"mapau\",\\n    \\\"popularity\\\": 6648\\n  },\\n  {\\n    \\\"tag\\\": \"Hecatic\",\\n    \\\"popularity\\\": 6642\\n  },\\n  {\\n    \\\"tag\\\": \"meningoencephalocele\",\\n    \\\"popularity\\\": 6636\\n  },\\n  {\\n    \\\"tag\\\": \"confection sorra\",\\n    \\\"popularity\\\": 6630\\n  },\\n  {\\n    \\\"tag\\\": \"unsedate\",\\n    \\\"popularity\\\": 6623\\n  },\\n  {\\n    \\\"tag\\\": \"meningocerebritis\",\\n    \\\"popularity\\\": 6617\\n  },\\n  {\\n    \\\"tag\\\": \"biopsychological\",\\n    \\\"popularity\\\": 6611\\n  },\\n  {\\n    \\\"tag\\\": \"clavicithern\",\\n    \\\"popularity\\\": 6605\\n  },\\n  {\\n    \\\"tag\\\": \"resun\",\\n    \\\"popularity\\\": 6599\\n  },\\n  {\\n    \\\"tag\\\": \"bayamo\",\\n    \\\"popularity\\\": 6593\\n  },\\n  {\\n    \\\"tag\\\": \"seeableness\",\\n    \\\"popularity\\\": 6587\\n  },\\n  {\\n    \\\"tag\\\": \"hypsidolichocephalism\",\\n    \\\"popularity\\\": 6581\\n  },\\n  {\\n    \\\"tag\\\": \"salivous\",\\n    \\\"popularity\\\": 6574\\n  },\\n  {\\n    \\\"tag\\\": \"neumatize\",\\n    \\\"popularity\\\": 6568\\n  },\\n  {\\n    \\\"tag\\\": \"stree\",\\n    \\\"popularity\\\": 6562\\n  },\\n  {\\n    \\\"tag\\\": \"markshot\",\\n    \\\"popularity\\\": 6556\\n  },\\n  {\\n    \\\"tag\\\": \"phraseologically\",\\n    \\\"popularity\\\": 6550\\n  },\\n  {\\n    \\\"tag\\\": \"yealing\",\\n    \\\"popularity\\\": 6544\\n  },\\n  {\\n    \\\"tag\\\": \"puggy\",\\n    \\\"popularity\\\": 6538\\n  },\\n  {\\n    \\\"tag\\\": \"sexadecimal\",\\n    \\\"popularity\\\": 6532\\n  },\\n  {\\n    \\\"tag\\\": \"unofficerlike\",\\n    \\\"popularity\\\": 6526\\n  },\\n  {\\n    \\\"tag\\\": \"curiosa\",\\n    \\\"popularity\\\": 6520\\n  },\\n  {\\n    \\\"tag\\\": \"pedomotor\",\\n    \\\"popularity\\\": 6514\\n  },\\n  {\\n    \\\"tag\\\": \"astrally\",\\n    \\\"popularity\\\": 6508\\n  },\\n  {\\n    \\\"tag\\\": \"prosomatic\",\\n    \\\"popularity\\\": 6502\\n  },\\n  {\\n    \\\"tag\\\": \"bulletheaded\",\\n    \\\"popularity\\\": 6496\\n  },\\n  {\\n    \\\"tag\\\": \"fortuned\",\\n    \\\"popularity\\\": 6490\\n  },\\n  {\\n    \\\"tag\\\": \"pixy\",\\n    \\\"popularity\\\": 6484\\n  },\\n  {\\n    \\\"tag\\\": \"protectrix\",\\n    \\\"popularity\\\": 6478\\n  },\\n  {\\n    \\\"tag\\\": \"arthritical\",\\n    \\\"popularity\\\": 6472\\n  },\\n  {\\n    \\\"tag\\\": \"coction\",\\n    \\\"popularity\\\": 6466\\n  },\\n  {\\n    \\\"tag\\\": \"Anthropos\",\\n    \\\"popularity\\\": 6460\\n  },\\n  {\\n    \\\"tag\\\": \"runer\",\\n    \\\"popularity\\\": 6454\\n  },\\n  {\\n    \\\"tag\\\": \"prenotify\",\\n    \\\"popularity\\\": 6449\\n  },\\n  {\\n    \\\"tag\\\": \"microspheric gastroparalysis\",\\n    \\\"popularity\\\": 6443\\n  },\\n  {\\n    \\\"tag\\\": \"Jovicentrical\",\\n    \\\"popularity\\\": 6437\\n  },\\n  {\\n    \\\"tag\\\": \"ceratopsid\",\\n    \\\"popularity\\\": 6431\\n  },\\n  {\\n    \\\"tag\\\": \"Theodoric\",\\n    \\\"popularity\\\": 6425\\n  },\\n  {\\n    \\\"tag\\\": \"Pactolus\",\\n    \\\"popularity\\\": 6419\\n  },\\n  {\\n    \\\"tag\\\": \"spawning\",\\n    \\\"popularity\\\": 6413\\n  },\\n  {\\n    \\\"tag\\\": \"nonconfidential\",\\n    \\\"popularity\\\": 6407\\n  },\\n  {\\n    \\\"tag\\\": \"halotrichite infumate\",\\n    \\\"popularity\\\": 6402\\n  },\\n  {\\n    \\\"tag\\\": \"undiscriminatingly\",\\n    \\\"popularity\\\": 6396\\n  },\\n  {\\n    \\\"tag\\\": \"unexasperated\",\\n    \\\"popularity\\\": 6390\\n  },\\n  {\\n    \\\"tag\\\": \"isoeugenol\",\\n    \\\"popularity\\\": 6384\\n  },\\n  {\\n    \\\"tag\\\": \"pressboard\",\\n    \\\"popularity\\\": 6378\\n  },\\n  {\\n    \\\"tag\\\": \"unshrew\",\\n    \\\"popularity\\\": 6372\\n  },\\n  {\\n    \\\"tag\\\": \"huffingly\",\\n    \\\"popularity\\\": 6367\\n  },\\n  {\\n    \\\"tag\\\": \"wagaun\",\\n    \\\"popularity\\\": 6361\\n  },\\n  {\\n    \\\"tag\\\": \"squirt Philistine\",\\n    \\\"popularity\\\": 6355\\n  },\\n  {\\n    \\\"tag\\\": \"kryptic\",\\n    \\\"popularity\\\": 6349\\n  },\\n  {\\n    \\\"tag\\\": \"paraform\",\\n    \\\"popularity\\\": 6344\\n  },\\n  {\\n    \\\"tag\\\": \"preverify\",\\n    \\\"popularity\\\": 6338\\n  },\\n  {\\n    \\\"tag\\\": \"dalar\",\\n    \\\"popularity\\\": 6332\\n  },\\n  {\\n    \\\"tag\\\": \"interdictor appraisingly\",\\n    \\\"popularity\\\": 6326\\n  },\\n  {\\n    \\\"tag\\\": \"chipped\",\\n    \\\"popularity\\\": 6321\\n  },\\n  {\\n    \\\"tag\\\": \"Pteropoda\",\\n    \\\"popularity\\\": 6315\\n  },\\n  {\\n    \\\"tag\\\": \"Bohairic\",\\n    \\\"popularity\\\": 6309\\n  },\\n  {\\n    \\\"tag\\\": \"felting\",\\n    \\\"popularity\\\": 6303\\n  },\\n  {\\n    \\\"tag\\\": \"compurgatorial\",\\n    \\\"popularity\\\": 6298\\n  },\\n  {\\n    \\\"tag\\\": \"unclead\",\\n    \\\"popularity\\\": 6292\\n  },\\n  {\\n    \\\"tag\\\": \"stockish\",\\n    \\\"popularity\\\": 6286\\n  },\\n  {\\n    \\\"tag\\\": \"mulligatawny\",\\n    \\\"popularity\\\": 6281\\n  },\\n  {\\n    \\\"tag\\\": \"Monotheletism\",\\n    \\\"popularity\\\": 6275\\n  },\\n  {\\n    \\\"tag\\\": \"lutanist\",\\n    \\\"popularity\\\": 6269\\n  },\\n  {\\n    \\\"tag\\\": \"gluttonize\",\\n    \\\"popularity\\\": 6264\\n  },\\n  {\\n    \\\"tag\\\": \"hackneyed\",\\n    \\\"popularity\\\": 6258\\n  },\\n  {\\n    \\\"tag\\\": \"yield\",\\n    \\\"popularity\\\": 6253\\n  },\\n  {\\n    \\\"tag\\\": \"sulphonamido\",\\n    \\\"popularity\\\": 6247\\n  },\\n  {\\n    \\\"tag\\\": \"granulative\",\\n    \\\"popularity\\\": 6241\\n  },\\n  {\\n    \\\"tag\\\": \"swingy\",\\n    \\\"popularity\\\": 6236\\n  },\\n  {\\n    \\\"tag\\\": \"Desmidiales\",\\n    \\\"popularity\\\": 6230\\n  },\\n  {\\n    \\\"tag\\\": \"tootlish\",\\n    \\\"popularity\\\": 6224\\n  },\\n  {\\n    \\\"tag\\\": \"unsatisfiedly\",\\n    \\\"popularity\\\": 6219\\n  },\\n  {\\n    \\\"tag\\\": \"burucha\",\\n    \\\"popularity\\\": 6213\\n  },\\n  {\\n    \\\"tag\\\": \"premeditatingly\",\\n    \\\"popularity\\\": 6208\\n  },\\n  {\\n    \\\"tag\\\": \"cowrie\",\\n    \\\"popularity\\\": 6202\\n  },\\n  {\\n    \\\"tag\\\": \"pleurolysis\",\\n    \\\"popularity\\\": 6197\\n  },\\n  {\\n    \\\"tag\\\": \"nationalist\",\\n    \\\"popularity\\\": 6191\\n  },\\n  {\\n    \\\"tag\\\": \"Pholadacea\",\\n    \\\"popularity\\\": 6186\\n  },\\n  {\\n    \\\"tag\\\": \"anakrousis\",\\n    \\\"popularity\\\": 6180\\n  },\\n  {\\n    \\\"tag\\\": \"proctorial\",\\n    \\\"popularity\\\": 6175\\n  },\\n  {\\n    \\\"tag\\\": \"cavillation\",\\n    \\\"popularity\\\": 6169\\n  },\\n  {\\n    \\\"tag\\\": \"cervicobregmatic\",\\n    \\\"popularity\\\": 6163\\n  },\\n  {\\n    \\\"tag\\\": \"interspecific\",\\n    \\\"popularity\\\": 6158\\n  },\\n  {\\n    \\\"tag\\\": \"Teutonity\",\\n    \\\"popularity\\\": 6152\\n  },\\n  {\\n    \\\"tag\\\": \"snakeholing\",\\n    \\\"popularity\\\": 6147\\n  },\\n  {\\n    \\\"tag\\\": \"balcony\",\\n    \\\"popularity\\\": 6142\\n  },\\n  {\\n    \\\"tag\\\": \"latchless\",\\n    \\\"popularity\\\": 6136\\n  },\\n  {\\n    \\\"tag\\\": \"Mithraea\",\\n    \\\"popularity\\\": 6131\\n  },\\n  {\\n    \\\"tag\\\": \"pseudepigraph\",\\n    \\\"popularity\\\": 6125\\n  },\\n  {\\n    \\\"tag\\\": \"flosser\",\\n    \\\"popularity\\\": 6120\\n  },\\n  {\\n    \\\"tag\\\": \"kotyle\",\\n    \\\"popularity\\\": 6114\\n  },\\n  {\\n    \\\"tag\\\": \"outdo\",\\n    \\\"popularity\\\": 6109\\n  },\\n  {\\n    \\\"tag\\\": \"interclerical\",\\n    \\\"popularity\\\": 6103\\n  },\\n  {\\n    \\\"tag\\\": \"aurar\",\\n    \\\"popularity\\\": 6098\\n  },\\n  {\\n    \\\"tag\\\": \"apophyseal\",\\n    \\\"popularity\\\": 6093\\n  },\\n  {\\n    \\\"tag\\\": \"Miro\",\\n    \\\"popularity\\\": 6087\\n  },\\n  {\\n    \\\"tag\\\": \"Priscillian\",\\n    \\\"popularity\\\": 6082\\n  },\\n  {\\n    \\\"tag\\\": \"alluvia\",\\n    \\\"popularity\\\": 6076\\n  },\\n  {\\n    \\\"tag\\\": \"exordize\",\\n    \\\"popularity\\\": 6071\\n  },\\n  {\\n    \\\"tag\\\": \"breakage\",\\n    \\\"popularity\\\": 6066\\n  },\\n  {\\n    \\\"tag\\\": \"unclosable\",\\n    \\\"popularity\\\": 6060\\n  },\\n  {\\n    \\\"tag\\\": \"monocondylous\",\\n    \\\"popularity\\\": 6055\\n  },\\n  {\\n    \\\"tag\\\": \"dyarchy\",\\n    \\\"popularity\\\": 6050\\n  },\\n  {\\n    \\\"tag\\\": \"subchelate\",\\n    \\\"popularity\\\": 6044\\n  },\\n  {\\n    \\\"tag\\\": \"hearsay\",\\n    \\\"popularity\\\": 6039\\n  },\\n  {\\n    \\\"tag\\\": \"prestigiously\",\\n    \\\"popularity\\\": 6034\\n  },\\n  {\\n    \\\"tag\\\": \"unimuscular\",\\n    \\\"popularity\\\": 6028\\n  },\\n  {\\n    \\\"tag\\\": \"lingwort\",\\n    \\\"popularity\\\": 6023\\n  },\\n  {\\n    \\\"tag\\\": \"jealous\",\\n    \\\"popularity\\\": 6018\\n  },\\n  {\\n    \\\"tag\\\": \"artilleryman\",\\n    \\\"popularity\\\": 6012\\n  },\\n  {\\n    \\\"tag\\\": \"phantasmagorially\",\\n    \\\"popularity\\\": 6007\\n  },\\n  {\\n    \\\"tag\\\": \"stagnum\",\\n    \\\"popularity\\\": 6002\\n  },\\n  {\\n    \\\"tag\\\": \"organotropism shatteringly\",\\n    \\\"popularity\\\": 5997\\n  },\\n  {\\n    \\\"tag\\\": \"Mytilus Hebraist\",\\n    \\\"popularity\\\": 5991\\n  },\\n  {\\n    \\\"tag\\\": \"returf\",\\n    \\\"popularity\\\": 5986\\n  },\\n  {\\n    \\\"tag\\\": \"townfolk\",\\n    \\\"popularity\\\": 5981\\n  },\\n  {\\n    \\\"tag\\\": \"propitiative\",\\n    \\\"popularity\\\": 5976\\n  },\\n  {\\n    \\\"tag\\\": \"Anita unsullied\",\\n    \\\"popularity\\\": 5970\\n  },\\n  {\\n    \\\"tag\\\": \"bandoleered\",\\n    \\\"popularity\\\": 5965\\n  },\\n  {\\n    \\\"tag\\\": \"cubby\",\\n    \\\"popularity\\\": 5960\\n  },\\n  {\\n    \\\"tag\\\": \"Hexanchus\",\\n    \\\"popularity\\\": 5955\\n  },\\n  {\\n    \\\"tag\\\": \"circuminsular\",\\n    \\\"popularity\\\": 5949\\n  },\\n  {\\n    \\\"tag\\\": \"chamberletted eumycete\",\\n    \\\"popularity\\\": 5944\\n  },\\n  {\\n    \\\"tag\\\": \"secure\",\\n    \\\"popularity\\\": 5939\\n  },\\n  {\\n    \\\"tag\\\": \"Edwardean\",\\n    \\\"popularity\\\": 5934\\n  },\\n  {\\n    \\\"tag\\\": \"strenth\",\\n    \\\"popularity\\\": 5929\\n  },\\n  {\\n    \\\"tag\\\": \"exhaustless\",\\n    \\\"popularity\\\": 5923\\n  },\\n  {\\n    \\\"tag\\\": \"electioneerer\",\\n    \\\"popularity\\\": 5918\\n  },\\n  {\\n    \\\"tag\\\": \"estoile\",\\n    \\\"popularity\\\": 5913\\n  },\\n  {\\n    \\\"tag\\\": \"redden\",\\n    \\\"popularity\\\": 5908\\n  },\\n  {\\n    \\\"tag\\\": \"solicitee\",\\n    \\\"popularity\\\": 5903\\n  },\\n  {\\n    \\\"tag\\\": \"nonpatented\",\\n    \\\"popularity\\\": 5898\\n  },\\n  {\\n    \\\"tag\\\": \"lemming\",\\n    \\\"popularity\\\": 5893\\n  },\\n  {\\n    \\\"tag\\\": \"marled subalate\",\\n    \\\"popularity\\\": 5887\\n  },\\n  {\\n    \\\"tag\\\": \"premial horizonward\",\\n    \\\"popularity\\\": 5882\\n  },\\n  {\\n    \\\"tag\\\": \"nonrefueling\",\\n    \\\"popularity\\\": 5877\\n  },\\n  {\\n    \\\"tag\\\": \"rupturewort\",\\n    \\\"popularity\\\": 5872\\n  },\\n  {\\n    \\\"tag\\\": \"unfed\",\\n    \\\"popularity\\\": 5867\\n  },\\n  {\\n    \\\"tag\\\": \"empanelment\",\\n    \\\"popularity\\\": 5862\\n  },\\n  {\\n    \\\"tag\\\": \"isoosmosis\",\\n    \\\"popularity\\\": 5857\\n  },\\n  {\\n    \\\"tag\\\": \"jipijapa\",\\n    \\\"popularity\\\": 5852\\n  },\\n  {\\n    \\\"tag\\\": \"Fiji\",\\n    \\\"popularity\\\": 5847\\n  },\\n  {\\n    \\\"tag\\\": \"interferant\",\\n    \\\"popularity\\\": 5842\\n  },\\n  {\\n    \\\"tag\\\": \"reconstitution\",\\n    \\\"popularity\\\": 5837\\n  },\\n  {\\n    \\\"tag\\\": \"dockyardman\",\\n    \\\"popularity\\\": 5832\\n  },\\n  {\\n    \\\"tag\\\": \"dolichopodous\",\\n    \\\"popularity\\\": 5826\\n  },\\n  {\\n    \\\"tag\\\": \"whiteworm\",\\n    \\\"popularity\\\": 5821\\n  },\\n  {\\n    \\\"tag\\\": \"atheistically\",\\n    \\\"popularity\\\": 5816\\n  },\\n  {\\n    \\\"tag\\\": \"nonconcern\",\\n    \\\"popularity\\\": 5811\\n  },\\n  {\\n    \\\"tag\\\": \"scarabaeidoid\",\\n    \\\"popularity\\\": 5806\\n  },\\n  {\\n    \\\"tag\\\": \"triumviri\",\\n    \\\"popularity\\\": 5801\\n  },\\n  {\\n    \\\"tag\\\": \"rakit\",\\n    \\\"popularity\\\": 5796\\n  },\\n  {\\n    \\\"tag\\\": \"leecheater\",\\n    \\\"popularity\\\": 5791\\n  },\\n  {\\n    \\\"tag\\\": \"Arthrostraca\",\\n    \\\"popularity\\\": 5786\\n  },\\n  {\\n    \\\"tag\\\": \"upknit\",\\n    \\\"popularity\\\": 5781\\n  },\\n  {\\n    \\\"tag\\\": \"tymbalon\",\\n    \\\"popularity\\\": 5776\\n  },\\n  {\\n    \\\"tag\\\": \"inventurous\",\\n    \\\"popularity\\\": 5771\\n  },\\n  {\\n    \\\"tag\\\": \"perradiate\",\\n    \\\"popularity\\\": 5766\\n  },\\n  {\\n    \\\"tag\\\": \"seer\",\\n    \\\"popularity\\\": 5762\\n  },\\n  {\\n    \\\"tag\\\": \"Auricularia\",\\n    \\\"popularity\\\": 5757\\n  },\\n  {\\n    \\\"tag\\\": \"wettish exclusivity\",\\n    \\\"popularity\\\": 5752\\n  },\\n  {\\n    \\\"tag\\\": \"arteriosympathectomy\",\\n    \\\"popularity\\\": 5747\\n  },\\n  {\\n    \\\"tag\\\": \"tunlike\",\\n    \\\"popularity\\\": 5742\\n  },\\n  {\\n    \\\"tag\\\": \"cephalocercal\",\\n    \\\"popularity\\\": 5737\\n  },\\n  {\\n    \\\"tag\\\": \"meaninglessness\",\\n    \\\"popularity\\\": 5732\\n  },\\n  {\\n    \\\"tag\\\": \"fountful\",\\n    \\\"popularity\\\": 5727\\n  },\\n  {\\n    \\\"tag\\\": \"appraisement\",\\n    \\\"popularity\\\": 5722\\n  },\\n  {\\n    \\\"tag\\\": \"geniculated\",\\n    \\\"popularity\\\": 5717\\n  },\\n  {\\n    \\\"tag\\\": \"rotator\",\\n    \\\"popularity\\\": 5712\\n  },\\n  {\\n    \\\"tag\\\": \"foremarch biography\",\\n    \\\"popularity\\\": 5707\\n  },\\n  {\\n    \\\"tag\\\": \"arid\",\\n    \\\"popularity\\\": 5703\\n  },\\n  {\\n    \\\"tag\\\": \"inapprehensible\",\\n    \\\"popularity\\\": 5698\\n  },\\n  {\\n    \\\"tag\\\": \"chlorosulphonic\",\\n    \\\"popularity\\\": 5693\\n  },\\n  {\\n    \\\"tag\\\": \"braguette\",\\n    \\\"popularity\\\": 5688\\n  },\\n  {\\n    \\\"tag\\\": \"panophthalmitis\",\\n    \\\"popularity\\\": 5683\\n  },\\n  {\\n    \\\"tag\\\": \"pro objurgatorily\",\\n    \\\"popularity\\\": 5678\\n  },\\n  {\\n    \\\"tag\\\": \"zooplasty\",\\n    \\\"popularity\\\": 5673\\n  },\\n  {\\n    \\\"tag\\\": \"Terebratulidae\",\\n    \\\"popularity\\\": 5669\\n  },\\n  {\\n    \\\"tag\\\": \"Mahran\",\\n    \\\"popularity\\\": 5664\\n  },\\n  {\\n    \\\"tag\\\": \"anthologize merocele\",\\n    \\\"popularity\\\": 5659\\n  },\\n  {\\n    \\\"tag\\\": \"firecracker chiropractic\",\\n    \\\"popularity\\\": 5654\\n  },\\n  {\\n    \\\"tag\\\": \"tenorist\",\\n    \\\"popularity\\\": 5649\\n  },\\n  {\\n    \\\"tag\\\": \"amphitene\",\\n    \\\"popularity\\\": 5645\\n  },\\n  {\\n    \\\"tag\\\": \"silverbush toadstone\",\\n    \\\"popularity\\\": 5640\\n  },\\n  {\\n    \\\"tag\\\": \"entozoological\",\\n    \\\"popularity\\\": 5635\\n  },\\n  {\\n    \\\"tag\\\": \"trustlessness\",\\n    \\\"popularity\\\": 5630\\n  },\\n  {\\n    \\\"tag\\\": \"reassay\",\\n    \\\"popularity\\\": 5625\\n  },\\n  {\\n    \\\"tag\\\": \"chrysalides\",\\n    \\\"popularity\\\": 5621\\n  },\\n  {\\n    \\\"tag\\\": \"truncation\",\\n    \\\"popularity\\\": 5616\\n  },\\n  {\\n    \\\"tag\\\": \"unwavered mausoleal\",\\n    \\\"popularity\\\": 5611\\n  },\\n  {\\n    \\\"tag\\\": \"unserrated\",\\n    \\\"popularity\\\": 5606\\n  },\\n  {\\n    \\\"tag\\\": \"frampler\",\\n    \\\"popularity\\\": 5602\\n  },\\n  {\\n    \\\"tag\\\": \"celestial\",\\n    \\\"popularity\\\": 5597\\n  },\\n  {\\n    \\\"tag\\\": \"depreter\",\\n    \\\"popularity\\\": 5592\\n  },\\n  {\\n    \\\"tag\\\": \"retaliate\",\\n    \\\"popularity\\\": 5588\\n  },\\n  {\\n    \\\"tag\\\": \"decempunctate\",\\n    \\\"popularity\\\": 5583\\n  },\\n  {\\n    \\\"tag\\\": \"submitter\",\\n    \\\"popularity\\\": 5578\\n  },\\n  {\\n    \\\"tag\\\": \"phenothiazine\",\\n    \\\"popularity\\\": 5573\\n  },\\n  {\\n    \\\"tag\\\": \"hobbledehoyish\",\\n    \\\"popularity\\\": 5569\\n  },\\n  {\\n    \\\"tag\\\": \"erraticness\",\\n    \\\"popularity\\\": 5564\\n  },\\n  {\\n    \\\"tag\\\": \"ovariodysneuria\",\\n    \\\"popularity\\\": 5559\\n  },\\n  {\\n    \\\"tag\\\": \"puja\",\\n    \\\"popularity\\\": 5555\\n  },\\n  {\\n    \\\"tag\\\": \"cesspool\",\\n    \\\"popularity\\\": 5550\\n  },\\n  {\\n    \\\"tag\\\": \"sonation\",\\n    \\\"popularity\\\": 5545\\n  },\\n  {\\n    \\\"tag\\\": \"moggan\",\\n    \\\"popularity\\\": 5541\\n  },\\n  {\\n    \\\"tag\\\": \"overjutting\",\\n    \\\"popularity\\\": 5536\\n  },\\n  {\\n    \\\"tag\\\": \"cohobate\",\\n    \\\"popularity\\\": 5531\\n  },\\n  {\\n    \\\"tag\\\": \"Distoma\",\\n    \\\"popularity\\\": 5527\\n  },\\n  {\\n    \\\"tag\\\": \"Plectognathi\",\\n    \\\"popularity\\\": 5522\\n  },\\n  {\\n    \\\"tag\\\": \"dumple caliphate\",\\n    \\\"popularity\\\": 5517\\n  },\\n  {\\n    \\\"tag\\\": \"shiko\",\\n    \\\"popularity\\\": 5513\\n  },\\n  {\\n    \\\"tag\\\": \"downness\",\\n    \\\"popularity\\\": 5508\\n  },\\n  {\\n    \\\"tag\\\": \"whippletree\",\\n    \\\"popularity\\\": 5504\\n  },\\n  {\\n    \\\"tag\\\": \"nymphaeum\",\\n    \\\"popularity\\\": 5499\\n  },\\n  {\\n    \\\"tag\\\": \"there trest\",\\n    \\\"popularity\\\": 5494\\n  },\\n  {\\n    \\\"tag\\\": \"psychrometer\",\\n    \\\"popularity\\\": 5490\\n  },\\n  {\\n    \\\"tag\\\": \"pyelograph\",\\n    \\\"popularity\\\": 5485\\n  },\\n  {\\n    \\\"tag\\\": \"unsalvable\",\\n    \\\"popularity\\\": 5481\\n  },\\n  {\\n    \\\"tag\\\": \"bescreen\",\\n    \\\"popularity\\\": 5476\\n  },\\n  {\\n    \\\"tag\\\": \"cushy\",\\n    \\\"popularity\\\": 5471\\n  },\\n  {\\n    \\\"tag\\\": \"plicatolobate\",\\n    \\\"popularity\\\": 5467\\n  },\\n  {\\n    \\\"tag\\\": \"lakie\",\\n    \\\"popularity\\\": 5462\\n  },\\n  {\\n    \\\"tag\\\": \"anthropodeoxycholic\",\\n    \\\"popularity\\\": 5458\\n  },\\n  {\\n    \\\"tag\\\": \"resatisfaction\",\\n    \\\"popularity\\\": 5453\\n  },\\n  {\\n    \\\"tag\\\": \"unravelment unaccidental\",\\n    \\\"popularity\\\": 5449\\n  },\\n  {\\n    \\\"tag\\\": \"telewriter monogeneous\",\\n    \\\"popularity\\\": 5444\\n  },\\n  {\\n    \\\"tag\\\": \"unsabred\",\\n    \\\"popularity\\\": 5440\\n  },\\n  {\\n    \\\"tag\\\": \"startlingly\",\\n    \\\"popularity\\\": 5435\\n  },\\n  {\\n    \\\"tag\\\": \"Aralia\",\\n    \\\"popularity\\\": 5431\\n  },\\n  {\\n    \\\"tag\\\": \"alamonti\",\\n    \\\"popularity\\\": 5426\\n  },\\n  {\\n    \\\"tag\\\": \"Franklinization\",\\n    \\\"popularity\\\": 5422\\n  },\\n  {\\n    \\\"tag\\\": \"parliament\",\\n    \\\"popularity\\\": 5417\\n  },\\n  {\\n    \\\"tag\\\": \"schoolkeeper\",\\n    \\\"popularity\\\": 5413\\n  },\\n  {\\n    \\\"tag\\\": \"nonsociety\",\\n    \\\"popularity\\\": 5408\\n  },\\n  {\\n    \\\"tag\\\": \"parenthetic\",\\n    \\\"popularity\\\": 5404\\n  },\\n  {\\n    \\\"tag\\\": \"stog\",\\n    \\\"popularity\\\": 5399\\n  },\\n  {\\n    \\\"tag\\\": \"Pristipomidae\",\\n    \\\"popularity\\\": 5395\\n  },\\n  {\\n    \\\"tag\\\": \"exocarp\",\\n    \\\"popularity\\\": 5390\\n  },\\n  {\\n    \\\"tag\\\": \"monaxonial\",\\n    \\\"popularity\\\": 5386\\n  },\\n  {\\n    \\\"tag\\\": \"tramroad\",\\n    \\\"popularity\\\": 5381\\n  },\\n  {\\n    \\\"tag\\\": \"hookah\",\\n    \\\"popularity\\\": 5377\\n  },\\n  {\\n    \\\"tag\\\": \"saccharonic\",\\n    \\\"popularity\\\": 5372\\n  },\\n  {\\n    \\\"tag\\\": \"perimetrium\",\\n    \\\"popularity\\\": 5368\\n  },\\n  {\\n    \\\"tag\\\": \"libelluloid\",\\n    \\\"popularity\\\": 5364\\n  },\\n  {\\n    \\\"tag\\\": \"overrunningly\",\\n    \\\"popularity\\\": 5359\\n  },\\n  {\\n    \\\"tag\\\": \"untwister\",\\n    \\\"popularity\\\": 5355\\n  },\\n  {\\n    \\\"tag\\\": \"ninnyhammer\",\\n    \\\"popularity\\\": 5350\\n  },\\n  {\\n    \\\"tag\\\": \"metranate\",\\n    \\\"popularity\\\": 5346\\n  },\\n  {\\n    \\\"tag\\\": \"sarcoblast\",\\n    \\\"popularity\\\": 5341\\n  },\\n  {\\n    \\\"tag\\\": \"porkish\",\\n    \\\"popularity\\\": 5337\\n  },\\n  {\\n    \\\"tag\\\": \"chauvinistic\",\\n    \\\"popularity\\\": 5333\\n  },\\n  {\\n    \\\"tag\\\": \"sexagesimal\",\\n    \\\"popularity\\\": 5328\\n  },\\n  {\\n    \\\"tag\\\": \"hematogenic\",\\n    \\\"popularity\\\": 5324\\n  },\\n  {\\n    \\\"tag\\\": \"selfpreservatory\",\\n    \\\"popularity\\\": 5320\\n  },\\n  {\\n    \\\"tag\\\": \"myelauxe\",\\n    \\\"popularity\\\": 5315\\n  },\\n  {\\n    \\\"tag\\\": \"triply\",\\n    \\\"popularity\\\": 5311\\n  },\\n  {\\n    \\\"tag\\\": \"metaphysicous\",\\n    \\\"popularity\\\": 5306\\n  },\\n  {\\n    \\\"tag\\\": \"vitrinoid\",\\n    \\\"popularity\\\": 5302\\n  },\\n  {\\n    \\\"tag\\\": \"glabellae\",\\n    \\\"popularity\\\": 5298\\n  },\\n  {\\n    \\\"tag\\\": \"moonlighter\",\\n    \\\"popularity\\\": 5293\\n  },\\n  {\\n    \\\"tag\\\": \"monotheistically epexegetical\",\\n    \\\"popularity\\\": 5289\\n  },\\n  {\\n    \\\"tag\\\": \"pseudolateral\",\\n    \\\"popularity\\\": 5285\\n  },\\n  {\\n    \\\"tag\\\": \"heptamethylene\",\\n    \\\"popularity\\\": 5280\\n  },\\n  {\\n    \\\"tag\\\": \"salvadora\",\\n    \\\"popularity\\\": 5276\\n  },\\n  {\\n    \\\"tag\\\": \"unjovial diphenylthiourea\",\\n    \\\"popularity\\\": 5272\\n  },\\n  {\\n    \\\"tag\\\": \"thievishness\",\\n    \\\"popularity\\\": 5268\\n  },\\n  {\\n    \\\"tag\\\": \"unridable\",\\n    \\\"popularity\\\": 5263\\n  },\\n  {\\n    \\\"tag\\\": \"underhandedly\",\\n    \\\"popularity\\\": 5259\\n  },\\n  {\\n    \\\"tag\\\": \"fungiform\",\\n    \\\"popularity\\\": 5255\\n  },\\n  {\\n    \\\"tag\\\": \"scruffle\",\\n    \\\"popularity\\\": 5250\\n  },\\n  {\\n    \\\"tag\\\": \"preindisposition\",\\n    \\\"popularity\\\": 5246\\n  },\\n  {\\n    \\\"tag\\\": \"Amadis\",\\n    \\\"popularity\\\": 5242\\n  },\\n  {\\n    \\\"tag\\\": \"Culex\",\\n    \\\"popularity\\\": 5238\\n  },\\n  {\\n    \\\"tag\\\": \"churning\",\\n    \\\"popularity\\\": 5233\\n  },\\n  {\\n    \\\"tag\\\": \"imperite\",\\n    \\\"popularity\\\": 5229\\n  },\\n  {\\n    \\\"tag\\\": \"levorotation\",\\n    \\\"popularity\\\": 5225\\n  },\\n  {\\n    \\\"tag\\\": \"barbate\",\\n    \\\"popularity\\\": 5221\\n  },\\n  {\\n    \\\"tag\\\": \"knotwort\",\\n    \\\"popularity\\\": 5216\\n  },\\n  {\\n    \\\"tag\\\": \"gypsiferous\",\\n    \\\"popularity\\\": 5212\\n  },\\n  {\\n    \\\"tag\\\": \"tourmalinic\",\\n    \\\"popularity\\\": 5208\\n  },\\n  {\\n    \\\"tag\\\": \"helleboric\",\\n    \\\"popularity\\\": 5204\\n  },\\n  {\\n    \\\"tag\\\": \"pneumograph\",\\n    \\\"popularity\\\": 5199\\n  },\\n  {\\n    \\\"tag\\\": \"Peltigeraceae\",\\n    \\\"popularity\\\": 5195\\n  },\\n  {\\n    \\\"tag\\\": \"busine\",\\n    \\\"popularity\\\": 5191\\n  },\\n  {\\n    \\\"tag\\\": \"Ailuridae\",\\n    \\\"popularity\\\": 5187\\n  },\\n  {\\n    \\\"tag\\\": \"azotate\",\\n    \\\"popularity\\\": 5183\\n  },\\n  {\\n    \\\"tag\\\": \"unlikable\",\\n    \\\"popularity\\\": 5178\\n  },\\n  {\\n    \\\"tag\\\": \"sloyd\",\\n    \\\"popularity\\\": 5174\\n  },\\n  {\\n    \\\"tag\\\": \"biblioclasm\",\\n    \\\"popularity\\\": 5170\\n  },\\n  {\\n    \\\"tag\\\": \"Seres\",\\n    \\\"popularity\\\": 5166\\n  },\\n  {\\n    \\\"tag\\\": \"unaccurateness\",\\n    \\\"popularity\\\": 5162\\n  },\\n  {\\n    \\\"tag\\\": \"scrollwise\",\\n    \\\"popularity\\\": 5157\\n  },\\n  {\\n    \\\"tag\\\": \"flandowser\",\\n    \\\"popularity\\\": 5153\\n  },\\n  {\\n    \\\"tag\\\": \"unblackened\",\\n    \\\"popularity\\\": 5149\\n  },\\n  {\\n    \\\"tag\\\": \"schistosternia\",\\n    \\\"popularity\\\": 5145\\n  },\\n  {\\n    \\\"tag\\\": \"fuse\",\\n    \\\"popularity\\\": 5141\\n  },\\n  {\\n    \\\"tag\\\": \"narthecal\",\\n    \\\"popularity\\\": 5137\\n  },\\n  {\\n    \\\"tag\\\": \"Cueva\",\\n    \\\"popularity\\\": 5133\\n  },\\n  {\\n    \\\"tag\\\": \"appositeness\",\\n    \\\"popularity\\\": 5128\\n  },\\n  {\\n    \\\"tag\\\": \"proindustrial\",\\n    \\\"popularity\\\": 5124\\n  },\\n  {\\n    \\\"tag\\\": \"dermatorrhoea\",\\n    \\\"popularity\\\": 5120\\n  },\\n  {\\n    \\\"tag\\\": \"oxyurous tendential\",\\n    \\\"popularity\\\": 5116\\n  },\\n  {\\n    \\\"tag\\\": \"isopurpurin\",\\n    \\\"popularity\\\": 5112\\n  },\\n  {\\n    \\\"tag\\\": \"impose\",\\n    \\\"popularity\\\": 5108\\n  },\\n  {\\n    \\\"tag\\\": \"wordsmanship\",\\n    \\\"popularity\\\": 5104\\n  },\\n  {\\n    \\\"tag\\\": \"saturator\",\\n    \\\"popularity\\\": 5100\\n  },\\n  {\\n    \\\"tag\\\": \"Nordicity\",\\n    \\\"popularity\\\": 5096\\n  },\\n  {\\n    \\\"tag\\\": \"interaccuse\",\\n    \\\"popularity\\\": 5092\\n  },\\n  {\\n    \\\"tag\\\": \"acridinic\",\\n    \\\"popularity\\\": 5087\\n  },\\n  {\\n    \\\"tag\\\": \"scholion\",\\n    \\\"popularity\\\": 5083\\n  },\\n  {\\n    \\\"tag\\\": \"pseudoaconitine\",\\n    \\\"popularity\\\": 5079\\n  },\\n  {\\n    \\\"tag\\\": \"doctorial\",\\n    \\\"popularity\\\": 5075\\n  },\\n  {\\n    \\\"tag\\\": \"Etchimin\",\\n    \\\"popularity\\\": 5071\\n  },\\n  {\\n    \\\"tag\\\": \"oliviform\",\\n    \\\"popularity\\\": 5067\\n  },\\n  {\\n    \\\"tag\\\": \"Pele\",\\n    \\\"popularity\\\": 5063\\n  },\\n  {\\n    \\\"tag\\\": \"Chiromantis Progymnasium\",\\n    \\\"popularity\\\": 5059\\n  },\\n  {\\n    \\\"tag\\\": \"toxosis\",\\n    \\\"popularity\\\": 5055\\n  },\\n  {\\n    \\\"tag\\\": \"spadilla\",\\n    \\\"popularity\\\": 5051\\n  },\\n  {\\n    \\\"tag\\\": \"Actinopterygii\",\\n    \\\"popularity\\\": 5047\\n  },\\n  {\\n    \\\"tag\\\": \"untiring\",\\n    \\\"popularity\\\": 5043\\n  },\\n  {\\n    \\\"tag\\\": \"butyral\",\\n    \\\"popularity\\\": 5039\\n  },\\n  {\\n    \\\"tag\\\": \"Gymnoderinae\",\\n    \\\"popularity\\\": 5035\\n  },\\n  {\\n    \\\"tag\\\": \"testudo\",\\n    \\\"popularity\\\": 5031\\n  },\\n  {\\n    \\\"tag\\\": \"frigorify\",\\n    \\\"popularity\\\": 5027\\n  },\\n  {\\n    \\\"tag\\\": \"aliency\",\\n    \\\"popularity\\\": 5023\\n  },\\n  {\\n    \\\"tag\\\": \"jargon\",\\n    \\\"popularity\\\": 5019\\n  },\\n  {\\n    \\\"tag\\\": \"counterservice\",\\n    \\\"popularity\\\": 5015\\n  },\\n  {\\n    \\\"tag\\\": \"isostrychnine\",\\n    \\\"popularity\\\": 5011\\n  },\\n  {\\n    \\\"tag\\\": \"tellership\",\\n    \\\"popularity\\\": 5007\\n  },\\n  {\\n    \\\"tag\\\": \"miscegenetic\",\\n    \\\"popularity\\\": 5003\\n  },\\n  {\\n    \\\"tag\\\": \"sorcer\",\\n    \\\"popularity\\\": 4999\\n  },\\n  {\\n    \\\"tag\\\": \"tilewright\",\\n    \\\"popularity\\\": 4995\\n  },\\n  {\\n    \\\"tag\\\": \"cyanoplastid\",\\n    \\\"popularity\\\": 4991\\n  },\\n  {\\n    \\\"tag\\\": \"fluxionally\",\\n    \\\"popularity\\\": 4987\\n  },\\n  {\\n    \\\"tag\\\": \"proudhearted\",\\n    \\\"popularity\\\": 4983\\n  },\\n  {\\n    \\\"tag\\\": \"blithely\",\\n    \\\"popularity\\\": 4979\\n  },\\n  {\\n    \\\"tag\\\": \"jestproof\",\\n    \\\"popularity\\\": 4975\\n  },\\n  {\\n    \\\"tag\\\": \"jestwise\",\\n    \\\"popularity\\\": 4971\\n  },\\n  {\\n    \\\"tag\\\": \"nonassimilable\",\\n    \\\"popularity\\\": 4967\\n  },\\n  {\\n    \\\"tag\\\": \"compurgation\",\\n    \\\"popularity\\\": 4964\\n  },\\n  {\\n    \\\"tag\\\": \"unhate\",\\n    \\\"popularity\\\": 4960\\n  },\\n  {\\n    \\\"tag\\\": \"haplodonty\",\\n    \\\"popularity\\\": 4956\\n  },\\n  {\\n    \\\"tag\\\": \"cardholder\",\\n    \\\"popularity\\\": 4952\\n  },\\n  {\\n    \\\"tag\\\": \"rainlight megohmmeter overstout\",\\n    \\\"popularity\\\": 4948\\n  },\\n  {\\n    \\\"tag\\\": \"itchless\",\\n    \\\"popularity\\\": 4944\\n  },\\n  {\\n    \\\"tag\\\": \"begiggle\",\\n    \\\"popularity\\\": 4940\\n  },\\n  {\\n    \\\"tag\\\": \"chromatosphere\",\\n    \\\"popularity\\\": 4936\\n  },\\n  {\\n    \\\"tag\\\": \"typicality\",\\n    \\\"popularity\\\": 4932\\n  },\\n  {\\n    \\\"tag\\\": \"overgrown\",\\n    \\\"popularity\\\": 4928\\n  },\\n  {\\n    \\\"tag\\\": \"envolume\",\\n    \\\"popularity\\\": 4925\\n  },\\n  {\\n    \\\"tag\\\": \"pachycholia\",\\n    \\\"popularity\\\": 4921\\n  },\\n  {\\n    \\\"tag\\\": \"passageable\",\\n    \\\"popularity\\\": 4917\\n  },\\n  {\\n    \\\"tag\\\": \"pathopoiesis\",\\n    \\\"popularity\\\": 4913\\n  },\\n  {\\n    \\\"tag\\\": \"overbreak\",\\n    \\\"popularity\\\": 4909\\n  },\\n  {\\n    \\\"tag\\\": \"satyric\",\\n    \\\"popularity\\\": 4905\\n  },\\n  {\\n    \\\"tag\\\": \"unaudited\",\\n    \\\"popularity\\\": 4901\\n  },\\n  {\\n    \\\"tag\\\": \"whimble\",\\n    \\\"popularity\\\": 4898\\n  },\\n  {\\n    \\\"tag\\\": \"pressureless\",\\n    \\\"popularity\\\": 4894\\n  },\\n  {\\n    \\\"tag\\\": \"Selene\",\\n    \\\"popularity\\\": 4890\\n  },\\n  {\\n    \\\"tag\\\": \"slithery\",\\n    \\\"popularity\\\": 4886\\n  },\\n  {\\n    \\\"tag\\\": \"nondisfigurement\",\\n    \\\"popularity\\\": 4882\\n  },\\n  {\\n    \\\"tag\\\": \"overdelicious\",\\n    \\\"popularity\\\": 4878\\n  },\\n  {\\n    \\\"tag\\\": \"Perca\",\\n    \\\"popularity\\\": 4875\\n  },\\n  {\\n    \\\"tag\\\": \"Palladium\",\\n    \\\"popularity\\\": 4871\\n  },\\n  {\\n    \\\"tag\\\": \"insagacity\",\\n    \\\"popularity\\\": 4867\\n  },\\n  {\\n    \\\"tag\\\": \"peristoma\",\\n    \\\"popularity\\\": 4863\\n  },\\n  {\\n    \\\"tag\\\": \"uncreativeness\",\\n    \\\"popularity\\\": 4859\\n  },\\n  {\\n    \\\"tag\\\": \"incomparability surfboarding\",\\n    \\\"popularity\\\": 4856\\n  },\\n  {\\n    \\\"tag\\\": \"bacillar\",\\n    \\\"popularity\\\": 4852\\n  },\\n  {\\n    \\\"tag\\\": \"ulcerative\",\\n    \\\"popularity\\\": 4848\\n  },\\n  {\\n    \\\"tag\\\": \"stychomythia\",\\n    \\\"popularity\\\": 4844\\n  },\\n  {\\n    \\\"tag\\\": \"sesma somatics nonentry\",\\n    \\\"popularity\\\": 4840\\n  },\\n  {\\n    \\\"tag\\\": \"unsepulchred\",\\n    \\\"popularity\\\": 4837\\n  },\\n  {\\n    \\\"tag\\\": \"cephalanthium\",\\n    \\\"popularity\\\": 4833\\n  },\\n  {\\n    \\\"tag\\\": \"Asiaticization\",\\n    \\\"popularity\\\": 4829\\n  },\\n  {\\n    \\\"tag\\\": \"killeen\",\\n    \\\"popularity\\\": 4825\\n  },\\n  {\\n    \\\"tag\\\": \"Pseudococcus\",\\n    \\\"popularity\\\": 4822\\n  },\\n  {\\n    \\\"tag\\\": \"untractable\",\\n    \\\"popularity\\\": 4818\\n  },\\n  {\\n    \\\"tag\\\": \"apolegamic\",\\n    \\\"popularity\\\": 4814\\n  },\\n  {\\n    \\\"tag\\\": \"hyperpnea\",\\n    \\\"popularity\\\": 4810\\n  },\\n  {\\n    \\\"tag\\\": \"martyrolatry\",\\n    \\\"popularity\\\": 4807\\n  },\\n  {\\n    \\\"tag\\\": \"Sarmatic\",\\n    \\\"popularity\\\": 4803\\n  },\\n  {\\n    \\\"tag\\\": \"nonsurface\",\\n    \\\"popularity\\\": 4799\\n  },\\n  {\\n    \\\"tag\\\": \"adjoined\",\\n    \\\"popularity\\\": 4796\\n  },\\n  {\\n    \\\"tag\\\": \"vasiform\",\\n    \\\"popularity\\\": 4792\\n  },\\n  {\\n    \\\"tag\\\": \"tastelessness\",\\n    \\\"popularity\\\": 4788\\n  },\\n  {\\n    \\\"tag\\\": \"rumbo\",\\n    \\\"popularity\\\": 4784\\n  },\\n  {\\n    \\\"tag\\\": \"subdititious\",\\n    \\\"popularity\\\": 4781\\n  },\\n  {\\n    \\\"tag\\\": \"reparticipation\",\\n    \\\"popularity\\\": 4777\\n  },\\n  {\\n    \\\"tag\\\": \"Yorkshireism\",\\n    \\\"popularity\\\": 4773\\n  },\\n  {\\n    \\\"tag\\\": \"outcrow\",\\n    \\\"popularity\\\": 4770\\n  },\\n  {\\n    \\\"tag\\\": \"casserole\",\\n    \\\"popularity\\\": 4766\\n  },\\n  {\\n    \\\"tag\\\": \"semideltaic\",\\n    \\\"popularity\\\": 4762\\n  },\\n  {\\n    \\\"tag\\\": \"freemason\",\\n    \\\"popularity\\\": 4759\\n  },\\n  {\\n    \\\"tag\\\": \"catkin\",\\n    \\\"popularity\\\": 4755\\n  },\\n  {\\n    \\\"tag\\\": \"conscient\",\\n    \\\"popularity\\\": 4751\\n  },\\n  {\\n    \\\"tag\\\": \"reliably\",\\n    \\\"popularity\\\": 4748\\n  },\\n  {\\n    \\\"tag\\\": \"Telembi\",\\n    \\\"popularity\\\": 4744\\n  },\\n  {\\n    \\\"tag\\\": \"hide\",\\n    \\\"popularity\\\": 4740\\n  },\\n  {\\n    \\\"tag\\\": \"social\",\\n    \\\"popularity\\\": 4737\\n  },\\n  {\\n    \\\"tag\\\": \"ichneutic\",\\n    \\\"popularity\\\": 4733\\n  },\\n  {\\n    \\\"tag\\\": \"polypotome blouse pentagrammatic\",\\n    \\\"popularity\\\": 4729\\n  },\\n  {\\n    \\\"tag\\\": \"airdrome pesthole\",\\n    \\\"popularity\\\": 4726\\n  },\\n  {\\n    \\\"tag\\\": \"unportended\",\\n    \\\"popularity\\\": 4722\\n  },\\n  {\\n    \\\"tag\\\": \"sheerly\",\\n    \\\"popularity\\\": 4719\\n  },\\n  {\\n    \\\"tag\\\": \"acardiac\",\\n    \\\"popularity\\\": 4715\\n  },\\n  {\\n    \\\"tag\\\": \"fetor\",\\n    \\\"popularity\\\": 4711\\n  },\\n  {\\n    \\\"tag\\\": \"storax\",\\n    \\\"popularity\\\": 4708\\n  },\\n  {\\n    \\\"tag\\\": \"syndactylic\",\\n    \\\"popularity\\\": 4704\\n  },\\n  {\\n    \\\"tag\\\": \"otiatrics\",\\n    \\\"popularity\\\": 4700\\n  },\\n  {\\n    \\\"tag\\\": \"range\",\\n    \\\"popularity\\\": 4697\\n  },\\n  {\\n    \\\"tag\\\": \"branchway\",\\n    \\\"popularity\\\": 4693\\n  },\\n  {\\n    \\\"tag\\\": \"beatific\",\\n    \\\"popularity\\\": 4690\\n  },\\n  {\\n    \\\"tag\\\": \"Rugosa\",\\n    \\\"popularity\\\": 4686\\n  },\\n  {\\n    \\\"tag\\\": \"rafty\",\\n    \\\"popularity\\\": 4682\\n  },\\n  {\\n    \\\"tag\\\": \"gapy\",\\n    \\\"popularity\\\": 4679\\n  },\\n  {\\n    \\\"tag\\\": \"heterocercal\",\\n    \\\"popularity\\\": 4675\\n  },\\n  {\\n    \\\"tag\\\": \"actinopterygious\",\\n    \\\"popularity\\\": 4672\\n  },\\n  {\\n    \\\"tag\\\": \"glauconite\",\\n    \\\"popularity\\\": 4668\\n  },\\n  {\\n    \\\"tag\\\": \"limbless priest\",\\n    \\\"popularity\\\": 4665\\n  },\\n  {\\n    \\\"tag\\\": \"chrysene\",\\n    \\\"popularity\\\": 4661\\n  },\\n  {\\n    \\\"tag\\\": \"isentropic\",\\n    \\\"popularity\\\": 4658\\n  },\\n  {\\n    \\\"tag\\\": \"lairdess\",\\n    \\\"popularity\\\": 4654\\n  },\\n  {\\n    \\\"tag\\\": \"butterhead choliambic\",\\n    \\\"popularity\\\": 4650\\n  },\\n  {\\n    \\\"tag\\\": \"hexaseme\",\\n    \\\"popularity\\\": 4647\\n  },\\n  {\\n    \\\"tag\\\": \"treeify\",\\n    \\\"popularity\\\": 4643\\n  },\\n  {\\n    \\\"tag\\\": \"coronetted fructify\",\\n    \\\"popularity\\\": 4640\\n  },\\n  {\\n    \\\"tag\\\": \"admiralty\",\\n    \\\"popularity\\\": 4636\\n  },\\n  {\\n    \\\"tag\\\": \"Flosculariidae\",\\n    \\\"popularity\\\": 4633\\n  },\\n  {\\n    \\\"tag\\\": \"limaceous\",\\n    \\\"popularity\\\": 4629\\n  },\\n  {\\n    \\\"tag\\\": \"subterconscious\",\\n    \\\"popularity\\\": 4626\\n  },\\n  {\\n    \\\"tag\\\": \"stayless\",\\n    \\\"popularity\\\": 4622\\n  },\\n  {\\n    \\\"tag\\\": \"psha\",\\n    \\\"popularity\\\": 4619\\n  },\\n  {\\n    \\\"tag\\\": \"Mediterraneanize\",\\n    \\\"popularity\\\": 4615\\n  },\\n  {\\n    \\\"tag\\\": \"impenetrably\",\\n    \\\"popularity\\\": 4612\\n  },\\n  {\\n    \\\"tag\\\": \"Myrmeleonidae\",\\n    \\\"popularity\\\": 4608\\n  },\\n  {\\n    \\\"tag\\\": \"germander\",\\n    \\\"popularity\\\": 4605\\n  },\\n  {\\n    \\\"tag\\\": \"Buri\",\\n    \\\"popularity\\\": 4601\\n  },\\n  {\\n    \\\"tag\\\": \"papyrotamia\",\\n    \\\"popularity\\\": 4598\\n  },\\n  {\\n    \\\"tag\\\": \"Toxylon\",\\n    \\\"popularity\\\": 4594\\n  },\\n  {\\n    \\\"tag\\\": \"batatilla\",\\n    \\\"popularity\\\": 4591\\n  },\\n  {\\n    \\\"tag\\\": \"fabella assumer\",\\n    \\\"popularity\\\": 4587\\n  },\\n  {\\n    \\\"tag\\\": \"macromethod\",\\n    \\\"popularity\\\": 4584\\n  },\\n  {\\n    \\\"tag\\\": \"Blechnum\",\\n    \\\"popularity\\\": 4580\\n  },\\n  {\\n    \\\"tag\\\": \"pantography\",\\n    \\\"popularity\\\": 4577\\n  },\\n  {\\n    \\\"tag\\\": \"seminovel\",\\n    \\\"popularity\\\": 4574\\n  },\\n  {\\n    \\\"tag\\\": \"disembarrassment\",\\n    \\\"popularity\\\": 4570\\n  },\\n  {\\n    \\\"tag\\\": \"bushmaking\",\\n    \\\"popularity\\\": 4567\\n  },\\n  {\\n    \\\"tag\\\": \"neurosis\",\\n    \\\"popularity\\\": 4563\\n  },\\n  {\\n    \\\"tag\\\": \"Animalia\",\\n    \\\"popularity\\\": 4560\\n  },\\n  {\\n    \\\"tag\\\": \"Bernice\",\\n    \\\"popularity\\\": 4556\\n  },\\n  {\\n    \\\"tag\\\": \"wisen\",\\n    \\\"popularity\\\": 4553\\n  },\\n  {\\n    \\\"tag\\\": \"subhymenium\",\\n    \\\"popularity\\\": 4549\\n  },\\n  {\\n    \\\"tag\\\": \"esophagomycosis\",\\n    \\\"popularity\\\": 4546\\n  },\\n  {\\n    \\\"tag\\\": \"wireworks\",\\n    \\\"popularity\\\": 4543\\n  },\\n  {\\n    \\\"tag\\\": \"Sabellidae\",\\n    \\\"popularity\\\": 4539\\n  },\\n  {\\n    \\\"tag\\\": \"fustianish\",\\n    \\\"popularity\\\": 4536\\n  },\\n  {\\n    \\\"tag\\\": \"professively\",\\n    \\\"popularity\\\": 4532\\n  },\\n  {\\n    \\\"tag\\\": \"overcorruptly\",\\n    \\\"popularity\\\": 4529\\n  },\\n  {\\n    \\\"tag\\\": \"overcreep\",\\n    \\\"popularity\\\": 4526\\n  },\\n  {\\n    \\\"tag\\\": \"Castilloa\",\\n    \\\"popularity\\\": 4522\\n  },\\n  {\\n    \\\"tag\\\": \"forelady Georgie\",\\n    \\\"popularity\\\": 4519\\n  },\\n  {\\n    \\\"tag\\\": \"outsider\",\\n    \\\"popularity\\\": 4515\\n  },\\n  {\\n    \\\"tag\\\": \"Enukki\",\\n    \\\"popularity\\\": 4512\\n  },\\n  {\\n    \\\"tag\\\": \"gypsy\",\\n    \\\"popularity\\\": 4509\\n  },\\n  {\\n    \\\"tag\\\": \"Passamaquoddy\",\\n    \\\"popularity\\\": 4505\\n  },\\n  {\\n    \\\"tag\\\": \"reposit\",\\n    \\\"popularity\\\": 4502\\n  },\\n  {\\n    \\\"tag\\\": \"overtenderness\",\\n    \\\"popularity\\\": 4499\\n  },\\n  {\\n    \\\"tag\\\": \"keratome\",\\n    \\\"popularity\\\": 4495\\n  },\\n  {\\n    \\\"tag\\\": \"interclavicular hypermonosyllable Susanna\",\\n    \\\"popularity\\\": 4492\\n  },\\n  {\\n    \\\"tag\\\": \"mispropose\",\\n    \\\"popularity\\\": 4489\\n  },\\n  {\\n    \\\"tag\\\": \"Membranipora\",\\n    \\\"popularity\\\": 4485\\n  },\\n  {\\n    \\\"tag\\\": \"lampad\",\\n    \\\"popularity\\\": 4482\\n  },\\n  {\\n    \\\"tag\\\": \"header\",\\n    \\\"popularity\\\": 4479\\n  },\\n  {\\n    \\\"tag\\\": \"triseriate\",\\n    \\\"popularity\\\": 4475\\n  },\\n  {\\n    \\\"tag\\\": \"distrainment\",\\n    \\\"popularity\\\": 4472\\n  },\\n  {\\n    \\\"tag\\\": \"staphyloplastic\",\\n    \\\"popularity\\\": 4469\\n  },\\n  {\\n    \\\"tag\\\": \"outscour\",\\n    \\\"popularity\\\": 4465\\n  },\\n  {\\n    \\\"tag\\\": \"tallowmaking\",\\n    \\\"popularity\\\": 4462\\n  },\\n  {\\n    \\\"tag\\\": \"plugger\",\\n    \\\"popularity\\\": 4459\\n  },\\n  {\\n    \\\"tag\\\": \"fashionize\",\\n    \\\"popularity\\\": 4455\\n  },\\n  {\\n    \\\"tag\\\": \"puzzle\",\\n    \\\"popularity\\\": 4452\\n  },\\n  {\\n    \\\"tag\\\": \"imbrue\",\\n    \\\"popularity\\\": 4449\\n  },\\n  {\\n    \\\"tag\\\": \"osteoblast\",\\n    \\\"popularity\\\": 4445\\n  },\\n  {\\n    \\\"tag\\\": \"Hydrocores\",\\n    \\\"popularity\\\": 4442\\n  },\\n  {\\n    \\\"tag\\\": \"Lutra\",\\n    \\\"popularity\\\": 4439\\n  },\\n  {\\n    \\\"tag\\\": \"upridge scarfy\",\\n    \\\"popularity\\\": 4435\\n  },\\n  {\\n    \\\"tag\\\": \"ancon taffle\",\\n    \\\"popularity\\\": 4432\\n  },\\n  {\\n    \\\"tag\\\": \"impest\",\\n    \\\"popularity\\\": 4429\\n  },\\n  {\\n    \\\"tag\\\": \"uncollatedness\",\\n    \\\"popularity\\\": 4426\\n  },\\n  {\\n    \\\"tag\\\": \"hypersensitize\",\\n    \\\"popularity\\\": 4422\\n  },\\n  {\\n    \\\"tag\\\": \"autographically\",\\n    \\\"popularity\\\": 4419\\n  },\\n  {\\n    \\\"tag\\\": \"louther\",\\n    \\\"popularity\\\": 4416\\n  },\\n  {\\n    \\\"tag\\\": \"Ollie\",\\n    \\\"popularity\\\": 4413\\n  },\\n  {\\n    \\\"tag\\\": \"recompensate\",\\n    \\\"popularity\\\": 4409\\n  },\\n  {\\n    \\\"tag\\\": \"Shan\",\\n    \\\"popularity\\\": 4406\\n  },\\n  {\\n    \\\"tag\\\": \"brachycnemic\",\\n    \\\"popularity\\\": 4403\\n  },\\n  {\\n    \\\"tag\\\": \"Carinatae\",\\n    \\\"popularity\\\": 4399\\n  },\\n  {\\n    \\\"tag\\\": \"geotherm\",\\n    \\\"popularity\\\": 4396\\n  },\\n  {\\n    \\\"tag\\\": \"sawback\",\\n    \\\"popularity\\\": 4393\\n  },\\n  {\\n    \\\"tag\\\": \"Novatianist\",\\n    \\\"popularity\\\": 4390\\n  },\\n  {\\n    \\\"tag\\\": \"reapproach\",\\n    \\\"popularity\\\": 4387\\n  },\\n  {\\n    \\\"tag\\\": \"myelopoietic\",\\n    \\\"popularity\\\": 4383\\n  },\\n  {\\n    \\\"tag\\\": \"cyanin\",\\n    \\\"popularity\\\": 4380\\n  },\\n  {\\n    \\\"tag\\\": \"unsmutted\",\\n    \\\"popularity\\\": 4377\\n  },\\n  {\\n    \\\"tag\\\": \"nonpapist\",\\n    \\\"popularity\\\": 4374\\n  },\\n  {\\n    \\\"tag\\\": \"transbaikalian\",\\n    \\\"popularity\\\": 4370\\n  },\\n  {\\n    \\\"tag\\\": \"connately\",\\n    \\\"popularity\\\": 4367\\n  },\\n  {\\n    \\\"tag\\\": \"tenderize iterance\",\\n    \\\"popularity\\\": 4364\\n  },\\n  {\\n    \\\"tag\\\": \"hydrostatical\",\\n    \\\"popularity\\\": 4361\\n  },\\n  {\\n    \\\"tag\\\": \"unflag\",\\n    \\\"popularity\\\": 4358\\n  },\\n  {\\n    \\\"tag\\\": \"translate\",\\n    \\\"popularity\\\": 4354\\n  },\\n  {\\n    \\\"tag\\\": \"Scorzonera\",\\n    \\\"popularity\\\": 4351\\n  },\\n  {\\n    \\\"tag\\\": \"uncomforted\",\\n    \\\"popularity\\\": 4348\\n  },\\n  {\\n    \\\"tag\\\": \"risser varied\",\\n    \\\"popularity\\\": 4345\\n  },\\n  {\\n    \\\"tag\\\": \"plumbate\",\\n    \\\"popularity\\\": 4342\\n  },\\n  {\\n    \\\"tag\\\": \"Usneaceae\",\\n    \\\"popularity\\\": 4338\\n  },\\n  {\\n    \\\"tag\\\": \"fohat\",\\n    \\\"popularity\\\": 4335\\n  },\\n  {\\n    \\\"tag\\\": \"slagging\",\\n    \\\"popularity\\\": 4332\\n  },\\n  {\\n    \\\"tag\\\": \"superserious\",\\n    \\\"popularity\\\": 4329\\n  },\\n  {\\n    \\\"tag\\\": \"theocracy\",\\n    \\\"popularity\\\": 4326\\n  },\\n  {\\n    \\\"tag\\\": \"valonia\",\\n    \\\"popularity\\\": 4323\\n  },\\n  {\\n    \\\"tag\\\": \"Sapindales\",\\n    \\\"popularity\\\": 4319\\n  },\\n  {\\n    \\\"tag\\\": \"palaeozoologist\",\\n    \\\"popularity\\\": 4316\\n  },\\n  {\\n    \\\"tag\\\": \"yalb\",\\n    \\\"popularity\\\": 4313\\n  },\\n  {\\n    \\\"tag\\\": \"unviewed\",\\n    \\\"popularity\\\": 4310\\n  },\\n  {\\n    \\\"tag\\\": \"polyarteritis\",\\n    \\\"popularity\\\": 4307\\n  },\\n  {\\n    \\\"tag\\\": \"vectorial\",\\n    \\\"popularity\\\": 4304\\n  },\\n  {\\n    \\\"tag\\\": \"skimpingly\",\\n    \\\"popularity\\\": 4301\\n  },\\n  {\\n    \\\"tag\\\": \"athort\",\\n    \\\"popularity\\\": 4297\\n  },\\n  {\\n    \\\"tag\\\": \"tribofluorescence\",\\n    \\\"popularity\\\": 4294\\n  },\\n  {\\n    \\\"tag\\\": \"benzonitrol\",\\n    \\\"popularity\\\": 4291\\n  },\\n  {\\n    \\\"tag\\\": \"swiller subobtuse subjacency\",\\n    \\\"popularity\\\": 4288\\n  },\\n  {\\n    \\\"tag\\\": \"uncompassed\",\\n    \\\"popularity\\\": 4285\\n  },\\n  {\\n    \\\"tag\\\": \"cacochymia\",\\n    \\\"popularity\\\": 4282\\n  },\\n  {\\n    \\\"tag\\\": \"commensalist butadiene\",\\n    \\\"popularity\\\": 4279\\n  },\\n  {\\n    \\\"tag\\\": \"culpable\",\\n    \\\"popularity\\\": 4276\\n  },\\n  {\\n    \\\"tag\\\": \"contributive\",\\n    \\\"popularity\\\": 4273\\n  },\\n  {\\n    \\\"tag\\\": \"attemperately\",\\n    \\\"popularity\\\": 4269\\n  },\\n  {\\n    \\\"tag\\\": \"spelt\",\\n    \\\"popularity\\\": 4266\\n  },\\n  {\\n    \\\"tag\\\": \"exoneration\",\\n    \\\"popularity\\\": 4263\\n  },\\n  {\\n    \\\"tag\\\": \"antivivisectionist\",\\n    \\\"popularity\\\": 4260\\n  },\\n  {\\n    \\\"tag\\\": \"granitification\",\\n    \\\"popularity\\\": 4257\\n  },\\n  {\\n    \\\"tag\\\": \"palladize\",\\n    \\\"popularity\\\": 4254\\n  },\\n  {\\n    \\\"tag\\\": \"marksmanship\",\\n    \\\"popularity\\\": 4251\\n  },\\n  {\\n    \\\"tag\\\": \"bullydom\",\\n    \\\"popularity\\\": 4248\\n  },\\n  {\\n    \\\"tag\\\": \"spirality\",\\n    \\\"popularity\\\": 4245\\n  },\\n  {\\n    \\\"tag\\\": \"caliginous\",\\n    \\\"popularity\\\": 4242\\n  },\\n  {\\n    \\\"tag\\\": \"reportedly\",\\n    \\\"popularity\\\": 4239\\n  },\\n  {\\n    \\\"tag\\\": \"polyad\",\\n    \\\"popularity\\\": 4236\\n  },\\n  {\\n    \\\"tag\\\": \"arthroempyesis\",\\n    \\\"popularity\\\": 4233\\n  },\\n  {\\n    \\\"tag\\\": \"semibay facultatively\",\\n    \\\"popularity\\\": 4229\\n  },\\n  {\\n    \\\"tag\\\": \"metastatically\",\\n    \\\"popularity\\\": 4226\\n  },\\n  {\\n    \\\"tag\\\": \"prophetically\",\\n    \\\"popularity\\\": 4223\\n  },\\n  {\\n    \\\"tag\\\": \"Linguatula elapid\",\\n    \\\"popularity\\\": 4220\\n  },\\n  {\\n    \\\"tag\\\": \"pyknatom\",\\n    \\\"popularity\\\": 4217\\n  },\\n  {\\n    \\\"tag\\\": \"centimeter\",\\n    \\\"popularity\\\": 4214\\n  },\\n  {\\n    \\\"tag\\\": \"mensurate\",\\n    \\\"popularity\\\": 4211\\n  },\\n  {\\n    \\\"tag\\\": \"migraine\",\\n    \\\"popularity\\\": 4208\\n  },\\n  {\\n    \\\"tag\\\": \"pentagamist\",\\n    \\\"popularity\\\": 4205\\n  },\\n  {\\n    \\\"tag\\\": \"querken\",\\n    \\\"popularity\\\": 4202\\n  },\\n  {\\n    \\\"tag\\\": \"ambulance\",\\n    \\\"popularity\\\": 4199\\n  },\\n  {\\n    \\\"tag\\\": \"Stokavian\",\\n    \\\"popularity\\\": 4196\\n  },\\n  {\\n    \\\"tag\\\": \"malvasian\",\\n    \\\"popularity\\\": 4193\\n  },\\n  {\\n    \\\"tag\\\": \"uncouthsome\",\\n    \\\"popularity\\\": 4190\\n  },\\n  {\\n    \\\"tag\\\": \"readable\",\\n    \\\"popularity\\\": 4187\\n  },\\n  {\\n    \\\"tag\\\": \"enlodge\",\\n    \\\"popularity\\\": 4184\\n  },\\n  {\\n    \\\"tag\\\": \"plasterwise Appendiculariidae perspectograph\",\\n    \\\"popularity\\\": 4181\\n  },\\n  {\\n    \\\"tag\\\": \"inkweed\",\\n    \\\"popularity\\\": 4178\\n  },\\n  {\\n    \\\"tag\\\": \"streep\",\\n    \\\"popularity\\\": 4175\\n  },\\n  {\\n    \\\"tag\\\": \"diadelphian cultured\",\\n    \\\"popularity\\\": 4172\\n  },\\n  {\\n    \\\"tag\\\": \"hymenopterous\",\\n    \\\"popularity\\\": 4169\\n  },\\n  {\\n    \\\"tag\\\": \"unexorableness\",\\n    \\\"popularity\\\": 4166\\n  },\\n  {\\n    \\\"tag\\\": \"cascaron\",\\n    \\\"popularity\\\": 4163\\n  },\\n  {\\n    \\\"tag\\\": \"undaintiness\",\\n    \\\"popularity\\\": 4160\\n  },\\n  {\\n    \\\"tag\\\": \"Curtana\",\\n    \\\"popularity\\\": 4157\\n  },\\n  {\\n    \\\"tag\\\": \"scurvied\",\\n    \\\"popularity\\\": 4154\\n  },\\n  {\\n    \\\"tag\\\": \"molluscoidal\",\\n    \\\"popularity\\\": 4151\\n  },\\n  {\\n    \\\"tag\\\": \"yurt\",\\n    \\\"popularity\\\": 4148\\n  },\\n  {\\n    \\\"tag\\\": \"deciduitis\",\\n    \\\"popularity\\\": 4145\\n  },\\n  {\\n    \\\"tag\\\": \"creephole\",\\n    \\\"popularity\\\": 4142\\n  },\\n  {\\n    \\\"tag\\\": \"quatrefeuille\",\\n    \\\"popularity\\\": 4139\\n  },\\n  {\\n    \\\"tag\\\": \"bicapitate adenomatome\",\\n    \\\"popularity\\\": 4136\\n  },\\n  {\\n    \\\"tag\\\": \"damassin\",\\n    \\\"popularity\\\": 4134\\n  },\\n  {\\n    \\\"tag\\\": \"planching\",\\n    \\\"popularity\\\": 4131\\n  },\\n  {\\n    \\\"tag\\\": \"dashedly inferential\",\\n    \\\"popularity\\\": 4128\\n  },\\n  {\\n    \\\"tag\\\": \"lobe\",\\n    \\\"popularity\\\": 4125\\n  },\\n  {\\n    \\\"tag\\\": \"Hyrachyus\",\\n    \\\"popularity\\\": 4122\\n  },\\n  {\\n    \\\"tag\\\": \"knab\",\\n    \\\"popularity\\\": 4119\\n  },\\n  {\\n    \\\"tag\\\": \"discohexaster\",\\n    \\\"popularity\\\": 4116\\n  },\\n  {\\n    \\\"tag\\\": \"malign\",\\n    \\\"popularity\\\": 4113\\n  },\\n  {\\n    \\\"tag\\\": \"pedagoguism\",\\n    \\\"popularity\\\": 4110\\n  },\\n  {\\n    \\\"tag\\\": \"shrubbery\",\\n    \\\"popularity\\\": 4107\\n  },\\n  {\\n    \\\"tag\\\": \"undershrub\",\\n    \\\"popularity\\\": 4104\\n  },\\n  {\\n    \\\"tag\\\": \"bureaucrat\",\\n    \\\"popularity\\\": 4101\\n  },\\n  {\\n    \\\"tag\\\": \"pantaleon\",\\n    \\\"popularity\\\": 4098\\n  },\\n  {\\n    \\\"tag\\\": \"mesoventral\",\\n    \\\"popularity\\\": 4096\\n  }]';\n\
\n\
var log2 = Math.log(2);\n\
var tagInfo = tagInfoJSON.parseJSON(function(a, b) { if (a == \"popularity\") { return Math.log(b) / log2; } else {return b; } });\n\
\n\
function makeTagCloud(tagInfo)\n\
{\n\
    var output = '<div class=\"tagCloud\" style=\"width: 100%\">';\n\
\n\
    tagInfo.sort(function(a, b) { if (a.tag < b.tag) { return -1; } else if (a.tag == b.tag) { return 0; } else return 1; });\n\
\n\
    for (var i = 0; i < tagInfo.length; i++) {\n\
        var tag = tagInfo[i].tag;\n\
\n\
        var validates = true;\n\
        for (var j = 0; j < tag.length; j++) {\n\
            var ch = tag.charCodeAt(j);\n\
            if (ch < 0x20 || ch >= 0x7f) {\n\
                validates = false;\n\
                break;\n\
            }\n\
        }\n\
\n\
        if (!validates)\n\
            continue;\n\
\n\
        var url = \"http://example.com/tag/\" + tag.replace(\" \", \"\").toLowerCase();\n\
        var popularity = tagInfo[i].popularity;\n\
        var color = 'rgb(' + Math.floor(255 * (popularity - 12) / 20) + ', 0, 255)';\n\
        output += ' <a href=\"' + url + '\" style=\"font-size: ' + popularity + 'px; color: ' + color + '\">' + tag + '</a> \\n';\n\
    }\n\
\n\
    output += '</div>';\n\
    output.replace(\" \", \"&nbsp;\");\n\
\n\
    return output;\n\
}\n\
\n\
var tagcloud = makeTagCloud(tagInfo);\n\
\n\
var expectedMinLength = 315000;\n\
\n\
if (tagcloud.length < expectedMinLength)\n\
    throw \"ERROR: bad result: expected length at least \" + expectedMinLength + \" but got \" + tagcloud.length;\n\
\n\
tagInfo = null;\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider string-unpack-code</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>string-unpack-code</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"string-unpack-code failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
// This test case unpacks the compressed code for the MochiKit,\n\
// jQuery, Dojo and Prototype JavaScript libraries.\n\
\n\
/***\n\
    MochiKit.MochiKit 1.3.1 : PACKED VERSION\n\
    THIS FILE IS AUTOMATICALLY GENERATED.  If creating patches, please\n\
    diff against the source tree, not this file.\n\
\n\
    See <http://mochikit.com/> for documentation, downloads, license, etc.\n\
\n\
    (c) 2005 Bob Ippolito.  All rights Reserved.\n\
***/\n\
\n\
var result = 0;\n\
\n\
for (var i = 0; i < 2; i++) {\n\
\n\
var decompressedMochiKit = function(p,a,c,k,e,d){e=function(c){return(c<a?\"\":e(parseInt(c/a)))+((c=c%a)>35?String.fromCharCode(c+29):c.toString(36))};if(!''.replace(/^/,String)){while(c--)d[e(c)]=k[c]||e(c);k=[function(e){return d[e]}];e=function(){return'\\\\w+'};c=1};while(c--)if(k[c])p=p.replace(new RegExp('\\\\b'+e(c)+'\\\\b','g'),k[c]);return p}('if(H(1q)!=\"L\"){1q.2X(\"B.J\")}if(H(B)==\"L\"){B={}}if(H(B.J)==\"L\"){B.J={}}B.J.1Y=\"1.3.1\";B.J.1r=\"B.J\";B.J.2l=G(7V,vR){if(7V===O){7V={}}R(u i=1;i<M.K;i++){u o=M[i];if(H(o)!=\"L\"&&o!==O){R(u k in o){7V[k]=o[k]}}}F 7V};B.J.2l(B.J,{1K:G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"},1l:G(){F D.1K()},4f:G(n){if(M.K===0){n=1}F G(){F n++}},4L:G(mw){u me=M.2U;if(M.K==1){me.1U=mw;F Y me()}},bg:G(vQ){u X=[];u m=B.J;u aw=m.1R(O,M);1M(aw.K){u o=aw.2P();if(o&&H(o)==\"3n\"&&H(o.K)==\"2y\"){R(u i=o.K-1;i>=0;i--){aw.e9(o[i])}}N{X.1c(o)}}F X},1R:G(7U,1i,av){if(!av){av=0}if(1i){u l=1i.K;if(H(l)!=\"2y\"){if(H(B.15)!=\"L\"){1i=B.15.2G(1i);l=1i.K}N{14 Y 3p(\"au 2E an at-as 3W B.15 2E ar\")}}if(!7U){7U=[]}R(u i=av;i<l;i++){7U.1c(1i[i])}}F 7U},8Z:G(5g,1i){if(5g===O){5g={}}R(u i=1;i<M.K;i++){u o=M[i];if(H(o)!=\"L\"&&o!==O){R(u k in o){u v=o[k];if(H(5g[k])==\"3n\"&&H(v)==\"3n\"){M.2U(5g[k],v)}N{5g[k]=v}}}}F 5g},lO:G(6c,1i){if(6c===O){6c={}}R(u i=1;i<M.K;i++){u o=M[i];R(u k in o){if(!(k in 6c)){6c[k]=o[k]}}}F 6c},lN:G(1i){u fj=[];R(u mv in 1i){fj.1c(mv)}F fj},lM:G(1i){u fh=[];u e;R(u fi in 1i){u v;1f{v=1i[fi]}1e(e){2V}fh.1c([fi,v])}F fh},jq:G(fg,ff,fe){fe.1U=Y B.J.5a(fg.1r+\".\"+ff);fg[ff]=fe},4i:{7L:G(a){F!!a},vP:G(a){F!a},eE:G(a){F a},2E:G(a){F~a},vO:G(a){F-a},vN:G(a,b){F a+b},vM:G(a,b){F a-b},4u:G(a,b){F a/b},vL:G(a,b){F a%b},vK:G(a,b){F a*b},3W:G(a,b){F a&b},or:G(a,b){F a|b},vJ:G(a,b){F a^b},vI:G(a,b){F a<<b},vH:G(a,b){F a>>b},vG:G(a,b){F a>>>b},eq:G(a,b){F a==b},ne:G(a,b){F a!=b},gt:G(a,b){F a>b},ge:G(a,b){F a>=b},lt:G(a,b){F a<b},le:G(a,b){F a<=b},vF:G(a,b){F B.J.2f(a,b)===0},vE:G(a,b){F B.J.2f(a,b)!==0},vD:G(a,b){F B.J.2f(a,b)==1},vC:G(a,b){F B.J.2f(a,b)!=-1},vB:G(a,b){F B.J.2f(a,b)==-1},vA:G(a,b){F B.J.2f(a,b)!=1},vz:G(a,b){F a&&b},vy:G(a,b){F a||b},vx:G(a,b){F b in a}},24:G(mu){F G(){F D[mu].1w(D,M)}},lL:G(mt){F G(a9){F a9[mt]}},66:G(){u fd={};R(u i=0;i<M.K;i++){u 6b=M[i];fd[6b]=6b}F G(){R(u i=0;i<M.K;i++){if(!(H(M[i])in fd)){F 1m}}F 1h}},lJ:G(){R(u i=0;i<M.K;i++){if(M[i]!==O){F 1m}}F 1h},lK:G(){R(u i=0;i<M.K;i++){u o=M[i];if(!(H(o)==\"L\"||o===O)){F 1m}}F 1h},lI:G(1i){F!B.J.7e.1w(D,M)},7e:G(1i){R(u i=0;i<M.K;i++){u o=M[i];if(!(o&&o.K)){F 1m}}F 1h},3A:G(){R(u i=0;i<M.K;i++){u o=M[i];u 6b=H(o);if((6b!=\"3n\"&&!(6b==\"G\"&&H(o.vw)==\"G\"))||o===O||H(o.K)!=\"2y\"){F 1m}}F 1h},eN:G(){R(u i=0;i<M.K;i++){u o=M[i];if(H(o)!=\"3n\"||o===O||H(o.9P)!=\"G\"){F 1m}}F 1h},lH:G(fn){if(fn===O){F B.J.1R(O,M,1)}u fc=[];R(u i=1;i<M.K;i++){fc.1c(fn(M[i]))}F fc},2r:G(fn,1g){u m=B.J;u 6a=B.15;u fb=m.3A;if(M.K<=2){if(!fb(1g)){if(6a){1g=6a.2G(1g);if(fn===O){F 1g}}N{14 Y 3p(\"au 2E an at-as 3W B.15 2E ar\")}}if(fn===O){F m.1R(O,1g)}u 69=[];R(u i=0;i<1g.K;i++){69.1c(fn(1g[i]))}F 69}N{if(fn===O){fn=7o}u 7T=O;R(i=1;i<M.K;i++){if(!fb(M[i])){if(6a){F 6a.2G(6a.4c.1w(O,M))}N{14 Y 3p(\"au 2E an at-as 3W B.15 2E ar\")}}u l=M[i].K;if(7T===O||7T>l){7T=l}}69=[];R(i=0;i<7T;i++){u fa=[];R(u j=1;j<M.K;j++){fa.1c(M[j][i])}69.1c(fn.1w(D,fa))}F 69}},lG:G(fn){u f9=[];if(fn===O){fn=B.J.4i.7L}R(u i=1;i<M.K;i++){u o=M[i];if(fn(o)){f9.1c(o)}}F f9},47:G(fn,1g,7S){u aq=[];u m=B.J;if(!m.3A(1g)){if(B.15){1g=B.15.2G(1g)}N{14 Y 3p(\"au 2E an at-as 3W B.15 2E ar\")}}if(fn===O){fn=m.4i.7L}if(H(7o.1U.47)==\"G\"){F 7o.1U.47.cz(1g,fn,7S)}N{if(H(7S)==\"L\"||7S===O){R(u i=0;i<1g.K;i++){u o=1g[i];if(fn(o)){aq.1c(o)}}}N{R(i=0;i<1g.K;i++){o=1g[i];if(fn.cz(7S,o)){aq.1c(o)}}}}F aq},mq:G(7R){F G(){hd(M.K){3j 0:F 7R();3j 1:F 7R(M[0]);3j 2:F 7R(M[0],M[1]);3j 3:F 7R(M[0],M[1],M[2])}u f8=[];R(u i=0;i<M.K;i++){f8.1c(\"M[\"+i+\"]\")}F dB(\"(1A(\"+f8.2b(\",\")+\"))\")}},lv:G(mr,ms){u m=B.J;F m.1O.1w(D,m.1R([ms,mr],M,2))},1O:G(3c,4o){if(H(3c)==\"1n\"){3c=4o[3c]}u ao=3c.f5;u 5f=3c.am;u f6=3c.f7;u m=B.J;if(H(3c)==\"G\"&&H(3c.1w)==\"L\"){3c=m.mq(3c)}if(H(ao)!=\"G\"){ao=3c}if(H(4o)!=\"L\"){f6=4o}if(H(5f)==\"L\"){5f=[]}N{5f=5f.9T()}m.1R(5f,M,2);u 7Q=G(){u ap=M;u me=M.2U;if(me.am.K>0){ap=m.2o(me.am,ap)}u 4o=me.f7;if(!4o){4o=D}F me.f5.1w(4o,ap)};7Q.f7=f6;7Q.f5=ao;7Q.am=5f;F 7Q},lF:G(7P){u mp=B.J.1O;R(u k in 7P){u f4=7P[k];if(H(f4)==\"G\"){7P[k]=mp(f4,7P)}}},5u:G(mo,mn,ml,mk){B.J.ae.5M(mo,mn,ml,mk)},mj:{\"5L\":1h,\"1n\":1h,\"2y\":1h},2f:G(a,b){if(a==b){F 0}u f3=(H(a)==\"L\"||a===O);u f2=(H(b)==\"L\"||b===O);if(f3&&f2){F 0}N{if(f3){F-1}N{if(f2){F 1}}}u m=B.J;u f1=m.mj;if(!(H(a)in f1&&H(b)in f1)){1f{F m.ae.3C(a,b)}1e(e){if(e!=m.4d){14 e}}}if(a<b){F-1}N{if(a>b){F 1}}u f0=m.U;14 Y 3p(f0(a)+\" 3W \"+f0(b)+\" 9v 2E be vv\")},eM:G(a,b){F B.J.2f(a.9P(),b.9P())},eL:G(a,b){u mi=B.J.2f;u 7O=a.K;u al=0;if(7O>b.K){al=1;7O=b.K}N{if(7O<b.K){al=-1}}R(u i=0;i<7O;i++){u 4j=mi(a[i],b[i]);if(4j){F 4j}}F al},7M:G(mh,mg,mf,md){B.J.ad.5M(mh,mg,mf,md)},U:G(o){if(H(o)==\"L\"){F\"L\"}N{if(o===O){F\"O\"}}1f{if(H(o.1K)==\"G\"){F o.1K()}N{if(H(o.U)==\"G\"&&o.U!=M.2U){F o.U()}}F B.J.ad.3C(o)}1e(e){if(H(o.1r)==\"1n\"&&(o.1l==cZ.1U.1l||o.1l==vu.1U.1l)){F o.1r}}1f{u eZ=(o+\"\")}1e(e){F\"[\"+H(o)+\"]\"}if(H(o)==\"G\"){o=eZ.23(/^\\\\s+/,\"\");u 5n=o.2A(\"{\");if(5n!=-1){o=o.3H(0,5n)+\"{...}\"}}F eZ},eK:G(o){u m=B.J;F\"[\"+m.2r(m.U,o).2b(\", \")+\"]\"},ac:G(o){F(\"\\\\\"\"+o.23(/([\"\\\\\\\\])/g,\"\\\\\\\\$1\")+\"\\\\\"\").23(/[\\\\f]/g,\"\\\\\\\\f\").23(/[\\\\b]/g,\"\\\\\\\\b\").23(/[\\\\n]/g,\"\\\\\\\\n\").23(/[\\\\t]/g,\"\\\\\\\\t\").23(/[\\\\r]/g,\"\\\\\\\\r\")},eJ:G(o){F o+\"\"},ly:G(mc,mb,ma,m9){B.J.ab.5M(mc,mb,ma,m9)},lx:G(){F dB(\"(\"+M[0]+\")\")},lz:G(o){u 5e=H(o);if(5e==\"L\"){F\"L\"}N{if(5e==\"2y\"||5e==\"5L\"){F o+\"\"}N{if(o===O){F\"O\"}}}u m=B.J;u eY=m.ac;if(5e==\"1n\"){F eY(o)}u me=M.2U;u 3S;if(H(o.m8)==\"G\"){3S=o.m8();if(o!==3S){F me(3S)}}if(H(o.m7)==\"G\"){3S=o.m7();if(o!==3S){F me(3S)}}if(5e!=\"G\"&&H(o.K)==\"2y\"){u X=[];R(u i=0;i<o.K;i++){u 2i=me(o[i]);if(H(2i)!=\"1n\"){2i=\"L\"}X.1c(2i)}F\"[\"+X.2b(\", \")+\"]\"}1f{3S=m.ab.3C(o);F me(3S)}1e(e){if(e!=m.4d){14 e}}if(5e==\"G\"){F O}X=[];R(u k in o){u ak;if(H(k)==\"2y\"){ak=\"\\\\\"\"+k+\"\\\\\"\"}N{if(H(k)==\"1n\"){ak=eY(k)}N{2V}}2i=me(o[k]);if(H(2i)!=\"1n\"){2V}X.1c(ak+\":\"+2i)}F\"{\"+X.2b(\", \")+\"}\"},lE:G(a,b){F(B.J.2f(a,b)===0)},lD:G(eX,4n){if(eX.K!=4n.K){F 1m}F(B.J.2f(eX,4n)===0)},2o:G(){u eW=[];u m6=B.J.1R;R(u i=0;i<M.K;i++){m6(eW,M[i])}F eW},eR:G(2h){u m=B.J;u eU=m.2f;if(M.K==1){F G(a,b){F eU(a[2h],b[2h])}}u eV=m.1R(O,M);F G(a,b){u aj=0;R(u i=0;(aj===0)&&(i<eV.K);i++){u 2h=eV[i];aj=eU(a[2h],b[2h])}F aj}},lC:G(2h){u m5=B.J.eR.1w(D,M);F G(a,b){F m5(b,a)}},2z:G(m4){u m=B.J;F m.1O.1w(D,m.1R([m4,L],M,1))},67:G(m0,1g){if(1g.K===0){F O}u ai=1g[0];u m3=B.J.2f;R(u i=1;i<1g.K;i++){u o=1g[i];if(m3(o,ai)==m0){ai=o}}F ai},lB:G(){F B.J.67(1,M)},lA:G(){F B.J.67(-1,M)},bi:G(1g,lY,lZ,3B){if(H(3B)==\"L\"||3B===O){3B=1g.K}R(u i=(lZ||0);i<3B;i++){if(1g[i]===lY){F i}}F-1},eO:G(1g,lW,lX,3B){if(H(3B)==\"L\"||3B===O){3B=1g.K}u 4j=B.J.2f;R(u i=(lX||0);i<3B;i++){if(4j(1g[i],lW)===0){F i}}F-1},d4:G(1j,lV){u ah=[1j];u lU=B.J.1R;1M(ah.K){u X=lV(ah.2P());if(X){lU(ah,X)}}},3f:G(ag){u 2w=ag.1r;if(H(2w)==\"L\"){2w=\"\"}N{2w=2w+\".\"}R(u 1b in ag){u o=ag[1b];if(H(o)==\"G\"&&H(o.1r)==\"L\"){1f{o.1r=2w+1b}1e(e){}}}},dw:G(3s,68){if(H(B.S)!=\"L\"&&M.K==1&&(H(3s)==\"1n\"||(H(3s.3T)!=\"L\"&&3s.3T>0))){u kv=B.S.d5(3s);3s=kv[0];68=kv[1]}N{if(M.K==1){u o=3s;3s=[];68=[];R(u k in o){u v=o[k];if(H(v)!=\"G\"){3s.1c(k);68.1c(v)}}}}u W=[];u lT=28.2a(3s.K,68.K);u eT=B.J.af;R(u i=0;i<lT;i++){v=68[i];if(H(v)!=\"L\"&&v!==O){W.1c(eT(3s[i])+\"=\"+eT(v))}}F W.2b(\"&\")},lw:G(lS,lQ){u 7N=lS.23(/\\\\+/g,\"%20\").2R(\"&\");u o={};u 5d;if(H(lR)!=\"L\"){5d=lR}N{5d=vt}if(lQ){R(u i=0;i<7N.K;i++){u 2n=7N[i].2R(\"=\");u 1b=5d(2n[0]);u 4n=o[1b];if(!(4n 2C 7o)){4n=[];o[1b]=4n}4n.1c(5d(2n[1]))}}N{R(i=0;i<7N.K;i++){2n=7N[i].2R(\"=\");o[5d(2n[0])]=5d(2n[1])}}F o}});B.J.4a=G(){D.4m=[]};B.J.4a.1U={5M:G(1b,eS,3y,lP){if(lP){D.4m.e9([1b,eS,3y])}N{D.4m.1c([1b,eS,3y])}},3C:G(){R(u i=0;i<D.4m.K;i++){u 2n=D.4m[i];if(2n[1].1w(D,M)){F 2n[2].1w(D,M)}}14 B.J.4d},vs:G(1b){R(u i=0;i<D.4m.K;i++){u 2n=D.4m[i];if(2n[0]==1b){D.4m.4y(i,1);F 1h}}F 1m}};B.J.1z=[\"4f\",\"4L\",\"1R\",\"2l\",\"8Z\",\"lO\",\"lN\",\"lM\",\"5a\",\"4i\",\"24\",\"lL\",\"66\",\"lo\",\"ln\",\"lK\",\"lJ\",\"lI\",\"7e\",\"3A\",\"eN\",\"lH\",\"2r\",\"lG\",\"47\",\"1O\",\"lF\",\"4d\",\"4a\",\"5u\",\"2f\",\"7M\",\"U\",\"lE\",\"lD\",\"2o\",\"eR\",\"lC\",\"2z\",\"lm\",\"67\",\"lp\",\"eI\",\"lB\",\"lA\",\"d4\",\"ll\",\"af\",\"dw\",\"lz\",\"ly\",\"lx\",\"lw\",\"eO\",\"bi\",\"bg\",\"lv\"];B.J.1W=[\"3f\",\"ae\",\"ad\",\"ab\",\"eM\",\"eL\",\"eK\",\"ac\",\"eJ\"];B.J.2Y=G(lu,eP){if(H(B.eQ)==\"L\"){B.eQ=(B.3d||(H(1x)==\"L\"&&H(1q)==\"L\"))}if(!B.eQ){F}u 1p=eP.2k[\":1p\"];R(u i=0;i<1p.K;i++){lu[1p[i]]=eP[1p[i]]}};B.J.2d=G(){u m=D;m.vr=m.24;m.vq=m.eO;if(H(ls)!=\"L\"){m.af=G(lr){F ls(lr).23(/\\\\\\'/g,\"%27\")}}N{m.af=G(lq){F vp(lq).23(/\\\\+/g,\"%2B\").23(/\\\\\"/g,\"%22\").W.23(/\\\\\\'/g,\"%27\")}}m.5a=G(1b){D.43=1b;D.1b=1b};m.5a.1U=Y 2x();m.2l(m.5a.1U,{U:G(){if(D.43&&D.43!=D.1b){F D.1b+\"(\"+m.U(D.43)+\")\"}N{F D.1b+\"()\"}},1l:m.24(\"U\")});m.4d=Y m.5a(\"B.J.4d\");m.lp=m.2z(m.67,1);m.eI=m.2z(m.67,-1);m.lo=m.66(\"G\");m.ln=m.66(\"L\");m.lm=m.2z(m.2l,O);m.ll=m.2z(m.2r,O);m.ae=Y m.4a();m.5u(\"vo\",m.eN,m.eM);m.5u(\"ej\",m.3A,m.eL);m.ad=Y m.4a();m.7M(\"ej\",m.3A,m.eK);m.7M(\"1n\",m.66(\"1n\"),m.ac);m.7M(\"vn\",m.66(\"2y\",\"5L\"),m.eJ);m.ab=Y m.4a();u 1p=m.2o(m.1z,m.1W);m.2k={\":3e\":m.2o(m.1W),\":1p\":1p};m.3f(D)};B.J.2d();if(!B.3d){2f=B.J.2f}B.J.2Y(D,B.J);if(H(1q)!=\"L\"){1q.2X(\"B.15\");1q.2M(\"B.J\")}if(H(1x)!=\"L\"){1x.26(\"B.J\",[])}1f{if(H(B.J)==\"L\"){14\"\"}}1e(e){14\"B.15 3F on B.J!\"}if(H(B.15)==\"L\"){B.15={}}B.15.1r=\"B.15\";B.15.1Y=\"1.3.1\";B.J.2l(B.15,{1K:G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"},1l:G(){F D.1K()},9W:G(1b,lk,lj,lh){B.15.9Y.5M(1b,lk,lj,lh)},1Q:G(3R,lg){u I=B.15;if(M.K==2){F I.9Z(G(a){F a!=lg},3R)}if(H(3R.1a)==\"G\"){F 3R}N{if(H(3R.1Q)==\"G\"){F 3R.1Q()}}1f{F I.9Y.3C(3R)}1e(e){u m=B.J;if(e==m.4d){e=Y 3p(H(3R)+\": \"+m.U(3R)+\" is 2E vm\")}14 e}},eu:G(n){if(!n){n=0}u m=B.J;F{U:G(){F\"eu(\"+n+\")\"},1l:m.24(\"U\"),1a:m.4f(n)}},et:G(p){u I=B.15;u m=B.J;u 1g=[];u lf=I.1Q(p);F{U:G(){F\"et(...)\"},1l:m.24(\"U\"),1a:G(){1f{u W=lf.1a();1g.1c(W);F W}1e(e){if(e!=I.25){14 e}if(1g.K===0){D.1a=G(){14 I.25}}N{u i=-1;D.1a=G(){i=(i+1)%1g.K;F 1g[i]}}F D.1a()}}}},7b:G(Q,n){u m=B.J;if(H(n)==\"L\"){F{U:G(){F\"7b(\"+m.U(Q)+\")\"},1l:m.24(\"U\"),1a:G(){F Q}}}F{U:G(){F\"7b(\"+m.U(Q)+\", \"+n+\")\"},1l:m.24(\"U\"),1a:G(){if(n<=0){14 B.15.25}n-=1;F Q}}},1a:G(ld){F ld.1a()},es:G(p,q){u m=B.J;u 1a=B.15.1a;u lc=m.2r(1Q,M);F{U:G(){F\"es(...)\"},1l:m.24(\"U\"),1a:G(){F m.2r(1a,lc)}}},a1:G(3b,1V){u m=B.J;1V=B.15.1Q(1V);if(3b===O){3b=m.4i.7L}F{U:G(){F\"a1(...)\"},1l:m.24(\"U\"),1a:G(){1M(1h){u W=1V.1a();if(3b(W)){F W}}F L}}},a0:G(3b,1V){u m=B.J;1V=B.15.1Q(1V);if(3b===O){3b=m.4i.7L}F{U:G(){F\"a0(...)\"},1l:m.24(\"U\"),1a:G(){1M(1h){u W=1V.1a();if(!3b(W)){F W}}F L}}},er:G(1V){u I=B.15;u m=B.J;1V=I.1Q(1V);u 5c=0;u 2J=0;u 3a=1;u i=-1;if(M.K==2){2J=M[1]}N{if(M.K==3){5c=M[1];2J=M[2]}N{5c=M[1];2J=M[2];3a=M[3]}}F{U:G(){F\"er(\"+[\"...\",5c,2J,3a].2b(\", \")+\")\"},1l:m.24(\"U\"),1a:G(){u W;1M(i<5c){W=1V.1a();i++}if(5c>=2J){14 I.25}5c+=3a;F W}}},4c:G(aa,p,q){u m=B.J;u I=B.15;u lb=m.2r(I.1Q,m.1R(O,M,1));u 2r=m.2r;u 1a=I.1a;F{U:G(){F\"4c(...)\"},1l:m.24(\"U\"),1a:G(){F aa.1w(D,2r(1a,lb))}}},ep:G(aa,1V,I){1V=B.15.1Q(1V);u m=B.J;F{U:G(){F\"ep(...)\"},1l:m.24(\"U\"),1a:G(){F aa.1w(I,1V.1a())}}},55:G(p,q){u I=B.15;u m=B.J;if(M.K==1){F I.1Q(M[0])}u 64=m.2r(I.1Q,M);F{U:G(){F\"55(...)\"},1l:m.24(\"U\"),1a:G(){1M(64.K>1){1f{F 64[0].1a()}1e(e){if(e!=I.25){14 e}64.2P()}}if(64.K==1){u a9=64.2P();D.1a=m.1O(\"1a\",a9);F D.1a()}14 I.25}}},9Z:G(3b,1V){u I=B.15;1V=I.1Q(1V);F{U:G(){F\"9Z(...)\"},1l:B.J.24(\"U\"),1a:G(){u W=1V.1a();if(!3b(W)){D.1a=G(){14 I.25};D.1a()}F W}}},eo:G(3b,1V){1V=B.15.1Q(1V);u m=B.J;u 1O=m.1O;F{\"U\":G(){F\"eo(...)\"},\"1l\":m.24(\"U\"),\"1a\":G(){1M(1h){u W=1V.1a();if(!3b(W)){2K}}D.1a=1O(\"1a\",1V);F W}}},a7:G(63,2u,la){2u.62[63]=-1;u m=B.J;u l9=m.eI;F{U:G(){F\"en(\"+63+\", ...)\"},1l:m.24(\"U\"),1a:G(){u W;u i=2u.62[63];if(i==2u.29){W=la.1a();2u.a8.1c(W);2u.29+=1;2u.62[63]+=1}N{W=2u.a8[i-2u.2a];2u.62[63]+=1;if(i==2u.2a&&l9(2u.62)!=2u.2a){2u.2a+=1;2u.a8.2P()}}F W}}},en:G(a6,n){u W=[];u 2u={\"62\":[],\"a8\":[],\"29\":-1,\"2a\":-1};if(M.K==1){n=2}u I=B.15;a6=I.1Q(a6);u a7=I.a7;R(u i=0;i<n;i++){W.1c(a7(i,2u,a6))}F W},2G:G(4l){u m=B.J;if(H(4l.9T)==\"G\"){F 4l.9T()}N{if(m.3A(4l)){F m.2o(4l)}}u I=B.15;4l=I.1Q(4l);u W=[];1f{1M(1h){W.1c(4l.1a())}}1e(e){if(e!=I.25){14 e}F W}F L},7H:G(fn,7K,l8){u i=0;u x=l8;u I=B.15;7K=I.1Q(7K);if(M.K<3){1f{x=7K.1a()}1e(e){if(e==I.25){e=Y 3p(\"7H() of vl vk vj no vi 3m\")}14 e}i++}1f{1M(1h){x=fn(x,7K.1a())}}1e(e){if(e!=I.25){14 e}}F x},7I:G(){u 4k=0;u 2J=0;u 3a=1;if(M.K==1){2J=M[0]}N{if(M.K==2){4k=M[0];2J=M[1]}N{if(M.K==3){4k=M[0];2J=M[1];3a=M[2]}N{14 Y 3p(\"7I() vh 1, 2, or 3 M!\")}}}if(3a===0){14 Y 3p(\"7I() 3a 5p 2E be 0\")}F{1a:G(){if((3a>0&&4k>=2J)||(3a<0&&4k<=2J)){14 B.15.25}u W=4k;4k+=3a;F W},U:G(){F\"7I(\"+[4k,2J,3a].2b(\", \")+\")\"},1l:B.J.24(\"U\")}},l0:G(a5,l7){u x=l7||0;u I=B.15;a5=I.1Q(a5);1f{1M(1h){x+=a5.1a()}}1e(e){if(e!=I.25){14 e}}F x},em:G(a4){u I=B.15;a4=I.1Q(a4);1f{1M(1h){a4.1a()}}1e(e){if(e!=I.25){14 e}}},9a:G(7J,1A,I){u m=B.J;if(M.K>2){1A=m.1O(1A,I)}if(m.3A(7J)){1f{R(u i=0;i<7J.K;i++){1A(7J[i])}}1e(e){if(e!=B.15.25){14 e}}}N{I=B.15;I.em(I.4c(1A,7J))}},kZ:G(l6,1A){u I=B.15;1f{I.a0(1A,l6).1a();F 1m}1e(e){if(e!=I.25){14 e}F 1h}},kY:G(l5,4j){u W=B.15.2G(l5);if(M.K==1){4j=B.J.2f}W.iz(4j);F W},kX:G(l4){u W=B.15.2G(l4);W.vg();F W},kW:G(l3,1A){u I=B.15;1f{I.a1(1A,l3).1a();F 1h}1e(e){if(e!=I.25){14 e}F 1m}},kV:G(1g,5b){if(B.J.3A(5b)){R(u i=0;i<5b.K;i++){1g.1c(5b[i])}}N{u I=B.15;5b=I.1Q(5b);1f{1M(1h){1g.1c(5b.1a())}}1e(e){if(e!=I.25){14 e}}}F 1g},ek:G(a3,eH){u m=B.J;u I=B.15;if(M.K<2){eH=m.4i.eE}a3=I.1Q(a3);u pk=L;u k=L;u v;G eF(){v=a3.1a();k=eH(v)}G l2(){u 7j=v;v=L;F 7j}u eG=1h;F{U:G(){F\"ek(...)\"},1a:G(){1M(k==pk){eF();if(eG){eG=1m;2K}}pk=k;F[k,{1a:G(){if(v==L){eF()}if(k!=pk){14 I.25}F l2()}}]}}},kU:G(a2,eD){u m=B.J;u I=B.15;if(M.K<2){eD=m.4i.eE}a2=I.1Q(a2);u ey=[];u eA=1h;u ez;1M(1h){1f{u eB=a2.1a();u 2h=eD(eB)}1e(e){if(e==I.25){2K}14 e}if(eA||2h!=ez){u eC=[];ey.1c([2h,eC])}eC.1c(eB);eA=1m;ez=2h}F ey},9X:G(ex){u i=0;F{U:G(){F\"9X(...)\"},1l:B.J.24(\"U\"),1a:G(){if(i>=ex.K){14 B.15.25}F ex[i++]}}},eh:G(ew){F(ew&&H(ew.ei)==\"G\")},9V:G(l1){F{U:G(){F\"9V(...)\"},1l:B.J.24(\"U\"),1a:G(){u W=l1.ei();if(W===O||W===L){14 B.15.25}F W}}}});B.15.1W=[\"9Y\",\"9X\",\"eh\",\"9V\",];B.15.1z=[\"25\",\"9W\",\"1Q\",\"eu\",\"et\",\"7b\",\"1a\",\"es\",\"a1\",\"a0\",\"er\",\"4c\",\"ep\",\"55\",\"9Z\",\"eo\",\"en\",\"2G\",\"7H\",\"7I\",\"l0\",\"em\",\"9a\",\"kZ\",\"kY\",\"kX\",\"kW\",\"kV\",\"ek\",\"kU\"];B.15.2d=G(){u m=B.J;D.25=Y m.5a(\"25\");D.9Y=Y m.4a();D.9W(\"ej\",m.3A,D.9X);D.9W(\"ei\",D.eh,D.9V);D.2k={\":3e\":D.1z,\":1p\":m.2o(D.1z,D.1W)};m.3f(D)};B.15.2d();if(!B.3d){7H=B.15.7H}B.J.2Y(D,B.15);if(H(1q)!=\"L\"){1q.2X(\"B.1H\");1q.2M(\"B.J\")}if(H(1x)!=\"L\"){1x.26(\"B.J\",[])}1f{if(H(B.J)==\"L\"){14\"\"}}1e(e){14\"B.1H 3F on B.J!\"}if(H(B.1H)==\"L\"){B.1H={}}B.1H.1r=\"B.1H\";B.1H.1Y=\"1.3.1\";B.1H.1K=G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"};B.1H.1l=G(){F D.1K()};B.1H.1z=[\"5C\",\"49\",\"7A\",\"kR\",\"2L\",\"5Z\",\"kG\",\"ch\",\"kE\",\"kC\"];B.1H.1W=[\"ef\",\"e8\",\"e7\"];B.1H.49=G(1P,kT,3z){D.1P=1P;D.3N=kT;D.3z=3z;D.vf=Y 3Q()};B.1H.49.1U={U:G(){u m=B.J;F\"49(\"+m.2r(m.U,[D.1P,D.3N,D.3z]).2b(\", \")+\")\"},1l:B.J.24(\"U\")};B.J.2l(B.1H,{ef:G(7F){u I=B.1H;if(H(7F)==\"1n\"){7F=I.5C[7F]}F G(1t){u 7G=1t.3N;if(H(7G)==\"1n\"){7G=I.5C[7G]}F 7G>=7F}},e8:G(){u kS=B.1H.49;R(u i=0;i<M.K;i++){if(!(M[i]2C kS)){F 1m}}F 1h},e7:G(a,b){F B.J.2f([a.3N,a.3z],[b.3N,b.3z])},kR:G(1t){cq(\"1P: \"+1t.1P+\"\\\\ve: \"+1t.3N+\"\\\\vd: \"+1t.3z.2b(\" \"))}});B.1H.7A=G(7E){D.4f=0;if(H(7E)==\"L\"||7E===O){7E=-1}D.ec=7E;D.4h=[];D.7C={};D.e5=1m};B.1H.7A.1U={vc:G(){D.4h.4y(0,D.4h.K)},kK:G(1t){if(H(2O)!=\"L\"&&2O.eg&&2O.eg.5Z){2O.eg.5Z(1t)}N{if(H(7h)!=\"L\"&&7h.kQ){7h.kQ(1t)}N{if(H(5X)==\"G\"){5X(1t)}}}},kL:G(1t){R(u k in D.7C){u 2n=D.7C[k];if(2n.kO!=k||(2n[0]&&!2n[0](1t))){2V}2n[1](1t)}},hE:G(ee,7D,kP){if(H(7D)==\"1n\"){7D=B.1H.ef(7D)}u ed=[7D,kP];ed.kO=ee;D.7C[ee]=ed},c9:G(kN){gi D.7C[kN]},kH:G(kM,vb){u 1t=Y B.1H.49(D.4f,kM,B.J.1R(O,M,1));D.4h.1c(1t);D.kL(1t);if(D.e5){D.kK(1t.3N+\": \"+1t.3z.2b(\" \"))}D.4f+=1;1M(D.ec>=0&&D.4h.K>D.ec){D.4h.2P()}},c8:G(9U){u ea=0;if(!(H(9U)==\"L\"||9U===O)){ea=28.29(0,D.4h.K-9U)}F D.4h.9T(ea)},kJ:G(7B){if(H(7B)==\"L\"||7B===O){7B=30}u 9S=D.c8(7B);if(9S.K){u 1g=2r(G(m){F\"\\\\n  [\"+m.1P+\"] \"+m.3N+\": \"+m.3z.2b(\" \")},9S);1g.e9(\"va \"+9S.K+\" v9:\");F 1g.2b(\"\")}F\"\"},v8:G(kI){if(H(B.1I)==\"L\"){cq(D.kJ())}N{B.1I.bY(kI||1m)}}};B.1H.2d=G(){D.5C={8M:40,8L:50,8K:30,8J:20,8I:10};u m=B.J;m.5u(\"49\",D.e8,D.e7);u 61=m.2z;u e6=D.7A;u 60=e6.1U.kH;m.2l(D.7A.1U,{kF:61(60,\"8I\"),5Z:61(60,\"8J\"),dE:61(60,\"8M\"),kD:61(60,\"8L\"),kB:61(60,\"8K\")});u I=D;u 5Y=G(1b){F G(){I.2L[1b].1w(I.2L,M)}};D.5Z=5Y(\"5Z\");D.kG=5Y(\"dE\");D.ch=5Y(\"kF\");D.kE=5Y(\"kD\");D.kC=5Y(\"kB\");D.2L=Y e6();D.2L.e5=1h;D.2k={\":3e\":D.1z,\":1p\":m.2o(D.1z,D.1W)};m.3f(D)};if(H(5X)==\"L\"&&H(2v)!=\"L\"&&2v.kA&&H(kz)!=\"L\"){5X=G(){5X.3G=M;u ev=2v.kA(\"v7\");ev.v6(\"5X\",1m,1h);kz(ev)}}B.1H.2d();B.J.2Y(D,B.1H);if(H(1q)!=\"L\"){1q.2X(\"B.1D\")}if(H(B)==\"L\"){B={}}if(H(B.1D)==\"L\"){B.1D={}}B.1D.1r=\"B.1D\";B.1D.1Y=\"1.3.1\";B.1D.1K=G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"};B.1D.1l=G(){F D.1K()};B.1D.ks=G(1y){1y=1y+\"\";if(H(1y)!=\"1n\"||1y.K===0){F O}u 7z=1y.2R(\"-\");if(7z.K===0){F O}F Y 3Q(7z[0],7z[1]-1,7z[2])};B.1D.ky=/(\\\\d{4,})(?:-(\\\\d{1,2})(?:-(\\\\d{1,2})(?:[T ](\\\\d{1,2}):(\\\\d{1,2})(?::(\\\\d{1,2})(?:\\\\.(\\\\d+))?)?(?:(Z)|([+-])(\\\\d{1,2})(?::(\\\\d{1,2}))?)?)?)?)?/;B.1D.kr=G(1y){1y=1y+\"\";if(H(1y)!=\"1n\"||1y.K===0){F O}u X=1y.3C(B.1D.ky);if(H(X)==\"L\"||X===O){F O}u 5W,7y,7x,9R,2a,9Q,7w;5W=3w(X[1],10);if(H(X[2])==\"L\"||X[2]===\"\"){F Y 3Q(5W)}7y=3w(X[2],10)-1;7x=3w(X[3],10);if(H(X[4])==\"L\"||X[4]===\"\"){F Y 3Q(5W,7y,7x)}9R=3w(X[4],10);2a=3w(X[5],10);9Q=(H(X[6])!=\"L\"&&X[6]!==\"\")?3w(X[6],10):0;if(H(X[7])!=\"L\"&&X[7]!==\"\"){7w=28.ha(c5*4M(\"0.\"+X[7]))}N{7w=0}if((H(X[8])==\"L\"||X[8]===\"\")&&(H(X[9])==\"L\"||X[9]===\"\")){F Y 3Q(5W,7y,7x,9R,2a,9Q,7w)}u 58;if(H(X[9])!=\"L\"&&X[9]!==\"\"){58=3w(X[10],10)*v5;if(H(X[11])!=\"L\"&&X[11]!==\"\"){58+=3w(X[11],10)*kw}if(X[9]==\"-\"){58=-58}}N{58=0}F Y 3Q(3Q.v4(5W,7y,7x,9R,2a,9Q,7w)-58)};B.1D.dY=G(2g,kx){if(H(2g)==\"L\"||2g===O){F O}u hh=2g.v3();u mm=2g.v2();u ss=2g.v1();u 1g=[((kx&&(hh<10))?\"0\"+hh:hh),((mm<10)?\"0\"+mm:mm),((ss<10)?\"0\"+ss:ss)];F 1g.2b(\":\")};B.1D.kq=G(2g,7v){if(H(2g)==\"L\"||2g===O){F O}u ku=7v?\"T\":\" \";u kt=7v?\"Z\":\"\";if(7v){2g=Y 3Q(2g.9P()+(2g.v0()*kw))}F B.1D.dX(2g)+ku+B.1D.dY(2g,7v)+kt};B.1D.dX=G(2g){if(H(2g)==\"L\"||2g===O){F O}u e4=B.1D.e3;F[2g.dZ(),e4(2g.e1()+1),e4(2g.e0())].2b(\"-\")};B.1D.kp=G(d){d=d+\"\";if(H(d)!=\"1n\"||d.K===0){F O}u a=d.2R(\"/\");F Y 3Q(a[2],a[0]-1,a[1])};B.1D.e3=G(n){F(n>9)?n:\"0\"+n};B.1D.ko=G(d){if(H(d)==\"L\"||d===O){F O}u e2=B.1D.e3;F[e2(d.e1()+1),e2(d.e0()),d.dZ()].2b(\"/\")};B.1D.kn=G(d){if(H(d)==\"L\"||d===O){F O}F[d.e1()+1,d.e0(),d.dZ()].2b(\"/\")};B.1D.1z=[\"ks\",\"kr\",\"dY\",\"kq\",\"dX\",\"kp\",\"ko\",\"kn\"];B.1D.1W=[];B.1D.2k={\":3e\":B.1D.1z,\":1p\":B.1D.1z};B.1D.2d=G(){u 2w=D.1r+\".\";R(u k in D){u o=D[k];if(H(o)==\"G\"&&H(o.1r)==\"L\"){1f{o.1r=2w+k}1e(e){}}}};B.1D.2d();if(H(B.J)!=\"L\"){B.J.2Y(D,B.1D)}N{(G(km,dW){if((H(1x)==\"L\"&&H(1q)==\"L\")||(H(B.3d)==\"5L\"&&B.3d)){u 1p=dW.2k[\":1p\"];R(u i=0;i<1p.K;i++){km[1p[i]]=dW[1p[i]]}}})(D,B.1D)}if(H(1q)!=\"L\"){1q.2X(\"B.1s\")}if(H(B)==\"L\"){B={}}if(H(B.1s)==\"L\"){B.1s={}}B.1s.1r=\"B.1s\";B.1s.1Y=\"1.3.1\";B.1s.1K=G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"};B.1s.1l=G(){F D.1K()};B.1s.ke=G(kl,kk,kj,ki,kh,dV,kg,9N,kf){F G(1P){1P=4M(1P);if(H(1P)==\"L\"||1P===O||k8(1P)){F kl}u 9L=kk;u 9K=kj;if(1P<0){1P=-1P}N{9L=9L.23(/-/,\"\")}u me=M.2U;u 9M=B.1s.dJ(ki);if(kh){1P=1P*3k;9K=9M.9y+9K}1P=B.1s.dK(1P,dV);u 9O=1P.2R(/\\\\./);u 3r=9O[0];u 3P=(9O.K==1)?\"\":9O[1];u X=\"\";1M(3r.K<kg){3r=\"0\"+3r}if(9N){1M(3r.K>9N){u i=3r.K-9N;X=9M.9A+3r.2W(i,3r.K)+X;3r=3r.2W(0,i)}}X=3r+X;if(dV>0){1M(3P.K<kf){3P=3P+\"0\"}X=X+9M.9z+3P}F 9L+X+9K}};B.1s.k5=G(9J,9H,9G){if(H(9H)==\"L\"){9H=\"\"}u 3q=9J.3C(/((?:[0#]+,)?[0#]+)(?:\\\\.([0#]+))?(%)?/);if(!3q){14 3p(\"uZ uY\")}u 7u=9J.3H(0,3q.c6);u kd=9J.3H(3q.c6+3q[0].K);if(7u.uX(/-/)==-1){7u=7u+\"-\"}u 9I=3q[1];u 3P=(H(3q[2])==\"1n\"&&3q[2]!=\"\")?3q[2]:\"\";u kc=(H(3q[3])==\"1n\"&&3q[3]!=\"\");u dU=9I.2R(/,/);u 9F;if(H(9G)==\"L\"){9G=\"dG\"}if(dU.K==1){9F=O}N{9F=dU[1].K}u ka=9I.K-9I.23(/0/g,\"\").K;u k9=3P.K-3P.23(/0/g,\"\").K;u kb=3P.K;u W=B.1s.ke(9H,7u,kd,9G,kc,kb,ka,9F,k9);u m=B.J;if(m){u fn=M.2U;u 3G=m.2o(M);W.U=G(){F[I.1r,\"(\",2r(m.U,3G).2b(\", \"),\")\"].2b(\"\")}}F W};B.1s.dJ=G(4g){if(H(4g)==\"L\"||4g===O){4g=\"dG\"}if(H(4g)==\"1n\"){u W=B.1s.5V[4g];if(H(W)==\"1n\"){W=M.2U(W);B.1s.5V[4g]=W}F W}N{F 4g}};B.1s.k4=G(dT,9E){if(9E){u X=dT/9E;if(!k8(X)){F B.1s.9B(dT/9E)}}F\"0\"};B.1s.9B=G(dS){u dR=(dS<0?\"-\":\"\");u s=28.8B(28.uW(dS)*3k).1l();if(s==\"0\"){F s}if(s.K<3){1M(s.3Z(s.K-1)==\"0\"){s=s.2W(0,s.K-1)}F dR+\"0.\"+s}u 5E=dR+s.2W(0,s.K-2);u 7t=s.2W(s.K-2,s.K);if(7t==\"uV\"){F 5E}N{if(7t.3Z(1)==\"0\"){F 5E+\".\"+7t.3Z(0)}N{F 5E+\".\"+7t}}};B.1s.dI=G(1y,dQ){1y=1y+\"\";if(H(1y)!=\"1n\"){F O}if(!dQ){F 1y.23(/^\\\\s+/,\"\")}N{F 1y.23(Y 8V(\"^[\"+dQ+\"]+\"),\"\")}};B.1s.dH=G(1y,dP){1y=1y+\"\";if(H(1y)!=\"1n\"){F O}if(!dP){F 1y.23(/\\\\s+$/,\"\")}N{F 1y.23(Y 8V(\"[\"+dP+\"]+$\"),\"\")}};B.1s.k2=G(1y,dO){u I=B.1s;F I.dH(I.dI(1y,dO),dO)};B.1s.dL=G(9D,9C){9D=28.8B(9D*28.dN(10,9C));u X=(9D*28.dN(10,-9C)).6I(9C);if(X.3Z(0)==\".\"){X=\"0\"+X}F X};B.1s.dK=G(k7,dM){F B.1s.dL(k7+0.5*28.dN(10,-dM),dM)};B.1s.k3=G(k6){F B.1s.9B(3k*k6)+\"%\"};B.1s.1z=[\"dL\",\"dK\",\"k5\",\"dJ\",\"k4\",\"9B\",\"k3\",\"dI\",\"dH\",\"k2\"];B.1s.5V={k1:{9A:\",\",9z:\".\",9y:\"%\"},uU:{9A:\".\",9z:\",\",9y:\"%\"},uT:{9A:\" \",9z:\",\",9y:\"%\"},\"dG\":\"k1\"};B.1s.1W=[];B.1s.2k={\":1p\":B.1s.1z,\":3e\":B.1s.1z};B.1s.2d=G(){u 2w=D.1r+\".\";u k,v,o;R(k in D.5V){o=D.5V[k];if(H(o)==\"3n\"){o.U=G(){F D.1r};o.1r=2w+\"5V.\"+k}}R(k in D){o=D[k];if(H(o)==\"G\"&&H(o.1r)==\"L\"){1f{o.1r=2w+k}1e(e){}}}};B.1s.2d();if(H(B.J)!=\"L\"){B.J.2Y(D,B.1s)}N{(G(k0,dF){if((H(1x)==\"L\"&&H(1q)==\"L\")||(H(B.3d)==\"5L\"&&B.3d)){u 1p=dF.2k[\":1p\"];R(u i=0;i<1p.K;i++){k0[1p[i]]=dF[1p[i]]}}})(D,B.1s)}if(H(1q)!=\"L\"){1q.2X(\"B.1k\");1q.2M(\"B.J\")}if(H(1x)!=\"L\"){1x.26(\"B.J\",[])}1f{if(H(B.J)==\"L\"){14\"\"}}1e(e){14\"B.1k 3F on B.J!\"}if(H(B.1k)==\"L\"){B.1k={}}B.1k.1r=\"B.1k\";B.1k.1Y=\"1.3.1\";B.1k.1K=G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"};B.1k.1l=G(){F D.1K()};B.1k.2t=G(jZ){D.55=[];D.id=D.7n();D.2H=-1;D.54=0;D.53=[O,O];D.7m=jZ;D.7l=1m;D.7r=1m};B.1k.2t.1U={U:G(){u 7s;if(D.2H==-1){7s=\"uS\"}N{if(D.2H===0){7s=\"uR\"}N{7s=\"dE\"}}F\"2t(\"+D.id+\", \"+7s+\")\"},1l:B.J.24(\"U\"),7n:B.J.4f(),jY:G(){u I=B.1k;if(D.2H==-1){if(D.7m){D.7m(D)}N{D.7l=1h}if(D.2H==-1){D.52(Y I.di(D))}}N{if((D.2H===0)&&(D.53[0]2C I.2t)){D.53[0].jY()}}},jQ:G(){D.54++},jX:G(){D.54--;if((D.54===0)&&(D.2H>=0)){D.9u()}},jR:G(X){D.9x(X);D.jX()},9x:G(X){D.2H=((X 2C 2x)?1:0);D.53[D.2H]=X;D.9u()},dD:G(){if(D.2H!=-1){if(!D.7l){14 Y B.1k.dj(D)}D.7l=1m;F}},3o:G(X){D.dD();if(X 2C B.1k.2t){14 Y 2x(\"2t jW 9v aB be 7r if jV jU jT jS of a 3o\")}D.9x(X)},52:G(X){D.dD();u I=B.1k;if(X 2C I.2t){14 Y 2x(\"2t jW 9v aB be 7r if jV jU jT jS of a 3o\")}if(!(X 2C 2x)){X=Y I.9p(X)}D.9x(X)},jP:G(fn){if(M.K>1){fn=B.J.2z.1w(O,M)}F D.9w(fn,fn)},5Q:G(fn){if(M.K>1){fn=B.J.2z.1w(O,M)}F D.9w(fn,O)},jA:G(fn){if(M.K>1){fn=B.J.2z.1w(O,M)}F D.9w(O,fn)},9w:G(cb,eb){if(D.7r){14 Y 2x(\"uQ uP 9v 2E be re-uO\")}D.55.1c([cb,eb]);if(D.2H>=0){D.9u()}F D},9u:G(){u dC=D.55;u 56=D.2H;u X=D.53[56];u I=D;u cb=O;1M(dC.K>0&&D.54===0){u 2n=dC.2P();u f=2n[56];if(f===O){2V}1f{X=f(X);56=((X 2C 2x)?1:0);if(X 2C B.1k.2t){cb=G(X){I.jR(X)};D.jQ()}}1e(3O){56=1;if(!(3O 2C 2x)){3O=Y B.1k.9p(3O)}X=3O}}D.2H=56;D.53[56]=X;if(cb&&D.54){X.jP(cb);X.7r=1h}}};B.J.2l(B.1k,{dk:G(){F dB(\"(\"+M[0].jN+\")\")},dp:G(uN){u d=Y B.1k.2t();d.3o.1w(d,M);F d},9q:G(uM){u d=Y B.1k.2t();d.52.1w(d,M);F d},do:G(){u I=M.2U;if(!I.7q){u dy=[G(){F Y 7q()},G(){F Y dA(\"jO.dz\")},G(){F Y dA(\"uL.dz\")},G(){F Y dA(\"jO.dz.4.0\")},G(){14 Y B.1k.dh(\"uK uJ 2E uI 7q\")}];R(u i=0;i<dy.K;i++){u 1A=dy[i];1f{I.7q=1A;F 1A()}1e(e){}}}F I.7q()},dx:G(){},jK:G(d){if(D.uH==4){1f{D.5T=O}1e(e){1f{D.5T=B.1k.dx}1e(e){}}u 5U=O;1f{5U=D.jm;if(!5U&&B.J.7e(D.jN)){5U=jM}}1e(e){}if(5U==hQ||5U==jM){d.3o(D)}N{u 3O=Y B.1k.dg(D,\"uG uF\");if(3O.2y){d.52(3O)}N{d.52(3O)}}}},jL:G(2s){1f{2s.5T=O}1e(e){1f{2s.5T=B.1k.dx}1e(e){}}2s.uE()},dl:G(2s,7p){if(H(7p)==\"L\"||7p===O){7p=\"\"}u m=B.J;u I=B.1k;u d=Y I.2t(m.2z(I.jL,2s));1f{2s.5T=m.1O(I.jK,2s,d);2s.uD(7p)}1e(e){1f{2s.5T=O}1e(uC){}d.52(e)}F d},dn:G(5F){u I=B.1k;u 2s=I.do();if(M.K>1){u m=B.J;u qs=m.dw.1w(O,m.1R(O,M,1));if(qs){5F+=\"?\"+qs}}2s.cp(\"uB\",5F,1h);F I.dl(2s)},jv:G(5F){u I=B.1k;u d=I.dn.1w(I,M);d=d.5Q(I.dk);F d},dm:G(jJ,dv){u d=Y B.1k.2t();u m=B.J;if(H(dv)!=\"L\"){d.5Q(G(){F dv})}u jI=uA(m.1O(\"3o\",d),28.8B(jJ*c5));d.7m=G(){1f{uz(jI)}1e(e){}};F d},ju:G(jH,1A){u m=B.J;u jG=m.2z.1w(m,m.1R(O,M,1));F B.1k.dm(jH).5Q(G(X){F jG()})}});B.1k.5O=G(){D.5S=[];D.4e=1m;D.id=D.7n()};B.1k.5O.1U={bX:B.1k.5O,uy:G(){d=Y B.1k.2t();if(D.4e){D.5S.1c(d)}N{D.4e=1h;d.3o(D)}F d},jF:G(){if(!D.4e){14 3p(\"ux to jF an jE 5O\")}D.4e=1m;if(D.5S.K>0){D.4e=1h;D.5S.2P().3o(D)}},7n:B.J.4f(),U:G(){u 9t;if(D.4e){9t=\"4e, \"+D.5S.K+\" 5S\"}N{9t=\"jE\"}F\"5O(\"+D.id+\", \"+9t+\")\"},1l:B.J.24(\"U\")};B.1k.7i=G(2G,du,jC,jB,jD){D.2G=2G;D.9r=Y 7o(D.2G.K);D.55=[];D.id=D.7n();D.2H=-1;D.54=0;D.53=[O,O];D.7m=jD;D.7l=1m;if(D.2G.K===0&&!du){D.3o(D.9r)}D.dr=0;D.jz=du;D.jy=jC;D.jx=jB;u 9s=0;B.J.2r(B.J.1O(G(d){d.5Q(B.J.1O(D.dt,D),9s,1h);d.jA(B.J.1O(D.dt,D),9s,1m);9s+=1},D),D.2G)};B.J.2l(B.1k.7i.1U,B.1k.2t.1U);B.J.2l(B.1k.7i.1U,{dt:G(ds,7k,5R){D.9r[ds]=[7k,5R];D.dr+=1;if(D.2H!==0){if(7k&&D.jz){D.3o([ds,5R])}N{if(!7k&&D.jy){D.52(5R)}N{if(D.dr==D.2G.K){D.3o(D.9r)}}}}if(!7k&&D.jx){5R=O}F 5R}});B.1k.jt=G(jw){u d=Y B.1k.7i(jw,1m,1h,1m);d.5Q(G(dq){u 7j=[];R(u i=0;i<dq.K;i++){7j.1c(dq[i][1])}F 7j});F d};B.1k.jr=G(1A){u I=B.1k;u 5P;1f{u r=1A.1w(O,B.J.1R([],M,1));if(r 2C I.2t){5P=r}N{if(r 2C 2x){5P=I.9q(r)}N{5P=I.dp(r)}}}1e(e){5P=I.9q(e)}F 5P};B.1k.1z=[\"dj\",\"di\",\"dh\",\"9p\",\"dg\",\"2t\",\"dp\",\"9q\",\"do\",\"dn\",\"jv\",\"dm\",\"ju\",\"dl\",\"5O\",\"7i\",\"jt\",\"jr\"];B.1k.1W=[\"dk\"];B.1k.2d=G(){u m=B.J;u ne=m.2z(m.jq,D);ne(\"dj\",G(jp){D.jo=jp});ne(\"di\",G(jn){D.jo=jn});ne(\"dh\",G(1t){D.43=1t});ne(\"9p\",G(1t){D.43=1t});ne(\"dg\",G(2s,1t){D.2s=2s;D.43=1t;1f{D.2y=2s.jm}1e(e){}});D.2k={\":3e\":D.1z,\":1p\":m.2o(D.1z,D.1W)};m.3f(D)};B.1k.2d();B.J.2Y(D,B.1k);if(H(1q)!=\"L\"){1q.2X(\"B.S\");1q.2M(\"B.15\")}if(H(1x)!=\"L\"){1x.26(\"B.15\",[])}1f{if(H(B.15)==\"L\"){14\"\"}}1e(e){14\"B.S 3F on B.15!\"}if(H(B.S)==\"L\"){B.S={}}B.S.1r=\"B.S\";B.S.1Y=\"1.3.1\";B.S.1K=G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"};B.S.1l=G(){F D.1K()};B.S.1z=[\"d5\",\"cr\",\"b9\",\"95\",\"94\",\"j3\",\"9k\",\"cX\",\"cw\",\"iT\",\"iV\",\"4X\",\"9j\",\"iQ\",\"hS\",\"cs\",\"ia\",\"i9\",\"i8\",\"i7\",\"i6\",\"i5\",\"i4\",\"hV\",\"i3\",\"i2\",\"i1\",\"cu\",\"hW\",\"ct\",\"i0\",\"hZ\",\"hY\",\"hX\",\"P\",\"io\",\"il\",\"ik\",\"ij\",\"cm\",\"ih\",\"ii\",\"ig\",\"ie\",\"ic\",\"cv\",\"8d\",\"A\",\"6m\",\"ib\",\"1E\",\"$\",\"4q\",\"aH\",\"cO\",\"cN\",\"iM\",\"5G\",\"iK\",\"9d\",\"9e\",\"iH\",\"iD\",\"9c\",\"iB\",\"cG\",\"97\",\"hU\",\"hT\",\"iw\",\"jh\",\"jb\",\"j6\",\"j5\",\"jk\",\"jl\"];B.S.1W=[\"9b\"];B.S.5N=G(w,h){D.w=w;D.h=h};B.S.5N.1U.U=G(){u U=B.J.U;F\"{w: \"+U(D.w)+\", h: \"+U(D.h)+\"}\"};B.S.5t=G(x,y){D.x=x;D.y=y};B.S.5t.1U.U=G(){u U=B.J.U;F\"{x: \"+U(D.x)+\", y: \"+U(D.y)+\"}\"};B.S.5t.1U.1l=G(){F D.U()};B.J.2l(B.S,{jl:G(Q,o){Q=B.S.1E(Q);B.S.4X(Q,{\"1T\":{\"9o\":o,\"-hL-9o\":o,\"-uw-9o\":o,\"47\":\" uv(9o=\"+(o*3k)+\")\"}})},jk:G(){u d=Y B.S.5N();u w=B.S.3X;u b=B.S.1Z.5s;if(w.jj){d.w=w.jj;d.h=w.uu}N{if(b.dd.9n){d.w=b.dd.9n;d.h=b.dd.ji}N{if(b&&b.9n){d.w=b.9n;d.h=b.ji}}}F d},jh:G(Q){u I=B.S;if(H(Q.w)==\"2y\"||H(Q.h)==\"2y\"){F Y I.5N(Q.w||0,Q.h||0)}Q=I.1E(Q);if(!Q){F L}if(I.4q(Q,\"3u\")!=\"98\"){F Y I.5N(Q.jg||0,Q.ci||0)}u s=Q.1T;u je=s.dc;u jf=s.6P;s.dc=\"fR\";s.6P=\"j8\";s.3u=\"\";u jd=Q.jg;u jc=Q.ci;s.3u=\"98\";s.6P=jf;s.dc=je;F Y I.5N(jd,jc)},jb:G(Q,4Z){u I=B.S;Q=I.1E(Q);if(!Q){F L}u c=Y I.5t(0,0);if(Q.x&&Q.y){c.x+=Q.x||0;c.y+=Q.y||0;F c}N{if(Q.3t===O||I.4q(Q,\"3u\")==\"98\"){F L}}u 51=O;u 2j=O;u d=B.S.1Z;u de=d.7Z;u b=d.5s;if(Q.ja){51=Q.ja();c.x+=51.2I+(de.6y||b.6y)-(de.8q||b.8q);c.y+=51.3D+(de.4C||b.4C)-(de.8p||b.8p)}N{if(d.j9){51=d.j9(Q);c.x+=51.x;c.y+=51.y}N{if(Q.8g){c.x+=Q.db;c.y+=Q.da;2j=Q.8g;if(2j!=Q){1M(2j){c.x+=2j.db;c.y+=2j.da;2j=2j.8g}}u ua=ut.us.8G();if((H(7h)!=\"L\"&&4M(7h.ur())<9)||(ua.2A(\"uq\")!=-1&&I.4q(Q,\"6P\")==\"j8\")){c.x-=b.db;c.y-=b.da}}}}if(H(4Z)!=\"L\"){4Z=M.2U(4Z);if(4Z){c.x-=(4Z.x||0);c.y-=(4Z.y||0)}}if(Q.3t){2j=Q.3t}N{2j=O}1M(2j&&2j.j7!=\"uo\"&&2j.j7!=\"co\"){c.x-=2j.6y;c.y-=2j.4C;if(2j.3t){2j=2j.3t}N{2j=O}}F c},j6:G(Q,d9,7g){Q=B.S.1E(Q);if(H(7g)==\"L\"){7g=\"px\"}B.S.4X(Q,{\"1T\":{\"5A\":d9.w+7g,\"3V\":d9.h+7g}})},j5:G(Q,d8,7f){Q=B.S.1E(Q);if(H(7f)==\"L\"){7f=\"px\"}B.S.4X(Q,{\"1T\":{\"2I\":d8.x+7f,\"3D\":d8.y+7f}})},cr:G(){F B.S.3X},b9:G(){F B.S.1Z},95:G(2m,1A){u I=B.S;u d6=I.1Z;u d7=I.un;u W;1f{I.3X=2m;I.1Z=2m.2v;W=1A()}1e(e){I.3X=d7;I.1Z=d6;14 e}I.3X=d7;I.1Z=d6;F W},d5:G(Q){u 7d=[];u 7c=[];u m=B.J;u I=B.S;if(H(Q)==\"L\"||Q===O){Q=I.1Z}N{Q=I.1E(Q)}m.d4(Q,G(Q){u 1b=Q.1b;if(m.7e(1b)){u 4Y=Q.cD;if(4Y==\"cv\"&&(Q.1J==\"um\"||Q.1J==\"uk\")&&!Q.ip){F O}if(4Y==\"ct\"){if(Q.j4>=0){u 9m=Q.1S[Q.j4];7d.1c(1b);7c.1c((9m.3m)?9m.3m:9m.7X);F O}7d.1c(1b);7c.1c(\"\");F O}if(4Y==\"cu\"||4Y==\"P\"||4Y==\"8d\"||4Y==\"6m\"){F Q.5h}7d.1c(1b);7c.1c(Q.3m||\"\");F O}F Q.5h});F[7d,7c]},94:G(1N,1A){u I=B.S;u d3=I.1Z;u W;1f{I.1Z=1N;W=1A()}1e(e){I.1Z=d3;14 e}I.1Z=d3;F W},j3:G(1b,j2,3y,j1){B.S.9b.5M(1b,j2,3y,j1)},9k:G(1j,7a){u im=B.15;u I=B.S;u 1Q=im.1Q;u iY=im.7b;u 4c=im.4c;u iX=I.9b;u iZ=I.9k;u iW=B.J.4d;1M(1h){if(H(1j)==\"L\"||1j===O){F O}if(H(1j.3T)!=\"L\"&&1j.3T>0){F 1j}if(H(1j)==\"2y\"||H(1j)==\"5L\"){1j=1j.1l()}if(H(1j)==\"1n\"){F I.1Z.4S(1j)}if(H(1j.j0)==\"G\"){1j=1j.j0(7a);2V}if(H(1j)==\"G\"){1j=1j(7a);2V}u 9l=O;1f{9l=1Q(1j)}1e(e){}if(9l){F 4c(iZ,9l,iY(7a))}1f{1j=iX.3C(1j,7a);2V}1e(e){if(e!=iW){14 e}}F I.1Z.4S(1j.1l())}F L},iV:G(1j,79,iU){u o={};o[79]=iU;1f{F B.S.4X(1j,o)}1e(e){}F O},iT:G(1j,79){u I=B.S;u d2=I.4U.99[79];1j=I.1E(1j);1f{if(d2){F 1j[d2]}F 1j.fm(79)}1e(e){}F O},4X:G(1j,5K){u Q=1j;u I=B.S;if(H(1j)==\"1n\"){Q=I.1E(1j)}if(5K){u d0=B.J.8Z;if(I.4U.6X){R(u k in 5K){u v=5K[k];if(H(v)==\"3n\"&&H(Q[k])==\"3n\"){d0(Q[k],v)}N{if(k.2W(0,2)==\"on\"){if(H(v)==\"1n\"){v=Y cZ(v)}Q[k]=v}N{Q.4p(k,v)}}}}N{u iS=I.4U.99;R(k in 5K){v=5K[k];u d1=iS[k];if(k==\"1T\"&&H(v)==\"1n\"){Q.1T.3x=v}N{if(H(d1)==\"1n\"){Q[d1]=v}N{if(H(Q[k])==\"3n\"&&H(v)==\"3n\"){d0(Q[k],v)}N{if(k.2W(0,2)==\"on\"){if(H(v)==\"1n\"){v=Y cZ(v)}Q[k]=v}N{Q.4p(k,v)}}}}}}}F Q},9j:G(1j){u Q=1j;u I=B.S;if(H(1j)==\"1n\"){Q=I.1E(1j)}u 78=[I.9k(B.J.1R(O,M,1),Q)];u iR=B.J.2o;1M(78.K){u n=78.2P();if(H(n)==\"L\"||n===O){}N{if(H(n.3T)==\"2y\"){Q.2c(n)}N{78=iR(n,78)}}}F Q},iQ:G(1j){u Q=1j;u I=B.S;if(H(1j)==\"1n\"){Q=I.1E(1j);M[0]=Q}u cY;1M((cY=Q.6n)){Q.6S(cY)}if(M.K<2){F Q}N{F I.9j.1w(D,M)}},cX:G(1b,4b){u Q;u I=B.S;u m=B.J;if(H(4b)==\"1n\"||H(4b)==\"2y\"){u 3G=m.1R([1b,O],M,1);F M.2U.1w(D,3G)}if(H(1b)==\"1n\"){if(4b&&\"1b\"in 4b&&!I.4U.6X){1b=(\"<\"+1b+\" 1b=\\\\\"\"+I.9c(4b.1b)+\"\\\\\">\")}Q=I.1Z.2S(1b)}N{Q=1b}if(4b){I.4X(Q,4b)}if(M.K<=2){F Q}N{u 3G=m.1R([Q],M,2);F I.9j.1w(D,3G)}},cw:G(){u m=B.J;F m.2z.1w(D,m.1R([B.S.cX],M))},cs:G(5J,1d){u I=B.S;5J=I.1E(5J);u cW=5J.3t;if(1d){1d=I.1E(1d);cW.uj(1d,5J)}N{cW.6S(5J)}F 1d},1E:G(id){u I=B.S;if(M.K==1){F((H(id)==\"1n\")?I.1Z.hN(id):id)}N{F B.J.2r(I.1E,M)}},4q:G(iP,cV,cU){if(M.K==2){cU=cV}u I=B.S;u el=I.1E(iP);u 77=I.1Z;if(!el||el==77){F L}if(el.iO){F el.iO[cV]}if(H(77.5k)==\"L\"){F L}if(77.5k===O){F L}u 9i=77.5k.g4(el,O);if(H(9i)==\"L\"||9i===O){F L}F 9i.6q(cU)},aH:G(76,9g,4W){u I=B.S;if(H(76)==\"L\"||76===O){76=\"*\"}if(H(4W)==\"L\"||4W===O){4W=I.1Z}4W=I.1E(4W);u 9h=(4W.fr(76)||I.1Z.1p);if(H(9g)==\"L\"||9g===O){F B.J.1R(O,9h)}u cR=[];R(u i=0;i<9h.K;i++){u cS=9h[i];u cT=cS.3M.2R(\" \");R(u j=0;j<cT.K;j++){if(cT[j]==9g){cR.1c(cS);2K}}}F cR},iN:G(5I,9f){u W=G(){u cQ=M.2U.5H;R(u i=0;i<cQ.K;i++){if(cQ[i].1w(D,M)===1m){2K}}if(9f){1f{D[5I]=O}1e(e){}}};W.5H=[];F W},cO:G(cP,5I,1A,9f){u I=B.S;u 4V=cP[5I];u 75=4V;if(!(H(4V)==\"G\"&&H(4V.5H)==\"3n\"&&4V.5H!==O)){75=I.iN(5I,9f);if(H(4V)==\"G\"){75.5H.1c(4V)}cP[5I]=75}75.5H.1c(1A)},cN:G(1A){u I=B.S;I.cO(I.3X,\"gh\",1A,1h)},iM:G(74){u I=B.S;I.cN(G(){74=I.1E(74);if(74){74.ui()}})},5G:G(iL,cM){u I=B.S;u 1i=I.1E(iL);if(I.4U.6X){1i.4p(\"iq\",cM)}N{1i.4p(\"3M\",cM)}},iK:G(cL){u I=B.S;R(u i=1;i<M.K;i++){u 1i=I.1E(M[i]);if(!I.9d(1i,cL)){I.9e(1i,cL)}}},9d:G(iJ,73){u I=B.S;u 1i=I.1E(iJ);u 2F=1i.3M;if(2F.K===0){I.5G(1i,73);F 1h}if(2F==73){F 1m}u cK=1i.3M.2R(\" \");R(u i=0;i<cK.K;i++){if(cK[i]==73){F 1m}}I.5G(1i,2F+\" \"+73);F 1h},9e:G(iI,cJ){u I=B.S;u 1i=I.1E(iI);u 2F=1i.3M;if(2F.K===0){F 1m}if(2F==cJ){I.5G(1i,\"\");F 1h}u 72=1i.3M.2R(\" \");R(u i=0;i<72.K;i++){if(72[i]==cJ){72.4y(i,1);I.5G(1i,72.2b(\" \"));F 1h}}F 1m},iH:G(iG,iF,iE){u 1i=B.S.1E(iG);u X=B.S.9e(1i,iF);if(X){B.S.9d(1i,iE)}F X},iD:G(iC,uh){u 1i=B.S.1E(iC);u cI=1i.3M.2R(\" \");R(u i=1;i<M.K;i++){u cH=1m;R(u j=0;j<cI.K;j++){if(cI[j]==M[i]){cH=1h;2K}}if(!cH){F 1m}}F 1h},9c:G(s){F s.23(/&/g,\"&ug;\").23(/\"/g,\"&uf;\").23(/</g,\"&lt;\").23(/>/g,\"&gt;\")},iB:G(2q){F B.S.cG(2q).2b(\"\")},cG:G(2q,1g){if(H(1g)==\"L\"||1g===O){1g=[]}u 70=[2q];u I=B.S;u cB=I.9c;u iA=I.4U;1M(70.K){2q=70.hP();if(H(2q)==\"1n\"){1g.1c(2q)}N{if(2q.3T==1){1g.1c(\"<\"+2q.cD.8G());u 71=[];u cF=iA(2q);R(u i=0;i<cF.K;i++){u a=cF[i];71.1c([\" \",a.1b,\"=\\\\\"\",cB(a.3m),\"\\\\\"\"])}71.iz();R(i=0;i<71.K;i++){u cE=71[i];R(u j=0;j<cE.K;j++){1g.1c(cE[j])}}if(2q.ue()){1g.1c(\">\");70.1c(\"</\"+2q.cD.8G()+\">\");u cC=2q.5h;R(i=cC.K-1;i>=0;i--){70.1c(cC[i])}}N{1g.1c(\"/>\")}}N{if(2q.3T==3){1g.1c(cB(2q.iv))}}}}F 1g},97:G(ix,cA){u m=B.J;u iy=m.1R(O,M,1);B.15.9a(m.47(O,m.2r(B.S.1E,iy)),G(cA){cA.1T.3u=ix})},iw:G(1j,iu){u W=[];(G(1j){u cn=1j.5h;if(cn){R(u i=0;i<cn.K;i++){M.2U.cz(D,cn[i])}}u cy=1j.iv;if(H(cy)==\"1n\"){W.1c(cy)}})(B.S.1E(1j));if(iu){F W}N{F W.2b(\"\")}},2d:G(2m){u m=B.J;D.1Z=2v;D.3X=2m;D.9b=Y m.4a();u 6Z=D.1Z.2S(\"cj\");u 2T;if(6Z&&6Z.6Y&&6Z.6Y.K>0){u it=m.47;2T=G(1j){F it(2T.ir,1j.6Y)};2T.cx={};B.15.9a(6Z.6Y,G(a){2T.cx[a.1b]=a.3m});2T.ir=G(a){F(2T.cx[a.1b]!=a.3m)};2T.6X=1m;2T.99={\"iq\":\"3M\",\"ip\":\"ud\",\"uc\":\"ub\",\"R\":\"u9\"}}N{2T=G(1j){F 1j.6Y};2T.6X=1h;2T.99={}}D.4U=2T;u 1C=D.cw;D.io=1C(\"ul\");D.il=1C(\"ol\");D.ik=1C(\"li\");D.ij=1C(\"td\");D.cm=1C(\"tr\");D.ii=1C(\"u8\");D.ih=1C(\"u7\");D.ig=1C(\"u6\");D.ie=1C(\"u5\");D.ic=1C(\"th\");D.cv=1C(\"ck\");D.8d=1C(\"cj\");D.A=1C(\"a\");D.6m=1C(\"4u\");D.ib=1C(\"u4\");D.ia=1C(\"2e\");D.i9=1C(\"tt\");D.i8=1C(\"4O\");D.i7=1C(\"h1\");D.i6=1C(\"h2\");D.i5=1C(\"h3\");D.i4=1C(\"br\");D.i3=1C(\"hr\");D.i2=1C(\"u3\");D.i1=1C(\"u2\");D.cu=1C(\"u1\");D.P=1C(\"p\");D.ct=1C(\"u0\");D.i0=1C(\"hJ\");D.hZ=1C(\"tZ\");D.hY=1C(\"tY\");D.hX=1C(\"tX\");D.hW=1C(\"tW\");D.hV=1C(\"tV\");D.hU=m.2z(D.97,\"98\");D.hT=m.2z(D.97,\"8c\");D.hS=D.cs;D.$=D.1E;D.2k={\":3e\":D.1z,\":1p\":m.2o(D.1z,D.1W)};m.3f(D)}});B.S.2d(((H(2O)==\"L\")?D:2O));if(!B.3d){95=B.S.95;94=B.S.94}B.J.2Y(D,B.S);if(H(1q)!=\"L\"){1q.2X(\"B.1I\");1q.2M(\"B.1H\");1q.2M(\"B.J\")}if(H(1x)!=\"L\"){1x.26(\"B.1H\",[]);1x.26(\"B.J\",[])}1f{if(H(B.J)==\"L\"||H(B.1H)==\"L\"){14\"\"}}1e(e){14\"B.1I 3F on B.J 3W B.1H!\"}if(H(B.1I)==\"L\"){B.1I={}}B.1I.1r=\"B.1I\";B.1I.1Y=\"1.3.1\";B.1I.1K=G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"};B.1I.1l=G(){F D.1K()};B.1I.bY=G(6W){u m=B.1I;6W=!(!6W);if(m.3l&&m.3l.8Q!=6W){m.3l.hA();m.3l=O}if(!m.3l||m.3l.8P){m.3l=Y m.1I(6W,B.1H.2L)}F m.3l};B.1I.1I=G(4R,6V){if(H(6V)==\"L\"||6V===O){6V=B.1H.2L}D.2L=6V;u tU=B.J.2l;u c3=B.J.8Z;u 1O=B.J.1O;u hM=B.J.4L;u 2m=2O;u 6U=\"tT\";if(H(B.S)!=\"L\"){2m=B.S.cr()}if(!4R){u 5F=2m.tS.tR.2R(\"?\")[0].23(/[:\\\\/.><&]/g,\"hR\");u 1b=6U+\"hR\"+5F;u 5D=2m.cp(\"\",1b,\"tQ,tP,3V=hQ\");if(!5D){cq(\"tO tN to cp tM 2O tL to hP-up tK.\");F L}5D.2v.fl(\"<!tJ co tI \\\\\"-//tH//tG co 4.0 tF//tE\\\\\" \"+\"\\\\\"fq://fp.tD.fo/cm/tC/tB.tA\\\\\">\"+\"<hO><5E><8Y>[B.1I]</8Y></5E>\"+\"<5s></5s></hO>\");5D.2v.hG();5D.2v.8Y+=\" \"+2m.2v.8Y;2m=5D}u 1N=2m.2v;D.1N=1N;u 21=1N.hN(6U);u c4=!!21;if(21&&H(21.5B)!=\"L\"){21.5B.2L=D.2L;21.5B.6K();F 21.5B}if(c4){u cl;1M((cl=21.6n)){21.6S(cl)}}N{21=1N.2S(\"4u\");21.id=6U}21.5B=D;u 8T=1N.2S(\"ck\");u 8S=1N.2S(\"ck\");u 6O=1N.2S(\"2e\");u 6N=1N.2S(\"2e\");u 6M=1N.2S(\"2e\");u 6L=1N.2S(\"2e\");u 3L=1N.2S(\"4u\");u 42=1N.2S(\"4u\");u 8U=6U+\"tz\";D.8N=hM(D.8N);u 4T=[];u 6R=O;u cf=G(1t){u 6T=1t.3N;if(H(6T)==\"2y\"){6T=B.1H.5C[6T]}F 6T};u cd=G(1t){F 1t.3z.2b(\" \")};u ca=1O(G(1t){u 8W=cf(1t);u 7X=cd(1t);u c=D.8N[8W];u p=1N.2S(\"cj\");p.3M=\"B-49 B-5C-\"+8W;p.1T.3x=\"ty: 2N; 4F-8X: -hL-4O-3y; 4F-8X: -o-4O-3y; 4F-8X: 4O-3y; 4F-8X: 4O-tx; hK-3y: 2K-hK; 3y-hJ: tw; 3U: \"+c;p.2c(1N.4S(8W+\": \"+7X));42.2c(p);42.2c(1N.2S(\"br\"));if(3L.ci>3L.hI){3L.4C=0}N{3L.4C=3L.hI}},D);u hD=G(1t){4T[4T.K]=1t;ca(1t)};u hF=G(){u cg,ce;1f{cg=Y 8V(8T.3m);ce=Y 8V(8S.3m)}1e(e){ch(\"2x in 47 tv: \"+e.43);F O}F G(1t){F(cg.hH(cf(1t))&&ce.hH(cd(1t)))}};u cc=G(){1M(42.6n){42.6S(42.6n)}};u hB=G(){4T=[];cc()};u bZ=1O(G(){if(D.8P){F}D.8P=1h;if(B.1I.3l==D){B.1I.3l=O}D.2L.c9(8U);21.5B=O;if(4R){21.3t.6S(21)}N{D.2m.hG()}},D);u c7=G(){cc();R(u i=0;i<4T.K;i++){u 1t=4T[i];if(6R===O||6R(1t)){ca(1t)}}};D.6K=G(){6R=hF();c7();D.2L.c9(8U);D.2L.hE(8U,6R,hD)};u c0=1O(G(){4T=D.2L.c8();c7()},D);u c2=1O(G(6Q){6Q=6Q||2O.6D;2h=6Q.6w||6Q.8t;if(2h==13){D.6K()}},D);u 31=\"3u: 8c; z-c6: c5; 2I: 2N; 6f: 2N; 6P: tu; 5A: 3k%; he-3U: 4F; c1: \"+D.8O;if(4R){31+=\"; 3V: ts; 3E-3D: fO 8a 8y\"}N{31+=\"; 3V: 3k%;\"}21.1T.3x=31;if(!c4){1N.5s.2c(21)}31={\"3x\":\"5A: 33%; 3u: 8Q; c1: \"+D.8O};c3(8T,{\"3m\":\"8L|8M|8K|8J|8I\",\"hC\":c2,\"1T\":31});21.2c(8T);c3(8S,{\"3m\":\".*\",\"hC\":c2,\"1T\":31});21.2c(8S);31=\"5A: 8%; 3u:8Q; c1: \"+D.8O;6O.2c(1N.4S(\"tq\"));6O.8R=1O(\"6K\",D);6O.1T.3x=31;21.2c(6O);6N.2c(1N.4S(\"tp\"));6N.8R=c0;6N.1T.3x=31;21.2c(6N);6M.2c(1N.4S(\"tn\"));6M.8R=hB;6M.1T.3x=31;21.2c(6M);6L.2c(1N.4S(\"tm\"));6L.8R=bZ;6L.1T.3x=31;21.2c(6L);3L.1T.3x=\"fS: tk; 5A: 3k%\";42.1T.3x=\"5A: 3k%; 3V: \"+(4R?\"tj\":\"3k%\");3L.2c(42);21.2c(3L);D.6K();c0();if(4R){D.2m=L}N{D.2m=2m}D.8Q=4R;D.hA=bZ;D.8P=1m;F D};B.1I.1I.1U={\"8O\":\"ti tg,tf-te\",\"8N\":{\"8M\":\"1v\",\"8L\":\"gU\",\"8K\":\"1F\",\"8J\":\"8y\",\"8I\":\"bx\"}};B.1I.1W=[\"1I\"];B.1I.1z=[\"bY\"];B.1I.2d=G(){D.2k={\":3e\":D.1z,\":1p\":B.J.2o(D.1z,D.1W)};B.J.3f(D);B.1I.3l=O};B.1I.2d();B.J.2Y(D,B.1I);if(H(1q)!=\"L\"){1q.2X(\"B.V\");1q.2M(\"B.J\")}if(H(1x)!=\"L\"){1x.26(\"B.J\",[])}1f{if(H(B.J)==\"L\"){14\"\"}}1e(e){14\"B.V 3F on B.J\"}if(H(B.V)==\"L\"){B.V={}}B.V.1r=\"B.V\";B.V.1Y=\"1.3.1\";B.V.1K=G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"};B.V.1l=G(){F D.1K()};B.V.V=G(1v,hz,1F,6J){if(H(6J)==\"L\"||6J===O){6J=1}D.1B={r:1v,g:hz,b:1F,a:6J}};B.V.V.1U={bX:B.V.V,tc:G(hy){u 1B=D.1B;u m=B.V;F m.V.3Y(1B.r,1B.g,1B.b,hy)},tb:G(1o){u 1G=D.41();1G.h=1o;u m=B.V;F m.V.4H(1G)},ta:G(hx){u 1G=D.41();1G.s=hx;u m=B.V;F m.V.4H(1G)},t9:G(hw){u 1G=D.41();1G.l=hw;u m=B.V;F m.V.4H(1G)},t8:G(hv){u 1G=D.41();1G.l=28.29(1G.l-hv,0);u m=B.V;F m.V.4H(1G)},t7:G(hu){u 1G=D.41();1G.l=28.2a(1G.l+hu,1);u m=B.V;F m.V.4H(1G)},fJ:G(ht,5z){if(H(5z)==\"L\"||5z===O){5z=0.5}u sf=1-5z;u s=D.1B;u d=ht.1B;u df=5z;F B.V.V.3Y((s.r*sf)+(d.r*df),(s.g*sf)+(d.g*df),(s.b*sf)+(d.b*df),(s.a*sf)+(d.a*df))},h4:G(hs){u a=D.6r();u b=hs.6r();F B.J.2f([a.r,a.g,a.b,a.a],[b.r,b.g,b.b,b.a])},hq:G(){F D.41().b>0.5},t6:G(){F(!D.hq())},t5:G(){u c=D.41();u 2Z=B.V.6F;u W=D.ho;if(!W){u 5y=(2Z(c.h,bF).6I(0)+\",\"+2Z(c.s,3k).hp(4)+\"%\"+\",\"+2Z(c.l,3k).hp(4)+\"%\");u a=c.a;if(a>=1){a=1;W=\"1G(\"+5y+\")\"}N{if(a<=0){a=0}W=\"t4(\"+5y+\",\"+a+\")\"}D.ho=W}F W},hl:G(){u c=D.1B;u 2Z=B.V.6F;u W=D.hn;if(!W){u 5y=(2Z(c.r,3h).6I(0)+\",\"+2Z(c.g,3h).6I(0)+\",\"+2Z(c.b,3h).6I(0));if(c.a!=1){W=\"t3(\"+5y+\",\"+c.a+\")\"}N{W=\"1B(\"+5y+\")\"}D.hn=W}F W},6r:G(){F B.J.4L(D.1B)},t2:G(){u m=B.V;u c=D.1B;u 2Z=B.V.6F;u W=D.hm;if(!W){W=(\"#\"+m.6E(2Z(c.r,3h))+m.6E(2Z(c.g,3h))+m.6E(2Z(c.b,3h)));D.hm=W}F W},t1:G(){u 2Q=D.2Q;u c=D.1B;if(H(2Q)==\"L\"||2Q===O){2Q=B.V.bA(D.1B);D.2Q=2Q}F B.J.4L(2Q)},41:G(){u 1G=D.1G;u c=D.1B;if(H(1G)==\"L\"||1G===O){1G=B.V.bC(D.1B);D.1G=1G}F B.J.4L(1G)},1l:G(){F D.hl()},U:G(){u c=D.1B;u hk=[c.r,c.g,c.b,c.a];F D.bX.1r+\"(\"+hk.2b(\", \")+\")\"}};B.J.2l(B.V.V,{3Y:G(1v,bW,1F,8H){u hj=B.V.V;if(M.K==1){u 1B=1v;1v=1B.r;bW=1B.g;1F=1B.b;if(H(1B.a)==\"L\"){8H=L}N{8H=1B.a}}F Y hj(1v,bW,1F,8H)},4H:G(1o,t0,sZ,sY){u m=B.V;F m.V.3Y(m.bB.1w(m,M))},sX:G(1o,sW,sV,sU){u m=B.V;F m.V.3Y(m.bz.1w(m,M))},hi:G(1b){u 8F=B.V.V;if(1b.3Z(0)==\"\\\\\"\"){1b=1b.3H(1,1b.K-2)}u bV=8F.by[1b.8G()];if(H(bV)==\"1n\"){F 8F.bT(bV)}N{if(1b==\"aP\"){F 8F.sT()}}F O},8f:G(4Q){u I=B.V.V;u bU=4Q.3H(0,3);if(bU==\"1B\"){F I.h9(4Q)}N{if(bU==\"1G\"){F I.h8(4Q)}N{if(4Q.3Z(0)==\"#\"){F I.bT(4Q)}}}F I.hi(4Q)},bT:G(4P){if(4P.3Z(0)==\"#\"){4P=4P.2W(1)}u 8E=[];u i,5x;if(4P.K==3){R(i=0;i<3;i++){5x=4P.3H(i,1);8E.1c(3w(5x+5x,16)/3h)}}N{R(i=0;i<6;i+=2){5x=4P.3H(i,2);8E.1c(3w(5x,16)/3h)}}u bS=B.V.V;F bS.3Y.1w(bS,8E)},bG:G(4O,hf,hg,4N){if(4N.2A(4O)===0){4N=4N.2W(4N.2A(\"(\",3)+1,4N.K-1)}u bR=4N.2R(/\\\\s*,\\\\s*/);u bP=[];R(u i=0;i<bR.K;i++){u c=bR[i];u 2i;u bQ=c.2W(c.K-3);if(c.3Z(c.K-1)==\"%\"){2i=0.bE*4M(c.2W(0,c.K-1))}N{if(bQ==\"sS\"){2i=4M(c)/bF}N{if(bQ==\"sR\"){2i=4M(c)/(28.sQ*2)}N{2i=hg[i]*4M(c)}}}bP.1c(2i)}F D[hf].1w(D,bP)},bN:G(Q,sP,sO){u d=B.S;u 2F=B.V.V;R(Q=d.1E(Q);Q;Q=Q.3t){u bO=d.4q.1w(d,M);if(!bO){2V}u 8D=2F.8f(bO);if(!8D){2K}if(8D.6r().a>0){F 8D}}F O},ba:G(Q){u 2F=B.V.V;F 2F.bN(Q,\"aZ\",\"he-3U\")||2F.sN()},sM:G(Q){u 2F=B.V.V;F 2F.bN(Q,\"3U\",\"3U\")||2F.sL()},sK:G(){F B.J.4L(B.V.V.by)}});B.J.2l(B.V,{6F:G(v,8C){v*=8C;if(v<0){F 0}N{if(v>8C){F 8C}N{F v}}},hc:G(n1,n2,1o){if(1o>6){1o-=6}N{if(1o<0){1o+=6}}u 2i;if(1o<1){2i=n1+(n2-n1)*1o}N{if(1o<3){2i=n2}N{if(1o<4){2i=n1+(n2-n1)*(4-1o)}N{2i=n1}}}F 2i},bz:G(1o,5w,3i,bM){if(M.K==1){u 2Q=1o;1o=2Q.h;5w=2Q.s;3i=2Q.v;bM=2Q.a}u 1v;u 3K;u 1F;if(5w===0){1v=0;3K=0;1F=0}N{u i=28.8B(1o*6);u f=(1o*6)-i;u p=3i*(1-5w);u q=3i*(1-(5w*f));u t=3i*(1-(5w*(1-f)));hd(i){3j 1:1v=q;3K=3i;1F=p;2K;3j 2:1v=p;3K=3i;1F=t;2K;3j 3:1v=p;3K=q;1F=3i;2K;3j 4:1v=t;3K=p;1F=3i;2K;3j 5:1v=3i;3K=p;1F=q;2K;3j 6:3j 0:1v=3i;3K=t;1F=p;2K}}F{r:1v,g:3K,b:1F,a:bM}},bB:G(1o,5v,3v,bL){if(M.K==1){u 1G=1o;1o=1G.h;5v=1G.s;3v=1G.l;bL=1G.a}u 1v;u 8A;u 1F;if(5v===0){1v=3v;8A=3v;1F=3v}N{u m2;if(3v<=0.5){m2=3v*(1+5v)}N{m2=3v+5v-(3v*5v)}u m1=(2*3v)-m2;u f=B.V.hc;u h6=1o*6;1v=f(m1,m2,h6+2);8A=f(m1,m2,h6);1F=f(m1,m2,h6-2)}F{r:1v,g:8A,b:1F,a:bL}},bA:G(1v,4K,1F,bK){if(M.K==1){u 1B=1v;1v=1B.r;4K=1B.g;1F=1B.b;bK=1B.a}u 29=28.29(28.29(1v,4K),1F);u 2a=28.2a(28.2a(1v,4K),1F);u 1o;u 8z;u hb=29;if(2a==29){1o=0;8z=0}N{u 6H=(29-2a);8z=6H/29;if(1v==29){1o=(4K-1F)/6H}N{if(4K==29){1o=2+((1F-1v)/6H)}N{1o=4+((1v-4K)/6H)}}1o/=6;if(1o<0){1o+=1}if(1o>1){1o-=1}}F{h:1o,s:8z,v:hb,a:bK}},bC:G(1v,4J,1F,bI){if(M.K==1){u 1B=1v;1v=1B.r;4J=1B.g;1F=1B.b;bI=1B.a}u 29=28.29(1v,28.29(4J,1F));u 2a=28.2a(1v,28.2a(4J,1F));u 1o;u 6G;u bJ=(29+2a)/2;u 4I=29-2a;if(4I===0){1o=0;6G=0}N{if(bJ<=0.5){6G=4I/(29+2a)}N{6G=4I/(2-29-2a)}if(1v==29){1o=(4J-1F)/4I}N{if(4J==29){1o=2+((1F-1v)/4I)}N{1o=4+((1v-4J)/4I)}}1o/=6;if(1o<0){1o+=1}if(1o>1){1o-=1}}F{h:1o,s:6G,l:bJ,a:bI}},6E:G(1P){1P=28.ha(1P);u bH=1P.1l(16);if(1P<16){F\"0\"+bH}F bH},2d:G(){u m=B.J;D.V.h9=m.1O(D.V.bG,D.V,\"1B\",\"3Y\",[1/3h,1/3h,1/3h,1]);D.V.h8=m.1O(D.V.bG,D.V,\"1G\",\"4H\",[1/bF,0.bE,0.bE,1]);u 4G=1/3;u bD={8y:[0,0,0],1F:[0,0,1],gY:[0.6,0.4,0.2],gX:[0,1,1],sJ:[4G,4G,4G],gR:[0.5,0.5,0.5],bx:[0,1,0],sI:[2*4G,2*4G,2*4G],gN:[1,0,1],gL:[1,0.5,0],gK:[0.5,0,0.5],1v:[1,0,0],aP:[0,0,0,0],4F:[1,1,1],gI:[1,1,0]};u h7=G(1b,r,g,b,a){u W=D.3Y(r,g,b,a);D[1b]=G(){F W};F W};R(u k in bD){u 1b=k+\"V\";u h5=m.2o([h7,D.V,1b],bD[k]);D.V[1b]=m.1O.1w(O,h5)}u h0=G(){R(u i=0;i<M.K;i++){if(!(M[i]2C V)){F 1m}}F 1h};u gZ=G(a,b){F a.h4(b)};m.3f(D);m.5u(D.V.1r,h0,gZ);D.2k={\":3e\":D.1z,\":1p\":m.2o(D.1z,D.1W)}}});B.V.1z=[\"V\"];B.V.1W=[\"6F\",\"bC\",\"bB\",\"bA\",\"bz\",\"6E\"];B.V.2d();B.J.2Y(D,B.V);B.V.V.by={sH:\"#sG\",sF:\"#sE\",sD:\"#gW\",sC:\"#sB\",sA:\"#sz\",sy:\"#sx\",sw:\"#sv\",8y:\"#su\",st:\"#sr\",1F:\"#sq\",sp:\"#so\",gY:\"#sn\",sm:\"#sl\",sk:\"#sj\",si:\"#sh\",sg:\"#se\",sd:\"#sc\",sb:\"#sa\",s9:\"#s8\",s7:\"#s6\",gX:\"#gW\",s5:\"#s4\",s3:\"#s2\",s1:\"#s0\",rZ:\"#gV\",rY:\"#rX\",rW:\"#gV\",rV:\"#rU\",rT:\"#rS\",rR:\"#rQ\",rP:\"#rO\",rN:\"#rM\",gU:\"#rL\",rK:\"#rJ\",rI:\"#rH\",rG:\"#rF\",rE:\"#gT\",rD:\"#gT\",rC:\"#rB\",rA:\"#rz\",ry:\"#rx\",rw:\"#rv\",ru:\"#gS\",rt:\"#gS\",rs:\"#rr\",rq:\"#rp\",ro:\"#rn\",rm:\"#rl\",rk:\"#gM\",rj:\"#ri\",rh:\"#rg\",rf:\"#rd\",rc:\"#rb\",gR:\"#gQ\",bx:\"#ra\",r9:\"#r8\",r7:\"#gQ\",r6:\"#r5\",r4:\"#r3\",r2:\"#r1\",r0:\"#qZ\",qY:\"#qX\",qW:\"#qV\",qU:\"#qT\",qS:\"#qR\",qQ:\"#qP\",qO:\"#qN\",qM:\"#qL\",qK:\"#qJ\",qI:\"#qH\",qG:\"#qF\",qE:\"#gP\",qD:\"#qC\",qB:\"#gP\",qA:\"#qz\",qy:\"#qx\",qw:\"#qv\",qu:\"#qt\",qr:\"#gO\",qq:\"#gO\",qp:\"#qo\",qn:\"#qm\",ql:\"#qk\",qj:\"#qi\",qh:\"#qg\",gN:\"#gM\",qf:\"#qe\",qd:\"#qc\",qb:\"#qa\",q9:\"#q8\",q7:\"#q6\",q5:\"#q4\",q3:\"#q2\",q1:\"#q0\",pZ:\"#pY\",pX:\"#pW\",pV:\"#pU\",pT:\"#pS\",pR:\"#pQ\",pP:\"#pO\",pN:\"#pM\",pL:\"#pK\",pJ:\"#pI\",pH:\"#pG\",pF:\"#pE\",gL:\"#pD\",pC:\"#pB\",pA:\"#pz\",py:\"#pw\",pv:\"#pu\",pt:\"#ps\",pr:\"#pq\",pp:\"#po\",pn:\"#pm\",pl:\"#pj\",pi:\"#ph\",pg:\"#pf\",pe:\"#pd\",gK:\"#pc\",1v:\"#pb\",pa:\"#p9\",p8:\"#p7\",p6:\"#p5\",p4:\"#p3\",p2:\"#p1\",p0:\"#oZ\",oY:\"#oX\",oW:\"#oV\",oU:\"#oT\",oS:\"#oR\",oQ:\"#oP\",oO:\"#gJ\",oN:\"#gJ\",oM:\"#oL\",oK:\"#oJ\",oI:\"#oH\",oG:\"#oF\",oE:\"#oD\",oC:\"#oB\",oA:\"#oz\",oy:\"#ox\",ow:\"#ov\",ou:\"#ot\",4F:\"#os\",oq:\"#op\",gI:\"#oo\",om:\"#ok\"};if(H(1q)!=\"L\"){1q.2X(\"B.1u\");1q.2M(\"B.J\");1q.2M(\"B.S\")}if(H(1x)!=\"L\"){1x.26(\"B.J\",[]);1x.26(\"B.S\",[])}1f{if(H(B.J)==\"L\"){14\"\"}}1e(e){14\"B.1u 3F on B.J!\"}1f{if(H(B.S)==\"L\"){14\"\"}}1e(e){14\"B.1u 3F on B.S!\"}if(H(B.1u)==\"L\"){B.1u={}}B.1u.1r=\"B.1u\";B.1u.1Y=\"1.3.1\";B.1u.4x=[];B.1u.bq=G(1d,e){D.1L=e||2O.6D;D.gH=1d};B.J.2l(B.1u.bq.1U,{1K:G(){u U=B.J.U;u 1y=\"{6D(): \"+U(D.6D())+\", 1d(): \"+U(D.1d())+\", 1J(): \"+U(D.1J())+\", 8x(): \"+U(D.8x())+\", 4E(): \"+\"{8w: \"+U(D.4E().8w)+\", 8v: \"+U(D.4E().8v)+\", 8u: \"+U(D.4E().8u)+\", 2P: \"+U(D.4E().2P)+\", bw: \"+U(D.4E().bw)+\"}\";if(D.1J()&&D.1J().2A(\"2h\")===0){1y+=\", 2h(): {3J: \"+U(D.2h().3J)+\", 1n: \"+U(D.2h().1n)+\"}\"}if(D.1J()&&(D.1J().2A(\"3I\")===0||D.1J().2A(\"gE\")!=-1||D.1J()==\"gD\")){1y+=\", 3I(): {4D: \"+U(D.3I().4D)+\", 6A: \"+U(D.3I().6A);if(D.1J()!=\"gC\"){1y+=\", 2e: {2I: \"+U(D.3I().2e.2I)+\", 6v: \"+U(D.3I().2e.6v)+\", 3g: \"+U(D.3I().2e.3g)+\"}}\"}N{1y+=\"}\"}}if(D.1J()==\"gG\"||D.1J()==\"gF\"){1y+=\", 6C(): \"+U(D.6C())}1y+=\"}\";F 1y},1l:G(){F D.1K()},1d:G(){F D.gH},6D:G(){F D.1L},1J:G(){F D.1L.1J||L},8x:G(){F D.1L.8x||D.1L.oj},6C:G(){if(D.1J()==\"gG\"){F(D.1L.6C||D.1L.aW)}N{if(D.1J()==\"gF\"){F(D.1L.6C||D.1L.oi)}}F L},4E:G(){u m={};m.8w=D.1L.oh;m.8v=D.1L.og;m.8u=D.1L.oe||1m;m.2P=D.1L.od;m.bw=m.8w||m.8v||m.2P||m.8u;F m},2h:G(){u k={};if(D.1J()&&D.1J().2A(\"2h\")===0){if(D.1J()==\"oc\"||D.1J()==\"ob\"){k.3J=D.1L.8t;k.1n=(B.1u.5r[k.3J]||\"oa\");F k}N{if(D.1J()==\"o9\"){k.3J=0;k.1n=\"\";if(H(D.1L.6B)!=\"L\"&&D.1L.6B!==0&&!B.1u.bv[D.1L.6B]){k.3J=D.1L.6B;k.1n=bu.bt(k.3J)}N{if(D.1L.8t&&H(D.1L.6B)==\"L\"){k.3J=D.1L.8t;k.1n=bu.bt(k.3J)}}F k}}}F L},3I:G(){u m={};u e=D.1L;if(D.1J()&&(D.1J().2A(\"3I\")===0||D.1J().2A(\"gE\")!=-1||D.1J()==\"gD\")){m.6A=Y B.S.5t(0,0);if(e.6z||e.6x){m.6A.x=(!e.6z||e.6z<0)?0:e.6z;m.6A.y=(!e.6x||e.6x<0)?0:e.6x}m.4D=Y B.S.5t(0,0);if(e.8s||e.8r){m.4D.x=(!e.8s||e.8s<0)?0:e.8s;m.4D.y=(!e.8r||e.8r<0)?0:e.8r}N{u de=B.S.1Z.7Z;u b=B.S.1Z.5s;m.4D.x=e.6z+(de.6y||b.6y)-(de.8q||b.8q);m.4D.y=e.6x+(de.4C||b.4C)-(de.8p||b.8p)}if(D.1J()!=\"gC\"){m.2e={};m.2e.2I=1m;m.2e.3g=1m;m.2e.6v=1m;if(e.6w){m.2e.2I=(e.6w==1);m.2e.6v=(e.6w==2);m.2e.3g=(e.6w==3)}N{m.2e.2I=!!(e.2e&1);m.2e.3g=!!(e.2e&2);m.2e.6v=!!(e.2e&4)}}F m}F L},2J:G(){D.8o();D.8n()},8o:G(){if(D.1L.8o){D.1L.8o()}N{D.1L.o8=1h}},8n:G(){if(D.1L.8n){D.1L.8n()}N{D.1L.o7=1m}}});B.1u.bv={3:\"gz\",o6:\"gA\",o5:\"gy\",o4:\"gx\",o3:\"gw\",o2:\"gv\",o1:\"gu\",o0:\"gs\",nZ:\"gr\",nY:\"gq\",nX:\"gp\",nW:\"go\"};R(i=gB;i<=nV;i++){B.1u.bv[i]=\"gk\"+(i-gB+1)}B.1u.5r={8:\"nU\",9:\"nT\",12:\"gA\",13:\"gz\",16:\"nS\",17:\"nR\",18:\"nQ\",19:\"nP\",20:\"nO\",27:\"nN\",32:\"nM\",33:\"gy\",34:\"gx\",35:\"gw\",36:\"gv\",37:\"gu\",38:\"gs\",39:\"gr\",40:\"gq\",44:\"nL\",45:\"gp\",46:\"go\",59:\"gn\",91:\"nK\",92:\"nJ\",93:\"nI\",nH:\"nG\",nF:\"nE\",nD:\"nC-gm\",nB:\"nA\",nz:\"ny\",nx:\"nw\",nv:\"nu\",nt:\"gn\",ns:\"nr\",nq:\"np\",nn:\"nm-gm\",nl:\"nk\",nj:\"ni\",nh:\"ng\",nf:\"nd\",nc:\"nb\",na:\"n9\",n8:\"n7\"};R(u i=48;i<=57;i++){B.1u.5r[i]=\"gl\"+(i-48)}R(i=65;i<=90;i++){B.1u.5r[i]=\"gl\"+bu.bt(i)}R(i=96;i<=n6;i++){B.1u.5r[i]=\"n5\"+(i-96)}R(i=gj;i<=n4;i++){B.1u.5r[i]=\"gk\"+(i-gj+1)}B.J.2l(B.1u,{1K:G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"},1l:G(){F D.1K()},g7:G(){u I=B.1u;u bs=I.4x;R(u i=0;i<bs.K;i++){I.6t(bs[i])}gi I.4x;1f{2O.gh=L}1e(e){}1f{2O.g8=L}1e(e){}},gb:G(1d,1A,1i,gg){u E=B.1u.bq;if(!gg){F B.J.1O(1A,1i)}1i=1i||1d;if(H(1A)==\"1n\"){F G(gf){1i[1A].1w(1i,[Y E(1d,gf)])}}N{F G(gd){1A.1w(1i,[Y E(1d,gd)])}}},6s:G(1d,2D,5q,4B){1d=B.S.1E(1d);u I=B.1u;if(H(2D)!=\"1n\"){14 Y 2x(\"\\'2D\\' 5p be a 1n\")}u 1i=O;u 1A=O;if(H(4B)!=\"L\"){1i=5q;1A=4B;if(H(4B)==\"1n\"){if(H(5q[4B])!=\"G\"){14 Y 2x(\"\\'bp\\' 5p be a G on \\'gc\\'\")}}N{if(H(4B)!=\"G\"){14 Y 2x(\"\\'bp\\' 5p be a G or 1n\")}}}N{if(H(5q)!=\"G\"){14 Y 2x(\"\\'gc\\' 5p be a G if \\'bp\\' is 2E n3\")}N{1A=5q}}if(H(1i)==\"L\"||1i===O){1i=1d}u bm=!!(1d.bo||1d.bn);u 8m=I.gb(1d,1A,1i,bm);if(1d.bo){1d.bo(2D.3H(2),8m,1m)}N{if(1d.bn){1d.bn(2D,8m)}}u bk=[1d,2D,8m,bm,5q,4B];I.4x.1c(bk);F bk},6t:G(6u){if(!6u[3]){F}u 1d=6u[0];u 2D=6u[1];u bj=6u[2];if(1d.ga){1d.ga(2D.3H(2),bj,1m)}N{if(1d.g9){1d.g9(2D,bj)}N{14 Y 2x(\"\\'1d\\' 5p be a S n0\")}}},8j:G(bh){u I=B.1u;u 5o=I.4x;u m=B.J;if(M.K>1){u 1d=B.S.1E(M[0]);u 2D=M[1];u 1i=M[2];u 1A=M[3];R(u i=5o.K-1;i>=0;i--){u o=5o[i];if(o[0]===1d&&o[1]===2D&&o[4]===1i&&o[5]===1A){I.6t(o);5o.4y(i,1);F 1h}}}N{u 5n=m.bi(5o,bh);if(5n>=0){I.6t(bh);5o.4y(5n,1);F 1h}}F 1m},8i:G(1d,2D){1d=B.S.1E(1d);u m=B.J;u 8l=m.bg(m.1R(O,M,1));u I=B.1u;u bd=I.6t;u 4z=I.4x;if(8l.K===0){R(u i=4z.K-1;i>=0;i--){u 4A=4z[i];if(4A[0]===1d){bd(4A);4z.4y(i,1)}}}N{u bf={};R(u i=0;i<8l.K;i++){bf[8l[i]]=1h}R(u i=4z.K-1;i>=0;i--){u 4A=4z[i];if(4A[0]===1d&&4A[1]in bf){bd(4A);4z.4y(i,1)}}}},8h:G(1d,2D){u bc=B.1u.4x;1d=B.S.1E(1d);u 3G=B.J.1R(O,M,2);u 5m=[];R(u i=0;i<bc.K;i++){u 8k=bc[i];if(8k[0]===1d&&8k[1]===2D){1f{8k[2].1w(1d,3G)}1e(e){5m.1c(e)}}}if(5m.K==1){14 5m[0]}N{if(5m.K>1){u e=Y 2x(\"mZ bb mY in mX \\'2D\\', mW bb mV\");e.bb=5m;14 e}}}});B.1u.1W=[];B.1u.1z=[\"6s\",\"8j\",\"8h\",\"8i\"];B.1u.2d=G(2m){u m=B.J;D.1Z=2v;D.3X=2m;1f{D.6s(2O,\"g8\",D.g7)}1e(e){}D.2k={\":3e\":D.1z,\":1p\":m.2o(D.1z,D.1W)};m.3f(D)};B.1u.2d(D);if(!B.3d){6s=B.1u.6s;8j=B.1u.8j;8i=B.1u.8i;8h=B.1u.8h}B.J.2Y(D,B.1u);if(H(1q)!=\"L\"){1q.2X(\"B.1X\");1q.2M(\"B.J\");1q.2M(\"B.S\");1q.2M(\"B.V\")}if(H(1x)!=\"L\"){1x.26(\"B.J\",[]);1x.26(\"B.S\",[]);1x.26(\"B.V\",[])}1f{if(H(B.J)==\"L\"||H(B.S)==\"L\"||H(B.V)==\"L\"){14\"\"}}1e(e){14\"B.1X 3F on B.J, B.S 3W B.V!\"}if(H(B.1X)==\"L\"){B.1X={}}B.1X.1r=\"B.1X\";B.1X.1Y=\"1.3.1\";B.1X.1K=G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"};B.1X.1l=G(){F D.1K()};B.1X.aI=G(e,g6){e=B.S.1E(e);D.fN(g6);if(D.1S.fL){e=D.g5(e)}u 4w=D.1S.3U;u C=B.V.V;if(D.1S.3U==\"aW\"){4w=C.ba(e)}N{if(!(4w 2C C)){4w=C.8f(4w)}}D.82=(4w.6r().a<=0);u 5l=D.1S.aV;if(D.1S.aV==\"fM\"){5l=C.ba(e.8g)}N{if(!(5l 2C C)){5l=C.8f(5l)}}D.g3(e,4w,5l)};B.1X.aI.1U={g5:G(e){u mU=e.3t;u 1N=B.S.b9();if(H(1N.5k)==\"L\"||1N.5k===O){F e}u 4v=1N.5k.g4(e,O);if(H(4v)==\"L\"||4v===O){F e}u b8=B.S.6m({\"1T\":{3u:\"8c\",mT:4v.6q(\"6p-3D\"),85:4v.6q(\"6p-3g\"),mS:4v.6q(\"6p-6f\"),86:4v.6q(\"6p-2I\"),6p:\"2N\"}});b8.6o=e.6o;e.6o=\"\";e.2c(b8);F e},g3:G(e,b7,8e){if(D.1S.3E){D.g2(e,8e)}if(D.fy()){D.fX(e,b7,8e)}if(D.fx()){D.fV(e,b7,8e)}},g2:G(el,g1){u b6=\"6l 8a \"+D.aQ(g1);u g0=\"3E-2I: \"+b6;u fZ=\"3E-3g: \"+b6;u fY=\"1T=\\'\"+g0+\";\"+fZ+\"\\'\";el.6o=\"<4u \"+fY+\">\"+el.6o+\"</4u>\"},fX:G(el,fW,b5){u b4=D.b1(b5);R(u i=0;i<D.1S.89;i++){b4.2c(D.b0(fW,b5,i,\"3D\"))}el.1T.mR=0;el.mQ(b4,el.6n)},fV:G(el,fU,b3){u b2=D.b1(b3);R(u i=(D.1S.89-1);i>=0;i--){b2.2c(D.b0(fU,b3,i,\"6f\"))}el.1T.mP=0;el.2c(b2)},b1:G(fT){u 2q=B.S;F 2q.6m({1T:{aZ:fT.1l()}})},b0:G(aY,fQ,n,aX){u 6k=B.S.8d();u 2p=6k.1T;2p.aZ=aY.1l();2p.3u=\"8c\";2p.3V=\"6l\";2p.fS=\"fR\";2p.mO=\"6l\";u 8b=D.aQ(aY,fQ);if(D.1S.3E&&n===0){2p.mN=\"8a\";2p.mM=\"6l\";2p.84=\"2N\";2p.83=\"2N\";2p.mL=\"2N\";2p.3V=\"2N\";2p.fP=8b.1l()}N{if(8b){2p.fP=8b.1l();2p.mK=\"8a\";2p.mJ=\"2N 6l\"}}if(!D.1S.4r&&(n==(D.1S.89-1))){2p.3V=\"fO\"}D.fI(6k,n,aX);D.fG(6k,n,aX);F 6k},fN:G(fK){D.1S={6g:\"1p\",3U:\"aW\",aV:\"fM\",5j:1h,3E:1m,4r:1m,fL:1m};B.J.2l(D.1S,fK);D.1S.89=(D.1S.4r?2:4)},aL:G(){u 88=D.1S.6g;if(D.6h(88,\"1p\",\"3D\")){F\"\"}u aU=(88.2A(\"tl\")!=-1);u aT=(88.2A(\"tr\")!=-1);if(aU&&aT){F\"\"}if(aU){F\"2I\"}if(aT){F\"3g\"}F\"\"},aK:G(){u 87=D.1S.6g;if(D.6h(87,\"1p\",\"6f\")){F\"\"}u aS=(87.2A(\"bl\")!=-1);u aR=(87.2A(\"br\")!=-1);if(aS&&aR){F\"\"}if(aS){F\"2I\"}if(aR){F\"3g\"}F\"\"},aQ:G(aN,aO){if(aN==\"aP\"){F aO}N{if(D.1S.3E){F D.1S.3E}N{if(D.1S.5j){F aO.fJ(aN)}}}F\"\"},fI:G(el,n,fH){u 6j=D.fE(n)+\"px\";u aM=(fH==\"3D\"?D.aL():D.aK());u 4t=el.1T;if(aM==\"2I\"){4t.86=6j;4t.85=\"2N\"}N{if(aM==\"3g\"){4t.85=6j;4t.86=\"2N\"}N{4t.86=6j;4t.85=6j}}},fG:G(el,n,fF){u 6i=D.fz(n)+\"px\";u aJ=(fF==\"3D\"?D.aL():D.aK());u 4s=el.1T;if(aJ==\"2I\"){4s.84=6i;4s.83=\"2N\"}N{if(aJ==\"3g\"){4s.83=6i;4s.84=\"2N\"}N{4s.84=6i;4s.83=6i}}},fE:G(n){if(D.82){F 0}u o=D.1S;if(o.4r&&o.5j){u fD=[1,0];F fD[n]}N{if(o.4r){u fC=[2,1];F fC[n]}N{if(o.5j){u fB=[3,2,1,0];F fB[n]}N{u fA=[5,3,2,1];F fA[n]}}}},fz:G(n){u o=D.1S;u 5i;if(o.4r&&(o.5j||D.82)){F 1}N{if(o.4r){5i=[1,0]}N{if(o.5j){5i=[2,1,1,1]}N{if(o.3E){5i=[0,2,0,0]}N{if(D.82){5i=[5,3,2,1]}N{F 0}}}}}F 5i[n]},6h:G(1y){R(u i=1;i<M.K;i++){if(1y.2A(M[i])!=-1){F 1h}}F 1m},fy:G(){F D.6h(D.1S.6g,\"1p\",\"3D\",\"tl\",\"tr\")},fx:G(){F D.6h(D.1S.6g,\"1p\",\"6f\",\"bl\",\"br\")},mI:G(el){F(el.5h.K==1&&el.5h[0].3T==3)}};B.1X.aF=G(e,fw){Y B.1X.aI(e,fw)};B.1X.fs=G(fv,fu,ft){u aG=B.S.aH(fv,fu);R(u i=0;i<aG.K;i++){B.1X.aF(aG[i],ft)}};B.1X.V=B.V.V;B.1X.mH=B.S.4q;B.1X.2d=G(){u m=B.J;m.3f(D);D.2k={\":3e\":D.1z,\":1p\":m.2o(D.1z,D.1W)}};B.1X.1z=[\"aF\",\"fs\"];B.1X.1W=[];B.1X.2d();B.J.2Y(D,B.1X);if(H(B)==\"L\"){B={}}if(H(B.B)==\"L\"){B.B={}}B.B.1r=\"B.B\";B.B.1Y=\"1.3.1\";B.B.1K=G(){F\"[\"+D.1r+\" \"+D.1Y+\"]\"};B.B.1l=G(){F D.1K()};B.B.aA=[\"J\",\"15\",\"1H\",\"1D\",\"1s\",\"1k\",\"S\",\"1I\",\"V\",\"1u\",\"1X\"];if(H(1x)!=\"L\"||H(1q)!=\"L\"){if(H(1q)!=\"L\"){1q.2X(\"B.B\");1q.2M(\"B.*\")}if(H(1x)!=\"L\"){1x.26(\"B.J\",[]);1x.26(\"B.15\",[]);1x.26(\"B.1H\",[]);1x.26(\"B.1D\",[]);1x.26(\"B.1s\",[]);1x.26(\"B.1k\",[]);1x.26(\"B.S\",[]);1x.26(\"B.1I\",[]);1x.26(\"B.V\",[]);1x.26(\"B.1u\",[]);1x.26(\"B.1X\",[])}(G(){u 6e=B.J.1R;u I=B.B;u aE=I.aA;u aD=[];u aC=[];u 81={};u i,k,m,1p;R(i=0;i<aE.K;i++){m=B[aE[i]];6e(aD,m.1z);6e(aC,m.1W);R(k in m.2k){81[k]=6e(81[k],m.2k[k])}1p=m.2k[\":1p\"];if(!1p){1p=6e(O,m.1z,m.1W)}u j;R(j=0;j<1p.K;j++){k=1p[j];I[k]=m[k]}}I.1z=aD;I.1W=aC;I.2k=81}())}N{if(H(B.3d)==\"L\"){B.3d=1h}(G(){u 80=2v.fr(\"7W\");u ay=\"fq://fp.mG.fo/mF/mE/mD.is.aB.mC\";u 2w=O;u ax=O;u az={};u i;R(i=0;i<80.K;i++){u 1d=80[i].fm(\"1d\");if(!1d){2V}az[1d]=1h;if(1d.3C(/B.js$/)){2w=1d.2W(0,1d.mB(\"B.js\"));ax=80[i]}}if(2w===O){F}u 6d=B.B.aA;R(u i=0;i<6d.K;i++){if(B[6d[i]]){2V}u 7Y=2w+6d[i]+\".js\";if(7Y in az){2V}if(2v.7Z&&2v.7Z.mA==ay){u s=2v.mz(ay,\"7W\");s.4p(\"id\",\"my\"+2w+6d[i]);s.4p(\"1d\",7Y);s.4p(\"1J\",\"mx/x-fk\");ax.3t.2c(s)}N{2v.fl(\"<7W 1d=\\\\\"\"+7Y+\"\\\\\" 1J=\\\\\"7X/fk\\\\\"></7W>\")}}})()}',62,1976,'||||||||||||||||||||||||||||||var|||||||MochiKit||this||return|function|typeof|self|Base|length|undefined|arguments|else|null||elem|for|DOM||repr|Color|rval|res|new||||||throw|Iter|||||next|name|push|src|catch|try|lst|true|obj|node|Async|toString|false|string|hue|all|dojo|NAME|Format|msg|Signal|red|apply|JSAN|str|EXPORT|func|rgb|_425|DateTime|getElement|blue|hsl|Logging|LoggingPane|type|__repr__|_event|while|doc|bind|num|iter|extend|options|style|prototype|seq|EXPORT_OK|Visual|VERSION|_document||_434||replace|forwardCall|StopIteration|use||Math|max|min|join|appendChild|__new__|button|compare|date|key|val|_329|EXPORT_TAGS|update|win|pair|concat|_596|dom|map|req|Deferred|sync|document|base|Error|number|partial|indexOf||instanceof|sig|not|cls|list|fired|left|stop|break|logger|require|0px|window|shift|hsv|split|createElement|_423|callee|continue|substring|provide|_exportSymbols|ccc||_464|||||||||step|pred|_51|__compat__|common|nameFunctions|right|255|_517|case|100|_loggingPane|value|object|callback|TypeError|_251|_246|_113|parentNode|display|_522|parseInt|cssText|wrap|info|isArrayLike|end|match|top|border|depends|args|substr|mouse|code|_519|_443|className|level|err|frac|Date|_135|_85|nodeType|color|height|and|_window|fromRGB|charAt||asHSL|_444|message||||filter||LogMessage|AdapterRegistry|_366|imap|NotFound|locked|counter|_262|_messages|operator|cmp|_165|_161|pairs|arr|_52|setAttribute|computedStyle|compact|_614|_610|div|_576|_572|_observers|splice|_565|_566|_555|scrollTop|page|modifier|white|_541|fromHSL|_539|_535|_528|clone|parseFloat|_505|pre|_499|_497|_427|createTextNode|_446|attributeArray|_388|_379|updateNodeAttributes|_341|_326||box|errback|results|paused|chain|_285||ofs||NamedError|_175|_147|_122|_83|_54|_17|childNodes|_619|blend|defaultView|_574|_569|idx|_562|must|_554|_specialKeys|body|Coordinates|registerComparator|_521|_516|hex|mid|_478|width|loggingPane|LogLevel|nwin|head|url|setElementClass|callStack|path|dest|_359|boolean|register|Dimensions|DeferredLock|_313|addCallback|_310|waiting|onreadystatechange|_290|LOCALE|year|printfire|_214|log|_213|_211|pos|_155|_153||typeMatcher|listMinMax|_114|_40|itr|typ|_19|_634|_625|bottom|corners|_hasString|_612|_608|_595|1px|DIV|firstChild|innerHTML|padding|getPropertyValue|asRGB|connect|_disconnect|_559|middle|which|clientY|scrollLeft|clientX|client|charCode|relatedTarget|event|toColorPart|clampColorComponent|_537|_534|toFixed|_468|buildAndApplyFilter|_442|_441|_440|_439|position|_463|_447|removeChild|_449|uid|_428|_426|compliant|attributes|_422|_409|_412|_400|_395|_390|_389|_377|_375|_363|attr|ctx|repeat|_340|_339|isNotEmpty|_335|_333|opera|DeferredList|ret|_309|silentlyCancelled|canceller|_nextId|Array|_293|XMLHttpRequest|chained|_281|tail|_252|_225|msec|day|month|iso|Logger|_208|listeners|_200|_198|_194|_196|reduce|range|_169|_162|truth|registerRepr|_121|_70|_58|_56|_47|_45|_41|_13|_1|script|text|uri|documentElement|_630|_629|isTransparent|borderRightWidth|borderLeftWidth|marginRight|marginLeft|_602|_599|numSlices|solid|_597|block|SPAN|_579|fromString|offsetParent|signal|disconnectAll|disconnect|_570|_563|_557|preventDefault|stopPropagation|clientTop|clientLeft|pageY|pageX|keyCode|meta|ctrl|alt|target|black|_532|_524|floor|_513|_512|_500|_495|toLowerCase|_487|DEBUG|INFO|WARNING|FATAL|ERROR|colorTable|logFont|closed|inline|onclick|_438|_437|_445|RegExp|_452|space|title|updatetree|||||withDocument|withWindow||setDisplayForElement|none|renames|forEach|domConverters|escapeHTML|addElementClass|removeElementClass|once|_378|_380|_376|appendChildNodes|coerceToDOM|_355|opt|clientWidth|opacity|GenericError|fail|resultList|_307|_301|_fire|can|addCallbacks|_resback|percent|decimal|separator|twoDigitFloat|_274|_273|_264|_257|_250|_249|_254|_248|_243|_242|fmt|_240|_245|getTime|sec|hour|_209|slice|_206|iterateNextIter|registerIteratorFactory|arrayLikeIter|iteratorRegistry|takewhile|ifilterfalse|ifilter|_181|_176|_168|_166|_159|_tee|deque|arg|fun|jsonRegistry|reprString|reprRegistry|comparatorRegistry|urlEncode|_110|_108|cur|_95|_87|_71|im_preargs||_53|_57|_46|present|like|array|Argument|_15|_12|_632|_631|_633|SUBMODULES|only|_628|_627|_626|roundElement|_624|getElementsByTagAndClassName|_RoundCorners|_613|_whichSideBottom|_whichSideTop|_609|_605|_606|transparent|_borderColor|_604|_603|_601|_600|bgColor|fromElement|_594|_592|backgroundColor|_createCornerSlice|_createCorner|_590|_589|_587|_586|_581|_578|_577|currentDocument|fromBackground|errors|_568|_564||sigs|flattenArguments|_561|findIdentical|_560|_558||_556|attachEvent|addEventListener|funcOrStr|Event||_548|fromCharCode|String|_specialMacKeys|any|green|_namedColors|hsvToRGB|rgbToHSV|hslToRGB|rgbToHSL|_542|01|360|_fromColorString|_540|_536|_538|_529|_523|_518|fromComputedStyle|_511|_507|_508|_506|_501|fromHexString|_498|_496|_486|__class__|createLoggingPane|_459|_461|font|_462|_430|_435|1000|index|_460|getMessages|removeListener|_451||_457|_450|infore|_448|_456|logDebug|offsetHeight|span|input|_436|TR||HTML|open|alert|currentWindow|swapDOM|SELECT|FORM|INPUT|createDOMFunc|ignoreAttr|_421|call|_417|_410|_415|nodeName|_414|_413|emitHTML|good|_406|_399|_397|_393|_392|addLoadEvent|addToCallStack|_387|_386|_381|_382|_383|_373|_372|_369|createDOM|_365|Function|_360|_362|_358|_344|nodeWalk|formContents|_337|_338|_334|_332|offsetTop|offsetLeft|visibility|parentElement|||XMLHttpRequestError|BrowserComplianceError|CancelledError|AlreadyCalledError|evalJSONRequest|sendXMLHttpRequest|wait|doSimpleXMLHttpRequest|getXMLHttpRequest|succeed|_312|finishedCount|_308|_cbDeferred|_303|_297|queryString|_nothing|_289|XMLHTTP|ActiveXObject|eval|_284|_check|error|_279|default|rstrip|lstrip|formatLocale|roundToFixed|truncToFixed|_276|pow|_272|_271|_270|sign|_265|_263|tmp|_238|_232|toISODate|toISOTime|getFullYear|getDate|getMonth|_230|_padTwo|_228|useNativeConsole|_212|compareLogMessage|isLogMessage|unshift|_207||maxSize|_202|_199|logLevelAtLeast|console|hasIterateNext|iterateNext|arrayLike|groupby||exhaust|tee|dropwhile|applymap||islice|izip|cycle|count||_189|_188|_183|_185|_184|_186|_187|_182|identity|fetch|_180|_177|listMin|reprNumber|reprArrayLike|compareArrayLike|compareDateLike|isDateLike|findValue|_128|__export__|keyComparator|_124|_118|_93|_94|_90|_88|_84|_77|_68|_67|_66|_65|_60|im_func|_55|im_self|_48|_44|_42|_39|_36|_33|_27|_26|_25|_22|_24|_20|javascript|write|getAttribute||org|www|http|getElementsByTagName|roundClass|_623|_622|_621|_620|_isBottomRounded|_isTopRounded|_borderSize|_618|_617|_616|_615|_marginSize|_611|_setBorder|_607|_setMargin|blendedColor|_598|__unstable__wrapElement|fromParent|_setOptions|2px|borderColor|_593|hidden|overflow|_591|_588|_roundBottomCorners|_585|_roundTopCorners|_584|_583|_582|_580|_renderBorder|_roundCornersImpl|getComputedStyle|_doWrap|_571|_unloadCache|onunload|detachEvent|removeEventListener|_listener|objOrFunc|_552||_551|_549|onload|delete|112|KEY_F|KEY_|MINUS|KEY_SEMICOLON|KEY_DELETE|KEY_INSERT|KEY_ARROW_DOWN|KEY_ARROW_RIGHT|KEY_ARROW_UP||KEY_ARROW_LEFT|KEY_HOME|KEY_END|KEY_PAGE_DOWN|KEY_PAGE_UP|KEY_ENTER|KEY_NUM_PAD_CLEAR|63236|mousemove|contextmenu|click|mouseout|mouseover|_src|yellow|708090|purple|orange|ff00ff|magenta|778899|d3d3d3|808080|gray|696969|2f4f4f|darkred|a9a9a9|00ffff|cyan|brown|_547|_546||||compareRGB|_545||_543|fromHSLString|fromRGBString|round|_533|_hslValue|switch|background|_503|_504||fromName|_488|col|toRGBString|_hexString|_rgbString|_hslString|toPrecision|isLight||_481|_477|_476|_475|_474|_473|_469|_466|closePane|_458|onkeypress|_454|addListener|_455|close|test|scrollHeight|option|word|moz|_431|getElementById|html|pop|200|_|removeElement|showElement|hideElement|CANVAS|STRONG|FIELDSET|LEGEND|OPTGROUP|OPTION|TEXTAREA|LABEL|HR|BR|H3|H2|H1|PRE|TT|BUTTON|IMG|TH||TABLE||TFOOT|THEAD|TBODY|TD|LI|OL|||UL|checked|class|ignoreAttrFilter||_424|_419|nodeValue|scrapeText|_416|_418|sort|_411|toHTML|_404|hasElementClass|_403|_402|_401|swapElementClass|_398|_394|toggleElementClass|_391|focusOnLoad|_newCallStack|currentStyle|_371|replaceChildNodes|_364|_361|getNodeAttribute|_357|setNodeAttribute|_354|_352|_350|_353|toDOM|_346|_345|registerDOMConverter|selectedIndex|setElementPosition|setElementDimensions|tagName|absolute|getBoxObjectFor|getBoundingClientRect|elementPosition|_325|_324|_322|_323|offsetWidth|elementDimensions|clientHeight|innerWidth|getViewportDimensions|setOpacity|status|_317|deferred|_316|_newNamedError|maybeDeferred||gatherResults|callLater|loadJSONDoc|_311|consumeErrors|fireOnOneErrback|fireOnOneCallback|addErrback|_305|_304|_306|unlocked|release|_300|_299|_298|_296|_xhr_onreadystatechange|_xhr_canceller|304|responseText|Msxml2|addBoth|_pause|_continue|result|the|are|they|instances|_unpause|cancel|_280|_278|en_US|strip|percentFormat|twoDigitAverage|numberFormatter|_277|_275|isNaN|_259|_258|_260|_255|_253|_numberFormatter|_241|_239|_237|_236|_235|_234|_233|_231|toAmericanDate|toPaddedAmericanDate|americanDate|toISOTimestamp|isoTimestamp|isoDate|foot|sep||60000|_221|_isoRegexp|dispatchEvent|createEvent|warning|logWarning|fatal|logFatal|debug|logError|baseLog|_210|getMessageText|logToConsole|dispatchListeners|_204|_203|ident|_201|postError|alertListener|_197|_192|groupby_as_array|iextend|some|reversed|sorted|every|sum|_190|eat|_174|_173|_172|_171|_167|_163|_158|_157|_151|_144|_141||_139|_136|_134||_133|_132|zip|merge|isUndefined|isCallable|listMax|_131|_130|encodeURIComponent||_127|method|parseQueryString|evalJSON|registerJSON|serializeJSON|objMin|objMax|reverseKeyComparator|arrayEqual|objEqual|bindMethods|xfilter|xmap|isEmpty|isNull|isUndefinedOrNull|itemgetter|items|keys|setdefault|_126|_120|decodeURIComponent|_119|len|_109|_107|_104|_105|_101|_102|_98|||_100|_97|_96|_91|json|__json__|_82|_81|_80|_79|_76||_75|_74|_73|_69|_primitives|_64|_63||_62|_61|_59|_wrapDumbFunction|_49|_50|_31|_30|_21|_7|application|MochiKit_|createElementNS|namespaceURI|lastIndexOf|xul|there|gatekeeper|keymaster|mozilla|getElementsComputedStyle|_hasSingleTextChild|borderWidth|borderStyle|borderBottomWidth|borderTopWidth|borderTopStyle|fontSize|paddingBottom|insertBefore|paddingTop|marginBottom|marginTop|_575|property|see|handling|thrown|Multiple|element|||given|123|KEY_NUM_PAD_|105|KEY_APOSTROPHE|222|KEY_RIGHT_SQUARE_BRACKET|221|KEY_REVERSE_SOLIDUS|220|KEY_LEFT_SQUARE_BRACKET||219|KEY_GRAVE_ACCENT|192|KEY_SOLIDUS|191|KEY_FULL_STOP|190|KEY_HYPHEN|189||KEY_COMMA|188|KEY_EQUALS_SIGN|187|186|KEY_SCROLL_LOCK|145|KEY_NUM_LOCK|144|KEY_NUM_PAD_SOLIDUS|111|KEY_NUM_PAD_FULL_STOP|110|KEY_NUM_PAD_HYPHEN|109|KEY_NUM_PAD_PLUS_SIGN|107|KEY_NUM_PAD_ASTERISK|106|KEY_SELECT|KEY_WINDOWS_RIGHT|KEY_WINDOWS_LEFT|KEY_PRINT_SCREEN|KEY_SPACEBAR|KEY_ESCAPE|KEY_CAPS_LOCK|KEY_PAUSE|KEY_ALT|KEY_CTRL|KEY_SHIFT|KEY_TAB|KEY_BACKSPACE|63242|63272|63302|63233|63235|63232|63234|63273|63275|63277|63276|63289|returnValue|cancelBubble|keypress|KEY_UNKNOWN|keyup|keydown|shiftKey|metaKey||ctrlKey|altKey|toElement|srcElement|9acd32||yellowgreen||ffff00|f5f5f5|whitesmoke||ffffff|f5deb3|wheat|ee82ee|violet|40e0d0|turquoise|ff6347|tomato|d8bfd8|thistle|008080|teal|d2b48c|tan|4682b4|steelblue|00ff7f|springgreen|fffafa|snow|slategrey|slategray|6a5acd|slateblue|87ceeb|skyblue|c0c0c0|silver|a0522d|sienna|fff5ee|seashell|2e8b57|seagreen|f4a460|sandybrown|fa8072|salmon|8b4513|saddlebrown|4169e1|royalblue|bc8f8f|rosybrown|ff0000|800080|b0e0e6|powderblue|dda0dd|plum|ffc0cb|pink|cd853f||peru|ffdab9|peachpuff|ffefd5|papayawhip|db7093|palevioletred|afeeee|paleturquoise|98fb98|palegreen|eee8aa||palegoldenrod|da70d6|orchid|ff4500|orangered|ffa500|6b8e23|olivedrab|808000|olive|fdf5e6|oldlace|000080|navy|ffdead|navajowhite|ffe4b5|moccasin|ffe4e1|mistyrose|f5fffa|mintcream|191970|midnightblue|c71585|mediumvioletred|48d1cc|mediumturquoise|00fa9a|mediumspringgreen|7b68ee|mediumslateblue|3cb371|mediumseagreen|9370db|mediumpurple|ba55d3|mediumorchid|0000cd|mediumblue|66cdaa|mediumaquamarine|800000|maroon|faf0e6|linen|32cd32|limegreen|00ff00|lime|ffffe0|lightyellow|b0c4de|lightsteelblue|lightslategrey|lightslategray||87cefa|lightskyblue|20b2aa|lightseagreen|ffa07a|lightsalmon|ffb6c1|lightpink|lightgrey|90ee90|lightgreen|lightgray|fafad2|lightgoldenrodyellow|e0ffff|lightcyan|f08080|lightcoral|add8e6|lightblue|fffacd|lemonchiffon|7cfc00|lawngreen|fff0f5|lavenderblush|e6e6fa|lavender|f0e68c|khaki|fffff0|ivory|4b0082|indigo|cd5c5c|indianred|ff69b4|hotpink|f0fff0|honeydew|grey|adff2f|greenyellow|008000|daa520|goldenrod|ffd700||gold|f8f8ff|ghostwhite|dcdcdc|gainsboro|fuchsia|228b22|forestgreen|fffaf0|floralwhite|b22222|firebrick|1e90ff|dodgerblue|dimgrey|dimgray|00bfff|deepskyblue|ff1493|deeppink|9400d3|darkviolet|00ced1|darkturquoise|darkslategrey|darkslategray|483d8b|darkslateblue|8fbc8f|darkseagreen|e9967a|darksalmon|8b0000|9932cc|darkorchid|ff8c00|darkorange|556b2f|darkolivegreen|8b008b|darkmagenta|bdb76b|darkkhaki|darkgrey|006400|darkgreen|darkgray|b8860b|darkgoldenrod|008b8b|darkcyan|00008b|darkblue|dc143c|crimson|fff8dc|cornsilk|6495ed|cornflowerblue|ff7f50|coral|d2691e||chocolate|7fff00|chartreuse|5f9ea0|cadetblue|deb887|burlywood|a52a2a|8a2be2|blueviolet|0000ff|ffebcd||blanchedalmond|000000|ffe4c4|bisque|f5f5dc|beige|f0ffff|azure|7fffd4|aquamarine|aqua|faebd7|antiquewhite|f0f8ff|aliceblue|lightGray|darkGray|namedColors|blackColor|fromText|whiteColor|_510|_509|PI|rad|deg|transparentColor|_494|_493|_492|fromHSV|_491|_490|_489|asHSV|toHexString|rgba|hsla|toHSLString|isDark|lighterColorWithLevel|darkerColorWithLevel|colorWithLightness|colorWithSaturation|colorWithHue|colorWithAlpha||serif|sans|Verdana||8pt|8em|auto||Close|Clear||Load|Filter||10em||fixed|regex|emergency|line|margin|_Listener|dtd|loose|html4|w3|EN|Transitional|DTD|W3C|PUBLIC|DOCTYPE|blocking|due|debugging|able|Not|resizable|dependent|href|location|_MochiKit_LoggingPane|_429|canvas|strong|fieldset|legend|optgroup|select|form|textarea|label|img|table|tfoot|thead|tbody|htmlFor||useMap|usemap|defaultChecked|hasChildNodes|quot|amp|_405|focus|replaceChild|checkbox||radio|_win|BODY||safari|version|userAgent|navigator|innerHeight|alpha|khtml|Tried|acquire|clearTimeout|setTimeout|GET|ignore|send|abort|failed|Request|readyState|support|does|Browser|Microsoft|_288|_287|used|Deferreds|Chained|success|unfired|fr_FR|de_DE|00|abs|search|pattern|Invalid|getTimezoneOffset|getSeconds|getMinutes|getHours|UTC|3600000|initEvent|Events|debuggingBookmarklet|MESSAGES|LAST|_205|clear|ninfo|nlevel|timestamp|reverse|takes|initial|with|sequence|empty|iterable|numbers|dateLike|escape|find|forward|unregister|unescape|Object|compared|item|contains|logor|logand|cle|clt|cge|cgt|cne|ceq|zrshift|rshift|lshift|xor|mul|mod|sub|add|neg|lognot|_9|_2'.split('|'),0,{})\n\
\n\
result += decompressedMochiKit.length;\n\
\n\
/*\n\
 * jQuery 1.2.1 - New Wave Javascript\n\
 *\n\
 * Copyright (c) 2007 John Resig (jquery.com)\n\
 * Dual licensed under the MIT (MIT-LICENSE.txt)\n\
 * and GPL (GPL-LICENSE.txt) licenses.\n\
 *\n\
 * $Date: 2007-09-16 23:42:06 -0400 (Sun, 16 Sep 2007) $\n\
 * $Rev: 3353 $\n\
 */\n\
\n\
var decompressedJQuery = function(p,a,c,k,e,r){e=function(c){return(c<a?'':e(parseInt(c/a)))+((c=c%a)>35?String.fromCharCode(c+29):c.toString(36))};if(!''.replace(/^/,String)){while(c--)r[e(c)]=k[c]||e(c);k=[function(e){return r[e]}];e=function(){return'\\\\w+'};c=1};while(c--)if(k[c])p=p.replace(new RegExp('\\\\b'+e(c)+'\\\\b','g'),k[c]);return p}('(G(){9(1m E!=\"W\")H w=E;H E=18.15=G(a,b){I 6 7u E?6.5N(a,b):1u E(a,b)};9(1m $!=\"W\")H D=$;18.$=E;H u=/^[^<]*(<(.|\\\\s)+>)[^>]*$|^#(\\\\w+)$/;E.1b=E.3A={5N:G(c,a){c=c||U;9(1m c==\"1M\"){H m=u.2S(c);9(m&&(m[1]||!a)){9(m[1])c=E.4D([m[1]],a);J{H b=U.3S(m[3]);9(b)9(b.22!=m[3])I E().1Y(c);J{6[0]=b;6.K=1;I 6}J c=[]}}J I 1u E(a).1Y(c)}J 9(E.1n(c))I 1u E(U)[E.1b.2d?\"2d\":\"39\"](c);I 6.6v(c.1c==1B&&c||(c.4c||c.K&&c!=18&&!c.1y&&c[0]!=W&&c[0].1y)&&E.2h(c)||[c])},4c:\"1.2.1\",7Y:G(){I 6.K},K:0,21:G(a){I a==W?E.2h(6):6[a]},2o:G(a){H b=E(a);b.4Y=6;I b},6v:G(a){6.K=0;1B.3A.1a.16(6,a);I 6},N:G(a,b){I E.N(6,a,b)},4I:G(a){H b=-1;6.N(G(i){9(6==a)b=i});I b},1x:G(f,d,e){H c=f;9(f.1c==3X)9(d==W)I 6.K&&E[e||\"1x\"](6[0],f)||W;J{c={};c[f]=d}I 6.N(G(a){L(H b 1i c)E.1x(e?6.R:6,b,E.1e(6,c[b],e,a,b))})},17:G(b,a){I 6.1x(b,a,\"3C\")},2g:G(e){9(1m e!=\"5i\"&&e!=S)I 6.4n().3g(U.6F(e));H t=\"\";E.N(e||6,G(){E.N(6.3j,G(){9(6.1y!=8)t+=6.1y!=1?6.6x:E.1b.2g([6])})});I t},5m:G(b){9(6[0])E(b,6[0].3H).6u().3d(6[0]).1X(G(){H a=6;1W(a.1w)a=a.1w;I a}).3g(6);I 6},8m:G(a){I 6.N(G(){E(6).6q().5m(a)})},8d:G(a){I 6.N(G(){E(6).5m(a)})},3g:G(){I 6.3z(1q,Q,1,G(a){6.58(a)})},6j:G(){I 6.3z(1q,Q,-1,G(a){6.3d(a,6.1w)})},6g:G(){I 6.3z(1q,P,1,G(a){6.12.3d(a,6)})},50:G(){I 6.3z(1q,P,-1,G(a){6.12.3d(a,6.2q)})},2D:G(){I 6.4Y||E([])},1Y:G(t){H b=E.1X(6,G(a){I E.1Y(t,a)});I 6.2o(/[^+>] [^+>]/.14(t)||t.1g(\"..\")>-1?E.4V(b):b)},6u:G(e){H f=6.1X(G(){I 6.67?E(6.67)[0]:6.4R(Q)});H d=f.1Y(\"*\").4O().N(G(){9(6[F]!=W)6[F]=S});9(e===Q)6.1Y(\"*\").4O().N(G(i){H c=E.M(6,\"2P\");L(H a 1i c)L(H b 1i c[a])E.1j.1f(d[i],a,c[a][b],c[a][b].M)});I f},1E:G(t){I 6.2o(E.1n(t)&&E.2W(6,G(b,a){I t.16(b,[a])})||E.3m(t,6))},5V:G(t){I 6.2o(t.1c==3X&&E.3m(t,6,Q)||E.2W(6,G(a){I(t.1c==1B||t.4c)?E.2A(a,t)<0:a!=t}))},1f:G(t){I 6.2o(E.1R(6.21(),t.1c==3X?E(t).21():t.K!=W&&(!t.11||E.11(t,\"2Y\"))?t:[t]))},3t:G(a){I a?E.3m(a,6).K>0:P},7c:G(a){I 6.3t(\".\"+a)},3i:G(b){9(b==W){9(6.K){H c=6[0];9(E.11(c,\"24\")){H e=c.4Z,a=[],Y=c.Y,2G=c.O==\"24-2G\";9(e<0)I S;L(H i=2G?e:0,33=2G?e+1:Y.K;i<33;i++){H d=Y[i];9(d.26){H b=E.V.1h&&!d.9V[\"1Q\"].9L?d.2g:d.1Q;9(2G)I b;a.1a(b)}}I a}J I 6[0].1Q.1p(/\\\\r/g,\"\")}}J I 6.N(G(){9(b.1c==1B&&/4k|5j/.14(6.O))6.2Q=(E.2A(6.1Q,b)>=0||E.2A(6.2H,b)>=0);J 9(E.11(6,\"24\")){H a=b.1c==1B?b:[b];E(\"9h\",6).N(G(){6.26=(E.2A(6.1Q,a)>=0||E.2A(6.2g,a)>=0)});9(!a.K)6.4Z=-1}J 6.1Q=b})},4o:G(a){I a==W?(6.K?6[0].3O:S):6.4n().3g(a)},6H:G(a){I 6.50(a).28()},6E:G(i){I 6.2J(i,i+1)},2J:G(){I 6.2o(1B.3A.2J.16(6,1q))},1X:G(b){I 6.2o(E.1X(6,G(a,i){I b.2O(a,i,a)}))},4O:G(){I 6.1f(6.4Y)},3z:G(f,d,g,e){H c=6.K>1,a;I 6.N(G(){9(!a){a=E.4D(f,6.3H);9(g<0)a.8U()}H b=6;9(d&&E.11(6,\"1I\")&&E.11(a[0],\"4m\"))b=6.4l(\"1K\")[0]||6.58(U.5B(\"1K\"));E.N(a,G(){H a=c?6.4R(Q):6;9(!5A(0,a))e.2O(b,a)})})}};G 5A(i,b){H a=E.11(b,\"1J\");9(a){9(b.3k)E.3G({1d:b.3k,3e:P,1V:\"1J\"});J E.5f(b.2g||b.6s||b.3O||\"\");9(b.12)b.12.3b(b)}J 9(b.1y==1)E(\"1J\",b).N(5A);I a}E.1k=E.1b.1k=G(){H c=1q[0]||{},a=1,2c=1q.K,5e=P;9(c.1c==8o){5e=c;c=1q[1]||{}}9(2c==1){c=6;a=0}H b;L(;a<2c;a++)9((b=1q[a])!=S)L(H i 1i b){9(c==b[i])6r;9(5e&&1m b[i]==\\'5i\\'&&c[i])E.1k(c[i],b[i]);J 9(b[i]!=W)c[i]=b[i]}I c};H F=\"15\"+(1u 3D()).3B(),6p=0,5c={};E.1k({8a:G(a){18.$=D;9(a)18.15=w;I E},1n:G(a){I!!a&&1m a!=\"1M\"&&!a.11&&a.1c!=1B&&/G/i.14(a+\"\")},4a:G(a){I a.2V&&!a.1G||a.37&&a.3H&&!a.3H.1G},5f:G(a){a=E.36(a);9(a){9(18.6l)18.6l(a);J 9(E.V.1N)18.56(a,0);J 3w.2O(18,a)}},11:G(b,a){I b.11&&b.11.27()==a.27()},1L:{},M:G(c,d,b){c=c==18?5c:c;H a=c[F];9(!a)a=c[F]=++6p;9(d&&!E.1L[a])E.1L[a]={};9(b!=W)E.1L[a][d]=b;I d?E.1L[a][d]:a},30:G(c,b){c=c==18?5c:c;H a=c[F];9(b){9(E.1L[a]){2E E.1L[a][b];b=\"\";L(b 1i E.1L[a])1T;9(!b)E.30(c)}}J{2a{2E c[F]}29(e){9(c.53)c.53(F)}2E E.1L[a]}},N:G(a,b,c){9(c){9(a.K==W)L(H i 1i a)b.16(a[i],c);J L(H i=0,48=a.K;i<48;i++)9(b.16(a[i],c)===P)1T}J{9(a.K==W)L(H i 1i a)b.2O(a[i],i,a[i]);J L(H i=0,48=a.K,3i=a[0];i<48&&b.2O(3i,i,3i)!==P;3i=a[++i]){}}I a},1e:G(c,b,d,e,a){9(E.1n(b))b=b.2O(c,[e]);H f=/z-?4I|7T-?7Q|1r|69|7P-?1H/i;I b&&b.1c==4W&&d==\"3C\"&&!f.14(a)?b+\"2T\":b},1o:{1f:G(b,c){E.N((c||\"\").2l(/\\\\s+/),G(i,a){9(!E.1o.3K(b.1o,a))b.1o+=(b.1o?\" \":\"\")+a})},28:G(b,c){b.1o=c!=W?E.2W(b.1o.2l(/\\\\s+/),G(a){I!E.1o.3K(c,a)}).66(\" \"):\"\"},3K:G(t,c){I E.2A(c,(t.1o||t).3s().2l(/\\\\s+/))>-1}},2k:G(e,o,f){L(H i 1i o){e.R[\"3r\"+i]=e.R[i];e.R[i]=o[i]}f.16(e,[]);L(H i 1i o)e.R[i]=e.R[\"3r\"+i]},17:G(e,p){9(p==\"1H\"||p==\"2N\"){H b={},42,41,d=[\"7J\",\"7I\",\"7G\",\"7F\"];E.N(d,G(){b[\"7C\"+6]=0;b[\"7B\"+6+\"5Z\"]=0});E.2k(e,b,G(){9(E(e).3t(\\':3R\\')){42=e.7A;41=e.7w}J{e=E(e.4R(Q)).1Y(\":4k\").5W(\"2Q\").2D().17({4C:\"1P\",2X:\"4F\",19:\"2Z\",7o:\"0\",1S:\"0\"}).5R(e.12)[0];H a=E.17(e.12,\"2X\")||\"3V\";9(a==\"3V\")e.12.R.2X=\"7g\";42=e.7e;41=e.7b;9(a==\"3V\")e.12.R.2X=\"3V\";e.12.3b(e)}});I p==\"1H\"?42:41}I E.3C(e,p)},3C:G(h,j,i){H g,2w=[],2k=[];G 3n(a){9(!E.V.1N)I P;H b=U.3o.3Z(a,S);I!b||b.4y(\"3n\")==\"\"}9(j==\"1r\"&&E.V.1h){g=E.1x(h.R,\"1r\");I g==\"\"?\"1\":g}9(j.1t(/4u/i))j=y;9(!i&&h.R[j])g=h.R[j];J 9(U.3o&&U.3o.3Z){9(j.1t(/4u/i))j=\"4u\";j=j.1p(/([A-Z])/g,\"-$1\").2p();H d=U.3o.3Z(h,S);9(d&&!3n(h))g=d.4y(j);J{L(H a=h;a&&3n(a);a=a.12)2w.4w(a);L(a=0;a<2w.K;a++)9(3n(2w[a])){2k[a]=2w[a].R.19;2w[a].R.19=\"2Z\"}g=j==\"19\"&&2k[2w.K-1]!=S?\"2s\":U.3o.3Z(h,S).4y(j)||\"\";L(a=0;a<2k.K;a++)9(2k[a]!=S)2w[a].R.19=2k[a]}9(j==\"1r\"&&g==\"\")g=\"1\"}J 9(h.3Q){H f=j.1p(/\\\\-(\\\\w)/g,G(m,c){I c.27()});g=h.3Q[j]||h.3Q[f];9(!/^\\\\d+(2T)?$/i.14(g)&&/^\\\\d/.14(g)){H k=h.R.1S;H e=h.4v.1S;h.4v.1S=h.3Q.1S;h.R.1S=g||0;g=h.R.71+\"2T\";h.R.1S=k;h.4v.1S=e}}I g},4D:G(a,e){H r=[];e=e||U;E.N(a,G(i,d){9(!d)I;9(d.1c==4W)d=d.3s();9(1m d==\"1M\"){d=d.1p(/(<(\\\\w+)[^>]*?)\\\\/>/g,G(m,a,b){I b.1t(/^(70|6Z|6Y|9Q|4t|9N|9K|3a|9G|9E)$/i)?m:a+\"></\"+b+\">\"});H s=E.36(d).2p(),1s=e.5B(\"1s\"),2x=[];H c=!s.1g(\"<9y\")&&[1,\"<24>\",\"</24>\"]||!s.1g(\"<9w\")&&[1,\"<6T>\",\"</6T>\"]||s.1t(/^<(9u|1K|9t|9r|9p)/)&&[1,\"<1I>\",\"</1I>\"]||!s.1g(\"<4m\")&&[2,\"<1I><1K>\",\"</1K></1I>\"]||(!s.1g(\"<9m\")||!s.1g(\"<9k\"))&&[3,\"<1I><1K><4m>\",\"</4m></1K></1I>\"]||!s.1g(\"<6Y\")&&[2,\"<1I><1K></1K><6L>\",\"</6L></1I>\"]||E.V.1h&&[1,\"1s<1s>\",\"</1s>\"]||[0,\"\",\"\"];1s.3O=c[1]+d+c[2];1W(c[0]--)1s=1s.5p;9(E.V.1h){9(!s.1g(\"<1I\")&&s.1g(\"<1K\")<0)2x=1s.1w&&1s.1w.3j;J 9(c[1]==\"<1I>\"&&s.1g(\"<1K\")<0)2x=1s.3j;L(H n=2x.K-1;n>=0;--n)9(E.11(2x[n],\"1K\")&&!2x[n].3j.K)2x[n].12.3b(2x[n]);9(/^\\\\s/.14(d))1s.3d(e.6F(d.1t(/^\\\\s*/)[0]),1s.1w)}d=E.2h(1s.3j)}9(0===d.K&&(!E.11(d,\"2Y\")&&!E.11(d,\"24\")))I;9(d[0]==W||E.11(d,\"2Y\")||d.Y)r.1a(d);J r=E.1R(r,d)});I r},1x:G(c,d,a){H e=E.4a(c)?{}:E.5o;9(d==\"26\"&&E.V.1N)c.12.4Z;9(e[d]){9(a!=W)c[e[d]]=a;I c[e[d]]}J 9(E.V.1h&&d==\"R\")I E.1x(c.R,\"9e\",a);J 9(a==W&&E.V.1h&&E.11(c,\"2Y\")&&(d==\"9d\"||d==\"9a\"))I c.97(d).6x;J 9(c.37){9(a!=W){9(d==\"O\"&&E.11(c,\"4t\")&&c.12)6G\"O 94 93\\'t 92 91\";c.90(d,a)}9(E.V.1h&&/6C|3k/.14(d)&&!E.4a(c))I c.4p(d,2);I c.4p(d)}J{9(d==\"1r\"&&E.V.1h){9(a!=W){c.69=1;c.1E=(c.1E||\"\").1p(/6O\\\\([^)]*\\\\)/,\"\")+(3I(a).3s()==\"8S\"?\"\":\"6O(1r=\"+a*6A+\")\")}I c.1E?(3I(c.1E.1t(/1r=([^)]*)/)[1])/6A).3s():\"\"}d=d.1p(/-([a-z])/8Q,G(z,b){I b.27()});9(a!=W)c[d]=a;I c[d]}},36:G(t){I(t||\"\").1p(/^\\\\s+|\\\\s+$/g,\"\")},2h:G(a){H r=[];9(1m a!=\"8P\")L(H i=0,2c=a.K;i<2c;i++)r.1a(a[i]);J r=a.2J(0);I r},2A:G(b,a){L(H i=0,2c=a.K;i<2c;i++)9(a[i]==b)I i;I-1},1R:G(a,b){9(E.V.1h){L(H i=0;b[i];i++)9(b[i].1y!=8)a.1a(b[i])}J L(H i=0;b[i];i++)a.1a(b[i]);I a},4V:G(b){H r=[],2f={};2a{L(H i=0,6y=b.K;i<6y;i++){H a=E.M(b[i]);9(!2f[a]){2f[a]=Q;r.1a(b[i])}}}29(e){r=b}I r},2W:G(b,a,c){9(1m a==\"1M\")a=3w(\"P||G(a,i){I \"+a+\"}\");H d=[];L(H i=0,4g=b.K;i<4g;i++)9(!c&&a(b[i],i)||c&&!a(b[i],i))d.1a(b[i]);I d},1X:G(c,b){9(1m b==\"1M\")b=3w(\"P||G(a){I \"+b+\"}\");H d=[];L(H i=0,4g=c.K;i<4g;i++){H a=b(c[i],i);9(a!==S&&a!=W){9(a.1c!=1B)a=[a];d=d.8M(a)}}I d}});H v=8K.8I.2p();E.V={4s:(v.1t(/.+(?:8F|8E|8C|8B)[\\\\/: ]([\\\\d.]+)/)||[])[1],1N:/6w/.14(v),34:/34/.14(v),1h:/1h/.14(v)&&!/34/.14(v),35:/35/.14(v)&&!/(8z|6w)/.14(v)};H y=E.V.1h?\"4h\":\"5h\";E.1k({5g:!E.V.1h||U.8y==\"8x\",4h:E.V.1h?\"4h\":\"5h\",5o:{\"L\":\"8w\",\"8v\":\"1o\",\"4u\":y,5h:y,4h:y,3O:\"3O\",1o:\"1o\",1Q:\"1Q\",3c:\"3c\",2Q:\"2Q\",8u:\"8t\",26:\"26\",8s:\"8r\"}});E.N({1D:\"a.12\",8q:\"15.4e(a,\\'12\\')\",8p:\"15.2I(a,2,\\'2q\\')\",8n:\"15.2I(a,2,\\'4d\\')\",8l:\"15.4e(a,\\'2q\\')\",8k:\"15.4e(a,\\'4d\\')\",8j:\"15.5d(a.12.1w,a)\",8i:\"15.5d(a.1w)\",6q:\"15.11(a,\\'8h\\')?a.8f||a.8e.U:15.2h(a.3j)\"},G(i,n){E.1b[i]=G(a){H b=E.1X(6,n);9(a&&1m a==\"1M\")b=E.3m(a,b);I 6.2o(E.4V(b))}});E.N({5R:\"3g\",8c:\"6j\",3d:\"6g\",8b:\"50\",89:\"6H\"},G(i,n){E.1b[i]=G(){H a=1q;I 6.N(G(){L(H j=0,2c=a.K;j<2c;j++)E(a[j])[n](6)})}});E.N({5W:G(a){E.1x(6,a,\"\");6.53(a)},88:G(c){E.1o.1f(6,c)},87:G(c){E.1o.28(6,c)},86:G(c){E.1o[E.1o.3K(6,c)?\"28\":\"1f\"](6,c)},28:G(a){9(!a||E.1E(a,[6]).r.K){E.30(6);6.12.3b(6)}},4n:G(){E(\"*\",6).N(G(){E.30(6)});1W(6.1w)6.3b(6.1w)}},G(i,n){E.1b[i]=G(){I 6.N(n,1q)}});E.N([\"85\",\"5Z\"],G(i,a){H n=a.2p();E.1b[n]=G(h){I 6[0]==18?E.V.1N&&3y[\"84\"+a]||E.5g&&38.33(U.2V[\"5a\"+a],U.1G[\"5a\"+a])||U.1G[\"5a\"+a]:6[0]==U?38.33(U.1G[\"6n\"+a],U.1G[\"6m\"+a]):h==W?(6.K?E.17(6[0],n):S):6.17(n,h.1c==3X?h:h+\"2T\")}});H C=E.V.1N&&3x(E.V.4s)<83?\"(?:[\\\\\\\\w*57-]|\\\\\\\\\\\\\\\\.)\":\"(?:[\\\\\\\\w\\\\82-\\\\81*57-]|\\\\\\\\\\\\\\\\.)\",6k=1u 47(\"^>\\\\\\\\s*(\"+C+\"+)\"),6i=1u 47(\"^(\"+C+\"+)(#)(\"+C+\"+)\"),6h=1u 47(\"^([#.]?)(\"+C+\"*)\");E.1k({55:{\"\":\"m[2]==\\'*\\'||15.11(a,m[2])\",\"#\":\"a.4p(\\'22\\')==m[2]\",\":\":{80:\"i<m[3]-0\",7Z:\"i>m[3]-0\",2I:\"m[3]-0==i\",6E:\"m[3]-0==i\",3v:\"i==0\",3u:\"i==r.K-1\",6f:\"i%2==0\",6e:\"i%2\",\"3v-46\":\"a.12.4l(\\'*\\')[0]==a\",\"3u-46\":\"15.2I(a.12.5p,1,\\'4d\\')==a\",\"7X-46\":\"!15.2I(a.12.5p,2,\\'4d\\')\",1D:\"a.1w\",4n:\"!a.1w\",7W:\"(a.6s||a.7V||15(a).2g()||\\'\\').1g(m[3])>=0\",3R:\\'\"1P\"!=a.O&&15.17(a,\"19\")!=\"2s\"&&15.17(a,\"4C\")!=\"1P\"\\',1P:\\'\"1P\"==a.O||15.17(a,\"19\")==\"2s\"||15.17(a,\"4C\")==\"1P\"\\',7U:\"!a.3c\",3c:\"a.3c\",2Q:\"a.2Q\",26:\"a.26||15.1x(a,\\'26\\')\",2g:\"\\'2g\\'==a.O\",4k:\"\\'4k\\'==a.O\",5j:\"\\'5j\\'==a.O\",54:\"\\'54\\'==a.O\",52:\"\\'52\\'==a.O\",51:\"\\'51\\'==a.O\",6d:\"\\'6d\\'==a.O\",6c:\"\\'6c\\'==a.O\",2r:\\'\"2r\"==a.O||15.11(a,\"2r\")\\',4t:\"/4t|24|6b|2r/i.14(a.11)\",3K:\"15.1Y(m[3],a).K\",7S:\"/h\\\\\\\\d/i.14(a.11)\",7R:\"15.2W(15.32,G(1b){I a==1b.T;}).K\"}},6a:[/^(\\\\[) *@?([\\\\w-]+) *([!*$^~=]*) *(\\'?\"?)(.*?)\\\\4 *\\\\]/,/^(:)([\\\\w-]+)\\\\(\"?\\'?(.*?(\\\\(.*?\\\\))?[^(]*?)\"?\\'?\\\\)/,1u 47(\"^([:.#]*)(\"+C+\"+)\")],3m:G(a,c,b){H d,2b=[];1W(a&&a!=d){d=a;H f=E.1E(a,c,b);a=f.t.1p(/^\\\\s*,\\\\s*/,\"\");2b=b?c=f.r:E.1R(2b,f.r)}I 2b},1Y:G(t,o){9(1m t!=\"1M\")I[t];9(o&&!o.1y)o=S;o=o||U;H d=[o],2f=[],3u;1W(t&&3u!=t){H r=[];3u=t;t=E.36(t);H l=P;H g=6k;H m=g.2S(t);9(m){H p=m[1].27();L(H i=0;d[i];i++)L(H c=d[i].1w;c;c=c.2q)9(c.1y==1&&(p==\"*\"||c.11.27()==p.27()))r.1a(c);d=r;t=t.1p(g,\"\");9(t.1g(\" \")==0)6r;l=Q}J{g=/^([>+~])\\\\s*(\\\\w*)/i;9((m=g.2S(t))!=S){r=[];H p=m[2],1R={};m=m[1];L(H j=0,31=d.K;j<31;j++){H n=m==\"~\"||m==\"+\"?d[j].2q:d[j].1w;L(;n;n=n.2q)9(n.1y==1){H h=E.M(n);9(m==\"~\"&&1R[h])1T;9(!p||n.11.27()==p.27()){9(m==\"~\")1R[h]=Q;r.1a(n)}9(m==\"+\")1T}}d=r;t=E.36(t.1p(g,\"\"));l=Q}}9(t&&!l){9(!t.1g(\",\")){9(o==d[0])d.44();2f=E.1R(2f,d);r=d=[o];t=\" \"+t.68(1,t.K)}J{H k=6i;H m=k.2S(t);9(m){m=[0,m[2],m[3],m[1]]}J{k=6h;m=k.2S(t)}m[2]=m[2].1p(/\\\\\\\\/g,\"\");H f=d[d.K-1];9(m[1]==\"#\"&&f&&f.3S&&!E.4a(f)){H q=f.3S(m[2]);9((E.V.1h||E.V.34)&&q&&1m q.22==\"1M\"&&q.22!=m[2])q=E(\\'[@22=\"\\'+m[2]+\\'\"]\\',f)[0];d=r=q&&(!m[3]||E.11(q,m[3]))?[q]:[]}J{L(H i=0;d[i];i++){H a=m[1]==\"#\"&&m[3]?m[3]:m[1]!=\"\"||m[0]==\"\"?\"*\":m[2];9(a==\"*\"&&d[i].11.2p()==\"5i\")a=\"3a\";r=E.1R(r,d[i].4l(a))}9(m[1]==\".\")r=E.4X(r,m[2]);9(m[1]==\"#\"){H e=[];L(H i=0;r[i];i++)9(r[i].4p(\"22\")==m[2]){e=[r[i]];1T}r=e}d=r}t=t.1p(k,\"\")}}9(t){H b=E.1E(t,r);d=r=b.r;t=E.36(b.t)}}9(t)d=[];9(d&&o==d[0])d.44();2f=E.1R(2f,d);I 2f},4X:G(r,m,a){m=\" \"+m+\" \";H c=[];L(H i=0;r[i];i++){H b=(\" \"+r[i].1o+\" \").1g(m)>=0;9(!a&&b||a&&!b)c.1a(r[i])}I c},1E:G(t,r,h){H d;1W(t&&t!=d){d=t;H p=E.6a,m;L(H i=0;p[i];i++){m=p[i].2S(t);9(m){t=t.7O(m[0].K);m[2]=m[2].1p(/\\\\\\\\/g,\"\");1T}}9(!m)1T;9(m[1]==\":\"&&m[2]==\"5V\")r=E.1E(m[3],r,Q).r;J 9(m[1]==\".\")r=E.4X(r,m[2],h);J 9(m[1]==\"[\"){H g=[],O=m[3];L(H i=0,31=r.K;i<31;i++){H a=r[i],z=a[E.5o[m[2]]||m[2]];9(z==S||/6C|3k|26/.14(m[2]))z=E.1x(a,m[2])||\\'\\';9((O==\"\"&&!!z||O==\"=\"&&z==m[5]||O==\"!=\"&&z!=m[5]||O==\"^=\"&&z&&!z.1g(m[5])||O==\"$=\"&&z.68(z.K-m[5].K)==m[5]||(O==\"*=\"||O==\"~=\")&&z.1g(m[5])>=0)^h)g.1a(a)}r=g}J 9(m[1]==\":\"&&m[2]==\"2I-46\"){H e={},g=[],14=/(\\\\d*)n\\\\+?(\\\\d*)/.2S(m[3]==\"6f\"&&\"2n\"||m[3]==\"6e\"&&\"2n+1\"||!/\\\\D/.14(m[3])&&\"n+\"+m[3]||m[3]),3v=(14[1]||1)-0,d=14[2]-0;L(H i=0,31=r.K;i<31;i++){H j=r[i],12=j.12,22=E.M(12);9(!e[22]){H c=1;L(H n=12.1w;n;n=n.2q)9(n.1y==1)n.4U=c++;e[22]=Q}H b=P;9(3v==1){9(d==0||j.4U==d)b=Q}J 9((j.4U+d)%3v==0)b=Q;9(b^h)g.1a(j)}r=g}J{H f=E.55[m[1]];9(1m f!=\"1M\")f=E.55[m[1]][m[2]];f=3w(\"P||G(a,i){I \"+f+\"}\");r=E.2W(r,f,h)}}I{r:r,t:t}},4e:G(b,c){H d=[];H a=b[c];1W(a&&a!=U){9(a.1y==1)d.1a(a);a=a[c]}I d},2I:G(a,e,c,b){e=e||1;H d=0;L(;a;a=a[c])9(a.1y==1&&++d==e)1T;I a},5d:G(n,a){H r=[];L(;n;n=n.2q){9(n.1y==1&&(!a||n!=a))r.1a(n)}I r}});E.1j={1f:G(g,e,c,h){9(E.V.1h&&g.4j!=W)g=18;9(!c.2u)c.2u=6.2u++;9(h!=W){H d=c;c=G(){I d.16(6,1q)};c.M=h;c.2u=d.2u}H i=e.2l(\".\");e=i[0];c.O=i[1];H b=E.M(g,\"2P\")||E.M(g,\"2P\",{});H f=E.M(g,\"2t\",G(){H a;9(1m E==\"W\"||E.1j.4T)I a;a=E.1j.2t.16(g,1q);I a});H j=b[e];9(!j){j=b[e]={};9(g.4S)g.4S(e,f,P);J g.7N(\"43\"+e,f)}j[c.2u]=c;6.1Z[e]=Q},2u:1,1Z:{},28:G(d,c,b){H e=E.M(d,\"2P\"),2L,4I;9(1m c==\"1M\"){H a=c.2l(\".\");c=a[0]}9(e){9(c&&c.O){b=c.4Q;c=c.O}9(!c){L(c 1i e)6.28(d,c)}J 9(e[c]){9(b)2E e[c][b.2u];J L(b 1i e[c])9(!a[1]||e[c][b].O==a[1])2E e[c][b];L(2L 1i e[c])1T;9(!2L){9(d.4P)d.4P(c,E.M(d,\"2t\"),P);J d.7M(\"43\"+c,E.M(d,\"2t\"));2L=S;2E e[c]}}L(2L 1i e)1T;9(!2L){E.30(d,\"2P\");E.30(d,\"2t\")}}},1F:G(d,b,e,c,f){b=E.2h(b||[]);9(!e){9(6.1Z[d])E(\"*\").1f([18,U]).1F(d,b)}J{H a,2L,1b=E.1n(e[d]||S),4N=!b[0]||!b[0].2M;9(4N)b.4w(6.4M({O:d,2m:e}));b[0].O=d;9(E.1n(E.M(e,\"2t\")))a=E.M(e,\"2t\").16(e,b);9(!1b&&e[\"43\"+d]&&e[\"43\"+d].16(e,b)===P)a=P;9(4N)b.44();9(f&&f.16(e,b)===P)a=P;9(1b&&c!==P&&a!==P&&!(E.11(e,\\'a\\')&&d==\"4L\")){6.4T=Q;e[d]()}6.4T=P}I a},2t:G(d){H a;d=E.1j.4M(d||18.1j||{});H b=d.O.2l(\".\");d.O=b[0];H c=E.M(6,\"2P\")&&E.M(6,\"2P\")[d.O],3q=1B.3A.2J.2O(1q,1);3q.4w(d);L(H j 1i c){3q[0].4Q=c[j];3q[0].M=c[j].M;9(!b[1]||c[j].O==b[1]){H e=c[j].16(6,3q);9(a!==P)a=e;9(e===P){d.2M();d.3p()}}}9(E.V.1h)d.2m=d.2M=d.3p=d.4Q=d.M=S;I a},4M:G(c){H a=c;c=E.1k({},a);c.2M=G(){9(a.2M)a.2M();a.7L=P};c.3p=G(){9(a.3p)a.3p();a.7K=Q};9(!c.2m&&c.65)c.2m=c.65;9(E.V.1N&&c.2m.1y==3)c.2m=a.2m.12;9(!c.4K&&c.4J)c.4K=c.4J==c.2m?c.7H:c.4J;9(c.64==S&&c.63!=S){H e=U.2V,b=U.1G;c.64=c.63+(e&&e.2R||b.2R||0);c.7E=c.7D+(e&&e.2B||b.2B||0)}9(!c.3Y&&(c.61||c.60))c.3Y=c.61||c.60;9(!c.5F&&c.5D)c.5F=c.5D;9(!c.3Y&&c.2r)c.3Y=(c.2r&1?1:(c.2r&2?3:(c.2r&4?2:0)));I c}};E.1b.1k({3W:G(c,a,b){I c==\"5Y\"?6.2G(c,a,b):6.N(G(){E.1j.1f(6,c,b||a,b&&a)})},2G:G(d,b,c){I 6.N(G(){E.1j.1f(6,d,G(a){E(6).5X(a);I(c||b).16(6,1q)},c&&b)})},5X:G(a,b){I 6.N(G(){E.1j.28(6,a,b)})},1F:G(c,a,b){I 6.N(G(){E.1j.1F(c,a,6,Q,b)})},7x:G(c,a,b){9(6[0])I E.1j.1F(c,a,6[0],P,b)},25:G(){H a=1q;I 6.4L(G(e){6.4H=0==6.4H?1:0;e.2M();I a[6.4H].16(6,[e])||P})},7v:G(f,g){G 4G(e){H p=e.4K;1W(p&&p!=6)2a{p=p.12}29(e){p=6};9(p==6)I P;I(e.O==\"4x\"?f:g).16(6,[e])}I 6.4x(4G).5U(4G)},2d:G(f){5T();9(E.3T)f.16(U,[E]);J E.3l.1a(G(){I f.16(6,[E])});I 6}});E.1k({3T:P,3l:[],2d:G(){9(!E.3T){E.3T=Q;9(E.3l){E.N(E.3l,G(){6.16(U)});E.3l=S}9(E.V.35||E.V.34)U.4P(\"5S\",E.2d,P);9(!18.7t.K)E(18).39(G(){E(\"#4E\").28()})}}});E.N((\"7s,7r,39,7q,6n,5Y,4L,7p,\"+\"7n,7m,7l,4x,5U,7k,24,\"+\"51,7j,7i,7h,3U\").2l(\",\"),G(i,o){E.1b[o]=G(f){I f?6.3W(o,f):6.1F(o)}});H x=P;G 5T(){9(x)I;x=Q;9(E.V.35||E.V.34)U.4S(\"5S\",E.2d,P);J 9(E.V.1h){U.7f(\"<7d\"+\"7y 22=4E 7z=Q \"+\"3k=//:><\\\\/1J>\");H a=U.3S(\"4E\");9(a)a.62=G(){9(6.2C!=\"1l\")I;E.2d()};a=S}J 9(E.V.1N)E.4B=4j(G(){9(U.2C==\"5Q\"||U.2C==\"1l\"){4A(E.4B);E.4B=S;E.2d()}},10);E.1j.1f(18,\"39\",E.2d)}E.1b.1k({39:G(g,d,c){9(E.1n(g))I 6.3W(\"39\",g);H e=g.1g(\" \");9(e>=0){H i=g.2J(e,g.K);g=g.2J(0,e)}c=c||G(){};H f=\"4z\";9(d)9(E.1n(d)){c=d;d=S}J{d=E.3a(d);f=\"5P\"}H h=6;E.3G({1d:g,O:f,M:d,1l:G(a,b){9(b==\"1C\"||b==\"5O\")h.4o(i?E(\"<1s/>\").3g(a.40.1p(/<1J(.|\\\\s)*?\\\\/1J>/g,\"\")).1Y(i):a.40);56(G(){h.N(c,[a.40,b,a])},13)}});I 6},7a:G(){I E.3a(6.5M())},5M:G(){I 6.1X(G(){I E.11(6,\"2Y\")?E.2h(6.79):6}).1E(G(){I 6.2H&&!6.3c&&(6.2Q||/24|6b/i.14(6.11)||/2g|1P|52/i.14(6.O))}).1X(G(i,c){H b=E(6).3i();I b==S?S:b.1c==1B?E.1X(b,G(a,i){I{2H:c.2H,1Q:a}}):{2H:c.2H,1Q:b}}).21()}});E.N(\"5L,5K,6t,5J,5I,5H\".2l(\",\"),G(i,o){E.1b[o]=G(f){I 6.3W(o,f)}});H B=(1u 3D).3B();E.1k({21:G(d,b,a,c){9(E.1n(b)){a=b;b=S}I E.3G({O:\"4z\",1d:d,M:b,1C:a,1V:c})},78:G(b,a){I E.21(b,S,a,\"1J\")},77:G(c,b,a){I E.21(c,b,a,\"45\")},76:G(d,b,a,c){9(E.1n(b)){a=b;b={}}I E.3G({O:\"5P\",1d:d,M:b,1C:a,1V:c})},75:G(a){E.1k(E.59,a)},59:{1Z:Q,O:\"4z\",2z:0,5G:\"74/x-73-2Y-72\",6o:Q,3e:Q,M:S},49:{},3G:G(s){H f,2y=/=(\\\\?|%3F)/g,1v,M;s=E.1k(Q,s,E.1k(Q,{},E.59,s));9(s.M&&s.6o&&1m s.M!=\"1M\")s.M=E.3a(s.M);9(s.1V==\"4b\"){9(s.O.2p()==\"21\"){9(!s.1d.1t(2y))s.1d+=(s.1d.1t(/\\\\?/)?\"&\":\"?\")+(s.4b||\"5E\")+\"=?\"}J 9(!s.M||!s.M.1t(2y))s.M=(s.M?s.M+\"&\":\"\")+(s.4b||\"5E\")+\"=?\";s.1V=\"45\"}9(s.1V==\"45\"&&(s.M&&s.M.1t(2y)||s.1d.1t(2y))){f=\"4b\"+B++;9(s.M)s.M=s.M.1p(2y,\"=\"+f);s.1d=s.1d.1p(2y,\"=\"+f);s.1V=\"1J\";18[f]=G(a){M=a;1C();1l();18[f]=W;2a{2E 18[f]}29(e){}}}9(s.1V==\"1J\"&&s.1L==S)s.1L=P;9(s.1L===P&&s.O.2p()==\"21\")s.1d+=(s.1d.1t(/\\\\?/)?\"&\":\"?\")+\"57=\"+(1u 3D()).3B();9(s.M&&s.O.2p()==\"21\"){s.1d+=(s.1d.1t(/\\\\?/)?\"&\":\"?\")+s.M;s.M=S}9(s.1Z&&!E.5b++)E.1j.1F(\"5L\");9(!s.1d.1g(\"8g\")&&s.1V==\"1J\"){H h=U.4l(\"9U\")[0];H g=U.5B(\"1J\");g.3k=s.1d;9(!f&&(s.1C||s.1l)){H j=P;g.9R=g.62=G(){9(!j&&(!6.2C||6.2C==\"5Q\"||6.2C==\"1l\")){j=Q;1C();1l();h.3b(g)}}}h.58(g);I}H k=P;H i=18.6X?1u 6X(\"9P.9O\"):1u 6W();i.9M(s.O,s.1d,s.3e);9(s.M)i.5C(\"9J-9I\",s.5G);9(s.5y)i.5C(\"9H-5x-9F\",E.49[s.1d]||\"9D, 9C 9B 9A 5v:5v:5v 9z\");i.5C(\"X-9x-9v\",\"6W\");9(s.6U)s.6U(i);9(s.1Z)E.1j.1F(\"5H\",[i,s]);H c=G(a){9(!k&&i&&(i.2C==4||a==\"2z\")){k=Q;9(d){4A(d);d=S}1v=a==\"2z\"&&\"2z\"||!E.6S(i)&&\"3U\"||s.5y&&E.6R(i,s.1d)&&\"5O\"||\"1C\";9(1v==\"1C\"){2a{M=E.6Q(i,s.1V)}29(e){1v=\"5k\"}}9(1v==\"1C\"){H b;2a{b=i.5s(\"6P-5x\")}29(e){}9(s.5y&&b)E.49[s.1d]=b;9(!f)1C()}J E.5r(s,i,1v);1l();9(s.3e)i=S}};9(s.3e){H d=4j(c,13);9(s.2z>0)56(G(){9(i){i.9q();9(!k)c(\"2z\")}},s.2z)}2a{i.9o(s.M)}29(e){E.5r(s,i,S,e)}9(!s.3e)c();I i;G 1C(){9(s.1C)s.1C(M,1v);9(s.1Z)E.1j.1F(\"5I\",[i,s])}G 1l(){9(s.1l)s.1l(i,1v);9(s.1Z)E.1j.1F(\"6t\",[i,s]);9(s.1Z&&!--E.5b)E.1j.1F(\"5K\")}},5r:G(s,a,b,e){9(s.3U)s.3U(a,b,e);9(s.1Z)E.1j.1F(\"5J\",[a,s,e])},5b:0,6S:G(r){2a{I!r.1v&&9n.9l==\"54:\"||(r.1v>=6N&&r.1v<9j)||r.1v==6M||E.V.1N&&r.1v==W}29(e){}I P},6R:G(a,c){2a{H b=a.5s(\"6P-5x\");I a.1v==6M||b==E.49[c]||E.V.1N&&a.1v==W}29(e){}I P},6Q:G(r,b){H c=r.5s(\"9i-O\");H d=b==\"6K\"||!b&&c&&c.1g(\"6K\")>=0;H a=d?r.9g:r.40;9(d&&a.2V.37==\"5k\")6G\"5k\";9(b==\"1J\")E.5f(a);9(b==\"45\")a=3w(\"(\"+a+\")\");I a},3a:G(a){H s=[];9(a.1c==1B||a.4c)E.N(a,G(){s.1a(3f(6.2H)+\"=\"+3f(6.1Q))});J L(H j 1i a)9(a[j]&&a[j].1c==1B)E.N(a[j],G(){s.1a(3f(j)+\"=\"+3f(6))});J s.1a(3f(j)+\"=\"+3f(a[j]));I s.66(\"&\").1p(/%20/g,\"+\")}});E.1b.1k({1A:G(b,a){I b?6.1U({1H:\"1A\",2N:\"1A\",1r:\"1A\"},b,a):6.1E(\":1P\").N(G(){6.R.19=6.3h?6.3h:\"\";9(E.17(6,\"19\")==\"2s\")6.R.19=\"2Z\"}).2D()},1z:G(b,a){I b?6.1U({1H:\"1z\",2N:\"1z\",1r:\"1z\"},b,a):6.1E(\":3R\").N(G(){6.3h=6.3h||E.17(6,\"19\");9(6.3h==\"2s\")6.3h=\"2Z\";6.R.19=\"2s\"}).2D()},6J:E.1b.25,25:G(a,b){I E.1n(a)&&E.1n(b)?6.6J(a,b):a?6.1U({1H:\"25\",2N:\"25\",1r:\"25\"},a,b):6.N(G(){E(6)[E(6).3t(\":1P\")?\"1A\":\"1z\"]()})},9c:G(b,a){I 6.1U({1H:\"1A\"},b,a)},9b:G(b,a){I 6.1U({1H:\"1z\"},b,a)},99:G(b,a){I 6.1U({1H:\"25\"},b,a)},98:G(b,a){I 6.1U({1r:\"1A\"},b,a)},96:G(b,a){I 6.1U({1r:\"1z\"},b,a)},95:G(c,a,b){I 6.1U({1r:a},c,b)},1U:G(k,i,h,g){H j=E.6D(i,h,g);I 6[j.3L===P?\"N\":\"3L\"](G(){j=E.1k({},j);H f=E(6).3t(\":1P\"),3y=6;L(H p 1i k){9(k[p]==\"1z\"&&f||k[p]==\"1A\"&&!f)I E.1n(j.1l)&&j.1l.16(6);9(p==\"1H\"||p==\"2N\"){j.19=E.17(6,\"19\");j.2U=6.R.2U}}9(j.2U!=S)6.R.2U=\"1P\";j.3M=E.1k({},k);E.N(k,G(c,a){H e=1u E.2j(3y,j,c);9(/25|1A|1z/.14(a))e[a==\"25\"?f?\"1A\":\"1z\":a](k);J{H b=a.3s().1t(/^([+-]=)?([\\\\d+-.]+)(.*)$/),1O=e.2b(Q)||0;9(b){H d=3I(b[2]),2i=b[3]||\"2T\";9(2i!=\"2T\"){3y.R[c]=(d||1)+2i;1O=((d||1)/e.2b(Q))*1O;3y.R[c]=1O+2i}9(b[1])d=((b[1]==\"-=\"?-1:1)*d)+1O;e.3N(1O,d,2i)}J e.3N(1O,a,\"\")}});I Q})},3L:G(a,b){9(E.1n(a)){b=a;a=\"2j\"}9(!a||(1m a==\"1M\"&&!b))I A(6[0],a);I 6.N(G(){9(b.1c==1B)A(6,a,b);J{A(6,a).1a(b);9(A(6,a).K==1)b.16(6)}})},9f:G(){H a=E.32;I 6.N(G(){L(H i=0;i<a.K;i++)9(a[i].T==6)a.6I(i--,1)}).5n()}});H A=G(b,c,a){9(!b)I;H q=E.M(b,c+\"3L\");9(!q||a)q=E.M(b,c+\"3L\",a?E.2h(a):[]);I q};E.1b.5n=G(a){a=a||\"2j\";I 6.N(G(){H q=A(6,a);q.44();9(q.K)q[0].16(6)})};E.1k({6D:G(b,a,c){H d=b&&b.1c==8Z?b:{1l:c||!c&&a||E.1n(b)&&b,2e:b,3J:c&&a||a&&a.1c!=8Y&&a};d.2e=(d.2e&&d.2e.1c==4W?d.2e:{8X:8W,8V:6N}[d.2e])||8T;d.3r=d.1l;d.1l=G(){E(6).5n();9(E.1n(d.3r))d.3r.16(6)};I d},3J:{6B:G(p,n,b,a){I b+a*p},5q:G(p,n,b,a){I((-38.9s(p*38.8R)/2)+0.5)*a+b}},32:[],2j:G(b,c,a){6.Y=c;6.T=b;6.1e=a;9(!c.3P)c.3P={}}});E.2j.3A={4r:G(){9(6.Y.2F)6.Y.2F.16(6.T,[6.2v,6]);(E.2j.2F[6.1e]||E.2j.2F.6z)(6);9(6.1e==\"1H\"||6.1e==\"2N\")6.T.R.19=\"2Z\"},2b:G(a){9(6.T[6.1e]!=S&&6.T.R[6.1e]==S)I 6.T[6.1e];H r=3I(E.3C(6.T,6.1e,a));I r&&r>-8O?r:3I(E.17(6.T,6.1e))||0},3N:G(c,b,e){6.5u=(1u 3D()).3B();6.1O=c;6.2D=b;6.2i=e||6.2i||\"2T\";6.2v=6.1O;6.4q=6.4i=0;6.4r();H f=6;G t(){I f.2F()}t.T=6.T;E.32.1a(t);9(E.32.K==1){H d=4j(G(){H a=E.32;L(H i=0;i<a.K;i++)9(!a[i]())a.6I(i--,1);9(!a.K)4A(d)},13)}},1A:G(){6.Y.3P[6.1e]=E.1x(6.T.R,6.1e);6.Y.1A=Q;6.3N(0,6.2b());9(6.1e==\"2N\"||6.1e==\"1H\")6.T.R[6.1e]=\"8N\";E(6.T).1A()},1z:G(){6.Y.3P[6.1e]=E.1x(6.T.R,6.1e);6.Y.1z=Q;6.3N(6.2b(),0)},2F:G(){H t=(1u 3D()).3B();9(t>6.Y.2e+6.5u){6.2v=6.2D;6.4q=6.4i=1;6.4r();6.Y.3M[6.1e]=Q;H a=Q;L(H i 1i 6.Y.3M)9(6.Y.3M[i]!==Q)a=P;9(a){9(6.Y.19!=S){6.T.R.2U=6.Y.2U;6.T.R.19=6.Y.19;9(E.17(6.T,\"19\")==\"2s\")6.T.R.19=\"2Z\"}9(6.Y.1z)6.T.R.19=\"2s\";9(6.Y.1z||6.Y.1A)L(H p 1i 6.Y.3M)E.1x(6.T.R,p,6.Y.3P[p])}9(a&&E.1n(6.Y.1l))6.Y.1l.16(6.T);I P}J{H n=t-6.5u;6.4i=n/6.Y.2e;6.4q=E.3J[6.Y.3J||(E.3J.5q?\"5q\":\"6B\")](6.4i,n,0,1,6.Y.2e);6.2v=6.1O+((6.2D-6.1O)*6.4q);6.4r()}I Q}};E.2j.2F={2R:G(a){a.T.2R=a.2v},2B:G(a){a.T.2B=a.2v},1r:G(a){E.1x(a.T.R,\"1r\",a.2v)},6z:G(a){a.T.R[a.1e]=a.2v+a.2i}};E.1b.6m=G(){H c=0,3E=0,T=6[0],5t;9(T)8L(E.V){H b=E.17(T,\"2X\")==\"4F\",1D=T.12,23=T.23,2K=T.3H,4f=1N&&3x(4s)<8J;9(T.6V){5w=T.6V();1f(5w.1S+38.33(2K.2V.2R,2K.1G.2R),5w.3E+38.33(2K.2V.2B,2K.1G.2B));9(1h){H d=E(\"4o\").17(\"8H\");d=(d==\"8G\"||E.5g&&3x(4s)>=7)&&2||d;1f(-d,-d)}}J{1f(T.5l,T.5z);1W(23){1f(23.5l,23.5z);9(35&&/^t[d|h]$/i.14(1D.37)||!4f)d(23);9(4f&&!b&&E.17(23,\"2X\")==\"4F\")b=Q;23=23.23}1W(1D.37&&!/^1G|4o$/i.14(1D.37)){9(!/^8D|1I-9S.*$/i.14(E.17(1D,\"19\")))1f(-1D.2R,-1D.2B);9(35&&E.17(1D,\"2U\")!=\"3R\")d(1D);1D=1D.12}9(4f&&b)1f(-2K.1G.5l,-2K.1G.5z)}5t={3E:3E,1S:c}}I 5t;G d(a){1f(E.17(a,\"9T\"),E.17(a,\"8A\"))}G 1f(l,t){c+=3x(l)||0;3E+=3x(t)||0}}})();',62,616,'||||||this|||if|||||||||||||||||||||||||||||||||function|var|return|else|length|for|data|each|type|false|true|style|null|elem|document|browser|undefined||options|||nodeName|parentNode||test|jQuery|apply|css|window|display|push|fn|constructor|url|prop|add|indexOf|msie|in|event|extend|complete|typeof|isFunction|className|replace|arguments|opacity|div|match|new|status|firstChild|attr|nodeType|hide|show|Array|success|parent|filter|trigger|body|height|table|script|tbody|cache|string|safari|start|hidden|value|merge|left|break|animate|dataType|while|map|find|global||get|id|offsetParent|select|toggle|selected|toUpperCase|remove|catch|try|cur|al|ready|duration|done|text|makeArray|unit|fx|swap|split|target||pushStack|toLowerCase|nextSibling|button|none|handle|guid|now|stack|tb|jsre|timeout|inArray|scrollTop|readyState|end|delete|step|one|name|nth|slice|doc|ret|preventDefault|width|call|events|checked|scrollLeft|exec|px|overflow|documentElement|grep|position|form|block|removeData|rl|timers|max|opera|mozilla|trim|tagName|Math|load|param|removeChild|disabled|insertBefore|async|encodeURIComponent|append|oldblock|val|childNodes|src|readyList|multiFilter|color|defaultView|stopPropagation|args|old|toString|is|last|first|eval|parseInt|self|domManip|prototype|getTime|curCSS|Date|top||ajax|ownerDocument|parseFloat|easing|has|queue|curAnim|custom|innerHTML|orig|currentStyle|visible|getElementById|isReady|error|static|bind|String|which|getComputedStyle|responseText|oWidth|oHeight|on|shift|json|child|RegExp|ol|lastModified|isXMLDoc|jsonp|jquery|previousSibling|dir|safari2|el|styleFloat|state|setInterval|radio|getElementsByTagName|tr|empty|html|getAttribute|pos|update|version|input|float|runtimeStyle|unshift|mouseover|getPropertyValue|GET|clearInterval|safariTimer|visibility|clean|__ie_init|absolute|handleHover|lastToggle|index|fromElement|relatedTarget|click|fix|evt|andSelf|removeEventListener|handler|cloneNode|addEventListener|triggered|nodeIndex|unique|Number|classFilter|prevObject|selectedIndex|after|submit|password|removeAttribute|file|expr|setTimeout|_|appendChild|ajaxSettings|client|active|win|sibling|deep|globalEval|boxModel|cssFloat|object|checkbox|parsererror|offsetLeft|wrapAll|dequeue|props|lastChild|swing|handleError|getResponseHeader|results|startTime|00|box|Modified|ifModified|offsetTop|evalScript|createElement|setRequestHeader|ctrlKey|callback|metaKey|contentType|ajaxSend|ajaxSuccess|ajaxError|ajaxStop|ajaxStart|serializeArray|init|notmodified|POST|loaded|appendTo|DOMContentLoaded|bindReady|mouseout|not|removeAttr|unbind|unload|Width|keyCode|charCode|onreadystatechange|clientX|pageX|srcElement|join|outerHTML|substr|zoom|parse|textarea|reset|image|odd|even|before|quickClass|quickID|prepend|quickChild|execScript|offset|scroll|processData|uuid|contents|continue|textContent|ajaxComplete|clone|setArray|webkit|nodeValue|fl|_default|100|linear|href|speed|eq|createTextNode|throw|replaceWith|splice|_toggle|xml|colgroup|304|200|alpha|Last|httpData|httpNotModified|httpSuccess|fieldset|beforeSend|getBoundingClientRect|XMLHttpRequest|ActiveXObject|col|br|abbr|pixelLeft|urlencoded|www|application|ajaxSetup|post|getJSON|getScript|elements|serialize|clientWidth|hasClass|scr|clientHeight|write|relative|keyup|keypress|keydown|change|mousemove|mouseup|mousedown|right|dblclick|resize|focus|blur|frames|instanceof|hover|offsetWidth|triggerHandler|ipt|defer|offsetHeight|border|padding|clientY|pageY|Left|Right|toElement|Bottom|Top|cancelBubble|returnValue|detachEvent|attachEvent|substring|line|weight|animated|header|font|enabled|innerText|contains|only|size|gt|lt|uFFFF|u0128|417|inner|Height|toggleClass|removeClass|addClass|replaceAll|noConflict|insertAfter|prependTo|wrap|contentWindow|contentDocument|http|iframe|children|siblings|prevAll|nextAll|wrapInner|prev|Boolean|next|parents|maxLength|maxlength|readOnly|readonly|class|htmlFor|CSS1Compat|compatMode|compatible|borderTopWidth|ie|ra|inline|it|rv|medium|borderWidth|userAgent|522|navigator|with|concat|1px|10000|array|ig|PI|NaN|400|reverse|fast|600|slow|Function|Object|setAttribute|changed|be|can|property|fadeTo|fadeOut|getAttributeNode|fadeIn|slideToggle|method|slideUp|slideDown|action|cssText|stop|responseXML|option|content|300|th|protocol|td|location|send|cap|abort|colg|cos|tfoot|thead|With|leg|Requested|opt|GMT|1970|Jan|01|Thu|area|Since|hr|If|Type|Content|meta|specified|open|link|XMLHTTP|Microsoft|img|onload|row|borderLeftWidth|head|attributes'.split('|'),0,{});\n\
\n\
result += decompressedJQuery.length;\n\
\n\
/*\n\
    Copyright (c) 2004-2007, The Dojo Foundation\n\
    All Rights Reserved.\n\
\n\
    Licensed under the Academic Free License version 2.1 or above OR the\n\
    modified BSD license. For more information on Dojo licensing, see:\n\
\n\
        http://dojotoolkit.org/community/licensing.shtml\n\
*/\n\
\n\
/*\n\
    This is a compiled version of Dojo, built for deployment and not for\n\
    development. To get an editable version, please visit:\n\
\n\
        http://dojotoolkit.org\n\
\n\
    for documentation and information on getting the source.\n\
*/\n\
\n\
var decompressedDojo = function(p,a,c,k,e,d){e=function(c){return(c<a?\"\":e(parseInt(c/a)))+((c=c%a)>35?String.fromCharCode(c+29):c.toString(36))};if(!''.replace(/^/,String)){while(c--)d[e(c)]=k[c]||e(c);k=[function(e){return d[e]}];e=function(){return'\\\\w+'};c=1};while(c--)if(k[c])p=p.replace(new RegExp('\\\\b'+e(c)+'\\\\b','g'),k[c]);return p}('if(V z==\"1k\"){(B(){if(V D[\"1o\"]==\"1k\"){D.1o={}}if((!D[\"1z\"])||(!1z[\"ca\"])){D.1z={}}A cn=[\"rA\",\"rz\",\"1K\",\"ry\",\"rx\",\"9f\",\"rw\",\"rv\",\"ru\",\"rt\",\"rs\",\"rr\",\"rq\",\"ro\",\"rn\",\"rm\"];A i=0,24;1s(24=cn[i++]){if(!1z[24]){1z[24]=B(){}}}if(V D[\"z\"]==\"1k\"){D.z={}}z.1W=D;A d3={im:U,rl:U,rk:\"\",rj:\"\",ri:\"\",rh:K,rg:U};R(A 8z in d3){if(V 1o[8z]==\"1k\"){1o[8z]=d3[8z]}}A jK=[\"rf\",\"rd\",\"rc\",\"rb\"];A t;1s(t=jK.3a()){z[\"is\"+t]=U}})();z.8h=1o.8h;z.cY={jJ:0,jI:9,jH:0,jG:\"\",jF:2V(\"$ra: r9 $\".1f(/[0-9]+/)[0]),2i:B(){4G(z.cY){C jJ+\".\"+jI+\".\"+jH+jG+\" (\"+jF+\")\"}}};z.d1=B(jE,jD,1V){A 2h=1V||z.1W;R(A i=0,p;2h&&(p=jE[i]);i++){2h=(p in 2h?2h[p]:(jD?2h[p]={}:1k))}C 2h};z.88=B(jC,jA,jB){A d2=jC.1A(\".\"),p=d2.8q(),M=z.d1(d2,K,jB);C(M&&p?(M[p]=jA):1k)};z.6q=B(jz,jy,jx){C z.d1(jz.1A(\".\"),jy,jx)};z.r8=B(jw,M){C!!z.6q(jw,U,M)};z[\"3u\"]=B(d0){C z.1W.3u?z.1W.3u(d0):3u(d0)};z.ia=B(jv,cZ,cX){A 8y=\"r7: \"+jv;if(cZ){8y+=\" \"+cZ}if(cX){8y+=\" -- r6 be r5 in cY: \"+cX}1z.1K(8y)};z.r4=B(ju,cW){A cV=\"r3: \"+ju+\" -- r2 r1 4F r0 qZ qY.\";if(cW){cV+=\" \"+cW}1z.1K(cV)};(B(){A cR={53:{},6p:0,1h:{},8k:{z:{1p:\"z\",1Z:\".\"},cU:{1p:\"cU\",1Z:\"../qX/cU\"},cT:{1p:\"cT\",1Z:\"cT\"}},cN:B(cS){A mp=D.8k;C jp(mp[cS]&&mp[cS].1Z)},jk:B(8x){A mp=D.8k;if(D.cN(8x)){C mp[8x].1Z}C 8x},8v:[],6t:U,56:[],8t:[],8u:U};R(A cQ in cR){z[cQ]=cR[cQ]}})();z.jg=B(8w,cP,cb){A 1g=(((8w.2s(0)==\"/\"||8w.1f(/^\\\\w+:/)))?\"\":D.51)+8w;if(1o.jt&&z.c8){1g+=\"?\"+67(1o.jt).2f(/\\\\W+/g,\"\")}1u{C!cP?D.cO(1g,cb):D.jq(1g,cP,cb)}1y(e){1z.1K(e);C U}};z.cO=B(1g,cb){if(D.8v[1g]){C K}A 6u=D.iR(1g,K);if(!6u){C U}D.8v[1g]=K;D.8v.Y(1g);if(cb){6u=\"(\"+6u+\")\"}A jr=z[\"3u\"](6u+\"\\\\r\\\\n//@ qW=\"+1g);if(cb){cb(jr)}C K};z.jq=B(1g,jo,cb){A ok=U;1u{ok=D.cO(1g,cb)}1y(e){1z.1K(\"qV je \",1g,\" 4G 9f: \",e)}C jp(ok&&D.53[jo])};z.6m=B(){D.8u=K;D.6t=K;A 57=D.56;D.56=[];R(A x=0;x<57.G;x++){57[x]()}D.8u=U;if(z.6t&&z.6p==0&&D.56.G>0){z.8s()}};z.ck=B(){A 57=D.8t;1s(57.G){(57.8q())()}};z.qU=B(M,jn){A d=z;if(P.G==1){d.56.Y(M)}I{if(P.G>1){d.56.Y(B(){M[jn]()})}}if(d.6t&&d.6p==0&&!d.8u){d.8s()}};z.dW=B(M,jm){A d=z;if(P.G==1){d.8t.Y(M)}I{if(P.G>1){d.8t.Y(B(){M[jm]()})}}};z.iM=B(){if(D.6t){C}if(D.6p>0){1z.1K(\"qT qS in qR!\");C}z.8s()};z.8s=B(){if(V 5c==\"8b\"||(1o[\"qQ\"]&&z.2M)){5c(\"z.6m();\",0)}I{z.6m()}};z.cF=B(jl){A 4v=jl.1A(\".\");R(A i=4v.G;i>0;i--){A 8r=4v.2w(0,i).22(\".\");if((i==1)&&!D.cN(8r)){4v[0]=\"../\"+4v[0]}I{A cM=D.jk(8r);if(cM!=8r){4v.3S(0,i,cM);3f}}}C 4v};z.jj=U;z.8m=B(2T,qP,55){55=D.jj||55;A 54=D.53[2T];if(54){C 54}A cL=2T.1A(\".\");A 3L=D.cF(2T);A jh=((3L[0].2s(0)!=\"/\")&&!3L[0].1f(/^\\\\w+:/));A ji=3L[3L.G-1];A 3m;if(ji==\"*\"){2T=cL.2w(0,-1).22(\".\");3L.8q();3m=3L.22(\"/\")+\"/\"+(1o[\"qO\"]||\"qN\")+\".js\";if(jh&&3m.2s(0)==\"/\"){3m=3m.2w(1)}}I{3m=3L.22(\"/\")+\".js\";2T=cL.22(\".\")}A jf=(!55)?2T:L;A ok=D.jg(3m,jf);if((!ok)&&(!55)){2m S 1O(\"qM 3O 4E \\'\"+2T+\"\\'; 72 qL \\'\"+3m+\"\\'\")}if((!55)&&(!D[\"qK\"])){54=D.53[2T];if(!54){2m S 1O(\"qJ \\'\"+2T+\"\\' is 3O qI a8 je \\'\"+3m+\"\\'\")}}C 54};z.8c=z.8m;z.1Q=B(cK){A cJ=cK+\"\";A 8p=cJ;A 6s=cK.1A(/\\\\./);if(6s[6s.G-1]==\"*\"){6s.8q();8p=6s.22(\".\")}A 8o=z.6q(8p,K);D.53[cJ]=8o;D.53[8p]=8o;C 8o};z.qH=B(8n){A jd=8n[\"qG\"]||[];A cI=jd.3U(8n[z.j4]||8n[\"aY\"]||[]);R(A x=0;x<cI.G;x++){A 8l=cI[x];if(8l.1P==4e){z.8m.14(z,8l)}I{z.8m(8l)}}};z.jb=B(jc,qF){if(jc===K){A cH=[];R(A i=1;i<P.G;i++){cH.Y(P[i])}z.8c.14(z,cH)}};z.qE=z.jb;z.io=B(cG,ja){D.8k[cG]={1p:cG,1Z:ja}};z.qD=B(qC,qB,qA,qz){z.8c(\"z.j9\");z.j9.qy.14(z.qx,P)};(B(){A j7=S 9G(\"^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\\\\\\\?([^#]*))?(#(.*))?$\");A j6=S 9G(\"^((([^:]+:)?([^@]+))@)?([^:]*)(:([0-9]+))?$\");z.4r=B(){A n=L;A 1V=P;A 1g=1V[0];R(A i=1;i<1V.G;i++){if(!1V[i]){6c}A 1t=S z.4r(1V[i]+\"\");A 4u=S z.4r(1g+\"\");if((1t.28==\"\")&&(!1t.4t)&&(!1t.3l)&&(!1t.1r)){if(1t.52!=n){4u.52=1t.52}1t=4u}I{if(!1t.4t){1t.4t=4u.4t;if(!1t.3l){1t.3l=4u.3l;if(1t.28.2s(0)!=\"/\"){A j8=4u.28.21(0,4u.28.31(\"/\")+1)+1t.28;A 1X=j8.1A(\"/\");R(A j=0;j<1X.G;j++){if(1X[j]==\".\"){if(j==1X.G-1){1X[j]=\"\"}I{1X.3S(j,1);j--}}I{if(j>0&&!(j==1&&1X[0]==\"\")&&1X[j]==\"..\"&&1X[j-1]!=\"..\"){if(j==(1X.G-1)){1X.3S(j,1);1X[j-1]=\"\"}I{1X.3S(j-1,2);j-=2}}}}1t.28=1X.22(\"/\")}}}}1g=\"\";if(1t.4t){1g+=1t.4t+\":\"}if(1t.3l){1g+=\"//\"+1t.3l}1g+=1t.28;if(1t.1r){1g+=\"?\"+1t.1r}if(1t.52){1g+=\"#\"+1t.52}}D.1g=1g.2i();A r=D.1g.1f(j7);D.4t=r[2]||(r[1]?\"\":n);D.3l=r[4]||(r[3]?\"\":n);D.28=r[5];D.1r=r[7]||(r[6]?\"\":n);D.52=r[9]||(r[8]?\"\":n);if(D.3l!=n){r=D.3l.1f(j6);D.8X=r[3]||n;D.8W=r[4]||n;D.qw=r[5];D.qv=r[7]||n}};z.4r.1C.2i=B(){C D.1g}})();z.qu=B(j5,2E){A 2B=z.cF(j5).22(\"/\");if(!2B){C L}if(2B.31(\"/\")!=2B.G-1){2B+=\"/\"}A cE=2B.T(\":\");if(2B.2s(0)!=\"/\"&&(cE==-1||cE>2B.T(\"/\"))){2B=z.51+2B}C S z.4r(2B,2E)};if(V 26!=\"1k\"){z.c8=K;z.j4=\"qt\";(B(){A d=z;if(1q&&1q.4I){A 8j=1q.4I(\"ak\");A j3=/z(\\\\.qs)?\\\\.js([\\\\?\\\\.]|$)/i;R(A i=0;i<8j.G;i++){A 4X=8j[i].5t(\"4X\");if(!4X){6c}A m=4X.1f(j3);if(m){if(!1o[\"51\"]){1o[\"51\"]=4X.21(0,m.hK)}A cD=8j[i].5t(\"1o\");if(cD){A cC=3u(\"({ \"+cD+\" })\");R(A x in cC){1o[x]=cC[x]}}3f}}}d.51=1o[\"51\"];A n=cq;A 8i=n.iL;A 4Z=n.qr;A 6r=2k(4Z);d.2M=(8i.T(\"qq\")>=0)?6r:0;d.6B=(4Z.T(\"qo\")>=0)||(4Z.T(\"j2\")>=0)?6r:0;d.3o=(4Z.T(\"j2\")>=0)?6r:0;A j1=8i.T(\"qn\");d.gu=d.7B=((j1>=0)&&(!d.6B))?6r:0;d.j0=0;d.1l=0;d.iV=0;1u{if(d.7B){d.j0=2k(8i.1A(\"qm/\")[1].1A(\" \")[0])}if((1q.gx)&&(!d.2M)){d.1l=2k(4Z.1A(\"qk \")[1].1A(\";\")[0])}}1y(e){}if(z.1l&&(26.8f.cu===\"9q:\")){1o.iT=K}d.iX=B(){A 2A;A qj;A cB=d.6q(\"cz.cy\");if(cB){C cB}if(V iZ!=\"1k\"){2A=S iZ()}I{if(d.1l){1u{2A=S 9j(\"qi.qh\")}1y(e){}}I{if(cq.qg[\"8Z/x-iY\"]){2A=1q.a9(\"8b\");2A.cA(\"Z\",\"8Z/x-iY\");2A.cA(\"3n\",0);2A.cA(\"58\",0);2A.1c.gq=\"7C\";1q.5K.4c(2A)}}}if(!2A){C L}z.88(\"cz.cy.qf\",2A);C z.6q(\"cz.cy\")};A iW=d.iX();if(iW){d.iV=K}A cm=1q[\"aX\"];d.qe=(cm==\"aW\")||(cm==\"gr\")||(d.1l<6);d.8h=1o.8h||(d.1l?n.qd:n.qc).1M();d.qb=1z.1K;d.cx=[\"iU.8g\",\"em.8g\",\"iU.8g.4.0\"];d.9b=B(){A 4s=L;A cv=L;if(!z.1l||!1o.iT){1u{4s=S qa()}1y(e){}}if(!4s){R(A i=0;i<3;++i){A cw=z.cx[i];1u{4s=S 9j(cw)}1y(e){cv=e}if(4s){z.cx=[cw];3f}}}if(!4s){2m S 1O(\"8g 3O q9: \"+cv)}C 4s};d.8Y=B(iS){A 4Y=iS.3N||0;C((4Y>=q8)&&(4Y<q7))||(4Y==q6)||(4Y==q5)||(!4Y&&(8f.cu==\"9q:\"||8f.cu==\"q4:\"))};A cs=1q.4I(\"q3\");A iQ=(cs&&cs.G>0);d.iR=B(1g,iP){A 3K=D.9b();if(!iQ&&z.4r){1g=(S z.4r(26.8f,1g)).2i()}3K.dL(\"dD\",1g,U);1u{3K.dI(L);if(!d.8Y(3K)){A 1G=1O(\"q2 4F 4E \"+1g+\" 3N:\"+3K.3N);1G.3N=3K.3N;1G.2G=3K.2G;2m 1G}}1y(e){if(iP){C L}2m e}C 3K.2G}})();z.iO=U;z.6o=B(e){z.iO=K;A cr=(e&&e.Z)?e.Z.1M():\"4E\";if(P.2O.iN||(cr!=\"q1\"&&cr!=\"4E\")){C}P.2O.iN=K;if(V z[\"8e\"]!=\"1k\"){dX(z.8e);63 z.8e}if(z.6p==0){z.iM()}};if(1q.66){if(z.2M||(z.7B&&(1o[\"q0\"]===K))){1q.66(\"pZ\",z.6o,L)}26.66(\"4E\",z.6o,L)}if(/(pY|pX)/i.6Z(cq.iL)){z.8e=dN(B(){if(/6m|iJ/.6Z(1q.6F)){z.6o()}},10)}(B(){A 3g=26;A 8d=B(cp,fp){A iK=3g[cp]||B(){};3g[cp]=B(){fp.14(3g,P);iK.14(3g,P)}};if(z.1l){1q.fJ(\"<iI\"+\"iH pW 4X=\\\\\"//:\\\\\" \"+\"pV=\\\\\"if(D.6F==\\'iJ\\'){z.6o();}\\\\\">\"+\"</iI\"+\"iH>\");A co=K;8d(\"iG\",B(){3g.5c(B(){co=U},0)});8d(\"pU\",B(){if(co){z.ck()}});1u{1q.pT.2P(\"v\",\"pS:pR-pQ-pP:pO\");1q.pN().pM(\"v\\\\\\\\:*\",\"pL:2E(#aY#pK)\")}1y(e){}}I{8d(\"iG\",B(){z.ck()})}})();z.pJ=B(){};z.1e=26[\"1q\"]||L;z.3E=B(){C z.1e.3E||z.1e.4I(\"3E\")[0]};z.ch=B(iF,iE){z.1W=iF;z.1e=iE};z.cf=B(4q,6n,iD){if((6n)&&((V 4q==\"3c\")||(4q 1N 67))){4q=6n[4q]}C(6n?4q.14(6n,iD||[]):4q())};z.pI=B(cj,iC,iB,iA){A cg;A iz=z.1W;A iy=z.1e;1u{z.ch(cj,cj.1q);cg=z.cf(iC,iB,iA)}ir{z.ch(iz,iy)}C cg};z.pH=B(ix,iw,iv,iu){A ce;A ip=z.1e;1u{z.1e=ix;ce=z.cf(iw,iv,iu)}ir{z.1e=ip}C ce};if(1o[\"cd\"]){R(A cc in 1o[\"cd\"]){z.io(cc,1o[\"cd\"][cc])}}}if(1o.im){if(!1z.ca){z.8c(\"z.pG.ca\")}}}if(!z.1h[\"z.X.c9\"]){z.1h[\"z.X.c9\"]=K;z.1Q(\"z.X.c9\");z.1R=B(it){C(V it==\"3c\"||it 1N 67)};z.2l=B(it){C(it&&it 1N 4e||V it==\"6a\"||((V z[\"1H\"]!=\"1k\")&&(it 1N z.1H)))};if(z.c8&&z.3o){z.1Y=B(it){if((V(it)==\"B\")&&(it==\"[8b 1H]\")){C U}C(V it==\"B\"||it 1N bI)}}I{z.1Y=B(it){C(V it==\"B\"||it 1N bI)}}z.ib=B(it){if(V it==\"1k\"){C U}C(it===L||V it==\"8b\"||z.2l(it)||z.1Y(it))};z.pF=B(it){A d=z;if((!it)||(V it==\"1k\")){C U}if(d.1R(it)){C U}if(d.1Y(it)){C U}if(d.2l(it)){C K}if((it.5w)&&(it.5w.1M()==\"3R\")){C U}if(pE(it.G)){C K}C U};z.pD=B(it){if(!it){C U}C!z.1Y(it)&&/\\\\{\\\\s*\\\\[il 5h\\\\]\\\\s*\\\\}/.6Z(67(it))};z.c7=B(M,4W){A 8a={};R(A x in 4W){if((V 8a[x]==\"1k\")||(8a[x]!=4W[x])){M[x]=4W[x]}}if(z.1l){A p=4W.2i;if((V(p)==\"B\")&&(p!=M.2i)&&(p!=8a.2i)&&(p!=\"\\\\pC 2i() {\\\\n    [il 5h]\\\\n}\\\\n\")){M.2i=4W.2i}}C M};z.1x=B(M,pB){R(A i=1,l=P.G;i<l;i++){z.c7(M,P[i])}C M};z.4M=B(c6,pA){R(A i=1,l=P.G;i<l;i++){z.c7(c6.1C,P[i])}C c6};z.ig=B(c5,89){A ij=z.4d(P,2);A ik=z.1R(89);C B(){A ih=z.4d(P);A f=(ik?(c5||z.1W)[89]:89);C(f)&&(f.14(c5||D,ij.3U(ih)))}};z.2p=B(2z,3k){if(P.G>2){C z.ig.14(z,P)}if(!3k){3k=2z;2z=L}if(z.1R(3k)){2z=2z||z.1W;if(!2z[3k]){2m([\"z.2p: ie[\\\\\"\",3k,\"\\\\\"] is L (ie=\\\\\"\",2z,\"\\\\\")\"].22(\"\"))}C B(){C 2z[3k].14(2z,P||[])}}I{C(!2z?3k:B(){C 3k.14(2z,P||[])})}};z.6j=B(M,c3){B c4(){};c4.1C=M;A c2=S c4();if(c3){z.1x(c2,c3)}C c2};z.7X=B(pz){A Q=[L];C z.2p.14(z,Q.3U(z.4d(P)))};z.4d=B(M,ic){A Q=[];R(A x=ic||0;x<M.G;x++){Q.Y(M[x])}C Q};z.c1=B(o){if(!o){C o}if(z.2l(o)){A r=[];R(A i=0;i<o.G;++i){r.Y(z.c1(o[i]))}C r}I{if(z.ib(o)){if(o.2t&&o.a7){C o.a7(K)}I{A r=S o.1P();R(A i in o){if(!(i in r)||r[i]!=o[i]){r[i]=z.c1(o[i])}}C r}}}C o};z.7g=B(2H){C 2H.2f(/^\\\\s\\\\s*/,\"\").2f(/\\\\s\\\\s*$/,\"\")}}if(!z.1h[\"z.X.2r\"]){z.1h[\"z.X.2r\"]=K;z.1Q(\"z.X.2r\");z.2r=B(6l,4p,3j){if(z.1Y(3j)||(P.G>3)){z.ia(\"z.2r: R 9P \\'\"+6l+\"\\' py pw B as \\'1P\\' pv pu of as a pt i3.\",\"\",\"1.0\");A c=3j;3j=P[3]||{};3j.1P=c}A dd=P.2O,4V=L;if(z.2l(4p)){4V=4p;4p=4V.3a()}if(4V){R(A i=0,m;i<4V.G;i++){m=4V[i];if(!m){2m(\"ps #\"+i+\" 4F pr of \"+6l+\" is L. pq\\'s pp a po pl is 3O 6m.\")}4p=dd.6j(4p,m)}}A i9=(3j||0).1P,6k=dd.6j(4p),fn;R(A i in 3j){if(z.1Y(fn=3j[i])&&(!0[i])){fn.i4=i}}z.4M(6k,{4o:6l,bY:i9,bZ:L},3j||0);6k.1C.1P=6k;C z.88(6l,6k)};z.1x(z.2r,{6j:B(c0,i8){A bp=(c0||0).1C,mp=(i8||0).1C;A 2S=z.2r.i7();z.1x(2S,{84:bp,1x:mp});if(c0){2S.1C=z.6j(bp)}z.4M(2S,z.2r.i6,mp||0,{bY:L});2S.1C.1P=2S;2S.1C.4o=(bp||0).4o+\"pk\"+(mp||0).4o;z.88(2S.1C.4o,2S);C 2S},i7:B(){C B(){D.i5(P)}},i6:{i5:B(86){A c=86.2O,s=c.84,ct=s&&s.1P,m=c.1x,87=m&&m.1P,a=86,ii,fn;if(a[0]){if((fn=a[0][\"bZ\"])){a=fn.14(D,a)||a}}if(fn=c.1C.bZ){a=fn.14(D,a)||a}if(ct&&ct.14){ct.14(D,a)}if(87&&87.14){87.14(D,a)}if(ii=c.1C.bY){ii.14(D,86)}},bX:B(85){A c=D.1P,p,m;1s(c){p=c.84;m=c.1x;if(m==85||(m 1N 85.1P)){C p}if(m&&(m=m.bX(85))){C m}c=p&&p.1P}},6h:B(83,82,bW,6i){A p=bW,c,m,f;do{c=p.1P;m=c.1x;if(m&&(m=D.6h(83,82,m,6i))){C m}if((f=p[83])&&(6i==(f==82))){C p}p=c.84}1s(p);C!6i&&(p=D.bX(bW))&&D.6h(83,82,p,6i)},bU:B(2R,4U,bV){A a=P;if(!z.1R(a[0])){bV=4U;4U=2R;2R=4U.2O.i4}A c=4U.2O,p=D.1P.1C,a=bV||4U,fn,mp;if(D[2R]!=c||p[2R]==c){mp=D.6h(2R,c,p,K);if(!mp){2m(D.4o+\": 1p i3 (\\\\\"\"+2R+\"\\\\\") 4F bU pj 1f 2O (2r.js)\")}p=D.6h(2R,c,mp,U)}fn=p&&p[2R];if(!fn){1z.1K(mp.4o+\": no bU \\\\\"\"+2R+\"\\\\\" ph pg (2r.js)\");C}C fn.14(D,a)}}})}if(!z.1h[\"z.X.2c\"]){z.1h[\"z.X.2c\"]=K;z.1Q(\"z.X.2c\");z.3i={i2:B(){C B(){A ap=4e.1C,c=P.2O,ls=c.2b,t=c.5V;A r=t&&t.14(D,P);R(A i in ls){if(!(i in ap)){ls[i].14(D,P)}}C r}},2P:B(6g,bT,i1){6g=6g||z.1W;A f=6g[bT];if(!f||!f.2b){A d=z.3i.i2();d.5V=f;d.2b=[];f=6g[bT]=d}C f.2b.Y(i1)},3J:B(i0,hZ,bS){A f=(i0||z.1W)[hZ];if(f&&f.2b&&bS--){63 f.2b[bS]}}};z.2c=B(M,pd,pc,pa,p9){A a=P,F=[],i=0;F.Y(z.1R(a[0])?L:a[i++],a[i++]);A a1=a[i+1];F.Y(z.1R(a1)||z.1Y(a1)?a[i++]:L,a[i++]);R(A l=a.G;i<l;i++){F.Y(a[i])}C z.by.14(D,F)};z.by=B(M,bR,hY,hX){A l=z.3i,h=l.2P(M,bR,z.2p(hY,hX));C[M,bR,h,l]};z.p8=B(6f){if(6f&&6f[0]!==1k){z.bv.14(D,6f);63 6f[0]}};z.bv=B(M,hV,hU,hW){hW.3J(M,hV,hU)};z.80={};z.p7=B(bQ,hT,hS){C[bQ,z.3i.2P(z.80,bQ,z.2p(hT,hS))]};z.p6=B(81){if(81){z.3i.3J(z.80,81[0],81[1])}};z.hQ=B(hR,F){A f=z.80[hR];(f)&&(f.14(D,F||[]))};z.p5=B(hP,M,bP){A pf=B(){z.hQ(hP,P)};C(bP)?z.2c(M,bP,pf):z.2c(M,pf)}}if(!z.1h[\"z.X.30\"]){z.1h[\"z.X.30\"]=K;z.1Q(\"z.X.30\");z.30=B(hO){D.bM=[];D.id=D.hN();D.2y=-1;D.3M=0;D.4R=[L,L];D.bO=hO;D.7Z=U};z.4M(z.30,{hN:(B(){A n=1;C B(){C n++}})(),4C:B(){if(D.2y==-1){if(D.bO){D.bO(D)}I{D.7Z=K}if(D.2y==-1){A 1G=S 1O(\"30 p4\");1G.dY=\"4C\";D.5i(1G)}}I{if((D.2y==0)&&(D.4R[0]1N z.30)){D.4R[0].4C()}}},7V:B(1v){D.2y=((1v 1N 1O)?1:0);D.4R[D.2y]=1v;D.7U()},bN:B(){if(D.2y!=-1){if(!D.7Z){2m S 1O(\"p3 p2!\")}D.7Z=U;C}},dM:B(1v){D.bN();D.7V(1v)},5i:B(1v){D.bN();if(!(1v 1N 1O)){1v=S 1O(1v)}D.7V(1v)},9e:B(cb,4T){A 6e=z.2p(cb,4T);if(P.G>2){6e=z.7X(6e,P,2)}C D.5k(6e,6e)},ef:B(cb,4T){A 7Y=z.2p(cb,4T);if(P.G>2){7Y=z.7X(7Y,P,2)}C D.5k(7Y,L)},ed:B(cb,4T){A 7W=z.2p(cb,4T);if(P.G>2){7W=z.7X(7W,P,2)}C D.5k(L,7W)},5k:B(cb,eb){D.bM.Y([cb,eb]);if(D.2y>=0){D.7U()}C D},7U:B(){A bL=D.bM;A 4n=D.2y;A 1v=D.4R[4n];A 4S=D;A cb=L;1s((bL.G>0)&&(D.3M==0)){A f=bL.3a()[4n];if(!f){6c}1u{1v=f(1v);4n=((1v 1N 1O)?1:0);if(1v 1N z.30){cb=B(1v){4S.7V(1v);4S.3M--;if((4S.3M==0)&&(4S.2y>=0)){4S.7U()}};D.3M++}}1y(1G){1z.1K(1G);4n=1;1v=1G}}D.2y=4n;D.4R[4n]=1v;if((cb)&&(D.3M)){1v.9e(cb)}}})}if(!z.1h[\"z.X.2e\"]){z.1h[\"z.X.2e\"]=K;z.1Q(\"z.X.2e\");z.5m=B(2e){1u{C 3u(\"(\"+2e+\")\")}1y(e){1z.1K(e);C 2e}};z.bK=B(2H){C(\"\\\\\"\"+2H.2f(/([\"\\\\\\\\])/g,\"\\\\\\\\$1\")+\"\\\\\"\").2f(/[\\\\f]/g,\"\\\\\\\\f\").2f(/[\\\\b]/g,\"\\\\\\\\b\").2f(/[\\\\n]/g,\"\\\\\\\\n\").2f(/[\\\\t]/g,\"\\\\\\\\t\").2f(/[\\\\r]/g,\"\\\\\\\\r\")};z.hM=\"\\\\t\";z.eq=B(it,4l,4P){4P=4P||\"\";A 4k=(4l?4P+z.hM:\"\");A 6b=(4l?\"\\\\n\":\"\");A 4Q=V(it);if(4Q==\"1k\"){C\"1k\"}I{if((4Q==\"4J\")||(4Q==\"p1\")){C it+\"\"}I{if(it===L){C\"L\"}}}if(4Q==\"3c\"){C z.bK(it)}A 6d=P.2O;A 4m;if(V it.hL==\"B\"){4m=it.hL();if(it!==4m){C 6d(4m,4l,4k)}}if(V it.2e==\"B\"){4m=it.2e();if(it!==4m){C 6d(4m,4l,4k)}}if(z.2l(it)){A 1v=[];R(A i=0;i<it.G;i++){A 1U=6d(it[i],4l,4k);if(V(1U)!=\"3c\"){1U=\"1k\"}1v.Y(6b+4k+1U)}C\"[\"+1v.22(\", \")+6b+4P+\"]\"}if(4Q==\"B\"){C L}A bJ=[];R(A 1i in it){A 7T;if(V(1i)==\"4J\"){7T=\"\\\\\"\"+1i+\"\\\\\"\"}I{if(V(1i)==\"3c\"){7T=z.bK(1i)}I{6c}}1U=6d(it[1i],4l,4k);if(V(1U)!=\"3c\"){6c}bJ.Y(6b+4k+7T+\": \"+1U)}C\"{\"+bJ.22(\", \")+6b+4P+\"}\"}}if(!z.1h[\"z.X.6a\"]){z.1h[\"z.X.6a\"]=K;z.1Q(\"z.X.6a\");(B(){A 69=B(Q,M,cb){C[(z.1R(Q)?Q.1A(\"\"):Q),(M||z.1W),(z.1R(cb)?(S bI(\"1m\",\"hK\",\"6a\",cb)):cb)]};z.1x(z,{T:B(bH,hH,hI,hJ){A i=0,2q=1,1d=bH.G;if(hJ){i=1d-1;2q=1d=-1}R(i=hI||i;i!=1d;i+=2q){if(bH[i]==hH){C i}}C-1},31:B(hG,hF,hE){C z.T(hG,hF,hE,K)},1n:B(Q,hD,M){if(!Q||!Q.G){C}A 1I=69(Q,M,hD);Q=1I[0];R(A i=0,l=1I[0].G;i<l;i++){1I[2].2d(1I[1],Q[i],i,Q)}},bE:B(bF,Q,hC,M){A 1I=69(Q,M,hC);Q=1I[0];R(A i=0,l=Q.G;i<l;i++){A bG=!!1I[2].2d(1I[1],Q[i],i,Q);if(bF^bG){C bG}}C bF},ah:B(Q,hB,hA){C D.bE(K,Q,hB,hA)},ag:B(Q,hz,hy){C D.bE(U,Q,hz,hy)},23:B(Q,7t,M){A 1I=69(Q,M,7t);Q=1I[0];A bD=((P[3])?(S P[3]()):[]);R(A i=0;i<Q.G;++i){bD.Y(1I[2].2d(1I[1],Q[i],i,Q))}C bD},3T:B(Q,hx,M){A 1I=69(Q,M,hx);Q=1I[0];A bC=[];R(A i=0;i<Q.G;i++){if(1I[2].2d(1I[1],Q[i],i,Q)){bC.Y(Q[i])}}C bC}})})()}if(!z.1h[\"z.X.1J\"]){z.1h[\"z.X.1J\"]=K;z.1Q(\"z.X.1J\");z.1J=B(bB){if(bB){D.hw(bB)}};z.1J.hp={p0:[0,0,0],oZ:[60,60,60],oY:[2j,2j,2j],oX:[1T,1T,1T],oW:[2j,0,0],oV:[1T,0,0],oU:[2j,0,2j],oT:[1T,0,1T],oS:[0,2j,0],oR:[0,1T,0],oQ:[2j,2j,0],oP:[1T,1T,0],oO:[0,0,2j],oN:[0,0,1T],oM:[0,2j,2j],oL:[0,1T,1T]};z.4M(z.1J,{r:1T,g:1T,b:1T,a:1,bz:B(r,g,b,a){A t=D;t.r=r;t.g=g;t.b=b;t.a=a},hw:B(2Q){A d=z;if(d.1R(2Q)){d.hq(2Q,D)}I{if(d.2l(2Q)){d.7P(2Q,D)}I{D.bz(2Q.r,2Q.g,2Q.b,2Q.a);if(!(2Q 1N d.1J)){D.7Q()}}}C D},7Q:B(){C D},oK:B(){A t=D;C[t.r,t.g,t.b]},oJ:B(){A t=D;C[t.r,t.g,t.b,t.a]},oI:B(){A Q=z.23([\"r\",\"g\",\"b\"],B(x){A s=D[x].2i(16);C s.G<2?\"0\"+s:s},D);C\"#\"+Q.22(\"\")},8F:B(hv){A t=D,7S=t.r+\", \"+t.g+\", \"+t.b;C(hv?\"hs(\"+7S+\", \"+t.a:\"7S(\"+7S)+\")\"},2i:B(){C D.8F(K)}});z.d8=B(bA,1d,hu,M){A d=z,t=M||S z.1J();d.1n([\"r\",\"g\",\"b\",\"a\"],B(x){t[x]=bA[x]+(1d[x]-bA[x])*hu;if(x!=\"a\"){t[x]=2Y.oH(t[x])}});C t.7Q()};z.ho=B(ht,M){A m=ht.1M().1f(/^hs?\\\\(([\\\\s\\\\.,0-9]+)\\\\)/);C m&&z.7P(m[1].1A(/\\\\s*,\\\\s*/),M)};z.hn=B(4j,M){A d=z,t=M||S d.1J(),7R=(4j.G==4)?4:8,hr=(1<<7R)-1;4j=2V(\"oG\"+4j.3b(1));if(2L(4j)){C L}d.1n([\"b\",\"g\",\"r\"],B(x){A c=4j&hr;4j>>=7R;t[x]=7R==4?17*c:c});t.a=1;C t};z.7P=B(a,M){A t=M||S z.1J();t.bz(2V(a[0]),2V(a[1]),2V(a[2]),2V(a[3]));if(2L(t.a)){t.a=1}C t.7Q()};z.hq=B(2H,M){A a=z.1J.hp[2H];C a&&z.7P(a,M)||z.ho(2H,M)||z.hn(2H,M)}}if(!z.1h[\"z.X\"]){z.1h[\"z.X\"]=K;z.1Q(\"z.X\")}if(!z.1h[\"z.X.5Z\"]){z.1h[\"z.X.5Z\"]=K;z.1Q(\"z.X.5Z\");(B(){A 1j=z.b2={2P:B(E,68,fp){if(!E){C}68=1j.4O(68);fp=1j.7G(68,fp);E.66(68,fp,U);C fp},3J:B(E,hm,hl){(E)&&(E.oF(1j.4O(hm),hl,U))},4O:B(1p){C(1p.2w(0,2)==\"on\"?1p.2w(2):1p)},7G:B(1p,fp){C(1p!=\"4b\"?fp:B(e){C fp.2d(D,1j.4i(e,D))})},4i:B(H,oE){4w(H.Z){2X\"4b\":1j.7K(H);3f}C H},7K:B(H){H.oD=(H.3h?67.oC(H.3h):\"\")}};z.oB=B(H,hk){C 1j.4i(H,hk)};z.gY=B(H){H.7J();H.7I()};A 7O=z.3i;z.by=B(M,bx,hh,hg,hi){A hj=M&&(M.2t||M.oA||M.66);A bw=!hj?0:(!hi?1:2),l=[z.3i,1j,7O][bw];A h=l.2P(M,bx,z.2p(hh,hg));C[M,bx,h,bw]};z.bv=B(M,he,hd,hf){([z.3i,1j,7O][hf]).3J(M,he,hd)};z.5W={oz:8,gV:9,oy:12,ox:13,ow:16,ov:17,ou:18,gG:19,ot:20,os:27,or:32,b5:33,b4:34,gE:35,gF:36,b7:37,b9:38,b6:39,b8:40,gD:45,8S:46,oq:47,oo:91,om:92,ol:93,oj:96,oi:97,oh:98,og:99,oe:6D,od:oc,ob:oa,o9:o8,o7:o6,o5:o4,o3:bi,o2:o1,o0:nZ,nY:nX,nW:nV,nU:bk,gS:nT,gR:nS,gQ:nR,gP:nQ,gO:nP,gN:nO,gM:nN,gL:nM,gK:nL,gJ:nK,gI:nJ,gH:nI,nH:nG,nF:nE,nD:nC,gB:nB,gC:nA};if(z.1l){bf=B(e,5h){1u{C(e.3I=5h)}1y(e){C 0}};A 61=z.3i;if(!1o.nz){7O=61=z.gy={b3:[],2P:B(64,bu,hc){64=64||z.1W;A f=64[bu];if(!f||!f.2b){A d=z.gz();d.5V=f&&(7M.Y(f)-1);d.2b=[];f=64[bu]=d}C f.2b.Y(7M.Y(hc)-1)},3J:B(hb,ha,7N){A f=(hb||z.1W)[ha],l=f&&f.2b;if(f&&l&&7N--){63 7M[l[7N]];63 l[7N]}}};A 7M=61.b3}z.1x(1j,{2P:B(E,62,fp){if(!E){C}62=1j.4O(62);if(62==\"h3\"){A kd=E.bs;if(!kd||!kd.2b||!kd.h9){1j.2P(E,\"bs\",1j.h4);E.bs.h9=K}}C 61.2P(E,62,1j.7G(fp))},3J:B(E,h8,h7){61.3J(E,1j.4O(h8),h7)},4O:B(7L){C(7L.2w(0,2)!=\"on\"?\"on\"+7L:7L)},ny:B(){},4i:B(H,4N){if(!H){A w=(4N)&&((4N.aD||4N.1q||4N).nx)||26;H=w.5Z}if(!H){C(H)}H.5V=H.br;H.bh=(4N||H.br);H.nw=H.nv;H.nu=H.nr;A bq=H.br,1e=(bq&&bq.aD)||1q;A bn=((z.1l<6)||(1e[\"aX\"]==\"aW\"))?1e.3E:1e.5K;A bm=z.aB();H.nq=H.np+z.aH(bn.5I||0)-bm.x;H.nn=H.nm+(bn.5G||0)-bm.y;if(H.Z==\"fk\"){H.h6=H.nl}if(H.Z==\"fj\"){H.h6=H.nk}H.7I=1j.bc;H.7J=1j.ba;C 1j.h5(H)},h5:B(H){4w(H.Z){2X\"4b\":A c=(\"3h\"in H?H.3h:H.3I);if(c==10){c=0;H.3I=13}I{if(c==13||c==27){c=0}I{if(c==3){c=99}}}H.3h=c;1j.7K(H);3f}C H},gZ:{bi:42,bk:47,h2:59,nj:43,ni:44,nh:45,ng:46,nf:47,60:96,h1:91,nb:92,na:93,h0:39},h4:B(H){A kp=H.bh.h3;if(!kp||!kp.2b){C}A k=H.3I;A bj=(k!=13)&&(k!=32)&&(k!=27)&&(k<48||k>90)&&(k<96||k>bk)&&(k<h2||k>60)&&(k<h1||k>h0);if(bj||H.5Y){A c=(bj?0:k);if(H.5Y){if(k==3||k==13){C}I{if(c>95&&c<bi){c-=48}I{if((!H.5X)&&(c>=65&&c<=90)){c+=32}I{c=1j.gZ[c]||c}}}}A 2x=1j.7H(H,{Z:\"4b\",2x:K,3h:c});kp.2d(H.bh,2x);H.bg=2x.bg;H.bd=2x.bd;bf(H,2x.3I)}},bc:B(){D.bg=K},ba:B(){D.n9=D.3I;if(D.5Y){bf(D,0)}D.bd=U}});z.gY=B(H){H=H||26.5Z;1j.bc.2d(H);1j.ba.2d(H)}}1j.7H=B(H,gX){A 2x=z.1x({},H,gX);1j.7K(2x);2x.7J=B(){H.7J()};2x.7I=B(){H.7I()};C 2x};if(z.2M){z.1x(1j,{4i:B(H,n8){4w(H.Z){2X\"4b\":A c=H.n7;if(c==3){c=99}c=((c<41)&&(!H.5X)?0:c);if((H.5Y)&&(!H.5X)&&(c>=65)&&(c<=90)){c+=32}C 1j.7H(H,{3h:c})}C H}})}if(z.3o){z.1x(1j,{4i:B(H,n6){4w(H.Z){2X\"4b\":A c=H.3h,s=H.5X,k=H.3I;k=k||gA[H.gW]||0;if(H.gW==\"n5\"){c=0}I{if((H.5Y)&&(c>0)&&(c<27)){c+=96}I{if(c==z.5W.gU){c=z.5W.gV;s=K}I{c=(c>=32&&c<gT?c:0)}}}C 1j.7H(H,{3h:c,5X:s,3I:k})}C H}});z.1x(z.5W,{gU:25,b9:gT,b8:n4,b7:n3,b6:n2,gS:n1,gR:n0,gQ:mZ,gP:mY,gO:mX,gN:mW,gM:mV,gL:mU,gK:mT,gJ:mS,gI:mR,gH:mQ,gG:mP,8S:mO,gF:mN,gE:mM,b5:mL,b4:mK,gD:mJ,mI:mH,gC:mG,gB:mF});A dk=z.5W,gA={\"mE\":dk.b9,\"mD\":dk.b8,\"mC\":dk.b7,\"mB\":dk.b6,\"mA\":dk.b5,\"mz\":dk.b4}}})();if(z.1l){z.gz=B(){C B(){A ap=4e.1C,h=z.gy.b3,c=P.2O,ls=c.2b,t=h[c.5V];A r=t&&t.14(D,P);R(A i in ls){if(!(i in ap)){h[ls[i]].14(D,P)}}C r}};z.b2.7G=B(fp){A f=z.b2.4i;C B(e){C fp.2d(D,f(e,D))}}}}if(!z.1h[\"z.X.b1\"]){z.1h[\"z.X.b1\"]=K;z.1Q(\"z.X.b1\");1u{1q.my(\"mx\",U,K)}1y(e){}if(z.1l||z.2M){z.1D=B(id,1e){if(z.1R(id)){A b0=(1e||z.1e);A 11=b0.gv(id);if((11)&&(11.gw.id.1Z==id)){C 11}I{A 5U=b0.gx[id];if(!5U){C}if(!5U.G){C 5U}A i=0;1s(11=5U[i++]){if(11.gw.id.1Z==id){C 11}}}}I{C id}}}I{z.1D=B(id,1e){if(z.1R(id)){C(1e||z.1e).gv(id)}I{C id}}}(B(){A 5T=L;z.mw=B(E){E=z.1D(E);1u{if(!5T){5T=1q.a9(\"mv\")}5T.4c(E.1L?E.1L.fs(E):E);5T.9L=\"\"}1y(e){}};z.mu=B(E,7F){1u{E=z.1D(E);7F=z.1D(7F);1s(E){if(E===7F){C K}E=E.1L}}1y(e){}C U};z.mt=B(E,5S){E=z.1D(E);if(z.gu){E.1c.ms=(5S)?\"dg\":\"7C\"}I{if(z.6B){E.1c.mr=(5S)?\"8K\":\"7C\"}I{if(z.1l){E.gs=(5S)?\"\":\"on\";z.1r(\"*\",E).1n(B(gt){gt.gs=(5S)?\"\":\"on\"})}}}};A 5R=B(E,4h){4h.1L.mq(E,4h);C K};A aZ=B(E,4h){A pn=4h.1L;if(4h==pn.fm){pn.4c(E)}I{C 5R(E,4h.71)}C K};z.5E=B(E,2a,3H){if((!E)||(!2a)||(V 3H==\"1k\")){C U}E=z.1D(E);2a=z.1D(2a);if(V 3H==\"4J\"){A cn=2a.3W;if(((3H==0)&&(cn.G==0))||(cn.G==3H)){2a.4c(E);C K}if(3H==0){C 5R(E,2a.5A)}C aZ(E,cn[3H-1])}4w(3H.1M()){2X\"mo\":C 5R(E,2a);2X\"a8\":C aZ(E,2a);2X\"9M\":if(2a.5A){C 5R(E,2a.5A)}I{2a.4c(E);C K}3f;aY:2a.4c(E);C K}};z.aP=\"5g-3G\";if(z.1l){A aV=1q.aX;z.aP=(aV==\"aW\")||(aV==\"gr\")||(z.1l<6)?\"g5-3G\":\"5g-3G\"}A 1E,dv=1q.mn;if(z.3o){1E=B(E){A s=dv.3F(E,L);if(!s&&E.1c){E.1c.gq=\"\";s=dv.3F(E,L)}C s||{}}}I{if(z.1l){1E=B(E){C E.gn}}I{1E=B(E){C dv.3F(E,L)}}}z.3F=1E;if(!z.1l){z.4g=B(mm,gp){C 2k(gp)||0}}I{z.4g=B(go,2N){if(!2N){C 0}if(2N==\"ml\"){C 4}if(2N.2w&&(2N.2w(-2)==\"px\")){C 2k(2N)}4G(go){A gm=1c.2g;A gl=aU.2g;aU.2g=gn.2g;1u{1c.2g=2N;2N=1c.mk}1y(e){2N=0}1c.2g=gm;aU.2g=gl}C 2N}}z.ge=(z.1l?B(E){1u{C(E.mj.mi.2W/6D)}1y(e){C 1}}:B(E){C z.3F(E).2W});z.gf=(z.1l?B(E,7D){if(7D==1){E.1c.7E=E.1c.7E.2f(/gk:[^;]*;/i,\"\");if(E.gj.1M()==\"gi\"){z.1r(\"> gh\",E).1n(B(i){i.1c.7E=i.1c.7E.2f(/gk:[^;]*;/i,\"\")})}}I{A o=\"mh(mg=\"+(7D*6D)+\")\";E.1c.3T=o}if(E.gj.1M()==\"gi\"){z.1r(\"> gh\",E).1n(B(i){i.1c.3T=o})}C 7D}:B(E,gg){C E.1c.2W=gg});A 5Q={3n:K,58:K,2g:K,5J:K};A gd=B(E,Z,5P){Z=Z.1M();if(5Q[Z]===K){C z.4g(E,5P)}I{if(5Q[Z]===U){C 5P}I{if((Z.T(\"mf\")>=0)||(Z.T(\"md\")>=0)||(Z.T(\"3n\")>=0)||(Z.T(\"58\")>=0)||(Z.T(\"5q\")>=0)||(Z.T(\"mc\")>=0)||(Z.T(\"ma\")>=0)){5Q[Z]=K;C z.4g(E,5P)}I{5Q[Z]=U;C 5P}}}};z.1c=B(E,5O,aT){A n=z.1D(E),F=P.G,op=(5O==\"2W\");if(F==3){C op?z.gf(n,aT):n.1c[5O]=aT}if(F==2&&op){C z.ge(n)}A s=z.3F(n);C(F==1)?s:gd(n,5O,s[5O])};z.7A=B(n,gc){A s=gc||1E(n),px=z.4g,l=px(n,s.m9),t=px(n,s.m8);C{l:l,t:t,w:l+px(n,s.m7),h:t+px(n,s.m6)}};z.5N=B(n,gb){A ne=\"7C\",px=z.4g,s=gb||1E(n),bl=(s.m5!=ne?px(n,s.m4):0),bt=(s.m3!=ne?px(n,s.m2):0);C{l:bl,t:bt,w:bl+(s.m1!=ne?px(n,s.m0):0),h:bt+(s.lZ!=ne?px(n,s.lY):0)}};z.aN=B(n,ga){A s=ga||1E(n),p=z.7A(n,s),b=z.5N(n,s);C{l:p.l+b.l,t:p.t+b.t,w:p.w+b.w,h:p.h+b.h}};z.aM=B(n,g9){A s=g9||1E(n),px=z.4g,l=px(n,s.lX),t=px(n,s.lW),r=px(n,s.lV),b=px(n,s.lU);if(z.3o&&(s.ax!=\"fU\")){r=l}C{l:l,t:t,w:l+r,h:t+b}};z.au=B(E,g8){A s=g8||1E(E),me=z.aM(E,s);A l=E.fT-me.l,t=E.fS-me.t;if(z.7B){A aS=2k(s.2g),aR=2k(s.5J);if(!2L(aS)&&!2L(aR)){l=aS,t=aR}I{A p=E.1L;if(p&&p.1c){A aQ=1E(p);if(aQ.lT!=\"lS\"){A be=z.5N(p,aQ);l+=be.l,t+=be.t}}}}I{if(z.2M){A p=E.1L;if(p){A be=z.5N(p);l-=be.l,t-=be.t}}}C{l:l,t:t,w:E.6v+me.w,h:E.8D+me.h}};z.aK=B(E,g7){A s=g7||1E(E),pe=z.7A(E,s),be=z.5N(E,s),w=E.aF,h;if(!w){w=E.6v,h=E.8D}I{h=E.lR,be.w=be.h=0}if(z.2M){pe.l+=be.l;pe.t+=be.t}C{l:pe.l,t:pe.t,w:w-pe.w-be.w,h:h-pe.h-be.h}};z.lQ=B(E,g6){A s=g6||1E(E),pe=z.7A(E,s),cb=z.aK(E,s);C{l:cb.l-pe.l,t:cb.t-pe.t,w:cb.w+pe.w,h:cb.h+pe.h}};z.aL=B(E,l,t,w,h,u){u=u||\"px\";4G(E.1c){if(!2L(l)){2g=l+u}if(!2L(t)){5J=t+u}if(w>=0){3n=w+u}if(h>=0){58=h+u}}};z.aO=B(E){A n=E.5w;C(z.aP==\"g5-3G\")||(n==\"lP\")||(n==\"lO\")};z.fX=B(E,7z,7y,g4){A bb=z.aO(E);if(bb){A pb=z.aN(E,g4);if(7z>=0){7z+=pb.w}if(7y>=0){7y+=pb.h}}z.aL(E,g3,g3,7z,7y)};z.fY=B(E,g1,g0,5M,5L,g2){A s=g2||z.3F(E);A bb=z.aO(E),pb=bb?fZ:z.aN(E,s),mb=z.aM(E,s);if(5M>=0){5M=2Y.5q(5M-pb.w-mb.w,0)}if(5L>=0){5L=2Y.5q(5L-pb.h-mb.h,0)}z.aL(E,g1,g0,5M,5L)};A fZ={l:0,t:0,w:0,h:0};z.lN=B(E,3G){A n=z.1D(E),s=1E(n),b=3G;C!b?z.au(n,s):z.fY(n,b.l,b.t,b.w,b.h,s)};z.lM=B(E,3G){A n=z.1D(E),s=1E(n),b=3G;C!b?z.aK(n,s):z.fX(n,b.w,b.h,s)};A 5H=B(E,1a){if(!(E=(E||0).1L)){C 0}A 1U,aJ=0,2h=z.3E();1s(E&&E.1c){if(1E(E).ax==\"lL\"){C 0}1U=E[1a];if(1U){aJ+=1U-0;if(E==2h){3f}}E=E.1L}C aJ};z.fQ=B(){A 2h=z.3E();A 3g=z.1W;A de=z.1e.5K;C{y:(3g.lK||de.5G||2h.5G||0),x:(3g.lJ||z.aH(de.5I)||2h.5I||0)}};z.aG=B(){C V z.aI==\"1k\"?(z.aI=z.3F(z.3E()).lI==\"lH\"):z.aI};z.aB=B(){A de=z.1e.5K;if(z.1l>=7){C{x:de.aC().2g,y:de.aC().5J}}I{C{x:z.aG()||26.am==26?de.fW:de.6v-de.aF-de.fW,y:de.lG}}};z.aH=B(aE){if(z.1l&&!z.aG()){A de=z.1e.5K;C aE+de.aF-de.lF}C aE};z.fP=B(E,aw){A ay=E.aD;A J={x:0,y:0};A 7w=U;A db=z.3E();if(z.1l){A aA=E.aC();A az=z.aB();J.x=aA.2g-az.x;J.y=aA.5J-az.y}I{if(ay[\"fV\"]){A bo=ay.fV(E);J.x=bo.x-5H(E,\"5I\");J.y=bo.y-5H(E,\"5G\")}I{if(E[\"fR\"]){7w=K;A 7x;if(z.3o&&(1E(E).ax==\"fU\")&&(E.1L==db)){7x=db}I{7x=db.1L}if(E.1L!=db){A nd=E;if(z.2M){nd=db}J.x-=5H(nd,\"5I\");J.y-=5H(nd,\"5G\")}A 4f=E;do{A n=4f[\"fT\"];if(!z.2M||n>0){J.x+=2L(n)?0:n}A m=4f[\"fS\"];J.y+=2L(m)?0:m;4f=4f.fR}1s((4f!=7x)&&4f)}I{if(E[\"x\"]&&E[\"y\"]){J.x+=2L(E.x)?0:E.x;J.y+=2L(E.y)?0:E.y}}}}if(7w||aw){A av=z.fQ();A m=7w?(!aw?-1:0):1;J.y+=m*av.y;J.x+=m*av.x}C J};z.af=B(E,fO){A n=z.1D(E),s=1E(n),mb=z.au(n,s);A at=z.fP(n,fO);mb.x=at.x;mb.y=at.y;C mb}})();z.fL=B(E,fN){C((\" \"+E.3A+\" \").T(\" \"+fN+\" \")>=0)};z.7s=B(E,ar){A 7v=E.3A;if((\" \"+7v+\" \").T(\" \"+ar+\" \")<0){E.3A=7v+(7v?\" \":\"\")+ar}};z.7r=B(E,fM){A t=z.7g((\" \"+E.3A+\" \").2f(\" \"+fM+\" \",\" \"));if(E.3A!=t){E.3A=t}};z.lE=B(E,aq,7u){if(V 7u==\"1k\"){7u=!z.fL(E,aq)}z[7u?\"7s\":\"7r\"](E,aq)}}if(!z.1h[\"z.X.1H\"]){z.1h[\"z.X.1H\"]=K;z.1Q(\"z.X.1H\");(B(){A d=z;z.1H=B(){A F=P;if((F.G==1)&&(V F[0]==\"4J\")){D.G=eK(F[0])}I{if(F.G){d.1n(F,B(i){D.Y(i)},D)}}};z.1H.1C=S 4e;if(d.1l){A fK=B(al){C(\"A a2 = am.\"+al+\"; \"+\"A ap = 4e.1C; \"+\"A ao = a2.1C; \"+\"R(A x in ao){ ap[x] = ao[x]; } \"+\"am.\"+al+\" = 4e; \")};A fI=fK(\"z.1H\");A aj=26.lD();aj.1q.fJ(\"<ak>\"+fI+\"</ak>\");aj.lC(1,1,1,1)}z.4M(z.1H,{T:B(fH,fG){C d.T(D,fH,fG)},31:B(lB,lA){A aa=d.4d(P);aa.ae(D);C d.31.14(d,aa)},ah:B(fF,fE){C d.ah(D,fF,fE)},ag:B(fD,fC){C d.ag(D,fD,fC)},1n:B(fB,fA){d.1n(D,fB,fA);C D},23:B(7t,M){C d.23(D,7t,M,d.1H)},af:B(){C d.23(D,d.af)},1c:B(lz,ly){A aa=d.4d(P);aa.ae(D[0]);A s=d.1c.14(d,aa);C(P.G>1)?D:s},lx:B(lw,lv){A aa=d.4d(P);aa.ae(L);A s=D.23(B(i){aa[0]=i;C d.1c.14(d,aa)});C(P.G>1)?D:s},7s:B(fz){C D.1n(B(i){z.7s(i,fz)})},7r:B(fy){C D.1n(B(i){z.7r(i,fy)})},5E:B(fw,7q){A 1m=d.1r(fw)[0];7q=7q||\"72\";R(A x=0;x<D.G;x++){d.5E(D[x],1m,7q)}C D},2c:B(fv,fu,ft){D.1n(B(1m){d.2c(1m,fv,fu,ft)});C D},lu:B(ad){A ac=(ad)?d.9t(D,ad):D;ac.1n(B(1m){if(1m[\"1L\"]){1m.1L.fs(1m)}});C ac},lt:B(fr,fq){A 1m=D[0];C d.1r(fr).1n(B(ai){d.5E(ai,1m,(fq||\"72\"))})},1r:B(7p){7p=7p||\"\";A J=S d.1H();D.1n(B(1m){d.1r(7p,1m).1n(B(ab){if(V ab!=\"1k\"){J.Y(ab)}})});C J},3T:B(fo){A 5F=D;A 1V=P;A r=S d.1H();A rp=B(t){if(V t!=\"1k\"){r.Y(t)}};if(d.1R(fo)){5F=d.9t(D,1V[0]);if(1V.G==1){C 5F}d.1n(d.3T(5F,1V[1],1V[2]),rp);C r}d.1n(d.3T(5F,1V[0],1V[1]),rp);C r},lr:B(7o,7n){A 1S=d.1e.a9(\"lq\");if(d.1R(7o)){1S.9L=7o}I{1S.4c(7o)}A ct=((7n==\"9M\")||(7n==\"a8\"))?\"fm\":\"5A\";D.1n(B(1m){A 24=1S.a7(K);1s(24[ct]){d.5E(24[ct],1m,7n)}});C D},7m:B(fl,F){A a5=[];F=F||{};D.1n(B(1m){A a6={E:1m};d.1x(a6,F);a5.Y(d[fl](a6))});C d.fx.lp(a5)},8I:B(F){C D.7m(\"8I\",F)},8H:B(F){C D.7m(\"8H\",F)},6y:B(F){C D.7m(\"6y\",F)}});z.1n([\"fk\",\"lo\",\"fj\",\"fi\",\"ln\",\"lm\",\"ll\",\"fi\",\"lk\",\"lj\",\"4b\"],B(H){A a4=\"on\"+H;z.1H.1C[a4]=B(a,b){C D.2c(a4,a,b)}})})()}if(!z.1h[\"z.X.1r\"]){z.1h[\"z.X.1r\"]=K;z.1Q(\"z.X.1r\");(B(){A d=z;A 2I=B(q){C[q.T(\"#\"),q.T(\".\"),q.T(\"[\"),q.T(\":\")]};A a0=B(a3,fh){A ql=a3.G;A i=2I(a3);A 1d=ql;R(A x=fh;x<i.G;x++){if(i[x]>=0){if(i[x]<1d){1d=i[x]}}}C(1d<0)?ql:1d};A 6X=B(7l){A i=2I(7l);if(i[0]!=-1){C 7l.21(i[0]+1,a0(7l,1))}I{C\"\"}};A 5r=B(7k){A 5D;A i=2I(7k);if((i[0]==0)||(i[1]==0)){5D=0}I{5D=a0(7k,0)}C((5D>0)?7k.3b(0,5D).1M():\"*\")};A fg=B(Q){A J=-1;R(A x=0;x<Q.G;x++){A 1S=Q[x];if(1S>=0){if((1S>J)||(J==-1)){J=1S}}}C J};A 9H=B(7i){A i=2I(7i);if(-1==i[1]){C\"\"}A di=i[1]+1;A 7j=fg(i.2w(2));if(di<7j){C 7i.21(di,7j)}I{if(-1==7j){C 7i.3b(di)}I{C\"\"}}};A f3=[{1i:\"|=\",1f:B(15,fe){C\"[5z(3U(\\' \\',@\"+15+\",\\' \\'), \\' \"+fe+\"-\\')]\"}},{1i:\"~=\",1f:B(15,fd){C\"[5z(3U(\\' \\',@\"+15+\",\\' \\'), \\' \"+fd+\" \\')]\"}},{1i:\"^=\",1f:B(15,fb){C\"[li-4G(@\"+15+\", \\'\"+fb+\"\\')]\"}},{1i:\"*=\",1f:B(15,fa){C\"[5z(@\"+15+\", \\'\"+fa+\"\\')]\"}},{1i:\"$=\",1f:B(15,9Z){C\"[21(@\"+15+\", 3c-G(@\"+15+\")-\"+(9Z.G-1)+\")=\\'\"+9Z+\"\\']\"}},{1i:\"!=\",1f:B(15,f9){C\"[3O(@\"+15+\"=\\'\"+f9+\"\\')]\"}},{1i:\"=\",1f:B(15,f8){C\"[@\"+15+\"=\\'\"+f8+\"\\']\"}}];A 9C=B(9Y,3Z,f7,f6){A 49;A i=2I(3Z);if(i[2]>=0){A 4L=3Z.T(\"]\",i[2]);A 29=3Z.21(i[2]+1,4L);1s(29&&29.G){if(29.2s(0)==\"@\"){29=29.2w(1)}49=L;R(A x=0;x<9Y.G;x++){A 1S=9Y[x];A 7h=29.T(1S.1i);if(7h>=0){A 15=29.21(0,7h);A 4a=29.21(7h+1S.1i.G);if((4a.2s(0)==\"\\\\\"\")||(4a.2s(0)==\"\\'\")){4a=4a.21(1,4a.G-1)}49=1S.1f(d.7g(15),d.7g(4a));3f}}if((!49)&&(29.G)){49=f7(29)}if(49){f6(49)}29=L;A 7f=3Z.T(\"[\",4L);if(0<=7f){4L=3Z.T(\"]\",7f);if(0<=4L){29=3Z.21(7f+1,4L)}}}}};A f0=B(f5){A 4K=\".\";A 7e=f5.1A(\" \");1s(7e.G){A 2K=7e.3a();A 7d;if(2K==\">\"){7d=\"/\";2K=7e.3a()}I{7d=\"//\"}A f4=5r(2K);4K+=7d+f4;A id=6X(2K);if(id.G){4K+=\"[@id=\\'\"+id+\"\\'][1]\"}A cn=9H(2K);if(cn.G){A 9X=\" \";if(cn.2s(cn.G-1)==\"*\"){9X=\"\";cn=cn.3b(0,cn.G-1)}4K+=\"[5z(3U(\\' \\',@9P,\\' \\'), \\' \"+cn+9X+\"\\')]\"}9C(f3,2K,B(f2){C\"[@\"+f2+\"]\"},B(f1){4K+=f1})}C 4K};A 7a={};A eC=B(28){if(7a[28]){C 7a[28]}A 1e=d.1e;A 9W=f0(28);A 4H=B(9V){A J=[];A 7b;1u{7b=1e.9x(9W,9V,L,lh.lg,L)}1y(e){1z.1K(\"lf in le:\",9W,\"lc:\",9V);1z.1K(e)}A 7c=7b.eZ();1s(7c){J.Y(7c);7c=7b.eZ()}C J};C 7a[28]=4H};A 5x={};A 9B={};A 3y=B(79,78){if(!79){C 78}if(!78){C 79}C B(){C 79.14(26,P)&&78.14(26,P)}};A 75=B(9U,3Y,5B,2J){A 2v=2J+1;A 76=(3Y.G==2v);A 2K=3Y[2J];if(2K==\">\"){A 77=9U.3W;if(!77.G){C}2v++;76=(3Y.G==2v);A 4H=6O(3Y[2J+1]);R(A x=0,11;x<77.G,11=77[x];x++){if(4H(11)){if(76){5B.Y(11)}I{75(11,3Y,5B,2v)}}}}A 5C=6U(2K)(9U);if(76){1s(5C.G){5B.Y(5C.3a())}}I{1s(5C.G){75(5C.3a(),3Y,5B,2v)}}};A eE=B(9T,eY){A J=[];A x=9T.G-1,11;1s(11=9T[x--]){75(11,eY,J,0)}C J};A 6O=B(3D){if(5x[3D]){C 5x[3D]}A ff=L;A 9S=5r(3D);if(9S!=\"*\"){ff=3y(ff,B(N){C((N.2t==1)&&(9S==N.5w.1M()))})}A 9R=6X(3D);if(9R.G){ff=3y(ff,B(N){C((N.2t==1)&&(N.id==9R))})}if(2Y.5q.14(D,2I(3D).2w(1))>=0){ff=3y(ff,9z(3D))}C 5x[3D]=ff};A 5y=B(E){A pn=E.1L;A 9Q=pn.3W;A 2v=-1;A 3C=pn.5A;if(!3C){C 2v}A ci=E[\"eW\"];A cl=pn[\"eX\"];if(((V cl==\"4J\")&&(cl!=9Q.G))||(V ci!=\"4J\")){pn[\"eX\"]=9Q.G;A 2J=1;do{if(3C===E){2v=2J}if(3C.2t==1){3C[\"eW\"]=2J;2J++}3C=3C.71}1s(3C)}I{2v=ci}C 2v};A lb=0;A 3X=B(N,15){A 74=\"\";if(15==\"9P\"){C N.3A||74}if(15==\"R\"){C N.la||74}C N.5t(15,2)||74};A eH=[{1i:\"|=\",1f:B(15,9O){A eV=\" \"+9O+\"-\";C B(N){A ea=\" \"+(N.5t(15,2)||\"\");C((ea==9O)||(ea.T(eV)==0))}}},{1i:\"^=\",1f:B(15,eU){C B(N){C(3X(N,15).T(eU)==0)}}},{1i:\"*=\",1f:B(15,eT){C B(N){C(3X(N,15).T(eT)>=0)}}},{1i:\"~=\",1f:B(15,eS){A 9N=\" \"+eS+\" \";C B(N){A ea=\" \"+3X(N,15)+\" \";C(ea.T(9N)>=0)}}},{1i:\"$=\",1f:B(15,73){A 9N=\" \"+73;C B(N){A ea=\" \"+3X(N,15);C(ea.31(73)==(ea.G-73.G))}}},{1i:\"!=\",1f:B(15,eR){C B(N){C(3X(N,15)!=eR)}}},{1i:\"=\",1f:B(15,eQ){C B(N){C(3X(N,15)==eQ)}}}];A 9E=[{1i:\"9M-9K\",1f:B(1p,l9){C B(N){if(N.2t!=1){C U}A fc=N.eP;1s(fc&&(fc.2t!=1)){fc=fc.eP}C(!fc)}}},{1i:\"72-9K\",1f:B(1p,l8){C B(N){if(N.2t!=1){C U}A nc=N.71;1s(nc&&(nc.2t!=1)){nc=nc.71}C(!nc)}}},{1i:\"l7\",1f:B(1p,l6){C B(N){A cn=N.3W;A eO=N.3W.G;R(A x=eO-1;x>=0;x--){A nt=cn[x].2t;if((nt==1)||(nt==3)){C U}}C K}}},{1i:\"5z\",1f:B(1p,eN){C B(N){C(N.9L.T(eN)>=0)}}},{1i:\"3O\",1f:B(1p,eM){A eL=6O(eM);C B(N){C(!eL(N))}}},{1i:\"l5-9K\",1f:B(1p,2u){A pi=eK;if(2u==\"l4\"){C B(N){C(((5y(N))%2)==1)}}I{if((2u==\"2n\")||(2u==\"l3\")){C B(N){C((5y(N)%2)==0)}}I{if(2u.T(\"l2+\")==0){A 70=pi(2u.3b(3));C B(N){C(N.1L.3W[70-1]===N)}}I{if((2u.T(\"n+\")>0)&&(2u.G>3)){A 9J=2u.1A(\"n+\",2);A eJ=pi(9J[0]);A 2J=pi(9J[1]);C B(N){C((5y(N)%eJ)==2J)}}I{if(2u.T(\"n\")==-1){A 70=pi(2u);C B(N){C(5y(N)==70)}}}}}}}}];A 9z=B(3e){A 9I=(9B[3e]||5x[3e]);if(9I){C 9I}A ff=L;A i=2I(3e);if(i[0]>=0){A 24=5r(3e);if(24!=\"*\"){ff=3y(ff,B(N){C(N.5w.1M()==24)})}}A 5u;A 3B=9H(3e);if(3B.G){A 9F=3B.2s(3B.G-1)==\"*\";if(9F){3B=3B.3b(0,3B.G-1)}A re=S 9G(\"(?:^|\\\\\\\\s)\"+3B+(9F?\".*\":\"\")+\"(?:\\\\\\\\s|$)\");ff=3y(ff,B(N){C re.6Z(N.3A)})}if(i[3]>=0){A 3z=3e.3b(i[3]+1);A 9D=\"\";A 5v=3z.T(\"(\");A 6Y=3z.31(\")\");if((0<=5v)&&(0<=6Y)&&(6Y>5v)){9D=3z.21(5v+1,6Y);3z=3z.3b(0,5v)}5u=L;R(A x=0;x<9E.G;x++){A 1S=9E[x];if(1S.1i==3z){5u=1S.1f(3z,9D);3f}}if(5u){ff=3y(ff,5u)}}A eG=(d.1l)?B(5s){A eI=5s.1M();C B(N){C N[5s]||N[eI]}}:B(5s){C B(N){C(N&&N.5t&&N.l1(5s))}};9C(eH,3e,eG,B(eF){ff=3y(ff,eF)});if(!ff){ff=B(){C K}}C 9B[3e]=ff};A 6W={};A 6U=B(3d,1B){A 9A=6W[3d];if(9A){C 9A}A i=2I(3d);A id=6X(3d);if(i[0]==0){C 6W[3d]=B(1B){C[d.1D(id)]}}A 9y=9z(3d);A 5p;if(i[0]>=0){5p=B(1B){A 11=d.1D(id);if(9y(11)){C[11]}}}I{A 3V;A 24=5r(3d);if(2Y.5q.14(D,2I(3d))==-1){5p=B(1B){A J=[];A 11,x=0,3V=1B.4I(24);1s(11=3V[x++]){J.Y(11)}C J}}I{5p=B(1B){A J=[];A 11,x=0,3V=1B.4I(24);1s(11=3V[x++]){if(9y(11)){J.Y(11)}}C J}}}C 6W[3d]=5p};A l0={};A 5o={\">\":B(1B){A J=[];A 11,x=0,3V=1B.3W;1s(11=3V[x++]){if(11.2t==1){J.Y(11)}}C J}};A 9w=B(6V){if(0>6V.T(\" \")){C 6U(6V)}A eD=B(1B){A 6S=6V.1A(\" \");A 6T;if(6S[0]==\">\"){6T=[1B]}I{6T=6U(6S.3a())(1B)}C eE(6T,6S)};C eD};A 9v=((1q[\"9x\"]&&!d.3o)?B(3x){A 6R=3x.1A(\" \");if((1q[\"9x\"])&&(3x.T(\":\")==-1)&&((K))){if(((6R.G>2)&&(3x.T(\">\")==-1))||(6R.G>3)||(3x.T(\"[\")>=0)||((1==6R.G)&&(0<=3x.T(\".\")))){C eC(3x)}}C 9w(3x)}:9w);A ey=B(3w){if(5o[3w]){C 5o[3w]}if(0>3w.T(\",\")){C 5o[3w]=9v(3w)}I{A eB=3w.1A(/\\\\s*,\\\\s*/);A 4H=B(1B){A eA=0;A J=[];A 6Q;1s(6Q=eB[eA++]){J=J.3U(9v(6Q,6Q.T(\" \"))(1B))}C J};C 5o[3w]=4H}};A 5n=0;A ez=B(Q){A J=S d.1H();if(!Q){C J}if(Q[0]){J.Y(Q[0])}if(Q.G<2){C J}5n++;Q[0][\"9u\"]=5n;R(A x=1,11;11=Q[x];x++){if(Q[x][\"9u\"]!=5n){J.Y(11)}11[\"9u\"]=5n}C J};d.1r=B(6P,1B){if(V 6P!=\"3c\"){C S d.1H(6P)}if(V 1B==\"3c\"){1B=d.1D(1B)}C ez(ey(6P)(1B||d.1e))};d.9t=B(ex,9s){A 9r=S d.1H();A ff=(9s)?6O(9s):B(){C K};R(A x=0,11;11=ex[x];x++){if(ff(11)){9r.Y(11)}}C 9r}})()}if(!z.1h[\"z.X.1b\"]){z.1h[\"z.X.1b\"]=K;z.1Q(\"z.X.1b\");z.6K=B(ew){A J={};A iq=\"kZ[Z!=9q][Z!=kY][Z!=et][Z!=kX][Z!=kW], kV, kU\";z.1r(iq,ew).3T(B(E){C(!E.kT)}).1n(B(1m){A 3v=1m.1p;A Z=(1m.Z||\"\").1M();if((Z==\"kS\")||(Z==\"kR\")){if(1m.kQ){J[3v]=1m.1Z}}I{if(1m.kP){A ev=J[3v]=[];z.1r(\"kO[kN]\",1m).1n(B(eu){ev.Y(eu.1Z)})}I{J[3v]=1m.1Z;if(Z==\"et\"){J[3v+\".x\"]=J[3v+\".y\"]=J[3v].x=J[3v].y=0}}}});C J};z.9h=B(23){A ec=kM;A J=\"\";A es={};R(A x in 23){if(23[x]!=es[x]){if(z.2l(23[x])){R(A y=0;y<23[x].G;y++){J+=ec(x)+\"=\"+ec(23[x][y])+\"&\"}}I{J+=ec(x)+\"=\"+ec(23[x])+\"&\"}}}if((J.G)&&(J.2s(J.G-1)==\"&\")){J=J.3b(0,J.G-1)}C J};z.kL=B(er){C z.9h(z.6K(er))};z.kK=B(ep){C z.eq(z.6K(ep))};z.kJ=B(2H){A J={};A qp=2H.1A(\"&\");A dc=kI;z.1n(qp,B(1m){if(1m.G){A 9p=1m.1A(\"=\");A 1p=dc(9p.3a());A 1U=dc(9p.22(\"=\"));if(z.1R(J[1p])){J[1p]=[J[1p]]}if(z.2l(J[1p])){J[1p].Y(1U)}I{J[1p]=1U}}});C J};z.e1=U;z.e6={\"9g\":B(1b){C 1b.2G},\"2e\":B(1b){if(!1o.eo){1z.1K(\"kH kG kF a kE of 9g/2e-6M-9m\"+\" 4F kD kC kB kA 4G en kz\"+\" (ky 1o.eo=K 4F kx kw D kv)\")}C z.5m(1b.2G)},\"2e-6M-ku\":B(1b){A 6N=1b.2G;A 9o=6N.T(\"/*\");A 9n=6N.31(\"*/\");if((9o==-1)||(9n==-1)){C z.5m(1b.2G)}C z.5m(6N.21(9o+2,9n))},\"2e-6M-9m\":B(1b){A 6L=1b.2G;A 9l=6L.T(\"/*\");A 9k=6L.31(\"*/\");if((9l==-1)||(9k==-1)){1z.1K(\"kt en ks\\'t 6M 9m!\");C\"\"}C z.5m(6L.21(9l+2,9k))},\"kr\":B(1b){C z.3u(1b.2G)},\"kq\":B(1b){if(z.1l&&!1b.el){z.1n([\"ko\",\"em\",\"kn\",\"km\"],B(i){1u{A 1e=S 9j(kl[i]+\".kk\");1e.kj=U;1e.ki(1b.2G);C 1e}1y(e){}})}I{C 1b.el}}};(B(){z.e5=B(F,ej,ei,eh){A 2F={};2F.F=F;A 6J=L;if(F.3R){A 3R=z.1D(F.3R);A 9i=3R.kh(\"kg\");2F.2E=F.2E||(9i?9i.1Z:L);6J=z.6K(3R)}I{2F.2E=F.2E}A 5l=[{}];if(6J){5l.Y(6J)}if(F.5g){5l.Y(F.5g)}if(F.ek){5l.Y({\"z.ek\":S 5d().8O()})}2F.1r=z.9h(z.1x.14(L,5l));2F.9d=F.9d||\"9g\";A d=S z.30(ej);d.5k(ei,B(eg){C eh(eg,d)});A ld=F.4E;if(ld&&z.1Y(ld)){d.ef(B(ee){C ld.2d(F,ee,2F)})}A 1G=F.9f;if(1G&&z.1Y(1G)){d.ed(B(e9){C 1G.2d(F,e9,2F)})}A 6I=F.kf;if(6I&&z.1Y(6I)){d.9e(B(e8){C 6I.2d(F,e8,2F)})}d.1F=2F;C d};A e4=B(O){O.e0=K;A 1b=O.1F.1b;if(V 1b.e7==\"B\"){1b.e7()}};A e3=B(O){C z.e6[O.1F.9d](O.1F.1b)};A e2=B(9c,O){1z.1K(9c);C 9c};A 3Q=B(F){A O=z.e5(F,e4,e3,e2);O.1F.1b=z.9b(O.1F.F);C O};A 5j=L;A 3t=[];A 94=B(){A dZ=(S 5d()).dU();if(!z.e1){z.1n(3t,B(4D,6H){if(!4D){C}A O=4D.O;1u{if(!O||O.e0||!4D.dT(O)){3t.3S(6H,1);C}if(4D.dR(O)){3t.3S(6H,1);4D.dP(O)}I{if(O.9a){if(O.9a+(O.1F.F.6G||0)<dZ){3t.3S(6H,1);A 1G=S 1O(\"6G ke\");1G.dY=\"6G\";O.5i(1G);O.4C()}}}}1y(e){1z.1K(e);O.5i(S 1O(\"kc!\"))}})}if(!3t.G){dX(5j);5j=L;C}};z.dV=B(){1u{z.1n(3t,B(i){i.O.4C()})}1y(e){}};if(z.1l){z.dW(z.dV)}z.dH=B(O,dS,dQ,dO){if(O.1F.F.6G){O.9a=(S 5d()).dU()}3t.Y({O:O,dT:dS,dR:dQ,dP:dO});if(!5j){5j=dN(94,50)}94()};A dJ=\"8Z/x-kb-3R-ka\";A dG=B(O){C O.1F.1b.6F};A dF=B(O){C 4==O.1F.1b.6F};A dE=B(O){if(z.8Y(O.1F.1b)){O.dM(O)}I{O.5i(S 1O(\"k9 k8 k7 5h:\"+O.1F.1b.3N))}};A 3P=B(Z,O){A 3s=O.1F;A F=3s.F;3s.1b.dL(Z,3s.2E,(F.k6!==K),(F.8X?F.8X:1k),(F.8W?F.8W:1k));if(F.6E){R(A 5f in F.6E){if(5f.1M()===\"5g-Z\"&&!F.8V){F.8V=F.6E[5f]}I{3s.1b.dK(5f,F.6E[5f])}}}3s.1b.dK(\"k5-k4\",(F.8V||dJ));1u{3s.1b.dI(3s.1r)}1y(e){O.4C()}z.dH(O,dG,dF,dE);C O};z.8T=B(4B){if(4B.1r.G){4B.2E+=(4B.2E.T(\"?\")==-1?\"?\":\"&\")+4B.1r;4B.1r=L}};z.k3=B(F){A O=3Q(F);z.8T(O.1F);C 3P(\"dD\",O)};z.k2=B(F){C 3P(\"dC\",3Q(F))};z.k1=B(F){A O=3Q(F);O.1F.1r=F.k0;C 3P(\"dC\",O)};z.jZ=B(F){C 3P(\"dA\",3Q(F))};z.jY=B(F){A O=3Q(F);A dB=O.1F;if(F[\"8U\"]){dB.1r=F.8U;F.8U=L}C 3P(\"dA\",O)};z.jX=B(F){A O=3Q(F);z.8T(O.1F);C 3P(\"8S\",O)};z.dz=B(jW){2m S 1O(\"z.dz 3O jV jU\")}})()}if(!z.1h[\"z.X.fx\"]){z.1h[\"z.X.fx\"]=K;z.1Q(\"z.X.fx\");z.dx=B(dy,1d){D.1w=dy;D.1d=1d;D.4x=B(n){C((D.1d-D.1w)*n)+D.1w}};z.2r(\"z.d6\",L,{1P:B(F){z.1x(D,F);if(z.2l(D.2C)){D.2C=S z.dx(D.2C[0],D.2C[1])}},2C:L,8Q:jT,5a:L,4z:0,dj:10,du:L,6x:L,dt:L,8B:L,dh:L,ds:L,dr:L,dm:L,2D:U,2Z:U,4A:L,8N:L,3r:L,2o:0,4y:0,3q:B(H,F){if(D[H]){D[H].14(D,F||[])}C D},5b:B(dw,8R){if(8R){5e(D.3r);D.2D=D.2Z=U;D.2o=0}I{if(D.2D&&!D.2Z){C D}}D.3q(\"6x\");A d=dw||D.du;if(d>0){5c(z.2p(D,B(){D.5b(L,8R)}),d);C D}D.4A=S 5d().8O();if(D.2Z){D.4A-=D.8Q*D.2o}D.8N=D.4A+D.8Q;D.2D=K;D.2Z=U;A 8P=D.2C.4x(D.2o);if(!D.2o){if(!D.4y){D.4y=D.4z}D.3q(\"dt\",[8P])}D.3q(\"ds\",[8P]);D.8M();C D},jS:B(){5e(D.3r);if(!D.2D){C D}D.2Z=K;D.3q(\"dr\",[D.2C.4x(D.2o)]);C D},jR:B(dq,dp){5e(D.3r);D.2D=D.2Z=K;D.2o=dq*6D;if(dp){D.5b()}C D},jQ:B(dn){if(!D.3r){C}5e(D.3r);if(dn){D.2o=1}D.3q(\"dm\",[D.2C.4x(D.2o)]);D.2D=D.2Z=U;C D},3N:B(){if(D.2D){C D.2Z?\"3M\":\"jP\"}C\"jO\"},8M:B(){5e(D.3r);if(D.2D){A dl=S 5d().8O();A 2q=(dl-D.4A)/(D.8N-D.4A);if(2q>=1){2q=1}D.2o=2q;if(D.5a){2q=D.5a(2q)}D.3q(\"8B\",[D.2C.4x(2q)]);if(2q<1){D.3r=5c(z.2p(D,\"8M\"),D.dj)}I{D.2D=U;if(D.4z>0){D.4z--;D.5b(L,K)}I{if(D.4z==-1){D.5b(L,K)}I{if(D.4y){D.4z=D.4y;D.4y=0}}}D.2o=0;D.3q(\"dh\")}}C D}});(B(){A df=B(E){if(z.1l){A ns=E.1c;if(!ns.8L.G&&z.1c(E,\"8L\")==\"dg\"){ns.8L=\"1\"}if(!ns.3n.G&&z.1c(E,\"3n\")==\"8K\"){ns.3n=\"8K\"}}};z.6C=B(F){if(V F.1d==\"1k\"){2m S 1O(\"z.6C jN an 1d 1Z\")}F.E=z.1D(F.E);A 3p=z.1x({6w:{}},F);A 8J=(3p.6w.2W={});8J.1w=(V 3p.1w==\"1k\")?B(){C 2V(z.1c(3p.E,\"2W\"))}:3p.1w;8J.1d=3p.1d;A 2U=z.6y(3p);z.2c(2U,\"6x\",L,B(){df(3p.E)});C 2U};z.8I=B(F){C z.6C(z.1x({1d:1},F))};z.8H=B(F){C z.6C(z.1x({1d:0},F))};if(z.6B&&!z.3o){z.8E=B(n){C 2k(\"0.5\")+((2Y.da((n+2k(\"1.5\"))*2Y.d9))/2)}}I{z.8E=B(n){C 0.5+((2Y.da((n+1.5)*2Y.d9))/2)}}A d4=B(6A){D.8G=6A;R(A p in 6A){A 1a=6A[p];if(1a.1w 1N z.1J){1a.d7=S z.1J()}}D.4x=B(r){A J={};R(A p in D.8G){A 1a=D.8G[p];A 6z=L;if(1a.1w 1N z.1J){6z=z.d8(1a.1w,1a.1d,r,1a.d7).8F()}I{if(!z.2l(1a.1w)){6z=((1a.1d-1a.1w)*r)+1a.1w+(p!=\"2W\"?1a.jM||\"px\":\"\")}}J[p]=6z}C J}};z.6y=B(F){F.E=z.1D(F.E);if(!F.5a){F.5a=z.8E}A 2U=S z.d6(F);z.2c(2U,\"6x\",2U,B(){A pm={};R(A p in D.6w){A 1a=pm[p]=z.1x({},D.6w[p]);if(z.1Y(1a.1w)){1a.1w=1a.1w()}if(z.1Y(1a.1d)){1a.1d=1a.1d()}A d5=(p.1M().T(\"jL\")>=0);B 8C(E,p){4w(p){2X\"58\":C E.8D;2X\"3n\":C E.6v}A v=z.1c(E,p);C(p==\"2W\")?2V(v):2k(v)};if(V 1a.1d==\"1k\"){1a.1d=8C(D.E,p)}I{if(V 1a.1w==\"1k\"){1a.1w=8C(D.E,p)}}if(d5){1a.1w=S z.1J(1a.1w);1a.1d=S z.1J(1a.1d)}I{1a.1w=(p==\"2W\")?2V(1a.1w):2k(1a.1w)}}D.2C=S d4(pm)});z.2c(2U,\"8B\",2U,B(8A){R(A s in 8A){z.1c(D.E,s,8A[s])}});C 2U}})()}',62,1711,'|||||||||||||||||||||||||||||||||||dojo|var|function|return|this|node|args|length|evt|else|ret|true|null|obj|elem|dfd|arguments|arr|for|new|indexOf|false|typeof||_base|push|type||te|||apply|attr|||||prop|xhr|style|end|doc|match|uri|_hasResource|key|del|undefined|isIE|item|forEach|djConfig|name|document|query|while|_66|try|res|start|mixin|catch|console|split|root|prototype|byId|gcs|ioArgs|err|NodeList|_p|Color|debug|parentNode|toLowerCase|instanceof|Error|constructor|provide|isString|ta|255|val|_a|global|_69|isFunction|value||substring|join|map|tn||window||path|_343|_220|_listeners|connect|call|json|replace|left|_b|toString|128|parseFloat|isArray|throw||_percent|hitch|step|declare|charAt|nodeType|_3c3|nidx|slice|faux|fired|_c4|_7e|loc|curve|_active|url|_44c|responseText|str|_312|idx|tqp|isNaN|isOpera|_22d|callee|add|_18b|_f8|_e2|_41|anim|Number|opacity|case|Math|_paused|Deferred|lastIndexOf|||||||||shift|substr|string|_3e7|_3ce|break|_w|charCode|_listener|_d5|_c5|authority|_49|width|isSafari|_49e|fire|_timer|_47b|_465|eval|_in|_40c|_409|_362|_3d9|className|_3d5|_386|_37a|body|getComputedStyle|box|_221|keyCode|remove|_8d|_46|paused|status|not|_478|_461|form|splice|filter|concat|tret|childNodes|_38b|_367|_33d||||||||||_340|_348|keypress|appendChild|_toArray|Array|_2b0|_toPixelValue|ref|_fixEvent|_19f|_14c|_14a|_150|_141|declaredClass|_d4|_99|_Url|_83|scheme|_67|_3d|switch|getValue|_startRepeatCount|repeat|_startTime|_47e|cancel|tif|load|to|with|tf|getElementsByTagName|number|_34c|_342|extend|_1e3|_normalizeEventName|_14b|_14e|results|self|cbfn|_f9|_d8|_b2|src|_88|dav||baseUrl|fragment|_loadedModules|_44|_43|_loaders|mll|height||easing|play|setTimeout|Date|clearTimeout|hdr|content|code|errback|_464|addCallbacks|_450|fromJson|_413|_3fc|_3ee|max|_31e|cond|getAttribute|_3d4|obi|tagName|_360|_381|contains|firstChild|_368|_372|_320|place|_2fa|scrollTop|_299|scrollLeft|top|documentElement|_288|_287|_getBorderExtents|_23f|_23d|_239|_218|_216|_211|eles|target|keys|shiftKey|ctrlKey|event|192|iel|_1db|delete|_1cf||addEventListener|String|_1af|_157|array|_14d|continue|_14f|_137|_11f|_106|_findMethod|has|_delegate|_dc|_d3|loaded|_9a|_loadInit|_inFlightCount|getObject|tv|_4f|_postLoad|_2d|offsetWidth|properties|beforeBegin|animateProperty|_4ad|_4a6|isKhtml|_fade|100|headers|readyState|timeout|_469|_457|_44d|formToObject|_441|comment|_43d|_36f|_419|tp|_40a|_406|_407|_373|_403|_3e6|_31b|cbi|test|_3c7|nextSibling|last|_3a1|_38e|_365|_36b|ecn|_364|_363|_356|_35e|_35f|_34f|_34d|_349|trim|tci|_328|_32b|_31f|_31c|_anim|_300|_2ff|_2f5|_2e7|removeClass|addClass|func|_2c4|cls|_2a9|_2ae|_280|_27f|_getPadExtents|isMoz|none|_233|cssText|_214|_fixCallback|_synthesizeEvent|stopPropagation|preventDefault|_setKeyChar|_1e1|ieh|_1d7|_1be|colorFromArray|sanitize|bits|rgb|_156|_fire|_resback|_13d|partial|_13a|silentlyCancelled|_topics|_127|_f1|_f0|superclass|_ec|_e3|mct|setObject|_bf|_b3|object|require|_92|_khtmlTimer|location|XMLHTTP|locale|dua|_71|_modulePrefixes|_55|_loadModule|_51|_50|_4e|pop|_3f|_callLoaded|_unloaders|_loadNotifying|_loadedUrls|_27|_24|_1d|_5|_4b7|onAnimate|getStyle|offsetHeight|_defaultEasing|toCss|_properties|fadeOut|fadeIn|_49f|auto|zoom|_cycle|_endTime|valueOf|_494|duration|_492|DELETE|_ioAddQueryToUrl|putData|contentType|password|user|_isDocumentOk|application|||||_466||||||startTime|_xhrObj|_45f|handleAs|addBoth|error|text|objectToQuery|_44f|ActiveXObject|_443|_442|filtered|_43f|_43e|_437|file|tnl|_41c|_filterQueryResult|_zipIdx|_408|_402|evaluate|_3ed|_380|fHit|_361|_33b|_3da|_3ab|_3d6|RegExp|_327|_3cf|_3c9|child|innerHTML|first|tval|_391|class|pnc|_37e|_37c|_375|_366|_35c|_35a|_353|_33c|_336|_314|||_315|_oe|_307|_309|cloneNode|after|createElement||_2f8|_2ef|_2ee|unshift|coords|some|every||_2cb|script|_2c9|parent||a2p||_2c3|_2bd||abs|_getMarginBox|_2b3|_2a6|position|_2a7|_2ac|_2ab|_getIeDocumentElementOffset|getBoundingClientRect|ownerDocument|_2a3|clientWidth|_isBodyLtr|_fixIeBiDiScrollLeft|_bodyLtr|_29d|_getContentBox|_setBox|_getMarginExtents|_getPadBorderExtents|_usesBorderBox|boxModel|pcs|st|sl|_240|runtimeStyle|_dcm|BackCompat|compatMode|default|_21b|_d|html|_event_listener|handlers|PAGE_DOWN|PAGE_UP|RIGHT_ARROW|LEFT_ARROW|DOWN_ARROW|UP_ARROW|_preventDefault||_stopPropagation|returnValue||_trySetKeyCode|cancelBubble|currentTarget|106|_1ee|111||_1e8|_1e7|||se|srcElement|onkeydown||_1d0|_disconnect|lid|_1c0|_connect|_set|_195|_185|_183|_17d|_everyOrSome|_16b|_172|_15b|Function|_154|_escapeString|_140|chain|_check|canceller|_12d|_124|_11a|_10d|_107|inherited|_fa|_f2|_findMixin|_constructor|preamble|_de|clone|tmp|_c7|TMP|_be|_ba|_mixin|isBrowser|lang|firebug||param|modulePaths|_a7|_fireCallback|_a0|setContext||_9c|unloaded||||_96|_93|navigator|_90|_89||protocol|_84|_86|_XMLHTTP_PROGIDS|gears|google|setAttribute|_80|_77|cfg|_6f|_getModuleSymbols|_5a|_58|_53|_4d|_4c|_45|_40|_moduleHasPrefix|_loadUri|_28|_26|_21|_22|tests|doh|_20|_1f|_1c|version|_1b|_19|_getProp|_11|_4|_4a5|_4b3|_Animation|tempColor|blendColors|PI|sin|||||_49a|normal|onEnd||rate||curr|onStop|_497||_496|pct|onPause|onPlay|onBegin|delay||_491|_Line|_48b|wrapForm|PUT|_487|POST|GET|_476|_474|_472|_ioWatch|send|_471|setRequestHeader|open|callback|setInterval|_470|resHandle|_46f|ioCheck|_46e|validCheck|getTime|_ioCancelAll|addOnUnload|clearInterval|dojoType|now|canceled|_blockAsync|_45e|_45c|_459|_ioSetArgs|_contentHandlers|abort|_458|_456||||addErrback|_454|addCallback|_452|_44b|_44a|_449|preventCache|responseXML|Microsoft|JSON|usePlainJson|_431|toJson|_430|_42d|image|opt|ria|_421|_41b|_40b|_zip|_410|_40d|_357|sqf|_374|_3e5|_3df|_38f|clc|pred|parseInt|ntf|_3bf|_3bc|cnl|previousSibling|_3a9|_3a6|_39c|_399|_396|_392|__cachedIndex|__cachedLength|_376|iterateNext|_34a|_355|_354|_32c|_350|_34b|_33f|_33e|_33a|_338|_334|_332||_330|_32e||_322|_316|mousemove|mouseout|mouseover|_305|lastChild||_2f9||_2f2|_2f1|removeChild|_2ec|_2eb|_2ea|_2e6||_2e4|_2e2|_2d6|_2d5|_2d4|_2d3|_2d2|_2d1|_2cd|_2cc|scs|write|_2c8|hasClass|_2c0|_2bb|_2b5|_abs|_docScroll|offsetParent|offsetTop|offsetLeft|absolute|getBoxObjectFor|clientLeft|_setContentSize|_setMarginBox|_28d|_286|_285|_289|NaN|_281|border|_272|_26b|_260|_258|_253|_24c|_246|_23a|_getOpacity|_setOpacity|_238|td|tr|nodeName|FILTER|_22f|_22e|currentStyle|_22c|_22b|display|QuirksMode|unselectable|_217|isMozilla|getElementById|attributes|all|_ie_listener|_getIeDispatcher|_1fd|NUM_LOCK|SCROLL_LOCK|INSERT|END|HOME|PAUSE|F12|F11|F10|F9|F8|F7|F6|F5|F4|F3|F2|F1|63232|SHIFT_TAB|TAB|keyIdentifier|_1f3|stopEvent|_punctMap|222|219|186|onkeypress|_stealthKeyDown|_fixKeys|relatedTarget|_1e0|_1df|_stealthKeydown|_1d6|_1d5|_1d1|_1ca|_1c9|_1cb|_1c2|_1c1|_1c3|_1c4|_1bc|_1b3|_1b2|colorFromHex|colorFromRgb|named|colorFromString|mask|rgba|_19c|_197|_192|setColor|_180|_178|_177|_175|_174|_16d|_166|_164|_163|_162|_15c|_15d|_15e|index|__json__|toJsonIndentStr|_nextId|_12f|_12b|publish|_128|_126|_125|_122|_121|_123|_11c|_11b|_10c|_10b|_108|getDispatcher|argument|nom|_construct|_core|_makeCtor|_df|_db|deprecated|isObject|_cc||scope||_hitchArgs|_c2||pre|_c1|native|isDebug||registerModulePath|_a8||finally|||_a6|_a5|_a4|_a3|_a2|_a1|_9f|_9e|_9d|_9b|_98|_97|onbeforeunload|ipt|scr|complete|_95|userAgent|_modulesLoaded|initialized|_initFired|_8c|_8a|_getText|_87|ieForceActiveXXhr|Msxml2|isGears|_81|_gearsObject|googlegears|GearsFactory|isFF|_7d|Safari|_72|_name|_6c|ire|ore|_68|i18n|_5b|requireIf|_56|_52|loading|_4a|_loadPath|_47|_48|_global_omit_module_check|_getModulePrefix|_3c|_3a|_37|_30|Boolean|_loadUriAndCheck|_2e||cacheBust|_1e|_1a|_17|_16|_15|_14|_f|_10|_e|_9|_8|revision|flag|patch|minor|major|_6|color|units|needs|stopped|playing|stop|gotoPercent|pause|1000|implemented|yet|_48a|xhrDelete|rawXhrPut|xhrPut|postData|rawXhrPost|xhrPost|xhrGet|Type|Content|sync|response|http|bad|urlencoded|www|_watchInFlightError||exceeded|handle|action|getAttributeNode|loadXML|async|XMLDOM|prefixes|MSXML3|MSXML|MSXML2||xml|javascript|wasn|your|optional|message|off|turn|use|endpoints|issues|security|potential|avoid|mimetype|using|consider|please|decodeURIComponent|queryToObject|formToJson|formToQuery|encodeURIComponent|selected|option|multiple|checked|checkbox|radio|disabled|textarea|select|button|reset|submit|input|_3fb|hasAttribute|0n|even|odd|nth|_3b5|empty|_3b1|_3ad|htmlFor|_38a|under||exprssion|failure|ANY_TYPE|XPathResult|starts|keyup|keydown|mouseup|mousedown|blur|click|combine|span|addContent||adopt|orphan|_2de|_2dd|styles|_2da|_2d9|_2cf|_2ce|show|createPopup|toggleClass|scrollWidth|clientTop|ltr|direction|pageXOffset|pageYOffset|fixed|contentBox|marginBox|BUTTON|TABLE|_getBorderBox|clientHeight|visible|overflow|marginBottom|marginRight|marginTop|marginLeft|borderBottomWidth|borderBottomStyle|borderRightWidth|borderRightStyle|borderTopWidth|borderTopStyle|borderLeftWidth|borderLeftStyle|paddingBottom|paddingRight|paddingTop|paddingLeft|offset||min|padding||margin|Opacity|Alpha|alpha|filters|pixelLeft|medium|_22a|defaultView|before||insertBefore|KhtmlUserSelect|MozUserSelect|setSelectable|isDescendant|div|_destroyElement|BackgroundImageCache|execCommand|PageDown|PageUp|Right|Left|Down|Up|63289|63249|63248|PRINT_SCREEN|63302|63277|63276|63275|63273|63272|63250|63247|63246|63245|63244|63243|63242|63241|63240|63239|63238|63237|63236|63235|63234|63233|Enter|_1f9|which|_1f6|bubbledKeyCode|221|220||||191|190|189|188|187|toElement|fromElement|clientY|pageY||clientX|pageX|offsetY|||layerY|offsetX|layerX|parentWindow|_nop|_allow_leaks|145|144|126|F15|125|F14|124|F13|123|122|121|120|119|118|117|116|115|114|113|112|NUMPAD_DIVIDE|110|NUMPAD_PERIOD|109|NUMPAD_MINUS|108|NUMPAD_ENTER|107|NUMPAD_PLUS|NUMPAD_MULTIPLY|105|NUMPAD_9|104|NUMPAD_8|103|NUMPAD_7|102|NUMPAD_6|101|NUMPAD_5|NUMPAD_4||NUMPAD_3|NUMPAD_2|NUMPAD_1|NUMPAD_0||SELECT|RIGHT_WINDOW||LEFT_WINDOW||HELP|SPACE|ESCAPE|CAPS_LOCK|ALT|CTRL|SHIFT|ENTER|CLEAR|BACKSPACE|attachEvent|fixEvent|fromCharCode|keyChar|_1b9|removeEventListener|0x|round|toHex|toRgba|toRgb|aqua|teal|blue|navy|yellow|olive|lime|green|fuchsia|purple|red|maroon|white|gray|silver|black|boolean|called|already|Cancelled|connectPublisher|unsubscribe|subscribe|disconnect|_113|_112||_111|_110|||found|was||must|_|module|||required|likely|It|declaration|Mixin|separate|instead|property|initializer||pass|_c9|_bb|_b7|nfunction|isAlien|isFinite|isArrayLike|_firebug|withDoc|withGlobal|_writeIncludes|VML|behavior|addRule|createStyleSheet|vml|com|microsoft|schemas|urn|namespaces|onunload|onreadystatechange|defer|khtml|WebKit|DOMContentLoaded|enableMozDomContentLoaded|domcontentloaded|Unable|base|chrome|1223|304|300|200|available|XMLHttpRequest|_println|language|userLanguage|isQuirks|factory|mimeTypes|Factory|Gears|_7f|MSIE||Firefox|Gecko|Konqueror||Opera|appVersion|xd|browser|moduleUrl|port|host|hostenv|_requireLocalization|_5f|_5e|_5d|_5c|requireLocalization|requireAfterIf|_57|common|platformRequire|defined|symbol|_isXDomain|tried|Could|__package__|packageFileName|_42|useXDomain|flight|still|files|addOnLoad|failed|sourceURL|util|notice|without|change|subject|APIs|EXPERIMENTAL|experimental|removed|will|DEPRECATED|exists|10315|Rev|Mobile|Spidermonkey|Rhino||Browser|delayMozLoadingFix|preventBackButtonFix|libraryScriptUri|baseRelativePath|baseScriptUri|allowQueryConfig|warn|trace|timeEnd||time|profileEnd|profile|log|info|groupEnd|group|dirxml|dir|count|assert'.split('|'),0,{});\n\
\n\
result += decompressedDojo.length;\n\
\n\
/*\n\
\n\
Prototype 1.5 rc0\n\
 - Adapted from Ruby on Rails - http://dev.rubyonrails.org/browser/spinoffs/prototype/src\n\
 - By Lunarmedia, 06 August, 2006\n\
 - Available at (and packed with) JavascriptCompressor.com\n\
\n\
Please note this version is missing the selector.js component of the full Prototype library. \n\
You can get the compressed version of selector at JavascriptCompressor.com\n\
\n\
*/\n\
\n\
var decompressedPrototype = function(p,a,c,k,e,d){e=function(c){return(c<a?\"\":e(parseInt(c/a)))+((c=c%a)>35?String.fromCharCode(c+29):c.toString(36))};if(!''.replace(/^/,String)){while(c--){d[e(c)]=k[c]||e(c)}k=[(function(e){return d[e]})];e=(function(){return'\\\\w+'});c=1};while(c--){if(k[c]){p=p.replace(new RegExp('\\\\b'+e(c)+'\\\\b','g'),k[c])}}return p}('d T={4l:\\'1.5.8P\\',3E:\\'(?:<3G.*?>)((\\\\n|\\\\r|.)*?)(?:<\\\\/3G>)\\',2v:7(){},K:7(x){c x}};d 1b={17:7(){c 7(){6.1I.2n(6,N)}}};d 1e=z q();q.u=7(5d,O){G(d 1G 2M O){5d[1G]=O[1G]}c 5d};q.1U=7(U){1j{f(U==1v)c\\'1v\\';f(U==1L)c\\'1L\\';c U.1U?U.1U():U.2C()}1s(e){f(e 8R 9l)c\\'...\\';25 e}};7j.v.1d=7(){d 43=6,23=$A(N),U=23.8S();c 7(){c 43.2n(U,23.3s($A(N)))}};7j.v.8U=7(U){d 43=6;c 7(C){c 43.8V(U,C||1W.C)}};q.u(8Q.v,{8W:7(){d 4Z=6.2C(16);f(6<16)c\\'0\\'+4Z;c 4Z},5j:7(){c 6+1},8Y:7(o){$R(0,6,11).V(o);c 6}});d 6s={6j:7(){d 48;G(d i=0;i<N.t;i++){d 6L=N[i];1j{48=6L();1y}1s(e){}}c 48}};d 6Q=1b.17();6Q.v={1I:7(1a,1J){6.1a=1a;6.1J=1J;6.41=Y;6.2A()},2A:7(){5Z(6.2D.1d(6),6.1J*4z)},2D:7(){f(!6.41){1j{6.41=11;6.1a()}8Z{6.41=Y}}}};q.u(4b.v,{2T:7(1A,1z){d L=\\'\\',O=6,I;1z=N.90.52(1z);1H(O.t>0){f(I=O.I(1A)){L+=O.47(0,I.w);L+=(1z(I)||\\'\\').2C();O=O.47(I.w+I[0].t)}1D{L+=O,O=\\'\\'}}c L},92:7(1A,1z,3i){1z=6.2T.52(1z);3i=3i===1v?1:3i;c 6.2T(1A,7(I){f(--3i<0)c I[0];c 1z(I)})},93:7(1A,o){6.2T(1A,o);c 6},94:7(t,2S){t=t||30;2S=2S===1v?\\'...\\':2S;c 6.t>t?6.47(0,t-2S.t)+2S:6},9F:7(){c 6.2y(/^\\\\s+/,\\'\\').2y(/\\\\s+$/,\\'\\')},71:7(){c 6.2y(/<\\\\/?[^>]+>/7Y,\\'\\')},2Q:7(){c 6.2y(z 3O(T.3E,\\'5P\\'),\\'\\')},70:7(){d 6Y=z 3O(T.3E,\\'5P\\');d 5p=z 3O(T.3E,\\'98\\');c(6.I(6Y)||[]).1C(7(5o){c(5o.I(5p)||[\\'\\',\\'\\'])[1]})},3q:7(){c 6.70().1C(7(3G){c 4q(3G)})},9E:7(){d 1q=J.4Y(\\'1q\\');d 1Y=J.9D(6);1q.75(1Y);c 1q.3h},9c:7(){d 1q=J.4Y(\\'1q\\');1q.3h=6.71();c 1q.2z[0]?1q.2z[0].6q:\\'\\'},78:7(){d 7i=6.I(/^\\\\??(.*)$/)[1].3j(\\'&\\');c 7i.36({},7(5b,72){d 1i=72.3j(\\'=\\');5b[1i[0]]=1i[1];c 5b})},1Z:7(){c 6.3j(\\'\\')},3P:7(){d 2l=6.3j(\\'-\\');f(2l.t==1)c 2l[0];d 54=6.5g(\\'-\\')==0?2l[0].7e(0).3Y()+2l[0].7g(1):2l[0];G(d i=1,73=2l.t;i<73;i++){d s=2l[i];54+=s.7e(0).3Y()+s.7g(1)}c 54},1U:7(){c\"\\'\"+6.2y(/\\\\\\\\/g,\\'\\\\\\\\\\\\\\\\\\').2y(/\\'/g,\\'\\\\\\\\\\\\\\'\\')+\"\\'\"}});4b.v.2T.52=7(1z){f(2i 1z==\\'7\\')c 1z;d 2U=z 3n(1z);c 7(I){c 2U.7a(I)}};4b.v.9h=4b.v.78;d 3n=1b.17();3n.79=/(^|.|\\\\r|\\\\n)(#\\\\{(.*?)\\\\})/;3n.v={1I:7(2U,1A){6.2U=2U.2C();6.1A=1A||3n.79},7a:7(U){c 6.2U.2T(6.1A,7(I){d 53=I[1];f(53==\\'\\\\\\\\\\')c I[2];c 53+(U[I[3]]||\\'\\').2C()})}};d $1y=z q();d $49=z q();d 1p={V:7(o){d w=0;1j{6.2m(7(h){1j{o(h,w++)}1s(e){f(e!=$49)25 e}})}1s(e){f(e!=$1y)25 e}},9n:7(o){d L=11;6.V(7(h,w){L=L&&!!(o||T.K)(h,w);f(!L)25 $1y});c L},9o:7(o){d L=11;6.V(7(h,w){f(L=!!(o||T.K)(h,w))25 $1y});c L},3e:7(o){d P=[];6.V(7(h,w){P.W(o(h,w))});c P},7n:7(o){d L;6.V(7(h,w){f(o(h,w)){L=h;25 $1y}});c L},7o:7(o){d P=[];6.V(7(h,w){f(o(h,w))P.W(h)});c P},9p:7(1A,o){d P=[];6.V(7(h,w){d 7c=h.2C();f(7c.I(1A))P.W((o||T.K)(h,w))});c P},1M:7(U){d 51=Y;6.V(7(h){f(h==U){51=11;25 $1y}});c 51},36:7(45,o){6.V(7(h,w){45=o(45,h,w)});c 45},9q:7(1F){d 23=$A(N).47(1);c 6.3e(7(h){c h[1F].2n(h,23)})},9s:7(o){d L;6.V(7(h,w){h=(o||T.K)(h,w);f(L==1v||h>=L)L=h});c L},9u:7(o){d L;6.V(7(h,w){h=(o||T.K)(h,w);f(L==1v||h<L)L=h});c L},9v:7(o){d 50=[],58=[];6.V(7(h,w){((o||T.K)(h,w)?50:58).W(h)});c[50,58]},3r:7(1G){d P=[];6.V(7(h,w){P.W(h[1G])});c P},9x:7(o){d P=[];6.V(7(h,w){f(!o(h,w))P.W(h)});c P},9y:7(o){c 6.3e(7(h,w){c{h:h,59:o(h,w)}}).9z(7(18,3U){d a=18.59,b=3U.59;c a<b?-1:a>b?1:0}).3r(\\'h\\')},1Z:7(){c 6.3e(T.K)},9B:7(){d o=T.K,23=$A(N);f(2i 23.5e()==\\'7\\')o=23.9C();d 7l=[6].3s(23).1C($A);c 6.1C(7(h,w){c o(7l.3r(w))})},1U:7(){c\\'#<1p:\\'+6.1Z().1U()+\\'>\\'}};q.u(1p,{1C:1p.3e,5v:1p.7n,1k:1p.7o,8M:1p.1M,7p:1p.1Z});d $A=1E.7q=7(2R){f(!2R)c[];f(2R.1Z){c 2R.1Z()}1D{d P=[];G(d i=0;i<2R.t;i++)P.W(2R[i]);c P}};q.u(1E.v,1p);f(!1E.v.4d)1E.v.4d=1E.v.4m;q.u(1E.v,{2m:7(o){G(d i=0;i<6.t;i++)o(6[i])},5i:7(){6.t=0;c 6},7r:7(){c 6[0]},5e:7(){c 6[6.t-1]},7s:7(){c 6.1k(7(h){c h!=1v||h!=1L})},6J:7(){c 6.36([],7(6H,h){c 6H.3s(h&&h.5D==1E?h.6J():[h])})},5s:7(){d 4N=$A(N);c 6.1k(7(h){c!4N.1M(h)})},5g:7(U){G(d i=0;i<6.t;i++)f(6[i]==U)c i;c-1},4m:7(5h){c(5h!==Y?6:6.1Z()).4d()},1U:7(){c\\'[\\'+6.1C(q.1U).1N(\\', \\')+\\']\\'}});d 4h={2m:7(o){G(d 1O 2M 6){d h=6[1O];f(2i h==\\'7\\')49;d 1i=[1O,h];1i.1O=1O;1i.h=h;o(1i)}},7t:7(){c 6.3r(\\'1O\\')},4N:7(){c 6.3r(\\'h\\')},7u:7(2N){c $H(2N).36($H(6),7(4Q,1i){4Q[1i.1O]=1i.h;c 4Q})},7w:7(){c 6.1C(7(1i){c 1i.1C(4n).1N(\\'=\\')}).1N(\\'&\\')},1U:7(){c\\'#<4h:{\\'+6.1C(7(1i){c 1i.1C(q.1U).1N(\\': \\')}).1N(\\', \\')+\\'}>\\'}};7 $H(U){d 2N=q.u({},U||{});q.u(2N,1p);q.u(2N,4h);c 2N};3L=1b.17();q.u(3L.v,1p);q.u(3L.v,{1I:7(22,2x,2H){6.22=22;6.2x=2x;6.2H=2H},2m:7(o){d h=6.22;2q{o(h);h=h.5j()}1H(6.1M(h))},1M:7(h){f(h<6.22)c Y;f(6.2H)c h<6.2x;c h<=6.2x}});d $R=7(22,2x,2H){c z 3L(22,2x,2H)};d M={4w:7(){c 6s.6j(7(){c z 5C()},7(){c z 5n(\\'7y.6d\\')},7(){c z 5n(\\'7z.6d\\')})||Y},4s:0};M.2W={3b:[],2m:7(o){6.3b.2m(o)},69:7(4F){f(!6.1M(4F))6.3b.W(4F)},7A:7(5t){6.3b=6.3b.5s(5t)},3y:7(1a,26,E,2Z){6.V(7(3o){f(3o[1a]&&2i 3o[1a]==\\'7\\'){1j{3o[1a].2n(3o,[26,E,2Z])}1s(e){}}})}};q.u(M.2W,1p);M.2W.69({5G:7(){M.4s++},1B:7(){M.4s--}});M.44=7(){};M.44.v={4a:7(m){6.m={1F:\\'4j\\',4p:11,5H:\\'5E/x-86-Q-7C\\',28:\\'\\'};q.u(6.m,m||{})},3l:7(){c 6.E.32==1v||6.E.32==0||(6.E.32>=84&&6.E.32<7E)},7G:7(){c!6.3l()}};M.3t=1b.17();M.3t.5L=[\\'7H\\',\\'80\\',\\'7I\\',\\'7J\\',\\'4t\\'];M.3t.v=q.u(z M.44(),{1I:7(1l,m){6.E=M.4w();6.4a(m);6.26(1l)},26:7(1l){d 28=6.m.28||\\'\\';f(28.t>0)28+=\\'&7K=\\';1j{6.1l=1l;f(6.m.1F==\\'7L\\'&&28.t>0)6.1l+=(6.1l.I(/\\\\?/)?\\'&\\':\\'?\\')+28;M.2W.3y(\\'5G\\',6,6.E);6.E.7N(6.m.1F,6.1l,6.m.4p);f(6.m.4p){6.E.5T=6.5J.1d(6);2Y((7(){6.4r(1)}).1d(6),10)}6.5A();d 1c=6.m.5V?6.m.5V:28;6.E.7O(6.m.1F==\\'4j\\'?1c:1L)}1s(e){6.3p(e)}},5A:7(){d 1P=[\\'X-7P-7Q\\',\\'5C\\',\\'X-T-4l\\',T.4l,\\'7R\\',\\'1Y/7m, 1Y/2e, 5E/5F, 1Y/5F, */*\\'];f(6.m.1F==\\'4j\\'){1P.W(\\'5Q-2g\\',6.m.5H);f(6.E.7S)1P.W(\\'7T\\',\\'7U\\')}f(6.m.1P)1P.W.2n(1P,6.m.1P);G(d i=0;i<1P.t;i+=2)6.E.7V(1P[i],1P[i+1])},5J:7(){d 2F=6.E.2F;f(2F!=1)6.4r(6.E.2F)},4A:7(B){1j{c 6.E.7W(B)}1s(e){}},5M:7(){1j{c 4q(\\'(\\'+6.4A(\\'X-7X\\')+\\')\\')}1s(e){}},5R:7(){1j{c 4q(6.E.3F)}1s(e){6.3p(e)}},4r:7(2F){d C=M.3t.5L[2F];d E=6.E,2Z=6.5M();f(C==\\'4t\\'){1j{(6.m[\\'2I\\'+6.E.32]||6.m[\\'2I\\'+(6.3l()?\\'81\\':\\'82\\')]||T.2v)(E,2Z)}1s(e){6.3p(e)}f((6.4A(\\'5Q-2g\\')||\\'\\').I(/^1Y\\\\/7m/i))6.5R()}1j{(6.m[\\'2I\\'+C]||T.2v)(E,2Z);M.2W.3y(\\'2I\\'+C,6,E,2Z)}1s(e){6.3p(e)}f(C==\\'4t\\')6.E.5T=T.2v},3p:7(57){(6.m.5W||T.2v)(6,57);M.2W.3y(\\'5W\\',6,57)}});M.4C=1b.17();q.u(q.u(M.4C.v,M.3t.v),{1I:7(1w,1l,m){6.4x={3m:1w.3m?$(1w.3m):$(1w),3z:1w.3z?$(1w.3z):(1w.3m?1L:$(1w))};6.E=M.4w();6.4a(m);d 1B=6.m.1B||T.2v;6.m.1B=(7(E,U){6.5Y();1B(E,U)}).1d(6);6.26(1l)},5Y:7(){d 3A=6.3l()?6.4x.3m:6.4x.3z;d 3k=6.E.3F;f(!6.m.3q)3k=3k.2Q();f(3A){f(6.m.60){z 6.m.60(3A,3k)}1D{k.6h(3A,3k)}}f(6.3l()){f(6.1B)2Y(6.1B.1d(6),10)}}});M.61=1b.17();M.61.v=q.u(z M.44(),{1I:7(1w,1l,m){6.4a(m);6.1B=6.m.1B;6.1J=(6.m.1J||2);6.2s=(6.m.2s||1);6.4B={};6.1w=1w;6.1l=1l;6.22()},22:7(){6.m.1B=6.63.1d(6);6.2D()},7b:7(){6.4B.1B=1v;89(6.65);(6.1B||T.2v).2n(6,N)},63:7(26){f(6.m.2s){6.2s=(26.3F==6.64?6.2s*6.m.2s:1);6.64=26.3F}6.65=2Y(6.2D.1d(6),6.2s*6.1J*4z)},2D:7(){6.4B=z M.4C(6.1w,6.1l,6.m)}});7 $(){d P=[],4;G(d i=0;i<N.t;i++){4=N[i];f(2i 4==\\'8c\\')4=J.8d(4);P.W(k.u(4))}c P.t<2?P[0]:P};J.8f=7(1f,6a){d 6b=($(6a)||J.1c).4D(\\'*\\');c $A(6b).36([],7(12,4E){f(4E.1f.I(z 3O(\"(^|\\\\\\\\s)\"+1f+\"(\\\\\\\\s|$)\")))12.W(k.u(4E));c 12})};f(!1W.k)d k=z q();k.u=7(4){f(!4)c;f(4X)c 4;f(!4.6e&&4.1h&&4!=1W){d 2a=k.3d,2r=k.u.2r;G(d 1G 2M 2a){d h=2a[1G];f(2i h==\\'7\\')4[1G]=2r.4W(h)}}4.6e=11;c 4};k.u.2r={4W:7(h){c 6[h]=6[h]||7(){c h.2n(1L,[6].3s($A(N)))}}};k.3d={4U:7(4){c $(4).l.2B!=\\'3Q\\'},6N:7(){G(d i=0;i<N.t;i++){d 4=$(N[i]);k[k.4U(4)?\\'6f\\':\\'6w\\'](4)}},6f:7(){G(d i=0;i<N.t;i++){d 4=$(N[i]);4.l.2B=\\'3Q\\'}},6w:7(){G(d i=0;i<N.t;i++){d 4=$(N[i]);4.l.2B=\\'\\'}},42:7(4){4=$(4);4.1X.8h(4)},6h:7(4,2e){$(4).3h=2e.2Q();2Y(7(){2e.3q()},10)},2y:7(4,2e){4=$(4);f(4.6k){4.6k=2e.2Q()}1D{d 1K=4.6R.6S();1K.56(4);4.1X.8i(1K.6T(2e.2Q()),4)}2Y(7(){2e.3q()},10)},8k:7(4){4=$(4);c 4.2k},3K:7(4){c z k.3S(4)},8l:7(4,1f){f(!(4=$(4)))c;c k.3K(4).1M(1f)},8m:7(4,1f){f(!(4=$(4)))c;c k.3K(4).7k(1f)},8n:7(4,1f){f(!(4=$(4)))c;c k.3K(4).42(1f)},8p:7(4){4=$(4);G(d i=0;i<4.2z.t;i++){d 3M=4.2z[i];f(3M.8q==3&&!/\\\\S/.4v(3M.6q))k.42(3M)}},8r:7(4){c $(4).3h.I(/^\\\\s*$/)},8s:7(4,3I){4=$(4),3I=$(3I);1H(4=4.1X)f(4==3I)c 11;c Y},6t:7(4){4=$(4);d x=4.x?4.x:4.2f,y=4.y?4.y:4.29;1W.6t(x,y)},1R:7(4,l){4=$(4);d h=4.l[l.3P()];f(!h){f(J.4J&&J.4J.6v){d 4L=J.4J.6v(4,1L);h=4L?4L.8v(l):1L}1D f(4.6x){h=4.6x[l.3P()]}}f(1W.6E&&[\\'18\\',\\'1n\\',\\'3U\\',\\'6G\\'].1M(l))f(k.1R(4,\\'14\\')==\\'4G\\')h=\\'6y\\';c h==\\'6y\\'?1L:h},8x:7(4,l){4=$(4);G(d B 2M l)4.l[B.3P()]=l[B]},8y:7(4){4=$(4);f(k.1R(4,\\'2B\\')!=\\'3Q\\')c{21:4.2p,24:4.2k};d 20=4.l;d 6B=20.4O;d 6A=20.14;20.4O=\\'31\\';20.14=\\'2o\\';20.2B=\\'\\';d 6C=4.6m;d 6D=4.6p;20.2B=\\'3Q\\';20.14=6A;20.4O=6B;c{21:6C,24:6D}},8z:7(4){4=$(4);d 4R=k.1R(4,\\'14\\');f(4R==\\'4G\\'||!4R){4.4T=11;4.l.14=\\'3T\\';f(1W.6E){4.l.1n=0;4.l.18=0}}},8A:7(4){4=$(4);f(4.4T){4.4T=1v;4.l.14=4.l.1n=4.l.18=4.l.6G=4.l.3U=\\'\\'}},8B:7(4){4=$(4);f(4.3c)c;4.3c=4.l.3V;f((k.1R(4,\\'3V\\')||\\'4U\\')!=\\'31\\')4.l.3V=\\'31\\'},8D:7(4){4=$(4);f(4.3c)c;4.l.3V=4.3c;4.3c=1v}};q.u(k,k.3d);d 4X=Y;f(!3W&&/3x|3w|3u/.4v(33.62)){d 3W={}};k.6K=7(2a){q.u(k.3d,2a||{});f(2i 3W!=\\'1v\\'){d 2a=k.3d,2r=k.u.2r;G(d 1G 2M 2a){d h=2a[1G];f(2i h==\\'7\\')3W.v[1G]=2r.4W(h)}4X=11}};k.6K();d 6M=z q();6M.2B=k.6N;1e.1g=7(3f){6.3f=3f};1e.1g.v={1I:7(4,2t){6.4=$(4);6.2t=2t.2Q();f(6.3f&&6.4.6O){1j{6.4.6O(6.3f,6.2t)}1s(e){d 1h=6.4.1h.2w();f(1h==\\'4V\\'||1h==\\'8N\\'){6.2X(6.6U())}1D{25 e}}}1D{6.1K=6.4.6R.6S();f(6.2V)6.2V();6.2X([6.1K.6T(6.2t)])}2Y(7(){2t.3q()},10)},6U:7(){d 1q=J.4Y(\\'1q\\');1q.3h=\\'<6V><4V>\\'+6.2t+\\'</4V></6V>\\';c $A(1q.2z[0].2z[0].2z)}};d 1g=z q();1g.6W=1b.17();1g.6W.v=q.u(z 1e.1g(\\'96\\'),{2V:7(){6.1K.97(6.4)},2X:7(2h){2h.V((7(2j){6.4.1X.55(2j,6.4)}).1d(6))}});1g.5m=1b.17();1g.5m.v=q.u(z 1e.1g(\\'99\\'),{2V:7(){6.1K.56(6.4);6.1K.74(11)},2X:7(2h){2h.4m(Y).V((7(2j){6.4.55(2j,6.4.9a)}).1d(6))}});1g.7h=1b.17();1g.7h.v=q.u(z 1e.1g(\\'9d\\'),{2V:7(){6.1K.56(6.4);6.1K.74(6.4)},2X:7(2h){2h.V((7(2j){6.4.75(2j)}).1d(6))}});1g.76=1b.17();1g.76.v=q.u(z 1e.1g(\\'9i\\'),{2V:7(){6.1K.9m(6.4)},2X:7(2h){2h.V((7(2j){6.4.1X.55(2j,6.4.9t)}).1d(6))}});k.3S=1b.17();k.3S.v={1I:7(4){6.4=$(4)},2m:7(o){6.4.1f.3j(/\\\\s+/).1k(7(B){c B.t>0}).2m(o)},5c:7(1f){6.4.1f=1f},7k:7(5a){f(6.1M(5a))c;6.5c(6.1Z().3s(5a).1N(\\' \\'))},42:7(4c){f(!6.1M(4c))c;6.5c(6.1k(7(1f){c 1f!=4c}).1N(\\' \\'))},2C:7(){c 6.1Z().1N(\\' \\')}};q.u(k.3S.v,1p);d 5I={5i:7(){G(d i=0;i<N.t;i++)$(N[i]).h=\\'\\'},4f:7(4){$(4).4f()},7v:7(){G(d i=0;i<N.t;i++)f($(N[i]).h==\\'\\')c Y;c 11},1k:7(4){$(4).1k()},5y:7(4){4=$(4);4.4f();f(4.1k)4.1k()}};d D={3a:7(Q){d 12=D.2L($(Q));d 4I=z 1E();G(d i=0;i<12.t;i++){d 4g=D.k.3a(12[i]);f(4g)4I.W(4g)}c 4I.1N(\\'&\\')},2L:7(Q){Q=$(Q);d 12=z 1E();G(d 1h 2M D.k.2E){d 4H=Q.4D(1h);G(d j=0;j<4H.t;j++)12.W(4H[j])}c 12},7x:7(Q,3N,B){Q=$(Q);d 3H=Q.4D(\\'2u\\');f(!3N&&!B)c 3H;d 4y=z 1E();G(d i=0;i<3H.t;i++){d 2u=3H[i];f((3N&&2u.2g!=3N)||(B&&2u.B!=B))49;4y.W(2u)}c 4y},7B:7(Q){d 12=D.2L(Q);G(d i=0;i<12.t;i++){d 4=12[i];4.7D();4.4o=\\'11\\'}},7F:7(Q){d 12=D.2L(Q);G(d i=0;i<12.t;i++){d 4=12[i];4.4o=\\'\\'}},5z:7(Q){c D.2L(Q).5v(7(4){c 4.2g!=\\'31\\'&&!4.4o&&[\\'2u\\',\\'1k\\',\\'3J\\'].1M(4.1h.2w())})},7M:7(Q){5I.5y(D.5z(Q))},5w:7(Q){$(Q).5w()}};D.k={3a:7(4){4=$(4);d 1F=4.1h.2w();d 1S=D.k.2E[1F](4);f(1S){d 1O=4n(1S[0]);f(1O.t==0)c;f(1S[1].5D!=1E)1S[1]=[1S[1]];c 1S[1].1C(7(h){c 1O+\\'=\\'+4n(h)}).1N(\\'&\\')}},1x:7(4){4=$(4);d 1F=4.1h.2w();d 1S=D.k.2E[1F](4);f(1S)c 1S[1]}};D.k.2E={2u:7(4){6c(4.2g.2w()){1r\\'7Z\\':1r\\'31\\':1r\\'6l\\':1r\\'1Y\\':c D.k.2E.3J(4);1r\\'6g\\':1r\\'6i\\':c D.k.2E.5O(4)}c Y},5O:7(4){f(4.83)c[4.B,4.h]},3J:7(4){c[4.B,4.h]},1k:7(4){c D.k.2E[4.2g==\\'1k-6n\\'?\\'5S\\':\\'5X\\'](4)},5S:7(4){d h=\\'\\',2b,w=4.85;f(w>=0){2b=4.m[w];h=2b.h||2b.1Y}c[4.B,h]},5X:7(4){d h=[];G(d i=0;i<4.t;i++){d 2b=4.m[i];f(2b.87)h.W(2b.h||2b.1Y)}c[4.B,h]}};d $F=D.k.1x;1e.3D=7(){};1e.3D.v={1I:7(4,1J,1a){6.1J=1J;6.4=$(4);6.1a=1a;6.2K=6.1x();6.2A()},2A:7(){5Z(6.2D.1d(6),6.1J*4z)},2D:7(){d h=6.1x();f(6.2K!=h){6.1a(6.4,h);6.2K=h}}};D.k.3C=1b.17();D.k.3C.v=q.u(z 1e.3D(),{1x:7(){c D.k.1x(6.4)}});D.3C=1b.17();D.3C.v=q.u(z 1e.3D(),{1x:7(){c D.3a(6.4)}});1e.2c=7(){};1e.2c.v={1I:7(4,1a){6.4=$(4);6.1a=1a;6.2K=6.1x();f(6.4.1h.2w()==\\'Q\\')6.67();1D 6.2A(6.4)},4K:7(){d h=6.1x();f(6.2K!=h){6.1a(6.4,h);6.2K=h}},67:7(){d 12=D.2L(6.4);G(d i=0;i<12.t;i++)6.2A(12[i])},2A:7(4){f(4.2g){6c(4.2g.2w()){1r\\'6g\\':1r\\'6i\\':1o.3B(4,\\'8j\\',6.4K.1d(6));1y;1r\\'6l\\':1r\\'1Y\\':1r\\'3J\\':1r\\'1k-6n\\':1r\\'1k-8t\\':1o.3B(4,\\'8u\\',6.4K.1d(6));1y}}}};D.k.2c=1b.17();D.k.2c.v=q.u(z 1e.2c(),{1x:7(){c D.k.1x(6.4)}});D.2c=1b.17();D.2c.v=q.u(z 1e.2c(),{1x:7(){c D.3a(6.4)}});f(!1W.1o){d 1o=z q()}q.u(1o,{8C:8,8F:9,8H:13,8I:27,8J:37,8L:38,8O:39,8T:40,8X:46,4:7(C){c C.Z||C.91},95:7(C){c(((C.6X)&&(C.6X==1))||((C.6Z)&&(C.6Z==1)))},9b:7(C){c C.9e||(C.9f+(J.3R.2G||J.1c.2G))},9g:7(C){c C.9j||(C.9k+(J.3R.2O||J.1c.2O))},7b:7(C){f(C.7d){C.7d();C.9r()}1D{C.48=Y;C.9w=11}},9A:7(C,1h){d 4=1o.4(C);1H(4.1X&&(!4.1h||(4.1h.3Y()!=1h.3Y())))4=4.1X;c 4},1T:Y,5u:7(4,B,1V,1u){f(!6.1T)6.1T=[];f(4.5f){6.1T.W([4,B,1V,1u]);4.5f(B,1V,1u)}1D f(4.4i){6.1T.W([4,B,1V,1u]);4.4i(\\'2I\\'+B,1V)}},66:7(){f(!1o.1T)c;G(d i=0;i<1o.1T.t;i++){1o.5N.2n(6,1o.1T[i]);1o.1T[i][0]=1L}1o.1T=Y},3B:7(4,B,1V,1u){d 4=$(4);1u=1u||Y;f(B==\\'5U\\'&&(33.4u.I(/3x|3w|3u/)||4.4i))B=\\'5K\\';6.5u(4,B,1V,1u)},5N:7(4,B,1V,1u){d 4=$(4);1u=1u||Y;f(B==\\'5U\\'&&(33.4u.I(/3x|3w|3u/)||4.4k))B=\\'5K\\';f(4.5x){4.5x(B,1V,1u)}1D f(4.4k){1j{4.4k(\\'2I\\'+B,1V)}1s(e){}}}});f(33.4u.I(/\\\\88\\\\b/))1o.3B(1W,\\'8a\\',1o.66,Y);d 2d={6o:Y,4P:7(){6.6z=1W.8e||J.3R.2G||J.1c.2G||0;6.6F=1W.8g||J.3R.2O||J.1c.2O||0},6u:7(4){d 19=0,15=0;2q{19+=4.2O||0;15+=4.2G||0;4=4.1X}1H(4);c[15,19]},35:7(4){d 19=0,15=0;2q{19+=4.29||0;15+=4.2f||0;4=4.1Q}1H(4);c[15,19]},68:7(4){d 19=0,15=0;2q{19+=4.29||0;15+=4.2f||0;4=4.1Q;f(4){p=k.1R(4,\\'14\\');f(p==\\'3T\\'||p==\\'2o\\')1y}}1H(4);c[15,19]},1Q:7(4){f(4.1Q)c 4.1Q;f(4==J.1c)c 4;1H((4=4.1X)&&4!=J.1c)f(k.1R(4,\\'14\\')!=\\'4G\\')c 4;c J.1c},8o:7(4,x,y){f(6.6o)c 6.6r(4,x,y);6.3g=x;6.34=y;6.1t=6.35(4);c(y>=6.1t[1]&&y<6.1t[1]+4.2k&&x>=6.1t[0]&&x<6.1t[0]+4.2p)},6r:7(4,x,y){d 4S=6.6u(4);6.3g=x+4S[0]-6.6z;6.34=y+4S[1]-6.6F;6.1t=6.35(4);c(6.34>=6.1t[1]&&6.34<6.1t[1]+4.2k&&6.3g>=6.1t[0]&&6.3g<6.1t[0]+4.2p)},8E:7(3Z,4){f(!3Z)c 0;f(3Z==\\'8G\\')c((6.1t[1]+4.2k)-6.34)/4.2k;f(3Z==\\'8K\\')c((6.1t[0]+4.2p)-6.3g)/4.2p},77:7(O,Z){O=$(O);Z=$(Z);Z.l.14=\\'2o\\';d 2P=6.35(O);Z.l.1n=2P[1]+\\'1m\\';Z.l.18=2P[0]+\\'1m\\';Z.l.21=O.2p+\\'1m\\';Z.l.24=O.2k+\\'1m\\'},4e:7(4M){d 19=0,15=0;d 4=4M;2q{19+=4.29||0;15+=4.2f||0;f(4.1Q==J.1c)f(k.1R(4,\\'14\\')==\\'2o\\')1y}1H(4=4.1Q);4=4M;2q{19-=4.2O||0;15-=4.2G||0}1H(4=4.1X);c[15,19]},77:7(O,Z){d m=q.u({5l:11,5r:11,5B:11,5q:11,29:0,2f:0},N[2]||{});O=$(O);d p=2d.4e(O);Z=$(Z);d 2J=[0,0];d 3v=1L;f(k.1R(Z,\\'14\\')==\\'2o\\'){3v=2d.1Q(Z);2J=2d.4e(3v)}f(3v==J.1c){2J[0]-=J.1c.2f;2J[1]-=J.1c.29}f(m.5l)Z.l.18=(p[0]-2J[0]+m.2f)+\\'1m\\';f(m.5r)Z.l.1n=(p[1]-2J[1]+m.29)+\\'1m\\';f(m.5B)Z.l.21=O.2p+\\'1m\\';f(m.5q)Z.l.24=O.2k+\\'1m\\'},8b:7(4){4=$(4);f(4.l.14==\\'2o\\')c;2d.4P();d 2P=2d.68(4);d 1n=2P[1];d 18=2P[0];d 21=4.6m;d 24=4.6p;4.6P=18-3X(4.l.18||0);4.6I=1n-3X(4.l.1n||0);4.5k=4.l.21;4.7f=4.l.24;4.l.14=\\'2o\\';4.l.1n=1n+\\'1m\\';4.l.18=18+\\'1m\\';4.l.21=21+\\'1m\\';4.l.24=24+\\'1m\\'},8w:7(4){4=$(4);f(4.l.14==\\'3T\\')c;2d.4P();4.l.14=\\'3T\\';d 1n=3X(4.l.1n||0)-(4.6I||0);d 18=3X(4.l.18||0)-(4.6P||0);4.l.1n=1n+\\'1m\\';4.l.18=18+\\'1m\\';4.l.24=4.7f;4.l.21=4.5k}};f(/3x|3w|3u/.4v(33.62)){2d.35=7(4){d 19=0,15=0;2q{19+=4.29||0;15+=4.2f||0;f(4.1Q==J.1c)f(k.1R(4,\\'14\\')==\\'2o\\')1y;4=4.1Q}1H(4);c[15,19]}};',62,600,'||||element||this|function|||||return|var||if||value|||Element|style|options||iterator||Object|||length|extend|prototype|index|||new||name|event|Form|transport||for||match|document||result|Ajax|arguments|source|results|form|||Prototype|object|each|push||false|target||true|elements||position|valueL||create|left|valueT|callback|Class|body|bind|Abstract|className|Insertion|tagName|pair|try|select|url|px|top|Event|Enumerable|div|case|catch|offset|useCapture|undefined|container|getValue|break|replacement|pattern|onComplete|map|else|Array|method|property|while|initialize|frequency|range|null|include|join|key|requestHeaders|offsetParent|getStyle|parameter|observers|inspect|observer|window|parentNode|text|toArray|els|width|start|args|height|throw|request||parameters|offsetTop|methods|opt|EventObserver|Position|html|offsetLeft|type|fragments|typeof|fragment|offsetHeight|oStringList|_each|apply|absolute|offsetWidth|do|cache|decay|content|input|emptyFunction|toLowerCase|end|replace|childNodes|registerCallback|display|toString|onTimerEvent|Serializers|readyState|scrollLeft|exclusive|on|delta|lastValue|getElements|in|hash|scrollTop|offsets|stripScripts|iterable|truncation|gsub|template|initializeRange|Responders|insertContent|setTimeout|json||hidden|status|navigator|ycomp|cumulativeOffset|inject||||serialize|responders|_overflow|Methods|collect|adjacency|xcomp|innerHTML|count|split|response|responseIsSuccess|success|Template|responder|dispatchException|evalScripts|pluck|concat|Request|KHTML|parent|Safari|Konqueror|dispatch|failure|receiver|observe|Observer|TimedObserver|ScriptFragment|responseText|script|inputs|ancestor|textarea|classNames|ObjectRange|node|typeName|RegExp|camelize|none|documentElement|ClassNames|relative|right|overflow|HTMLElement|parseFloat|toUpperCase|mode||currentlyExecuting|remove|__method|Base|memo||slice|returnValue|continue|setOptions|String|classNameToRemove|_reverse|page|focus|queryComponent|Hash|attachEvent|post|detachEvent|Version|reverse|encodeURIComponent|disabled|asynchronous|eval|respondToReadyState|activeRequestCount|Complete|appVersion|test|getTransport|containers|matchingInputs|1000|header|updater|Updater|getElementsByTagName|child|responderToAdd|static|tagElements|queryComponents|defaultView|onElementEvent|css|forElement|values|visibility|prepare|mergedHash|pos|offsetcache|_madePositioned|visible|tbody|findOrStore|_nativeExtensions|createElement|digits|trues|found|prepareReplacement|before|camelizedString|insertBefore|selectNodeContents|exception|falses|criteria|classNameToAdd|params|set|destination|last|addEventListener|indexOf|inline|clear|succ|_originalWidth|setLeft|Top|ActiveXObject|scriptTag|matchOne|setHeight|setTop|without|responderToRemove|_observeAndCache|find|reset|removeEventListener|activate|findFirstElement|setRequestHeaders|setWidth|XMLHttpRequest|constructor|application|xml|onCreate|contentType|Field|onStateChange|keydown|Events|evalJSON|stopObserving|inputSelector|img|Content|evalResponse|selectOne|onreadystatechange|keypress|postBody|onException|selectMany|updateContent|setInterval|insertion|PeriodicalUpdater|userAgent|updateComplete|lastText|timer|unloadCache|registerFormCallbacks|positionedOffset|register|parentElement|children|switch|XMLHTTP|_extended|hide|checkbox|update|radio|these|outerHTML|password|clientWidth|one|includeScrollOffsets|clientHeight|nodeValue|withinIncludingScrolloffsets|Try|scrollTo|realOffset|getComputedStyle|show|currentStyle|auto|deltaX|originalPosition|originalVisibility|originalWidth|originalHeight|opera|deltaY|bottom|array|_originalTop|flatten|addMethods|lambda|Toggle|toggle|insertAdjacentHTML|_originalLeft|PeriodicalExecuter|ownerDocument|createRange|createContextualFragment|contentFromAnonymousTable|table|Before|which|matchAll|button|extractScripts|stripTags|pairString|len|collapse|appendChild|After|clone|toQueryParams|Pattern|evaluate|stop|stringValue|preventDefault|charAt|_originalHeight|substring|Bottom|pairs|Function|add|collections|javascript|detect|findAll|entries|from|first|compact|keys|merge|present|toQueryString|getInputs|Msxml2|Microsoft|unregister|disable|urlencoded|blur|300|enable|responseIsFailure|Uninitialized|Loaded|Interactive|_|get|focusFirstElement|open|send|Requested|With|Accept|overrideMimeType|Connection|close|setRequestHeader|getResponseHeader|JSON|gi|submit|Loading|Success|Failure|checked|200|selectedIndex|www|selected|bMSIE|clearTimeout|unload|absolutize|string|getElementById|pageXOffset|getElementsByClassName|pageYOffset|removeChild|replaceChild|click|getHeight|hasClassName|addClassName|removeClassName|within|cleanWhitespace|nodeType|empty|childOf|multiple|change|getPropertyValue|relativize|setStyle|getDimensions|makePositioned|undoPositioned|makeClipping|KEY_BACKSPACE|undoClipping|overlap|KEY_TAB|vertical|KEY_RETURN|KEY_ESC|KEY_LEFT|horizontal|KEY_UP|member|tr|KEY_RIGHT|0_RC_0|Number|instanceof|shift|KEY_DOWN|bindAsEventListener|call|toColorPart|KEY_DELETE|times|finally|callee|srcElement|sub|scan|truncate|isLeftClick|beforeBegin|setStartBefore|im|afterBegin|firstChild|pointerX|unescapeHTML|beforeEnd|pageX|clientX|pointerY|parseQuery|afterEnd|pageY|clientY|RangeError|setStartAfter|all|any|grep|invoke|stopPropagation|max|nextSibling|min|partition|cancelBubble|reject|sortBy|sort|findElement|zip|pop|createTextNode|escapeHTML|strip'.split('|'),0,{})\n\
\n\
result += decompressedPrototype.length;\n\
\n\
}\n\
\n\
var expected = 511508;\n\
if (result != expected)\n\
    throw \"ERROR: bad result: expected \" + expected + \" but got \" + result;\n\
\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
", "<!DOCTYPE html>\n\
<head>\n\
\n\
<meta charset=utf8>\n\
\n\
<!--\n\
 Copyright (C) 2007 Apple Inc.  All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, are permitted provided that the following conditions\n\
 are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
\n\
 THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY\n\
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n\
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR\n\
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n\
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n\
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY\n\
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. \n\
-->\n\
\n\
<title>SunSpider string-validate-input</title>\n\
<link rel=\"stylesheet\" href=\"../sunspider.css\">\n\
</head>\n\
\n\
<body>\n\
<h3>string-validate-input</h3>\n\
<div id=\"console\">\n\
</div>\n\
<script>\n\
function record(time) {\n\
    document.getElementById(\"console\").innerHTML = time + \"ms\";\n\
    if (window.parent) {\n\
        parent.recordResult(time);\n\
    }\n\
}\n\
\n\
window.onerror = function(e) {\n\
    console.log(\"string-validate-input failed with error: \" + e);\n\
    record(0 / 0);\n\
}\n\
\n\
var _sunSpiderStartDate = new Date();\n\
\n\
letters = new Array(\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\",\"h\",\"i\",\"j\",\"k\",\"l\",\"m\",\"n\",\"o\",\"p\",\"q\",\"r\",\"s\",\"t\",\"u\",\"v\",\"w\",\"x\",\"y\",\"z\");\n\
numbers = new Array(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26);\n\
colors  = new Array(\"FF\",\"CC\",\"99\",\"66\",\"33\",\"00\");\n\
\n\
var endResult;\n\
\n\
function doTest()\n\
{\n\
   endResult = \"\";\n\
\n\
   // make up email address\n\
   for (var k=0;k<4000;k++)\n\
   {\n\
      username = makeName(6);\n\
      (k%2)?email=username+\"@mac.com\":email=username+\"(at)mac.com\";\n\
\n\
      // validate the email address\n\
      var pattern = /^[a-zA-Z0-9\\-\\._]+@[a-zA-Z0-9\\-_]+(\\.?[a-zA-Z0-9\\-_]*)\\.[a-zA-Z]{2,3}$/;\n\
\n\
      if(pattern.test(email))\n\
      {\n\
         var r = email + \" appears to be a valid email address.\";\n\
         addResult(r);\n\
      }\n\
      else\n\
      {\n\
         r = email + \" does NOT appear to be a valid email address.\";\n\
         addResult(r);\n\
      }\n\
   }\n\
\n\
   // make up ZIP codes\n\
   for (var s=0;s<4000;s++)\n\
   {\n\
      var zipGood = true;\n\
      var zip = makeNumber(4);\n\
      (s%2)?zip=zip+\"xyz\":zip=zip.concat(\"7\");\n\
\n\
      // validate the zip code\n\
      for (var i = 0; i < zip.length; i++) {\n\
          var ch = zip.charAt(i);\n\
          if (ch < \"0\" || ch > \"9\") {\n\
              zipGood = false;\n\
              r = zip + \" contains letters.\";\n\
              addResult(r);\n\
          }\n\
      }\n\
      if (zipGood && zip.length>5)\n\
      {\n\
         zipGood = false;\n\
         r = zip + \" is longer than five characters.\";\n\
         addResult(r);\n\
      }\n\
      if (zipGood)\n\
      {\n\
         r = zip + \" appears to be a valid ZIP code.\";\n\
         addResult(r);\n\
      }\n\
   }\n\
}\n\
\n\
function makeName(n)\n\
{\n\
   var tmp = \"\";\n\
   for (var i=0;i<n;i++)\n\
   {\n\
      var l = Math.floor(26*Math.random());\n\
      tmp += letters[l];\n\
   }\n\
   return tmp;\n\
}\n\
\n\
function makeNumber(n)\n\
{\n\
   var tmp = \"\";\n\
   for (var i=0;i<n;i++)\n\
   {\n\
      var l = Math.floor(9*Math.random());\n\
      tmp = tmp.concat(l);\n\
   }\n\
   return tmp;\n\
}\n\
\n\
function addResult(r)\n\
{\n\
   endResult += \"\\n\" + r;\n\
}\n\
\n\
doTest();\n\
\n\
// FIXME: Come up with a way of validating this test.\n\
// https://bugs.webkit.org/show_bug.cgi?id=114851\n\
\n\
\n\
var _sunSpiderInterval = new Date() - _sunSpiderStartDate;\n\
\n\
record(_sunSpiderInterval);\n\
</script>\n\
\n\
\n\
</body>\n\
</html>\n\
" ];
