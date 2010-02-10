/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.AuditRules.IPAddressRegexp = /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/;

WebInspector.AuditRules.CacheableResponseCodes =
{
    200: true,
    203: true,
    206: true,
    300: true,
    301: true,
    410: true,

    304: true // Underlying resource is cacheable
}

/**
 * @param {Array} array Array of Elements (outerHTML is used) or strings (plain value is used as innerHTML)
 */
WebInspector.AuditRules.arrayAsUL = function(array, shouldLinkify)
{
    if (!array.length)
        return "";
    var ulElement = document.createElement("ul");
    for (var i = 0; i < array.length; ++i) {
        var liElement = document.createElement("li");
        if (array[i] instanceof Element)
            liElement.appendChild(array[i]);
        else if (shouldLinkify)
            liElement.appendChild(WebInspector.linkifyURLAsNode(array[i]));
        else
            liElement.innerHTML = array[i];
        ulElement.appendChild(liElement);
    }
    return ulElement.outerHTML;
}

WebInspector.AuditRules.getDomainToResourcesMap = function(resources, types, regexp, needFullResources)
{
    var domainToResourcesMap = {};
    for (var i = 0, size = resources.length; i < size; ++i) {
        var resource = resources[i];
        if (types && types.indexOf(resource.type) === -1)
            continue;
        var match = resource.url.match(regexp);
        if (!match)
            continue;
        var domain = match[2];
        var domainResources = domainToResourcesMap[domain];
        if (domainResources === undefined) {
          domainResources = [];
          domainToResourcesMap[domain] = domainResources;
        }
        domainResources.push(needFullResources ? resource : resource.url);
    }
    return domainToResourcesMap;
}

WebInspector.AuditRules.evaluateInTargetWindow = function(func, callback)
{
    InjectedScriptAccess.getDefault().evaluateOnSelf(func.toString(), callback);
}


WebInspector.AuditRules.GzipRule = function()
{
    WebInspector.AuditRule.call(this, "network-gzip", "Enable gzip compression");
}

WebInspector.AuditRules.GzipRule.prototype = {
    doRun: function(resources, result, callback)
    {
        try {
            var commonMessage = undefined;
            var totalSavings = 0;
            var compressedSize = 0
            var candidateSize = 0
            var outputResources = [];
            for (var i = 0, length = resources.length; i < length; ++i) {
                var resource = resources[i];
                if (this._shouldCompress(resource)) {
                    var size = resource.contentLength;
                    candidateSize += size;
                    if (this._isCompressed(resource)) {
                        compressedSize += size;
                        continue;
                    }
                    if (!commonMessage)
                        commonMessage = result.appendChild("");
                    var savings = 2 * size / 3;
                    totalSavings += savings;
                    outputResources.push(
                        String.sprintf("Compressing %s could save ~%s",
                        WebInspector.linkifyURL(resource.url), Number.bytesToString(savings)));
                }
            }
            if (commonMessage) {
              commonMessage.value =
                  String.sprintf("Compressing the following resources with gzip could reduce their " +
                      "transfer size by about two thirds (~%s):", Number.bytesToString(totalSavings));
              commonMessage.appendChild(WebInspector.AuditRules.arrayAsUL(outputResources));
              result.score = 100 * compressedSize / candidateSize;
              result.type = WebInspector.AuditRuleResult.Type.Violation;
            }
        } catch(e) {
            console.log(e);
        } finally {
            callback(result);
        }
    },

    _isCompressed: function(resource)
    {
        var encoding = resource.responseHeaders["Content-Encoding"];
        return encoding === "gzip" || encoding === "deflate";
    },

    _shouldCompress: function(resource)
    {
        return WebInspector.Resource.Type.isTextType(resource.type) && resource.domain && resource.contentLength !== undefined && resource.contentLength > 150;
    }
}

WebInspector.AuditRules.GzipRule.prototype.__proto__ = WebInspector.AuditRule.prototype;


WebInspector.AuditRules.CombineExternalResourcesRule = function(id, name, type, resourceTypeName, parametersObject)
{
    WebInspector.AuditRule.call(this, id, name, parametersObject);
    this._type = type;
    this._resourceTypeName = resourceTypeName;
}

WebInspector.AuditRules.CombineExternalResourcesRule.prototype = {
    doRun: function(resources, result, callback)
    {
        try {
            var domainToResourcesMap = WebInspector.AuditRules.getDomainToResourcesMap(resources, [this._type], WebInspector.URLRegExp);
            var penalizedResourceCount = 0;
            // TODO: refactor according to the chosen i18n approach
            for (var domain in domainToResourcesMap) {
                var domainResources = domainToResourcesMap[domain];
                var extraResourceCount = domainResources.length - this.getValue("AllowedPerDomain");
                if (extraResourceCount <= 0)
                    continue;
                penalizedResourceCount += extraResourceCount - 1;
                result.appendChild(
                    String.sprintf("There are %d %s files served from %s. Consider combining them into as few files as possible.",
                    domainResources.length, this._resourceTypeName, domain));
            }
            result.score = 100 - (penalizedResourceCount * this.getValue("ScorePerResource"));
            result.type = WebInspector.AuditRuleResult.Type.Hint;
        } catch(e) {
            console.log(e);
        } finally {
            callback(result);
        }
    }
};

WebInspector.AuditRules.CombineExternalResourcesRule.prototype.__proto__ = WebInspector.AuditRule.prototype;


