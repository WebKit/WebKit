// Copyright (C) 2019 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function timeDifference(dateString) {
    const dateDiff = new Date(new Date().getTime() - new Date(dateString).getTime());
    const hour = dateDiff.getUTCHours();
    const day = dateDiff.getUTCDate() - 1;
    const month = dateDiff.getUTCMonth();
    const year = dateDiff.getUTCFullYear() - 1970;
    if (hour === 0 && day === 0 && month === 0 && year ===0)
        return "less than 1 hour ago";
    if (day === 0 && month === 0 && year === 0)
        return `${hour} hours ago`;
    if (month === 0 && year === 0)
        return `${day} days ${hour} hours ago`;
    if (year === 0)
        return `${month} monthes ${day} days ago`;
    return `${year} years ${month} monthes ago`;
}

class Cookie {
    static getCookie(name) {
        const cookie = document.cookie.match(`${name}=[\w\W]+;`);
        if (cookie) {
            const raw = cookie[0].split('=')[1];
            //escape ';'
            return decodeURIComponent(raw.substr(0, raw.length - 1));
        }
        return null;
    }
    static createCookie(name, value, expiredDays=1, path='/') {
        let expiredTime = new Date();
        expiredTime.setTime(expiredTime.getTime() + expireDays * 24 * 3600 * 1000);
        document.cookie = `${name}=${encodeURIComponent(value)}; expires=${expiredTime.toUTCString()}; path=${path}`;
    }
    static eraseCookie(name) {
        if (getCookie(name))
            document.cookie = `${name}=; Max-Age=-99999999;`
    }
}

function isDarkMode () {
    if (!window.matchMedia) {
        return false;
    }
    const match = window.matchMedia('(prefers-color-scheme: dark)');
    return match.matches;
}

// Uses intersection observer: <https://developer.mozilla.org/en-US/docs/Web/API/Intersection_Observer_API>
function createInsertionObservers(element, callback=()=>{}, startThreshold=0.0, endTreshold=1.0, step=0.1, option={}) {
    const useOption = {};
    useOption.root = option.root instanceof HTMLElement ? option.root : null;
    useOption.rootMargin = option.rootMargin ? option.rootMargin : "0%";
    const thresholdArray = [];
    for (let i = startThreshold; i <= endTreshold; i+= step) {
        thresholdArray.push(i);
    }
    thresholdArray.forEach(threshold => {
        useOption.threshold = threshold;
        const observer = new IntersectionObserver(callback, useOption);
        observer.observe(element);
    });
}

function disableBackGesture() {
    window.addEventListener('mousewheel', e => {
        const target = e.target;
        for (let parent = target.parentElement; parent; parent = parent.parentElement ) {
            if (parent.scrollLeft !== 0 && e.deltaX < 0)
                return;
        }
        if (e.deltaX < 0)
            e.preventDefault();
    });
}

export {timeDifference, isDarkMode, Cookie, createInsertionObservers, disableBackGesture};
