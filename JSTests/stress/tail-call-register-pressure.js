"use strict";

function getModuleExports() {
    return () => {}
};

function ownKeys(ee, te) {
    var re = Object.keys(ee);
    if (Object.getOwnPropertySymbols) {
        var ne = Object.getOwnPropertySymbols(ee);
        te && (ne = ne.filter((function(te) {
            return Object.getOwnPropertyDescriptor(ee, te).enumerable
        }))),
        re.push.apply(re, ne)
    }
    return re
}

function _objectSpread(ee) {
    for (var te = 1; te < arguments.length; te++) {
        var re = null != arguments[te] ? arguments[te] : {};
        te % 2 ? ownKeys(Object(re), !0).forEach((function(te) {
            getModuleExports()(ee, te, re[te])
        })) : Object.getOwnPropertyDescriptors ? Object.defineProperties(ee, Object.getOwnPropertyDescriptors(re)) : ownKeys(Object(re)).forEach((function(te) {
            Object.defineProperty(ee, te, Object.getOwnPropertyDescriptor(re, te))
        }))
    }
    return ee
}

var ae = {
    cleanAndTrimSelfLink: function() {
    }
};

var browseItems = function(ee, te, re) {
    function parseBrowseItem(ee, te, re, ne, ie) {
        var ce, le, de, he;
        if (ee.hasEmbedded("linearInfo")) {
            var fe = ee.getEmbedded("linearInfo"),
                pe = Object(ae.cleanSelfLink)(fe.getFirstAction("channel").getRawActionUrl());
            if (!ne[pe] && !ie) return;
        }

        var ge = function parseBrowseItemUrl(ee, te) {
            var re = ee.getProp("entityId"),
                ne = ee.getProp("entityType"),
                ie = ee.getProp("seriesProgramId"),
                oe = ee.getProp("_type");
            return "entity/".concat(re)
        }(ee, he),
            me = ee.getProps();
        if (("NETWORK" !== te || ee.hasEmbedded("contentProvider")) && ("3X4_PROGRAM_LINEAR" !== te || ee.hasEmbedded("linearInfo"))) {
            var ve = ee.hasAction("programImageLink") ? ee.getFirstAction("programImageLink") : re.programImageLink,
                Se = ee.hasAction("programFallbackImageLink"),
                _t = Se ? ee.getFirstAction("programFallbackImageLink") : re.programFallbackImageLink;

            var Et, kt, Tt, Pt = _t.setParams(de).getActionUrl();
            return Et = Se ? Pt : ve.setParams(de).getActionUrl(), me.contentRating && me.contentRatingScheme && (kt = {
                scheme: me.contentRatingScheme,
                name: me.contentRating
            }), (null === (ce = me.ordering) || void 0 === ce ? void 0 : ce.criticScore) && (null === (le = me.ordering) || void 0 === le ? void 0 : le.fanScore) && (Tt = {
                rt: {
                    criticScore: Math.floor(me.ordering.criticScore),
                    criticCertified: Math.floor(me.ordering.criticScore) >= 75,
                    criticRotten: Math.floor(me.ordering.criticScore) < 60,
                    fanScore: Math.floor(me.ordering.fanScore),
                    fanRotten: Math.floor(me.ordering.fanScore) < 60
                }
            }), _objectSpread({
                _type: me._type
            }, me.adBrand && {
                adBrand: me.adBrand
            }, {}, me.entityId && {
                entityId: me.entityId
            }, {}, me.entityType && {
                entityType: me.entityType
            }, {}, me.episodeNumber && {
                episodeNumber: me.episodeNumber
            }, {}, he && {
                linearInfo: he
            }, {}, me.numberOfEpisodes && {
                numberOfEpisodes: me.numberOfEpisodes
            }, {}, me.programType && {
                programType: me.programType
            }, {}, kt && {
                rating: kt
            }, {}, Tt && {
                reviews: Tt
            }, {}, me.hasDVS && {
                dvs: me.hasDVS
            }, {}, me.isHD && {
                hd: me.isHD
            }, {}, me.closedCaption && {
                cc: me.closedCaption
            }, {}, me.isSAP && {
                sap: me.isSAP
            }, {}, me.releaseYear && {
                releaseYear: me.releaseYear
            }, {}, me.seasonNumber && {
                seasonNumber: me.seasonNumber
            }, {}, me.seriesTitle && {
                seriesTitle: me.seriesTitle
            }, {}, {
                tileRenderStyle: me.tileRenderStyle || te,
                title: me.title,
                subtitle: me.subtitle,
                image: Et,
                fallbackImage: Pt,
                url: ge
            })
        }
    }

    return parseBrowseItem(ee, te, re,{},{})
}

const runTest = function() {
    for (var i = 1; i < 100; ++i) {
        browseItems(
            {
                hasAction: () => {},
                hasEmbedded: (i) => { return i === "contentProvider"; },
                getProp: () => { return undefined },
                getProps: () => { return {} },
                getFirstAction: () => { return { getRawActionUrl: () => "url" } },
            },
            {},
            {
                programImageLink: { setParams: () => { return { getActionUrl: () => "programImageLink" }; } },
                programFallbackImageLink: { setParams: () => { return { getActionUrl: () => "programFallbackImageLinkrl" }; } },
            }
        )
    }
}

runTest()