WebInspector.AuditRules.CombineJsResourcesRule = function(parametersObject) {
    WebInspector.AuditRules.CombineExternalResourcesRule.call(this, "page-externaljs", "Combine external JavaScript", WebInspector.Resource.Type.Script, "JS", parametersObject);
}

WebInspector.AuditRules.CombineJsResourcesRule.prototype.__proto__ = WebInspector.AuditRules.CombineExternalResourcesRule.prototype;


WebInspector.AuditRules.CombineCssResourcesRule = function(parametersObject) {
    WebInspector.AuditRules.CombineExternalResourcesRule.call(this, "page-externalcss", "Combine external CSS", WebInspector.Resource.Type.Stylesheet, "CSS", parametersObject);
}

WebInspector.AuditRules.CombineCssResourcesRule.prototype.__proto__ = WebInspector.AuditRules.CombineExternalResourcesRule.prototype;


WebInspector.AuditRules.MinimizeDnsLookupsRule = function(parametersObject) {
    WebInspector.AuditRule.call(this, "network-minimizelookups", "Minimize DNS lookups", parametersObject);
}

WebInspector.AuditRules.MinimizeDnsLookupsRule.prototype = {
    doRun: function(resources, result, callback)
    {
        try {
            var violationDomains = [];
            var domainToResourcesMap = WebInspector.AuditRules.getDomainToResourcesMap(resources, undefined, WebInspector.URLRegExp);
            for (var domain in domainToResourcesMap) {
                if (domainToResourcesMap[domain].length > 1)
                    continue;
                var match = domain.match(WebInspector.URLRegExp);
                if (!match)
                    continue;
                if (!match[2].search(WebInspector.AuditRules.IPAddressRegexp))
                    continue; // an IP address
                violationDomains.push(match[2]);
            }
            if (violationDomains.length <= this.getValue("HostCountThreshold"))
                return;
            var commonMessage = result.appendChild(
                "The following domains only serve one resource each. If possible, avoid the extra DNS " +
                "lookups by serving these resources from existing domains.");
            commonMessage.appendChild(WebInspector.AuditRules.arrayAsUL(violationDomains));
            result.score = 100 - violationDomains.length * this.getValue("ViolationDomainScore");
        } catch(e) {
            console.log(e);
        } finally {
            callback(result);
        }
    }
}

WebInspector.AuditRules.MinimizeDnsLookupsRule.prototype.__proto__ = WebInspector.AuditRule.prototype;


WebInspector.AuditRules.ParallelizeDownloadRule = function(parametersObject)
{
    WebInspector.AuditRule.call(this, "network-parallelizehosts", "Parallelize downloads across hostnames", parametersObject);
}


WebInspector.AuditRules.ParallelizeDownloadRule.prototype = {
    doRun: function(resources, result, callback)
    {
        function hostSorter(a, b)
        {
            var aCount = domainToResourcesMap[a].length;
            var bCount = domainToResourcesMap[b].length;
            return (aCount < bCount) ? 1 : (aCount == bCount) ? 0 : -1;
        }

        try {
            var domainToResourcesMap = WebInspector.AuditRules.getDomainToResourcesMap(
                resources,
                [WebInspector.Resource.Type.Stylesheet, WebInspector.Resource.Type.Image],
                WebInspector.URLRegExp,
                true);

            var hosts = [];
            for (var url in domainToResourcesMap)
                hosts.push(url);

            if (!hosts.length)
                return; // no hosts (local file or something)

            hosts.sort(hostSorter);

            var optimalHostnameCount = this.getValue("OptimalHostnameCount");
            if (hosts.length > optimalHostnameCount)
                hosts.splice(optimalHostnameCount);

            var busiestHostResourceCount = domainToResourcesMap[hosts[0]].length;
            var resourceCountAboveThreshold = busiestHostResourceCount - this.getValue("MinRequestThreshold");
            if (resourceCountAboveThreshold <= 0)
                return;

            var avgResourcesPerHost = 0;
            for (var i = 0, size = hosts.length; i < size; ++i)
                avgResourcesPerHost += domainToResourcesMap[hosts[i]].length;

            // Assume optimal parallelization.
            avgResourcesPerHost /= optimalHostnameCount;

            avgResourcesPerHost = Math.max(avgResourcesPerHost, 1);

            var pctAboveAvg = (resourceCountAboveThreshold / avgResourcesPerHost) - 1.0;

            var minBalanceThreshold = this.getValue("MinBalanceThreshold");
            if (pctAboveAvg < minBalanceThreshold) {
                result.score = 100;
                return;
            }

            result.score = (1 - (pctAboveAvg - minBalanceThreshold)) * 100;
            result.type = WebInspector.AuditRuleResult.Type.Hint;

            var resourcesOnBusiestHost = domainToResourcesMap[hosts[0]];
            var commonMessage = result.appendChild(
                String.sprintf("This page makes %d parallelizable requests to %s" +
                ". Increase download parallelization by distributing the following" +
                " requests across multiple hostnames.", busiestHostResourceCount, hosts[0]));
            var outputResources = [];
            for (var i = 0, size = resourcesOnBusiestHost.length; i < size; ++i)
                outputResources.push(resourcesOnBusiestHost[i].url);
            commonMessage.appendChild(WebInspector.AuditRules.arrayAsUL(outputResources, true));
        } catch(e) {
            console.log(e);
        } finally {
            callback(result);
        }
    }
}

WebInspector.AuditRules.ParallelizeDownloadRule.prototype.__proto__ = WebInspector.AuditRule.prototype;


