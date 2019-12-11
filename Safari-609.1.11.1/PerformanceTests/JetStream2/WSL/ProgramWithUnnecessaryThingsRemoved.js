/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
"use strict";

function programWithUnnecessaryThingsRemoved(program)
{
    let nameFinder = new NameFinder();
    
    // Build our roots.
    for (let statement of program.topLevelStatements) {
        if (statement.origin.originKind === "user")
            nameFinder.add(statement.name);
    }
    
    // Unfortunately, we cannot know yet which operator casts we'll need.
    nameFinder.add("operator cast");
    
    // We need these even if the program doesn't mention them by name.
    nameFinder.add("void");
    nameFinder.add("bool");
    nameFinder.add("int");
    
    // Pull in things as necessary.
    while (nameFinder.worklist.length) {
        let name = nameFinder.worklist.pop();
        for (let thing of program.globalNameContext.underlyingThings(Anything, name))
            thing.visit(nameFinder);
    }
    
    let result = new Program();
    for (let name of nameFinder.set) {
        for (let thing of program.globalNameContext.underlyingThings(Anything, name))
            result.add(thing);
    }
    
    return result;
}

