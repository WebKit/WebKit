/*
 * Copyright (C) 2014-2015 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
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

"use strict";

if (typeof(SDP) == "undefined")
    var SDP = {};

(function () {
    var regexps = {
        "vline": "^v=([\\d]+).*$",
        "oline": "^o=([\\w\\-@\\.]+) ([\\d]+) ([\\d]+) IN (IP[46]) ([\\d\\.a-f\\:]+).*$",
        "sline": "^s=(.*)$",
        "tline": "^t=([\\d]+) ([\\d]+).*$",
        "cline": "^c=IN (IP[46]) ([\\d\\.a-f\\:]+).*$",
        "msidsemantic": "^a=msid-semantic: *WMS .*$",
        "mblock": "^m=(audio|video|application) ([\\d]+) ([A-Z/]+)([\\d ]*)$\\r?\\n",
        "mode": "^a=(sendrecv|sendonly|recvonly|inactive).*$",
        "mid": "^a=mid:([!#$%&'*+-.\\w]*).*$",
        "rtpmap": "^a=rtpmap:${type} ([\\w\\-]+)/([\\d]+)/?([\\d]+)?.*$",
        "fmtp": "^a=fmtp:${type} ([\\w\\-=; ]+).*$",
        "param": "([\\w\\-]+)=([\\w\\-]+);?",
        "nack": "^a=rtcp-fb:${type} nack$",
        "nackpli": "^a=rtcp-fb:${type} nack pli$",
        "ccmfir": "^a=rtcp-fb:${type} ccm fir$",
        "ericscream": "^a=rtcp-fb:${type} ericscream$",
        "rtcp": "^a=rtcp:([\\d]+)( IN (IP[46]) ([\\d\\.a-f\\:]+))?.*$",
        "rtcpmux": "^a=rtcp-mux.*$",
        "cname": "^a=ssrc:(\\d+) cname:([\\w+/\\-@\\.\\{\\}]+).*$",
        "msid": "^a=(ssrc:\\d+ )?msid:([\\w+/\\-=]+) +([\\w+/\\-=]+).*$",
        "ufrag": "^a=ice-ufrag:([\\w+/]*).*$",
        "pwd": "^a=ice-pwd:([\\w+/]*).*$",
        "candidate": "^a=candidate:(\\d+) (\\d) (UDP|TCP) ([\\d\\.]*) ([\\d\\.a-f\\:]*) (\\d*)" +
            " typ ([a-z]*)( raddr ([\\d\\.a-f\\:]*) rport (\\d*))?" +
            "( tcptype (active|passive|so))?.*$",
        "fingerprint": "^a=fingerprint:(sha-1|sha-256) ([A-Fa-f\\d\:]+).*$",
        "setup": "^a=setup:(actpass|active|passive).*$",
        "sctpmap": "^a=sctpmap:${port} ([\\w\\-]+)( [\\d]+)?.*$"
    };

    var templates = {
        "sdp":
            "v=${version}\r\n" +
            "o=${username} ${sessionId} ${sessionVersion} ${netType} ${addressType} ${address}\r\n" +
            "s=${sessionName}\r\n" +
            "t=${startTime} ${stopTime}\r\n" +
            "${bundleLine}" +
            "${msidsemanticLine}",

        "msidsemantic": "a=msid-semantic:WMS ${mediaStreamIds}\r\n",

        "mblock":
            "m=${type} ${port} ${protocol} ${fmt}\r\n" +
            "c=${netType} ${addressType} ${address}\r\n" +
            "${rtcpLine}" +
            "${rtcpMuxLine}" +
            "${bundleOnlyLine}" +
            "a=${mode}\r\n" +
            "${midLine}" +
            "${rtpMapLines}" +
            "${fmtpLines}" +
            "${nackLines}" +
            "${nackpliLines}" +
            "${ccmfirLines}" +
            "${ericScreamLines}" +
            "${cnameLines}" +
            "${msidLines}" +
            "${iceCredentialLines}" +
            "${candidateLines}" +
            "${dtlsFingerprintLine}" +
            "${dtlsSetupLine}" +
            "${sctpmapLine}",

        "rtcp": "a=rtcp:${port}${[ ]netType}${[ ]addressType}${[ ]address}\r\n",
        "rtcpMux": "a=rtcp-mux\r\n",
        "mid": "a=mid:${mid}\r\n",
        "bundle": "a=group:BUNDLE ${midsBundle}\r\n",

        "rtpMap": "a=rtpmap:${type} ${encodingName}/${clockRate}${[/]channels}\r\n",
        "fmtp": "a=fmtp:${type} ${parameters}\r\n",
        "nack": "a=rtcp-fb:${type} nack\r\n",
        "nackpli": "a=rtcp-fb:${type} nack pli\r\n",
        "ccmfir": "a=rtcp-fb:${type} ccm fir\r\n",
        "ericscream": "a=rtcp-fb:${type} ericscream\r\n",

        "cname": "a=ssrc:${ssrc} cname:${cname}\r\n",
        "msid": "a=msid:${mediaStreamId} ${mediaStreamTrackId}\r\n",

        "iceCredentials":
            "a=ice-ufrag:${ufrag}\r\n" +
            "a=ice-pwd:${password}\r\n",

        "candidate":
            "a=candidate:${foundation} ${componentId} ${transport} ${priority} ${address} ${port}" +
            " typ ${type}${[ raddr ]relatedAddress}${[ rport ]relatedPort}${[ tcptype ]tcpType}\r\n",

        "dtlsFingerprint": "a=fingerprint:${fingerprintHashFunction} ${fingerprint}\r\n",
        "dtlsSetup": "a=setup:${setup}\r\n",

        "sctpmap": "a=sctpmap:${port} ${app}${[ ]streams}\r\n"
    };

    function match(data, pattern, flags, alt) {
        var r = new RegExp(pattern, flags);
        return data.match(r) || alt && alt.match(r) || null;
    }

    function addDefaults(obj, defaults) {
        for (var p in defaults) {
            if (!defaults.hasOwnProperty(p))
                continue;
            if (typeof(obj[p]) == "undefined")
                obj[p] = defaults[p];
        }
    }

    function fillTemplate(template, info) {
        var text = template;
        for (var p in info) {
            if (!info.hasOwnProperty(p))
                continue;
            var r = new RegExp("\\${(\\[[^\\]]+\\])?" + p + "(\\[[^\\]]+\\])?}");
            text = text.replace(r, function (_, prefix, suffix) {
                if (!info[p] && info[p] != 0)
                    return "";
                prefix = prefix ? prefix.substr(1, prefix.length - 2) : "";
                suffix = suffix ? suffix.substr(1, suffix.length - 2) : "";
                return prefix + info[p] + suffix;
            });
        }
        return text;
    }

    SDP.parse = function (sdpText) {
        sdpText = new String(sdpText);
        var sdpObj = {};
        var parts = sdpText.split(new RegExp(regexps.mblock, "m")) || [sdpText];
        var sblock = parts.shift();
        var version = parseInt((match(sblock, regexps.vline, "m") || [])[1]);
        if (!isNaN(version))
            sdpObj.version = version;
        var originator = match(sblock, regexps.oline, "m");;
        if (originator) {
            sdpObj.originator = {
                "username": originator[1],
                "sessionId": originator[2],
                "sessionVersion": parseInt(originator[3]),
                "netType": "IN",
                "addressType": originator[4],
                "address": originator[5]
            };
        }
        var sessionName = match(sblock, regexps.sline, "m");
        if (sessionName)
            sdpObj.sessionName = sessionName[1];
        var sessionTime = match(sblock, regexps.tline, "m");
        if (sessionTime) {
            sdpObj.startTime = parseInt(sessionTime[1]);
            sdpObj.stopTime = parseInt(sessionTime[2]);
        }
        var hasMediaStreamId = !!match(sblock, regexps.msidsemantic, "m");
        sdpObj.mediaDescriptions = [];

        for (var i = 0; i < parts.length; i += 5) {
            var mediaDescription = {
                "type": parts[i],
                "port": parseInt(parts[i + 1]),
                "protocol": parts[i + 2],
            };
            var fmt = parts[i + 3].replace(/^[\s\uFEFF\xA0]+/, '')
                .split(/ +/)
                .map(function (x) {
                    return parseInt(x);
                });
            var mblock = parts[i + 4];

            var connection = match(mblock, regexps.cline, "m", sblock);
            if (connection) {
                mediaDescription.netType = "IN";
                mediaDescription.addressType = connection[1];
                mediaDescription.address = connection[2];
            }
            var mode = match(mblock, regexps.mode, "m", sblock);
            if (mode)
                mediaDescription.mode = mode[1];

            var mid = match(mblock, regexps.mid, "m", sblock);
            if (mid)
                mediaDescription.mid = mid[1];

            var payloadTypes = [];
            if (match(mediaDescription.protocol, "(UDP/TLS)?RTP/S?AVPF?")) {
                mediaDescription.payloads = [];
                payloadTypes = fmt;
            }
            payloadTypes.forEach(function (payloadType) {
                var payload = { "type": payloadType };
                var rtpmapLine = fillTemplate(regexps.rtpmap, payload);
                var rtpmap = match(mblock, rtpmapLine, "m");
                if (rtpmap) {
                    payload.encodingName = rtpmap[1];
                    payload.clockRate = parseInt(rtpmap[2]);
                    if (mediaDescription.type == "audio")
                        payload.channels = parseInt(rtpmap[3]) || 1;
                    else if (mediaDescription.type == "video") {
                        var nackLine = fillTemplate(regexps.nack, payload);
                        payload.nack = !!match(mblock, nackLine, "m");
                        var nackpliLine = fillTemplate(regexps.nackpli, payload);
                        payload.nackpli = !!match(mblock, nackpliLine, "m");
                        var ccmfirLine = fillTemplate(regexps.ccmfir, payload);
                        payload.ccmfir = !!match(mblock, ccmfirLine, "m");
                        var ericScreamLine = fillTemplate(regexps.ericscream, payload);
                        payload.ericscream = !!match(mblock, ericScreamLine, "m");
                    }
                } else if (payloadType == 0 || payloadType == 8) {
                    payload.encodingName = payloadType == 8 ? "PCMA" : "PCMU";
                    payload.clockRate = 8000;
                    payload.channels = 1;
                }
                var fmtpLine = fillTemplate(regexps.fmtp, payload);
                var fmtp = match(mblock, fmtpLine, "m");
                if (fmtp) {
                    payload.parameters = {};
                    fmtp[1].replace(new RegExp(regexps.param, "g"),
                        function(_, key, value) {
                            key = key.replace(/-([a-z])/g, function (_, c) {
                                return c.toUpperCase();
                            });
                            payload.parameters[key] = isNaN(+value) ? value : +value;
                    });
                }
                mediaDescription.payloads.push(payload);
            });

            var rtcp = match(mblock, regexps.rtcp, "m");
            if (rtcp) {
                mediaDescription.rtcp = {
                    "netType": "IN",
                    "port": parseInt(rtcp[1])
                };
                if (rtcp[2]) {
                    mediaDescription.rtcp.addressType = rtcp[3];
                    mediaDescription.rtcp.address = rtcp[4];
                }
            }
            var rtcpmux = match(mblock, regexps.rtcpmux, "m", sblock);
            if (rtcpmux) {
                if (!mediaDescription.rtcp)
                    mediaDescription.rtcp = {};
                mediaDescription.rtcp.mux = true;
            }

            var cnameLines = match(mblock, regexps.cname, "mg");
            if (cnameLines) {
                mediaDescription.ssrcs = [];
                cnameLines.forEach(function (line) {
                    var cname = match(line, regexps.cname, "m");
                    mediaDescription.ssrcs.push(parseInt(cname[1]));
                    if (!mediaDescription.cname)
                        mediaDescription.cname = cname[2];
                });
            }

            if (hasMediaStreamId) {
                var msid = match(mblock, regexps.msid, "m");
                if (msid) {
                    mediaDescription.mediaStreamId = msid[2];
                    mediaDescription.mediaStreamTrackId = msid[3];
                }
            }

            var ufrag = match(mblock, regexps.ufrag, "m", sblock);
            var pwd = match(mblock, regexps.pwd, "m", sblock);
            if (ufrag && pwd) {
                mediaDescription.ice = {
                    "ufrag": ufrag[1],
                    "password": pwd[1]
                };
            }
            var candidateLines = match(mblock, regexps.candidate, "mig");
            if (candidateLines) {
                if (!mediaDescription.ice)
                    mediaDescription.ice = {};
                mediaDescription.ice.candidates = [];
                candidateLines.forEach(function (line) {
                    var candidateLine = match(line, regexps.candidate, "mi");
                    var candidate = {
                        "foundation": candidateLine[1],
                        "componentId": parseInt(candidateLine[2]),
                        "transport": candidateLine[3].toUpperCase(),
                        "priority": parseInt(candidateLine[4]),
                        "address": candidateLine[5],
                        "port": parseInt(candidateLine[6]),
                        "type": candidateLine[7]
                    };
                    if (candidateLine[9])
                        candidate.relatedAddress = candidateLine[9];
                    if (!isNaN(candidateLine[10]))
                        candidate.relatedPort = parseInt(candidateLine[10]);
                    if (candidateLine[12])
                        candidate.tcpType = candidateLine[12];
                    else if (candidate.transport == "TCP") {
                        if (candidate.port == 0 || candidate.port == 9) {
                            candidate.tcpType = "active";
                            candidate.port = 9;
                        } else {
                            return;
                        }
                    }
                    mediaDescription.ice.candidates.push(candidate);
                });
            }

            var fingerprint = match(mblock, regexps.fingerprint, "mi", sblock);
            if (fingerprint) {
                mediaDescription.dtls = {
                    "fingerprintHashFunction": fingerprint[1].toLowerCase(),
                    "fingerprint": fingerprint[2].toUpperCase()
                };
            }
            var setup = match(mblock, regexps.setup, "m", sblock);
            if (setup) {
                if (!mediaDescription.dtls)
                    mediaDescription.dtls = {};
                mediaDescription.dtls.setup = setup[1];
            }

            if (mediaDescription.protocol == "DTLS/SCTP") {
                mediaDescription.sctp = {
                    "port": fmt[0]
                };
                var sctpmapLine = fillTemplate(regexps.sctpmap, mediaDescription.sctp);
                var sctpmap = match(mblock, sctpmapLine, "m");
                if (sctpmap) {
                    mediaDescription.sctp.app = sctpmap[1];
                    if (sctpmap[2])
                        mediaDescription.sctp.streams = parseInt(sctpmap[2]);
                }
            }

            sdpObj.mediaDescriptions.push(mediaDescription);
        }

        return sdpObj;
    };

    SDP.generate = function (sdpObj) {
        sdpObj = JSON.parse(JSON.stringify(sdpObj));
        addDefaults(sdpObj, {
            "version": 0,
            "originator": {},
            "sessionName": "-",
            "startTime": 0,
            "stopTime": 0,
            "bundlePolicy": "balanced",
            "mediaDescriptions": []
        });
        addDefaults(sdpObj.originator, {
            "username": "-",
            "sessionId": "" + Math.floor((Math.random() + +new Date()) * 1e6),
            "sessionVersion": 1,
            "netType": "IN",
            "addressType": "IP4",
            "address": "127.0.0.1"
        });
        var sdpText = fillTemplate(templates.sdp, sdpObj);
        sdpText = fillTemplate(sdpText, sdpObj.originator);

        var midsBundle = [];
        var mediatypesBundle = [];
        var msidsemanticLine = "";
        var mediaStreamIds = [];
        sdpObj.mediaDescriptions.forEach(function (mdesc) {
            if (mdesc.mediaStreamId && mdesc.mediaStreamTrackId
                && mediaStreamIds.indexOf(mdesc.mediaStreamId) == -1)
                mediaStreamIds.push(mdesc.mediaStreamId);
        });
        if (mediaStreamIds.length) {
            var msidsemanticLine = fillTemplate(templates.msidsemantic,
                { "mediaStreamIds": mediaStreamIds.join(" ") });
        }
        sdpText = fillTemplate(sdpText, { "msidsemanticLine": msidsemanticLine });

        sdpObj.mediaDescriptions.forEach(function (mediaDescription) {
            addDefaults(mediaDescription, {
                "port": 9,
                "protocol": "UDP/TLS/RTP/SAVPF",
                "netType": "IN",
                "addressType": "IP4",
                "address": "0.0.0.0",
                "mode": "sendrecv",
                "payloads": [],
                "rtcp": {}
            });
            var mblock = fillTemplate(templates.mblock, mediaDescription);

            var midBundleInfo = {"midLine": "", "bundleOnlyLine": ""};
            if (mediaDescription.mid) {
                midBundleInfo.midLine = fillTemplate(templates.mid, mediaDescription);
                if ((sdpObj.bundlePolicy == "balanced"   && mediatypesBundle.includes(mediaDescription.type)) ||
                    (sdpObj.bundlePolicy == "max-bundle" && mediatypesBundle.length > 0))
                    midBundleInfo.bundleOnlyLine = "a=bundle-only\r\n";
                mediatypesBundle.push(mediaDescription.type)
                midsBundle.push(mediaDescription.mid);
            }
            mblock = fillTemplate(mblock, midBundleInfo);

            var payloadInfo = {"rtpMapLines": "", "fmtpLines": "", "nackLines": "",
                "nackpliLines": "", "ccmfirLines": "", "ericScreamLines": ""};
            mediaDescription.payloads.forEach(function (payload) {
                if (payloadInfo.fmt)
                    payloadInfo.fmt += " " + payload.type;
                else
                    payloadInfo.fmt = payload.type;
                if (!payload.channels || payload.channels == 1)
                    payload.channels = null;
                payloadInfo.rtpMapLines += fillTemplate(templates.rtpMap, payload);
                if (payload.parameters) {
                    var fmtpInfo = { "type": payload.type, "parameters": "" };
                    for (var p in payload.parameters) {
                        var param = p.replace(/([A-Z])([a-z])/g, function (_, a, b) {
                            return "-" + a.toLowerCase() + b;
                        });
                        if (fmtpInfo.parameters)
                            fmtpInfo.parameters += ";";
                        fmtpInfo.parameters += param + "=" + payload.parameters[p];
                    }
                    payloadInfo.fmtpLines += fillTemplate(templates.fmtp, fmtpInfo);
                }
                if (payload.nack)
                    payloadInfo.nackLines += fillTemplate(templates.nack, payload);
                if (payload.nackpli)
                    payloadInfo.nackpliLines += fillTemplate(templates.nackpli, payload);
                if (payload.ccmfir)
                    payloadInfo.ccmfirLines += fillTemplate(templates.ccmfir, payload);
                if (payload.ericscream)
                    payloadInfo.ericScreamLines += fillTemplate(templates.ericscream, payload);
            });
            mblock = fillTemplate(mblock, payloadInfo);

            var rtcpInfo = {"rtcpLine": "", "rtcpMuxLine": ""};
            if (mediaDescription.rtcp.port) {
                addDefaults(mediaDescription.rtcp, {
                    "netType": "IN",
                    "addressType": "IP4",
                    "address": ""
                });
                if (!mediaDescription.rtcp.address)
                    mediaDescription.rtcp.netType = mediaDescription.rtcp.addressType = "";
                rtcpInfo.rtcpLine = fillTemplate(templates.rtcp, mediaDescription.rtcp);
            }
            if (mediaDescription.rtcp.mux)
                rtcpInfo.rtcpMuxLine = templates.rtcpMux;
            mblock = fillTemplate(mblock, rtcpInfo);

            var srcAttributeLines = { "cnameLines": "", "msidLines": "" };
            var srcAttributes = {
                "cname": mediaDescription.cname,
                "mediaStreamId": mediaDescription.mediaStreamId,
                "mediaStreamTrackId": mediaDescription.mediaStreamTrackId
            };
            if (mediaDescription.cname && mediaDescription.ssrcs) {
                mediaDescription.ssrcs.forEach(function (ssrc) {
                    srcAttributes.ssrc = ssrc;
                    srcAttributeLines.cnameLines += fillTemplate(templates.cname, srcAttributes);
                    if (mediaDescription.mediaStreamId && mediaDescription.mediaStreamTrackId)
                        srcAttributeLines.msidLines += fillTemplate(templates.msid, srcAttributes);
                });
            } else if (mediaDescription.mediaStreamId && mediaDescription.mediaStreamTrackId) {
                srcAttributes.ssrc = null;
                srcAttributeLines.msidLines += fillTemplate(templates.msid, srcAttributes);
            }
            mblock = fillTemplate(mblock, srcAttributeLines);

            var iceInfo = {"iceCredentialLines": "", "candidateLines": ""};
            if (mediaDescription.ice) {
                iceInfo.iceCredentialLines = fillTemplate(templates.iceCredentials,
                    mediaDescription.ice);
                if (mediaDescription.ice.candidates) {
                    mediaDescription.ice.candidates.forEach(function (candidate) {
                        addDefaults(candidate, {
                            "relatedAddress": null,
                            "relatedPort": null,
                            "tcpType": null
                        });
                        iceInfo.candidateLines += fillTemplate(templates.candidate, candidate);
                    });
                }
            }
            mblock = fillTemplate(mblock, iceInfo);

            var dtlsInfo = { "dtlsFingerprintLine": "", "dtlsSetupLine": "" };
            if (mediaDescription.dtls) {
                if (mediaDescription.dtls.fingerprint) {
                    dtlsInfo.dtlsFingerprintLine = fillTemplate(templates.dtlsFingerprint,
                        mediaDescription.dtls);
                }
                addDefaults(mediaDescription.dtls, {"setup": "actpass"});
                dtlsInfo.dtlsSetupLine = fillTemplate(templates.dtlsSetup, mediaDescription.dtls);
            }
            mblock = fillTemplate(mblock, dtlsInfo);

            var sctpInfo = {"sctpmapLine": "", "fmt": ""};
            if (mediaDescription.sctp) {
                addDefaults(mediaDescription.sctp, {"streams": null});
                sctpInfo.sctpmapLine = fillTemplate(templates.sctpmap, mediaDescription.sctp);
                sctpInfo.fmt = mediaDescription.sctp.port;
            }
            mblock = fillTemplate(mblock, sctpInfo);

            sdpText += mblock;
        });

        var bundleLine = "";
        if (midsBundle.length > 0)
            bundleLine = fillTemplate(templates.bundle, { "midsBundle": midsBundle.join(" ") });
        sdpText = fillTemplate(sdpText, { "bundleLine": bundleLine });

        return sdpText;
    };

    SDP.generateCandidateLine = function (candidateObj) {
        addDefaults(candidateObj, {
            "relatedAddress": null,
            "relatedPort": null,
            "tcpType": null
        });

        return fillTemplate(templates.candidate, candidateObj);
    };

    var expectedProperties = {
        "session": [ "version", "originator", "sessionName", "startTime", "stopTime" ],
        "mline": [ "type", "port", "protocol", "mode", "payloads", "rtcp", "dtls", "ice" ],
        "mlineSubObjects": {
            "rtcp": [ "mux" ],
            "ice": [ "ufrag", "password" ],
            "dtls": [ "setup", "fingerprintHashFunction", "fingerprint" ]
        }
    };

    function hasAllProperties(object, properties) {
        var missing = properties.filter(function (property) {
            return !object.hasOwnProperty(property);
        });

        return !missing.length;
    }

    SDP.verifyObject = function (sdpObj) {
        if (!hasAllProperties(sdpObj, expectedProperties.session))
            return false;

        for (var i = 0; i < sdpObj.mediaDescriptions.length; i++) {
            var mediaDescription = sdpObj.mediaDescriptions[i];

            if (!hasAllProperties(mediaDescription, expectedProperties.mline))
                return false;

            for (var p in expectedProperties.mlineSubObjects) {
                if (!hasAllProperties(mediaDescription[p], expectedProperties.mlineSubObjects[p]))
                    return false;
            }
        }

        return true;
    };

})();

function generate(json) {
    var object = JSON.parse(json);
    return SDP.generate(object);
}

function parse(sdp) {
    var object = SDP.parse(sdp);

    if (!SDP.verifyObject(object))
        return "ParseError";

    return JSON.stringify(object);
}

function generateCandidateLine(json) {
    var candidate = JSON.parse(json);
    return SDP.generateCandidateLine(candidate).substr(2);
}

function parseCandidateLine(candidateLine) {
    var mdesc = SDP.parse("m=application 0 NONE\r\na=" + candidateLine + "\r\n").mediaDescriptions[0];
    if (!mdesc.ice)
        return "ParseError";

    return JSON.stringify(mdesc.ice.candidates[0]);
}

if (typeof(module) != "undefined" && typeof(exports) != "undefined")
    module.exports = SDP;