// The reported CSS rule size is incorrect (parsed != original in WebKit),
// so use percentages instead, which gives a better approximation.
WebInspector.AuditRules.UnusedCssRule = function(parametersObject)
{
    WebInspector.AuditRule.call(this, "page-unusedcss", "Remove unused CSS", parametersObject);
}

WebInspector.AuditRules.UnusedCssRule.prototype = {
    _getUnusedStylesheetRatioMessage: function(unusedLength, type, location, styleSheetLength)
    {
        var url = type === "href"
            ? WebInspector.linkifyURL(location)
            : String.sprintf("Inline block #%s", location);
        var pctUnused = Math.round(unusedLength / styleSheetLength * 100);
        return String.sprintf("%s: %f%% (estimated) is not used by the current page.", url, pctUnused);
    },

    _getUnusedTotalRatioMessage: function(unusedLength, totalLength)
    {
        var pctUnused = Math.round(unusedLength / totalLength * 100);
        return String.sprintf("%d%% of CSS (estimated) is not used by the current page.", pctUnused);
    },

    doRun: function(resources, result, callback)
    {
        var self = this;
        function evalCallback(evalResult, isException) {
            try {
              if (isException)
                  return;

              var totalLength = 0;
              var totalUnusedLength = 0;
              var topMessage;
              var styleSheetMessage;
              for (var i = 0; i < evalResult.length; ) {
                  var type = evalResult[i++];
                  if (type === "totalLength") {
                      totalLength = evalResult[i++];
                      continue;
                  }

                  var styleSheetLength = evalResult[i++];
                  var location = evalResult[i++];
                  var unusedRules = evalResult[i++];
                  styleSheetMessage = undefined;
                  if (!topMessage)
                      topMessage = result.appendChild("");

                  var totalUnusedRuleLength = 0;
                  var ruleSelectors = [];
                  for (var j = 0; j < unusedRules.length; ++j) {
                      var rule = unusedRules[j];
                      totalUnusedRuleLength += parseInt(rule[1]);
                      if (!styleSheetMessage)
                          styleSheetMessage = result.appendChild("");
                      ruleSelectors.push(rule[0]);
                  }
                  styleSheetMessage.appendChild(WebInspector.AuditRules.arrayAsUL(ruleSelectors));

                  styleSheetMessage.value = self._getUnusedStylesheetRatioMessage(totalUnusedRuleLength, type, location, styleSheetLength);
                  totalUnusedLength += totalUnusedRuleLength;
              }
              if (totalUnusedLength) {
                  var totalUnusedPercent = totalUnusedLength / totalLength;
                  topMessage.value = self._getUnusedTotalRatioMessage(totalUnusedLength, totalLength);
                  var pctMultiplier = Math.log(Math.max(200, totalUnusedLength - 800)) / 7 - 0.6;
                  result.score = (1 - totalUnusedPercent * pctMultiplier) * 100;
                  result.type = WebInspector.AuditRuleResult.Type.Hint;
              } else
                  result.score = 100;
            } catch(e) {
                console.log(e);
            } finally {
                callback(result);
            }
        }

        function routine()
        {
            var styleSheets = document.styleSheets;
            if (!styleSheets)
                return {};
            var styleSheetToUnusedRules = [];
            var inlineBlockOrdinal = 0;
            var totalCSSLength = 0;
            var pseudoSelectorRegexp = /:hover|:link|:active|:visited|:focus/;
            for (var i = 0; i < styleSheets.length; ++i) {
                var styleSheet = styleSheets[i];
                if (!styleSheet.cssRules)
                    continue;
                var currentStyleSheetSize = 0;
                var unusedRules = [];
                for (var curRule = 0; curRule < styleSheet.cssRules.length; ++curRule) {
                    var rule = styleSheet.cssRules[curRule];
                    var textLength = rule.cssText ? rule.cssText.length : 0;
                    currentStyleSheetSize += textLength;
                    totalCSSLength += textLength;
                    if (rule.type !== 1 || rule.selectorText.match(pseudoSelectorRegexp))
                        continue;
                    var nodes = document.querySelectorAll(rule.selectorText);
                    if (nodes && nodes.length)
                        continue;
                    unusedRules.push([rule.selectorText, textLength]);
                }
                if (unusedRules.length) {
                    styleSheetToUnusedRules.push(styleSheet.href ? "href" : "inline");
                    styleSheetToUnusedRules.push(currentStyleSheetSize);
                    styleSheetToUnusedRules.push(styleSheet.href ? styleSheet.href : ++inlineBlockOrdinal);
                    styleSheetToUnusedRules.push(unusedRules);
                }
            }
            styleSheetToUnusedRules.push("totalLength");
            styleSheetToUnusedRules.push(totalCSSLength);
            return styleSheetToUnusedRules;
        }

        WebInspector.AuditRules.evaluateInTargetWindow(routine, evalCallback);
    }
}

WebInspector.AuditRules.UnusedCssRule.prototype.__proto__ = WebInspector.AuditRule.prototype;


WebInspector.AuditRules.CacheControlRule = function(id, name, parametersObject)
{
    WebInspector.AuditRule.call(this, id, name, parametersObject);
}

WebInspector.AuditRules.CacheControlRule.MillisPerMonth = 1000 * 60 * 60 * 24 * 30;

