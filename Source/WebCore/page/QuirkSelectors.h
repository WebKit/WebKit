/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/text/ASCIILiteral.h>

namespace WebCore::QuirkSelectors {

inline constexpr auto onSliderRole = "[role=slider], [role=slider] *"_s;
inline constexpr auto onDockPanelTabBar = ".lm-DockPanel-tabBar, .lm-DockPanel-tabBar *"_s;
// The assume-default-prevented path only ever inspected the event target.
inline constexpr auto onSliderRoleItself = "[role=slider]"_s;
inline constexpr auto onAmazonMagnifierLens = "#magnifierLens, :has(+ #magnifierLens)"_s;
inline constexpr auto onSoundCloudSceneLayer = ".sceneLayer"_s;
inline constexpr auto onCrosswordID = "[id*=crossword]"_s;
inline constexpr auto onEANetworkNav = "ea-network-nav"_s;
inline constexpr auto onExpediaOpeningMenu = ".uitk-menu-mounted .uitk-menu-container.uitk-menu-container-autoposition.uitk-menu-container-has-intersection-root-el.uitk-menu-open"_s;
inline constexpr auto onAviaButton = "avia-button"_s;
inline constexpr auto onGoogleDocsMLPromotion = ".docs-ml-promotion-action-container, .docs-ml-promotion-action-container > *, .docs-ml-promotion-action-container > * > *"_s;
inline constexpr auto onSuggestionsLabel = "[aria-label=Suggestions], [aria-label=Suggestions] *"_s;
inline constexpr auto onSwatchColorPicker = "[id^=swatchColorPicker]"_s;
inline constexpr auto onButtonInListItem = "[role=listitem i] > [role=button i]"_s;
inline constexpr auto onTreeItem = "[role=treeitem i], [role=treeitem i] *"_s;
inline constexpr auto onVideoJSTech = "video.vjs-tech, audio.vjs-tech"_s;
inline constexpr auto onKinjaLoginAvatar = ".js_switch-to-burner-login, .js_header-userbutton, .sc-1il3uru-3, .cIhKfd, .iyvn34-0, .bYIjtl, svg[aria-label=\"UserFilled icon\"], svg[aria-label=\"UserFilled icon\"] > path"_s;
inline constexpr auto onMicrosoftSignInButton = ".glyph_signIn_circle, .mectrl_headertext, .mectrl_header"_s;
inline constexpr auto onPlayStationSignInButton = ".web-toolbar__signin-button, .web-toolbar__signin-button-label, .sb-signin-button"_s;
inline constexpr auto onYouTubeWatchLaterIcon = ".ytp-watch-later-icon"_s;
inline constexpr auto onExpandablePanel = "[data-expc], [data-expc] *"_s;
inline constexpr auto onClaudeSidebar = "[aria-label=\"Sidebar\"]"_s;
inline constexpr auto onTikTokCommentsContainer = "[class*=DivBrowserModeContainer] > [class*=DivContentContainer]"_s;
inline constexpr auto onTikTokVideoContainer = "[class*=DivBrowserModeContainer] > [class*=DivVideoContainer]"_s;
inline constexpr auto onGoogleSitesButton = ".DPvwYc.sm8sCf"_s;
inline constexpr auto onYahooButton = ".DPvwYc.sm8sCf, .vjs-subs-cap-button.vjs-menu-button"_s;
inline constexpr auto onOutlookSuggestions = ".ms-Suggestions, .ms-Suggestions *"_s;
inline constexpr auto onElementContainingVideo = ":has(video)"_s;

} // namespace WebCore::QuirkSelectors
