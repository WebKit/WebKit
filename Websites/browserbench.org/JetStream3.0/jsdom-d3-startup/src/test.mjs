/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright 2025 Google LLC
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

import { JSDOM } from "jsdom";
import * as d3 from "d3";
import * as topojson from "topojson-client";

export function runTest(airportsData, usData) {
    const airports = d3.csvParse(airportsData, d3.autoType);

    // Setup jsdom
    const dom = new JSDOM(`<!DOCTYPE html><body></body>`);
    const document = dom.window.document;
    const body = d3.select(document.body);


    const width = 975;
    const height = 610;

    const svg = body.append("svg")
        .attr("width", width)
        .attr("height", height)
        .attr("viewBox", [0, 0, 975, 610])
        .attr("style", "width: 100%; height: auto; height: intrinsic;");

    const path = d3.geoPath();

    const states = topojson.feature(usData, usData.objects.states);
    svg.append("path")
        .datum(topojson.mesh(usData, usData.objects.states, (a, b) => a !== b))
        .attr("fill", "none")
        .attr("stroke", "currentColor")
        .attr("stroke-linejoin", "round")
        .attr("d", path);

    const delaunay = d3.Delaunay.from(airports, d => d.longitude, d => d.latitude);
    const voronoi = delaunay.voronoi([0, 0, width, height]);

    svg.append("g")
        .attr("fill", "none")
        .attr("stroke", "currentColor")
        .attr("stroke-opacity", 0.2)
        .selectAll("path")
        .data(airports)
        .join("path")
        .attr("d", (d, i) => voronoi.renderCell(i))
        .append("title")
        .text(d => `${d.name}
${d.city}, ${d.state}`);

    svg.append("path")
        .datum(states)
        .attr("fill", "none")
        .attr("d", path);

    svg.append("g")
        .attr("font-family", "sans-serif")
        .attr("font-size", 10)
        .attr("text-anchor", "middle")
        .selectAll("text")
        .data(airports)
        .join("text")
        .attr("transform", d => `translate(${d.longitude},${d.latitude})`)
        .call(text => text.append("tspan")
            .attr("y", "-0.7em")
            .attr("font-weight", "bold")
            .text(d => d.iata))
        .call(text => text.append("tspan")
            .attr("x", 0)
            .attr("y", "0.7em")
            .attr("fill-opacity", 0.7)
            .text(d => d.name));

    return body.html();
}