WebInspector.AuditRules.CacheControlRule.prototype = {

    InfoCheck: -1,
    FailCheck: 0,
    WarningCheck: 1,
    SevereCheck: 2,

    doRun: function(resources, result, callback)
    {
        try {
            var cacheableAndNonCacheableResources = this._cacheableAndNonCacheableResources(resources);
            if (cacheableAndNonCacheableResources[0].length) {
                result.score = 100;
                this.runChecks(cacheableAndNonCacheableResources[0], result);
            }
            this.handleNonCacheableResources(cacheableAndNonCacheableResources[1], result);
        } catch(e) {
            console.log(e);
        } finally {
            callback(result);
        }
    },

    handleNonCacheableResources: function()
    {
    },

    _cacheableAndNonCacheableResources: function(resources)
    {
        var processedResources = [[], []];
        for (var i = 0; i < resources.length; ++i) {
            var resource = resources[i];
            if (!this.isCacheableResource(resource))
                continue;
            if (this._isExplicitlyNonCacheable(resource))
                processedResources[1].push(resource);
            else
                processedResources[0].push(resource);
        }
        return processedResources;
    },

    execCheck: function(messageText, resourceCheckFunction, resources, severity, result)
    {
        var topMessage;
        var failingResources = 0;
        var resourceCount = resources.length;
        var outputResources = [];
        for (var i = 0; i < resourceCount; ++i) {
            if (resourceCheckFunction.call(this, resources[i])) {
                ++failingResources;
                if (!topMessage)
                    topMessage = result.appendChild(messageText);
                outputResources.push(resources[i].url);
            }
        }
        if (topMessage)
            topMessage.appendChild(WebInspector.AuditRules.arrayAsUL(outputResources, true));
        if (failingResources) {
            switch (severity) {
                case this.FailCheck:
                    result.score = 0;
                    result.type = WebInspector.AuditRuleResult.Type.Violation;
                    break;
                case this.SevereCheck:
                case this.WarningCheck:
                    result.score -= 50 * severity * failingResources / resourceCount;
                    result.type = WebInspector.AuditRuleResult.Type.Hint;
                    break;
            }
        }
        return topMessage;
    },

    freshnessLifetimeGreaterThan: function(resource, timeMs)
    {
        var dateHeader = this.responseHeader(resource, "Date");
        if (!dateHeader)
            return false;

        var dateHeaderMs = Date.parse(dateHeader);
        if (isNaN(dateHeaderMs))
            return false;

        var freshnessLifetimeMs;
        var maxAgeMatch = this.responseHeaderMatch(resource, "Cache-Control", "max-age=(\\d+)");

        if (maxAgeMatch)
            freshnessLifetimeMs = (maxAgeMatch[1]) ? 1000 * maxAgeMatch[1] : 0;
        else {
            var expiresHeader = this.responseHeader(resource, "Expires");
            if (expiresHeader) {
                var expDate = Date.parse(expiresHeader);
                if (!isNaN(expDate))
                    freshnessLifetimeMs = expDate - dateHeaderMs;
            }
        }

        return (isNaN(freshnessLifetimeMs)) ? false : freshnessLifetimeMs > timeMs;
    },

    responseHeader: function(resource, header)
    {
        return resource.responseHeaders[header];
    },

    hasResponseHeader: function(resource, header)
    {
        return resource.responseHeaders[header] !== undefined;
    },

    isCompressible: function(resource)
    {
        return WebInspector.Resource.Type.isTextType(resource.type);
    },

    isPubliclyCacheable: function(resource)
    {
        if (this._isExplicitlyNonCacheable(resource))
            return false;

        if (this.responseHeaderMatch(resource, "Cache-Control", "public"))
            return true;

        return resource.url.indexOf("?") == -1 && !this.responseHeaderMatch(resource, "Cache-Control", "private");
    },

    responseHeaderMatch: function(resource, header, regexp)
    {
        return resource.responseHeaders[header]
            ? resource.responseHeaders[header].match(new RegExp(regexp, "im"))
            : undefined;
    },

    hasExplicitExpiration: function(resource)
    {
        return this.hasResponseHeader(resource, "Date") &&
            (this.hasResponseHeader(resource, "Expires") || this.responseHeaderMatch(resource, "Cache-Control", "max-age"));
    },

    _isExplicitlyNonCacheable: function(resource)
    {
        var hasExplicitExp = this.hasExplicitExpiration(resource);
        return this.responseHeaderMatch(resource, "Cache-Control", "(no-cache|no-store|must-revalidate)") ||
            this.responseHeaderMatch(resource, "Pragma", "no-cache") ||
            (hasExplicitExp && !this.freshnessLifetimeGreaterThan(resource, 0)) ||
            (!hasExplicitExp && resource.url && resource.url.indexOf("?") >= 0) ||
            (!hasExplicitExp && !this.isCacheableResource(resource));
    },

    isCacheableResource: function(resource)
    {
        return resource.statusCode !== undefined && WebInspector.AuditRules.CacheableResponseCodes[resource.statusCode];
    }
}

WebInspector.AuditRules.CacheControlRule.prototype.__proto__ = WebInspector.AuditRule.prototype;


WebInspector.AuditRules.BrowserCacheControlRule = function(parametersObject)
{
    WebInspector.AuditRules.CacheControlRule.call(this, "http-browsercache", "Leverage browser caching", parametersObject);
}

