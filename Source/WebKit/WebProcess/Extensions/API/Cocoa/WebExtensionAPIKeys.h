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

#import <Foundation/Foundation.h>

static NSString * const accessLevelKey = @"accessLevel";
static NSString * const accessLevelTrustedAndUntrustedContexts = @"TRUSTED_AND_UNTRUSTED_CONTEXTS";
static NSString * const accessLevelTrustedContexts = @"TRUSTED_CONTEXTS";
static NSString * const actionClickBehaviorKey = @"openPanelOnActionClick";
static NSString * const actionCountDisplayActionCountAsBadgeTextKey = @"displayActionCountAsBadgeText";
static NSString * const actionCountIncrementKey = @"increment";
static NSString * const actionCountTabIDKey = @"tabId";
static NSString * const actionCountTabUpdateKey = @"tabUpdate";
static NSString * const activeKey = @"active";
static NSString * const addRulesKey = @"addRules";
static NSString * const allFramesKey = @"allFrames";
static NSString * const allKey = @"all";
static NSString * const alwaysOnTopKey = @"alwaysOnTop";
static NSString * const anyKey = @"any";
static NSString * const argsKey = @"args";
static NSString * const argumentsKey = @"arguments";
static NSString * const audibleKey = @"audible";
static NSString * const audioKey = @"audio";
static NSString * const authorValue = @"author";
static NSString * const basicSchemeKey = @"basic";
static NSString * const bookmarkKey = @"bookmark";
static NSString * const bypassCacheKey = @"bypassCache";
static NSString * const bytesKey = @"bytes";
static NSString * const challengerKey = @"challenger";
static NSString * const checkboxKey = @"checkbox";
static NSString * const checkedKey = @"checked";
static NSString * const childrenKey = @"children";
static NSString * const codeKey = @"code";
static NSString * const colorSchemesKey = @"colorSchemes";
static NSString * const commandKey = @"command";
static NSString * const completeKey = @"complete";
static NSString * const contextsKey = @"contexts";
static NSString * const cssKey = @"css";
static NSString * const cssOriginKey = @"cssOrigin";
static NSString * const currentWindowKey = @"currentWindow";
static NSString * const darkKey = @"dark";
static NSString * const dateAddedKey = @"dateAdded";
static NSString * const delayInMinutesKey = @"delayInMinutes";
static NSString * const deprecatedColorSchemesKey = @"color_schemes";
static NSString * const deprecatedIconVariantsKey = @"icon_variants";
static NSString * const descriptionKey = @"description";
static NSString * const digestSchemeKey = @"digest";
static NSString * const disableRulesetsKey = @"disableRulesetIds";
static NSString * const documentEnd = @"document_end";
static NSString * const documentIdKey = @"documentId";
static NSString * const documentIdle = @"document_idle";
static NSString * const documentIDsKey = @"documentIds";
static NSString * const documentLifecycleKey = @"documentLifecycle";
static NSString * const documentStart = @"document_start";
static NSString * const documentURLPatternsKey = @"documentUrlPatterns";
static NSString * const domainKey = @"domain";
static NSString * const editableKey = @"editable";
static NSString * const emptyAlarmName = @"";
static NSString * const emptyDataURLValue = @"data:,";
static NSString * const emptyTitleValue = @"";
static NSString * const emptyURLValue = @"";
static NSString * const enabledKey = @"enabled";
static NSString * const enableRulesetsKey = @"enableRulesetIds";
static NSString * const ephemeralPrefix = @"ephemeral-";
static NSString * const errorKey = @"error";
static NSString * const excludeMatchesKey = @"excludeMatches";
static NSString * const expirationDateKey = @"expirationDate";
static NSString * const extensionIdKey = @"extensionId";
static NSString * const fileKey = @"file";
static NSString * const filesKey = @"files";
static NSString * const focusedKey = @"focused";
static NSString * const folderKey = @"folder";
static NSString * const formatKey = @"format";
static NSString * const formDataKey = @"formData";
static NSString * const frameIdKey = @"frameId";
static NSString * const frameIDKey = @"frameId";
static NSString * const frameIDsKey = @"frameIds";
static NSString * const frameTypeKey = @"frameType";
static NSString * const frameURLKey = @"frameURL";
static NSString * const fromCacheKey = @"fromCache";
static NSString * const fromIndexKey = @"fromIndex";
static NSString * const fullscreenKey = @"fullscreen";
static NSString * const funcKey = @"func";
static NSString * const functionKey = @"function";
static NSString * const getDynamicOrSessionRulesRuleIDsKey = @"ruleIds";
static NSString * const getMatchedRulesMinTimeStampKey = @"minTimeStamp";
static NSString * const getMatchedRulesTabIDKey = @"tabId";
static NSString * const heightKey = @"height";
static NSString * const hiddenKey = @"hidden";
static NSString * const highlightedKey = @"highlighted";
static NSString * const hostKey = @"host";
static NSString * const hostOnlyKey = @"hostOnly";
static NSString * const httpOnlyKey = @"httpOnly";
static NSString * const iconsKey = @"icons";
static NSString * const iconVariantsKey = @"iconVariants";
static NSString * const idKey = @"id";
static NSString * const idsKey = @"ids";
static NSString * const ignoreCacheKey = @"ignoreCache";
static NSString * const imageDataKey = @"imageData";
static NSString * const imageKey = @"image";
static NSString * const incognitoKey = @"incognito";
static NSString * const indexKey = @"index";
static NSString * const initiatorKey = @"initiator";
static NSString * const isArticleKey = @"isArticle";
static NSString * const isExceptionKey = @"isException";
static NSString * const isInReaderModeKey = @"isInReaderMode";
static NSString * const isolatedWorld = @"isolated";
static NSString * const isProxyKey = @"isProxy";
static NSString * const isWindowClosingKey = @"isWindowClosing";
static NSString * const jpegValue = @"jpeg";
static NSString * const jsKey = @"js";
static NSString * const lastFocusedWindowKey = @"lastFocusedWindow";
static NSString * const laxKey = @"lax";
static NSString * const leftKey = @"left";
static NSString * const lightKey = @"light";
static NSString * const linkTextKey = @"linkText";
static NSString * const linkURLKey = @"linkUrl";
static NSString * const loadingKey = @"loading";
static NSString * const mainWorld = @"main";
static NSString * const matchesKey = @"matches";
static NSString * const matchOriginAsFallbackKey = @"matchOriginAsFallback";
static NSString * const maximizedKey = @"maximized";
static NSString * const mediaTypeKey = @"mediaType";
static NSString * const menuItemIDKey = @"menuItemId";
static NSString * const methodKey = @"method";
static NSString * const minimizedKey = @"minimized";
static NSString * const mutedInfoKey = @"mutedInfo";
static NSString * const mutedKey = @"muted";
static NSString * const nameKey = @"name";
static NSString * const newPositionKey = @"newPosition";
static NSString * const newShortcutKey = @"newShortcut";
static NSString * const newWindowIdKey = @"newWindowId";
static NSString * const noRestrictionKey = @"no_restriction";
static NSString * const normalKey = @"normal";
static NSString * const oldPositionKey = @"oldPosition";
static NSString * const oldShortcutKey = @"oldShortcut";
static NSString * const oldWindowIdKey = @"oldWindowId";
static NSString * const onclickKey = @"onclick";
static NSString * const openerTabIdKey = @"openerTabId";
static NSString * const openInReaderModeKey = @"openInReaderMode";
static NSString * const originKey = @"origin";
static NSString * const originsKey = @"origins";
static NSString * const pageURLKey = @"pageUrl";
static NSString * const panelKey = @"panel";
static NSString * const parentDocumentIdKey = @"parentDocumentId";
static NSString * const parentFrameIdKey = @"parentFrameId";
static NSString * const parentIdKey = @"parentId";
static NSString * const parentMenuItemIDKey = @"parentMenuItemId";
static NSString * const pathKey = @"path";
static NSString * const periodInMinutesKey = @"periodInMinutes";
static NSString * const permissionsKey = @"permissions";
static NSString * const persistAcrossSessionsKey = @"persistAcrossSessions";
static NSString * const persistentPrefix = @"persistent-";
static NSString * const pinnedKey = @"pinned";
static NSString * const pngValue = @"png";
static NSString * const populateKey = @"populate";
static NSString * const popupKey = @"popup";
static NSString * const portKey = @"port";
static NSString * const previousTabIdKey = @"previousTabId";
static NSString * const previousVersionKey = @"previousVersion";
static NSString * const qualityKey = @"quality";
static NSString * const queryKey = @"query";
static NSString * const radioKey = @"radio";
static NSString * const rawKey = @"raw";
static NSString * const realmKey = @"realm";
static NSString * const reasonKey = @"reason";
static NSString * const redirectURLKey = @"redirectUrl";
static NSString * const regexIsCaseSensitiveKey = @"isCaseSensitive";
static NSString * const regexKey = @"regex";
static NSString * const regexRequireCapturingKey = @"requireCapturing";
static NSString * const removeRulesKey = @"removeRuleIds";
static NSString * const requestBodyKey = @"requestBody";
static NSString * const requestHeadersKey = @"requestHeaders";
static NSString * const requestIdKey = @"requestId";
static NSString * const requestKey = @"request";
static NSString * const responseHeadersKey = @"responseHeaders";
static NSString * const ruleIdKey = @"ruleId";
static NSString * const ruleKey = @"rule";
static NSString * const rulesetIdKey = @"rulesetId";
static NSString * const runAtKey = @"runAt";
static NSString * const sameSiteKey = @"sameSite";
static NSString * const scheduledTimeKey = @"scheduledTime";
static NSString * const schemeKey = @"scheme";
static NSString * const secureKey = @"secure";
static NSString * const selectedKey = @"selected";
static NSString * const selectionTextKey = @"selectionText";
static NSString * const separatorKey = @"separator";
static NSString * const sessionKey = @"session";
static NSString * const shortcutKey = @"shortcut";
static NSString * const srcURLKey = @"srcUrl";
static NSString * const stateKey = @"state";
static NSString * const statusCodeKey = @"statusCode";
static NSString * const statusKey = @"status";
static NSString * const statusLineKey = @"statusLine";
static NSString * const storeIdKey = @"storeId";
static NSString * const strictKey = @"strict";
static NSString * const tabIdKey = @"tabId";
static NSString * const tabIDKey = @"tabId";
static NSString * const tabIdsKey = @"tabIds";
static NSString * const tabKey = @"tab";
static NSString * const tabsKey = @"tabs";
static NSString * const targetKey = @"target";
static NSString * const targetURLPatternsKey = @"targetUrlPatterns";
static NSString * const textKey = @"text";
static NSString * const timeStampKey = @"timeStamp";
static NSString * const titleKey = @"title";
static NSString * const toIndexKey = @"toIndex";
static NSString * const topKey = @"top";
static NSString * const typeKey = @"type";
static NSString * const unknownLanguageValue = @"und";
static NSString * const urlKey = @"url";
static NSString * const userValue = @"user";
static NSString * const valueKey = @"value";
static NSString * const variantsKey = @"variants";
static NSString * const versionKey = @"version";
static NSString * const videoKey = @"video";
static NSString * const visibleKey = @"visible";
static NSString * const wasCheckedKey = @"wasChecked";
static NSString * const whenKey = @"when";
static NSString * const widthKey = @"width";
static NSString * const windowIdKey = @"windowId";
static NSString * const windowTypeKey = @"windowType";
static NSString * const windowTypesKey = @"windowTypes";
static NSString * const worldKey = @"world";
