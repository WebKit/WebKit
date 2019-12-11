/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Javascript library for dynamically generating a simple SVG/SMIL reftest
 * with several copies of the same animation, each seeked to a different time.
 */

// Global variables
const START_TIMES = [ "4.0s",  "3.0s",  "2.7s",
                      "2.25s", "2.01s", "1.5s",
                      "1.4s",  "1.0s",  "0.5s" ];

const X_POSNS = [ "20px",  "70px",  "120px",
                  "20px",  "70px",  "120px",
                  "20px",  "70px",  "120px" ];

const Y_POSNS = [ "20px",  "20px",  "20px",
                  "70px",  "70px",  "70px",
                 "120px", "120px", "120px"  ];

const DURATION = "2s";
const SNAPSHOT_TIME ="3";
const SVGNS = "http://www.w3.org/2000/svg";

// Convenience wrapper using testAnimatedGrid to make 15pt-by-15pt rects
function testAnimatedRectGrid(animationTagName, animationAttrHashList) {
  var targetTagName = "rect";
  var targetAttrHash = {"width"  : "15px",
                        "height" : "15px" };
  testAnimatedGrid(targetTagName,    targetAttrHash,
                   animationTagName, animationAttrHashList);
}

// Convenience wrapper using testAnimatedGrid to make grid of text
function testAnimatedTextGrid(animationTagName, animationAttrHashList) {
  var targetTagName = "text";
  var targetAttrHash = { };
  testAnimatedGrid(targetTagName,    targetAttrHash,
                   animationTagName, animationAttrHashList);
}

// Generates a visual grid of elements of type "targetTagName", with the
// attribute values given in targetAttrHash.  Each generated element has
// exactly one child -- an animation element of type "animationTagName", with
// the attribute values given in animationAttrHash.
function testAnimatedGrid(targetTagName,    targetAttrHash,
                          animationTagName, animationAttrHashList) {
    // SANITY CHECK
  const numElementsToMake = START_TIMES.length;
  if (X_POSNS.length != numElementsToMake ||
      Y_POSNS.length != numElementsToMake) {
    return;
  }
  
  for (var i = 0; i < animationAttrHashList.length; i++) {
    var animationAttrHash = animationAttrHashList[i];
    // Default to fill="freeze" so we can test the final value of the animation
    if (!animationAttrHash["fill"]) {
      animationAttrHash["fill"] = "freeze";
    }
  }

  // Build the grid!
  var svg = document.documentElement;
  for (var i = 0; i < numElementsToMake; i++) {
    // Build target & animation elements
    var targetElem = buildElement(targetTagName, targetAttrHash);
    for (var j = 0; j < animationAttrHashList.length; j++) {
      var animationAttrHash = animationAttrHashList[j];
      var animElem = buildElement(animationTagName, animationAttrHash);

      // Customize them using global constant values
      targetElem.setAttribute("x", X_POSNS[i]);
      targetElem.setAttribute("y", Y_POSNS[i]);
      animElem.setAttribute("begin", START_TIMES[i]);
      animElem.setAttribute("dur", DURATION);

      // Append to target
      targetElem.appendChild(animElem);
    }
    // Insert target into DOM
    svg.appendChild(targetElem);
  }

  // Take snapshot
  setTimeAndSnapshot(SNAPSHOT_TIME, true);
}

function buildElement(tagName, attrHash) {
  var elem = document.createElementNS(SVGNS, tagName);
  for (var attrName in attrHash) {
    var attrValue = attrHash[attrName];
    elem.setAttribute(attrName, attrValue);
  }
  // If we're creating a text node, populate it with some text.
  if (tagName == "text") {
    elem.appendChild(document.createTextNode("abc"));
  }
  return elem;
}