WebInspector.AuditRules.BrowserCacheControlRule.prototype = {
    handleNonCacheableResources: function(resources, result)
    {
        if (resources.length) {
            var message = result.appendChild(
                "The following resources are explicitly non-cacheable. Consider making them cacheable if possible:");
            var resourceOutput = [];
            for (var i = 0; i < resources.length; ++i)
                resourceOutput.push(resources[i].url);
            message.appendChild(WebInspector.AuditRules.arrayAsUL(resourceOutput, true));
        }
    },

    runChecks: function(resources, result, callback)
    {
        this.execCheck(
            "The following resources are missing a cache expiration." +
            " Resources that do not specify an expiration may not be" +
            " cached by browsers:",
            this._missingExpirationCheck, resources, this.SevereCheck, result);
        this.execCheck(
            "The following resources specify a \"Vary\" header that" +
            " disables caching in most versions of Internet Explorer:",
            this._varyCheck, resources, this.SevereCheck, result);
        this.execCheck(
            "The following cacheable resources have a short" +
            " freshness lifetime:",
            this._oneMonthExpirationCheck, resources, this.WarningCheck, result);

        // Unable to implement the favicon check due to the WebKit limitations.

        this.execCheck(
            "To further improve cache hit rate, specify an expiration" +
            " one year in the future for the following cacheable" +
            " resources:",
            this._oneYearExpirationCheck, resources, this.InfoCheck, result);
    },

    _missingExpirationCheck: function(resource)
    {
        return this.isCacheableResource(resource) && !this.hasResponseHeader(resource, "Set-Cookie") && !this.hasExplicitExpiration(resource);
    },

    _varyCheck: function(resource)
    {
        var varyHeader = this.responseHeader(resource, "Vary");
        if (varyHeader) {
            varyHeader = varyHeader.replace(/User-Agent/gi, "");
            varyHeader = varyHeader.replace(/Accept-Encoding/gi, "");
            varyHeader = varyHeader.replace(/[, ]*/g, "");
        }
        return varyHeader && varyHeader.length && this.isCacheableResource(resource) && this.freshnessLifetimeGreaterThan(resource, 0);
    },

    _oneMonthExpirationCheck: function(resource)
    {
        return this.isCacheableResource(resource) &&
            !this.hasResponseHeader(resource, "Set-Cookie") &&
            !this.freshnessLifetimeGreaterThan(resource, WebInspector.AuditRules.CacheControlRule.MillisPerMonth) &&
            this.freshnessLifetimeGreaterThan(resource, 0);
    },

    _oneYearExpirationCheck: function(resource)
    {
        return this.isCacheableResource(resource) &&
            !this.hasResponseHeader(resource, "Set-Cookie") &&
            !this.freshnessLifetimeGreaterThan(resource, 11 * WebInspector.AuditRules.CacheControlRule.MillisPerMonth) &&
            this.freshnessLifetimeGreaterThan(resource, WebInspector.AuditRules.CacheControlRule.MillisPerMonth);
    }
}

WebInspector.AuditRules.BrowserCacheControlRule.prototype.__proto__ = WebInspector.AuditRules.CacheControlRule.prototype;


WebInspector.AuditRules.ProxyCacheControlRule = function(parametersObject) {
    WebInspector.AuditRules.CacheControlRule.call(this, "http-proxycache", "Leverage proxy caching", parametersObject);
}

WebInspector.AuditRules.ProxyCacheControlRule.prototype = {
    runChecks: function(resources, result, callback)
    {
        this.execCheck(
            "Resources with a \"?\" in the URL are not cached by most" +
            " proxy caching servers:",
            this._questionMarkCheck, resources, this.WarningCheck, result);
        this.execCheck(
            "Consider adding a \"Cache-Control: public\" header to the" +
            " following resources:",
            this._publicCachingCheck, resources, this.InfoCheck, result);
        this.execCheck(
            "The following publicly cacheable resources contain" +
            " a Set-Cookie header. This security vulnerability" +
            " can cause cookies to be shared by multiple users.",
            this._setCookieCacheableCheck, resources, this.FailCheck, result);
    },

    _questionMarkCheck: function(resource)
    {
        return resource.url.indexOf("?") >= 0 && !this.hasResponseHeader(resource, "Set-Cookie") && this.isPubliclyCacheable(resource);
    },

    _publicCachingCheck: function(resource)
    {
        return this.isCacheableResource(resource) &&
            !this.isCompressible(resource) &&
            !this.responseHeaderMatch(resource, "Cache-Control", "public") &&
            !this.hasResponseHeader(resource, "Set-Cookie");
    },

    _setCookieCacheableCheck: function(resource)
    {
        return this.hasResponseHeader(resource, "Set-Cookie") && this.isPubliclyCacheable(resource);
    }
}

WebInspector.AuditRules.ProxyCacheControlRule.prototype.__proto__ = WebInspector.AuditRules.CacheControlRule.prototype;


WebInspector.AuditRules.ImageDimensionsRule = function(parametersObject)
{
    WebInspector.AuditRule.call(this, "page-imagedims", "Specify image dimensions", parametersObject);
}

WebInspector.AuditRules.ImageDimensionsRule.prototype = {
    doRun: function(resources, result, callback)
    {
        function evalCallback(evalResult, isException)
        {
            try {
                if (isException)
                    return;
                if (!evalResult || !evalResult.totalImages)
                    return;
                result.score = 100;
                var topMessage = result.appendChild(
                    "A width and height should be specified for all images in order to " +
                    "speed up page display. The following image(s) are missing a width and/or height:");
                var map = evalResult.map;
                var outputResources = [];
                for (var url in map) {
                    var value = WebInspector.linkifyURL(url);
                    if (map[url] > 1)
                        value += " (" + map[url] + " uses)";
                    outputResources.push(value);
                    result.score -= this.getValue("ScorePerImageUse") * map[url];
                    result.type = WebInspector.AuditRuleResult.Type.Hint;
                }
                topMessage.appendChild(WebInspector.AuditRules.arrayAsUL(outputResources));
            } catch(e) {
                console.log(e);
            } finally {
                callback(result);
            }
        }

        function routine()
        {
            var images = document.getElementsByTagName("img");
            const widthRegExp = /width[^:;]*:/gim;
            const heightRegExp = /height[^:;]*:/gim;

            function hasDimension(element, cssText, rules, regexp, attributeName) {
                if (element.attributes.getNamedItem(attributeName) != null || (cssText && cssText.match(regexp)))
                    return true;

                if (!rules)
                    return false;
                for (var i = 0; i < rules.length; ++i) {
                    if (rules.item(i).style.cssText.match(regexp))
                        return true;
                }
                return false;
            }

            function hasWidth(element, cssText, rules) {
                return hasDimension(element, cssText, rules, widthRegExp, "width");
            }

            function hasHeight(element, cssText, rules) {
                return hasDimension(element, cssText, rules, heightRegExp, "height");
            }

            var urlToNoDimensionCount = {};
            var found = false;
            for (var i = 0; i < images.length; ++i) {
                var image = images[i];
                if (!image.src)
                    continue;
                var position = document.defaultView.getComputedStyle(image).getPropertyValue("position");
                if (position === "absolute")
                    continue;
                var cssText = (image.style && image.style.cssText) ? image.style.cssText : "";
                var rules = document.defaultView.getMatchedCSSRules(image, "", true);
                if (!hasWidth(image, cssText, rules) || !hasHeight(image, cssText, rules)) {
                    found = true;
                    if (urlToNoDimensionCount.hasOwnProperty(image.src))
                        ++urlToNoDimensionCount[image.src];
                    else
                        urlToNoDimensionCount[image.src] = 1;
                }
            }
            return found ? {totalImages: images.length, map: urlToNoDimensionCount} : null;
        }

        WebInspector.AuditRules.evaluateInTargetWindow(routine, evalCallback.bind(this));
    }
}

WebInspector.AuditRules.ImageDimensionsRule.prototype.__proto__ = WebInspector.AuditRule.prototype;


WebInspector.AuditRules.CssInHeadRule = function(parametersObject)
{
    WebInspector.AuditRule.call(this, "page-cssinhead", "Put CSS in the document head", parametersObject);
}

WebInspector.AuditRules.CssInHeadRule.prototype = {
    doRun: function(resources, result, callback)
    {
        function evalCallback(evalResult, isException)
        {
            try {
                if (isException)
                    return;
                if (!evalResult)
                    return;
                result.score = 100;
                var outputMessages = [];
                for (var url in evalResult) {
                    var urlViolations = evalResult[url];
                    var topMessage = result.appendChild(
                        String.sprintf("CSS in the %s document body adversely impacts rendering performance.",
                        WebInspector.linkifyURL(url)));
                    if (urlViolations[0]) {
                        outputMessages.push(
                            String.sprintf("%s style block(s) in the body should be moved to the document head.", urlViolations[0]));
                        result.score -= this.getValue("InlineURLScore") * urlViolations[0];
                    }
                    for (var i = 0; i < urlViolations[1].length; ++i) {
                        outputMessages.push(
                            String.sprintf("Link node %s should be moved to the document head", WebInspector.linkifyURL(urlViolations[1])));
                    }
                    result.score -= this.getValue("InlineStylesheetScore") * urlViolations[1];
                    result.type = WebInspector.AuditRuleResult.Type.Hint;
                }
                topMessage.appendChild(WebInspector.AuditRules.arrayAsUL(outputMessages));
            } catch(e) {
                console.log(e);
            } finally {
                callback(result);
            }
        }

        function routine()
        {
            function allViews() {
                var views = [document.defaultView];
                var curView = 0;
                while (curView < views.length) {
                    var view = views[curView];
                    var frames = view.frames;
                    for (var i = 0; i < frames.length; ++i) {
                        if (frames[i] !== view)
                            views.push(frames[i]);
                    }
                    ++curView;
                }
                return views;
            }

            var views = allViews();
            var urlToViolationsArray = {};
            var found = false;
            for (var i = 0; i < views.length; ++i) {
              var view = views[i];
              if (!view.document)
                  continue;

              var inlineStyles = view.document.querySelectorAll("body style");
              var inlineStylesheets = view.document.querySelectorAll(
                  "body link[rel~='stylesheet'][href]");
              if (!inlineStyles.length && !inlineStylesheets.length)
                  continue;

              found = true;
              var inlineStylesheetHrefs = [];
              for (var j = 0; j < inlineStylesheets.length; ++j)
                  inlineStylesheetHrefs.push(inlineStylesheets[j].href);

              urlToViolationsArray[view.location.href] =
                  [inlineStyles.length, inlineStylesheetHrefs];
            }
            return found ? urlToViolationsArray : null;
        }

        WebInspector.AuditRules.evaluateInTargetWindow(routine, evalCallback);
    }
}

WebInspector.AuditRules.CssInHeadRule.prototype.__proto__ = WebInspector.AuditRule.prototype;


WebInspector.AuditRules.StylesScriptsOrderRule = function(parametersObject)
{
    WebInspector.AuditRule.call(this, "page-stylescriptorder", "Optimize the order of styles and scripts", parametersObject);
}

WebInspector.AuditRules.StylesScriptsOrderRule.prototype = {
    doRun: function(resources, result, callback)
    {
        function evalCallback(evalResult, isException)
        {
            try {
                if (isException)
                    return;
                if (!evalResult)
                    return;

                result.score = 100;
                var lateCssUrls = evalResult['late'];
                if (lateCssUrls) {
                    var lateMessage = result.appendChild(
                        'The following external CSS files were included after ' +
                        'an external JavaScript file in the document head. To ' +
                        'ensure CSS files are downloaded in parallel, always ' +
                        'include external CSS before external JavaScript.');
                    lateMessage.appendChild(WebInspector.AuditRules.arrayAsUL(lateCssUrls, true));
                    result.score -= this.getValue("InlineBetweenResourcesScore") * lateCssUrls.length;
                    result.type = WebInspector.AuditRuleResult.Type.Violation;
                }
                if (evalResult['cssBeforeInlineCount']) {
                  var count = evalResult['cssBeforeInlineCount'];
                  result.appendChild(count + ' inline script block' +
                      (count > 1 ? 's were' : ' was') + ' found in the head between an ' +
                      'external CSS file and another resource. To allow parallel ' +
                      'downloading, move the inline script before the external CSS ' +
                      'file, or after the next resource.');
                  result.score -= this.getValue("CSSAfterJSURLScore") * count;
                  result.type = WebInspector.AuditRuleResult.Type.Violation;
                }
            } catch(e) {
                console.log(e);
            } finally {
                callback(result);
            }
        }

        function routine()
        {
            var lateStyles = document.querySelectorAll(
                "head script[src] ~ link[rel~='stylesheet'][href]");
            var stylesBeforeInlineScript = document.querySelectorAll(
                "head link[rel~='stylesheet'][href] ~ script:not([src])");

            var resultObject;
            if (!lateStyles.length && !stylesBeforeInlineScript.length)
                resultObject = null;
            else {
                resultObject = {};
                if (lateStyles.length) {
                  lateStyleUrls = [];
                  for (var i = 0; i < lateStyles.length; ++i)
                      lateStyleUrls.push(lateStyles[i].href);
                  resultObject["late"] = lateStyleUrls;
                }
                resultObject["cssBeforeInlineCount"] = stylesBeforeInlineScript.length;
            }
            return resultObject;
        }

        WebInspector.AuditRules.evaluateInTargetWindow(routine, evalCallback.bind(this));
    }
}

WebInspector.AuditRules.StylesScriptsOrderRule.prototype.__proto__ = WebInspector.AuditRule.prototype;


WebInspector.AuditRules.CookieRuleBase = function(id, name, parametersObject)
{
    WebInspector.AuditRule.call(this, id, name, parametersObject);
}

WebInspector.AuditRules.CookieRuleBase.prototype = {
    doRun: function(resources, result, callback)
    {
        var self = this;
        function resultCallback(receivedCookies, isAdvanced) {
            try {
                self.processCookies(isAdvanced ? receivedCookies : [], resources, result);
            } catch(e) {
                console.log(e);
            } finally {
                callback(result);
            }
        }
        WebInspector.Cookies.getCookiesAsync(resultCallback);
    },

    mapResourceCookies: function(resourcesByDomain, allCookies, callback)
    {
        for (var i = 0; i < allCookies.length; ++i) {
            for (var resourceDomain in resourcesByDomain) {
                if (WebInspector.Cookies.cookieDomainMatchesResourceDomain(allCookies[i].domain, resourceDomain))
                    this._callbackForResourceCookiePairs(resourcesByDomain[resourceDomain], allCookies[i], callback);
            }
        }
    },

    _callbackForResourceCookiePairs: function(resources, cookie, callback)
    {
        if (!resources)
            return;
        for (var i = 0; i < resources.length; ++i) {
            if (WebInspector.Cookies.cookieMatchesResourceURL(cookie, resources[i].url))
                callback(resources[i], cookie);
        }
    }
}

WebInspector.AuditRules.CookieRuleBase.prototype.__proto__ = WebInspector.AuditRule.prototype;


WebInspector.AuditRules.CookieSizeRule = function(parametersObject)
{
    WebInspector.AuditRules.CookieRuleBase.call(this, "http-cookiesize", "Minimize cookie size", parametersObject);
}

WebInspector.AuditRules.CookieSizeRule.prototype = {
    _average: function(cookieArray)
    {
        var total = 0;
        for (var i = 0; i < cookieArray.length; ++i)
            total += cookieArray[i].size;
        return cookieArray.length ? Math.round(total / cookieArray.length) : 0;
    },

    _max: function(cookieArray)
    {
        var result = 0;
        for (var i = 0; i < cookieArray.length; ++i)
            result = Math.max(cookieArray[i].size, result);
        return result;
    },

    processCookies: function(allCookies, resources, result)
    {
        function maxSizeSorter(a, b)
        {
            return b.maxCookieSize - a.maxCookieSize;
        }

        function avgSizeSorter(a, b)
        {
            return b.avgCookieSize - a.avgCookieSize;
        }

        var cookiesPerResourceDomain = {};

        function collectorCallback(resource, cookie)
        {
            var cookies = cookiesPerResourceDomain[resource.domain];
            if (!cookies) {
                cookies = [];
                cookiesPerResourceDomain[resource.domain] = cookies;
            }
            cookies.push(cookie);
        }

        if (!allCookies.length)
            return;

        var sortedCookieSizes = [];

        var domainToResourcesMap = WebInspector.AuditRules.getDomainToResourcesMap(resources,
                null,
                WebInspector.URLRegExp,
                true);
        var matchingResourceData = {};
        this.mapResourceCookies(domainToResourcesMap, allCookies, collectorCallback.bind(this));

        result.score = 100;
        for (var resourceDomain in cookiesPerResourceDomain) {
            var cookies = cookiesPerResourceDomain[resourceDomain];
            sortedCookieSizes.push({
                domain: resourceDomain,
                avgCookieSize: this._average(cookies),
                maxCookieSize: this._max(cookies)
            });
        }
        var avgAllCookiesSize = this._average(allCookies);

        var hugeCookieDomains = [];
        sortedCookieSizes.sort(maxSizeSorter);

        var maxBytesThreshold = this.getValue("MaxBytesThreshold");
        var minBytesThreshold = this.getValue("MinBytesThreshold");

        for (var i = 0, len = sortedCookieSizes.length; i < len; ++i) {
            var maxCookieSize = sortedCookieSizes[i].maxCookieSize;
            if (maxCookieSize > maxBytesThreshold)
                hugeCookieDomains.push(sortedCookieSizes[i].domain + ": " + Number.bytesToString(maxCookieSize));
        }

        var bigAvgCookieDomains = [];
        sortedCookieSizes.sort(avgSizeSorter);
        for (var i = 0, len = sortedCookieSizes.length; i < len; ++i) {
            var domain = sortedCookieSizes[i].domain;
            var avgCookieSize = sortedCookieSizes[i].avgCookieSize;
            if (avgCookieSize > minBytesThreshold && avgCookieSize < maxBytesThreshold)
                bigAvgCookieDomains.push(domain + ": " + Number.bytesToString(avgCookieSize));
        }
        result.appendChild("The average cookie size for all requests on this page is " + Number.bytesToString(avgAllCookiesSize));

        var message;
        if (hugeCookieDomains.length) {
            result.score = 75;
            result.type = WebInspector.AuditRuleResult.Type.Violation;
            message = result.appendChild(
                String.sprintf("The following domains have a cookie size in excess of %d " +
                " bytes. This is harmful because requests with cookies larger than 1KB" +
                " typically cannot fit into a single network packet.", maxBytesThreshold));
            message.appendChild(WebInspector.AuditRules.arrayAsUL(hugeCookieDomains));
        }

        if (bigAvgCookieDomains.length) {
            this.score -= Math.max(0, avgAllCookiesSize - minBytesThreshold) /
                (minBytesThreshold - minBytesThreshold) / this.getValue("TotalPoints");
            if (!result.type)
                result.type = WebInspector.AuditRuleResult.Type.Hint;
            message = result.appendChild(
                String.sprintf("The following domains have an average cookie size in excess of %d" +
                 " bytes. Reducing the size of cookies" +
                 " for these domains can reduce the time it takes to send requests.", minBytesThreshold));
            message.appendChild(WebInspector.AuditRules.arrayAsUL(bigAvgCookieDomains));
        }

        if (!bigAvgCookieDomains.length && !hugeCookieDomains.length)
            result.score = WebInspector.AuditCategoryResult.ScoreNA;
    }
}

WebInspector.AuditRules.CookieSizeRule.prototype.__proto__ = WebInspector.AuditRules.CookieRuleBase.prototype;


WebInspector.AuditRules.StaticCookielessRule = function(parametersObject)
{
    WebInspector.AuditRules.CookieRuleBase.call(this, "http-staticcookieless", "Serve static content from a cookieless domain", parametersObject);
}

WebInspector.AuditRules.StaticCookielessRule.prototype = {
    processCookies: function(allCookies, resources, result)
    {
        var domainToResourcesMap = WebInspector.AuditRules.getDomainToResourcesMap(resources,
                [WebInspector.Resource.Type.Stylesheet,
                 WebInspector.Resource.Type.Image],
                WebInspector.URLRegExp,
                true);
        var totalStaticResources = 0;
        var minResources = this.getValue("MinResources");
        for (var domain in domainToResourcesMap)
            totalStaticResources += domainToResourcesMap[domain].length;
        if (totalStaticResources < minResources)
            return;
        var matchingResourceData = {};
        this.mapResourceCookies(domainToResourcesMap, allCookies, this._collectorCallback.bind(this, matchingResourceData));

        var badUrls = [];
        var cookieBytes = 0;
        for (var url in matchingResourceData) {
            badUrls.push(url);
            cookieBytes += matchingResourceData[url]
        }
        if (badUrls.length < minResources)
            return;

        result.score = 100;
        var badPoints = cookieBytes / 75;
        var violationPct = Math.max(badUrls.length / totalStaticResources, 0.6);
        badPoints *= violationPct;
        result.score -= badPoints;
        result.score = Math.max(result.score, 0);
        result.type = WebInspector.AuditRuleResult.Type.Violation;
        result.appendChild(String.sprintf("%s of cookies were sent with the following static resources.", Number.bytesToString(cookieBytes)));
        var message = result.appendChild("Serve these static resources from a domain that does not set cookies:");
        message.appendChild(WebInspector.AuditRules.arrayAsUL(badUrls, true));
    },

    _collectorCallback: function(matchingResourceData, resource, cookie)
    {
        matchingResourceData[resource.url] = (matchingResourceData[resource.url] || 0) + cookie.size;
    }
}

WebInspector.AuditRules.StaticCookielessRule.prototype.__proto__ = WebInspector.AuditRules.CookieRuleBase.prototype;
