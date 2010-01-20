/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

// Use this file to assert that various WebKit API enum values continue
// matching WebCore defined enum values.

#include "config.h"

#include "AccessibilityObject.h"
#include "ApplicationCacheHost.h"
#include "EditorInsertAction.h"
#include "HTMLInputElement.h"
#include "MediaPlayer.h"
#include "NotificationPresenter.h"
#include "PasteboardPrivate.h"
#include "PlatformCursor.h"
#include "StringImpl.h"
#include "TextAffinity.h"
#include "WebAccessibilityObject.h"
#include "WebApplicationCacheHost.h"
#include "WebClipboard.h"
#include "WebCursorInfo.h"
#include "WebEditingAction.h"
#include "WebInputElement.h"
#include "WebMediaPlayer.h"
#include "WebNotificationPresenter.h"
#include "WebTextAffinity.h"
#include "WebTextCaseSensitivity.h"
#include <wtf/Assertions.h>

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(int(WebKit::webkit_name) == int(WebCore::webcore_name), mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleUnknown, UnknownRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleButton, ButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleRadioButton, RadioButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleCheckBox, CheckBoxRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleSlider, SliderRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTabGroup, TabGroupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTextField, TextFieldRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleStaticText, StaticTextRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTextArea, TextAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleScrollArea, ScrollAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRolePopUpButton, PopUpButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleMenuButton, MenuButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTable, TableRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleApplication, ApplicationRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleGroup, GroupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleRadioGroup, RadioGroupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleList, ListRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleScrollBar, ScrollBarRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleValueIndicator, ValueIndicatorRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleImage, ImageRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleMenuBar, MenuBarRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleMenu, MenuRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleMenuItem, MenuItemRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleColumn, ColumnRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleRow, RowRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleToolbar, ToolbarRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleBusyIndicator, BusyIndicatorRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleProgressIndicator, ProgressIndicatorRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleWindow, WindowRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDrawer, DrawerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleSystemWide, SystemWideRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleOutline, OutlineRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleIncrementor, IncrementorRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleBrowser, BrowserRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleComboBox, ComboBoxRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleSplitGroup, SplitGroupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleSplitter, SplitterRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleColorWell, ColorWellRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleGrowArea, GrowAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleSheet, SheetRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleHelpTag, HelpTagRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleMatte, MatteRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleRuler, RulerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleRulerMarker, RulerMarkerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleLink, LinkRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDisclosureTriangle, DisclosureTriangleRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleGrid, GridRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleCell, CellRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleColumnHeader, ColumnHeaderRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleRowHeader, RowHeaderRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleWebCoreLink, WebCoreLinkRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleImageMapLink, ImageMapLinkRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleImageMap, ImageMapRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleListMarker, ListMarkerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleWebArea, WebAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleHeading, HeadingRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleListBox, ListBoxRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleListBoxOption, ListBoxOptionRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleMenuListOption, MenuListOptionRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleMenuListPopup, MenuListPopupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTableHeaderContainer, TableHeaderContainerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDefinitionListTerm, DefinitionListTermRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDefinitionListDefinition, DefinitionListDefinitionRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleAnnotation, AnnotationRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleSliderThumb, SliderThumbRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleIgnored, IgnoredRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTab, TabRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTabList, TabListRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTabPanel, TabPanelRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTreeRole, TreeRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTreeGrid, TreeGridRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleTreeItemRole, TreeItemRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDirectory, DirectoryRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleEditableText, EditableTextRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleListItem, ListItemRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleLandmarkApplication, LandmarkApplicationRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleLandmarkBanner, LandmarkBannerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleLandmarkComplementary, LandmarkComplementaryRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleLandmarkContentInfo, LandmarkContentInfoRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleLandmarkMain, LandmarkMainRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleLandmarkNavigation, LandmarkNavigationRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleLandmarkSearch, LandmarkSearchRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleApplicationAlert, ApplicationAlertRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleApplicationAlertDialog, ApplicationAlertDialogRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleApplicationDialog, ApplicationDialogRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleApplicationLog, ApplicationLogRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleApplicationMarquee, ApplicationMarqueeRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleApplicationStatus, ApplicationStatusRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleApplicationTimer, ApplicationTimerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDocument, DocumentRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDocumentArticle, DocumentArticleRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDocumentMath, DocumentMathRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDocumentNote, DocumentNoteRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleDocumentRegion, DocumentRegionRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAccessibilityRoleUserInterfaceTooltip, UserInterfaceTooltipRole);

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Uncached, ApplicationCacheHost::UNCACHED);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Idle, ApplicationCacheHost::IDLE);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Checking, ApplicationCacheHost::CHECKING);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Downloading, ApplicationCacheHost::DOWNLOADING);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::UpdateReady, ApplicationCacheHost::UPDATEREADY);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Obsolete, ApplicationCacheHost::OBSOLETE);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::CheckingEvent, ApplicationCacheHost::CHECKING_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::ErrorEvent, ApplicationCacheHost::ERROR_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::NoUpdateEvent, ApplicationCacheHost::NOUPDATE_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::DownloadingEvent, ApplicationCacheHost::DOWNLOADING_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::ProgressEvent, ApplicationCacheHost::PROGRESS_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::UpdateReadyEvent, ApplicationCacheHost::UPDATEREADY_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::CachedEvent, ApplicationCacheHost::CACHED_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::ObsoleteEvent, ApplicationCacheHost::OBSOLETE_EVENT);
#endif

COMPILE_ASSERT_MATCHING_ENUM(WebClipboard::FormatHTML, PasteboardPrivate::HTMLFormat);
COMPILE_ASSERT_MATCHING_ENUM(WebClipboard::FormatBookmark, PasteboardPrivate::BookmarkFormat);
COMPILE_ASSERT_MATCHING_ENUM(WebClipboard::FormatSmartPaste, PasteboardPrivate::WebSmartPasteFormat);

COMPILE_ASSERT_MATCHING_ENUM(WebClipboard::BufferStandard, PasteboardPrivate::StandardBuffer);
COMPILE_ASSERT_MATCHING_ENUM(WebClipboard::BufferSelection, PasteboardPrivate::SelectionBuffer);

COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypePointer, PlatformCursor::TypePointer);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeCross, PlatformCursor::TypeCross);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeHand, PlatformCursor::TypeHand);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeIBeam, PlatformCursor::TypeIBeam);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeWait, PlatformCursor::TypeWait);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeHelp, PlatformCursor::TypeHelp);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeEastResize, PlatformCursor::TypeEastResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthResize, PlatformCursor::TypeNorthResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthEastResize, PlatformCursor::TypeNorthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthWestResize, PlatformCursor::TypeNorthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthResize, PlatformCursor::TypeSouthResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthEastResize, PlatformCursor::TypeSouthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthWestResize, PlatformCursor::TypeSouthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeWestResize, PlatformCursor::TypeWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthSouthResize, PlatformCursor::TypeNorthSouthResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeEastWestResize, PlatformCursor::TypeEastWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthEastSouthWestResize, PlatformCursor::TypeNorthEastSouthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthWestSouthEastResize, PlatformCursor::TypeNorthWestSouthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeColumnResize, PlatformCursor::TypeColumnResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeRowResize, PlatformCursor::TypeRowResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeMiddlePanning, PlatformCursor::TypeMiddlePanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeEastPanning, PlatformCursor::TypeEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthPanning, PlatformCursor::TypeNorthPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthEastPanning, PlatformCursor::TypeNorthEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthWestPanning, PlatformCursor::TypeNorthWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthPanning, PlatformCursor::TypeSouthPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthEastPanning, PlatformCursor::TypeSouthEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthWestPanning, PlatformCursor::TypeSouthWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeWestPanning, PlatformCursor::TypeWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeMove, PlatformCursor::TypeMove);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeVerticalText, PlatformCursor::TypeVerticalText);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeCell, PlatformCursor::TypeCell);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeContextMenu, PlatformCursor::TypeContextMenu);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeAlias, PlatformCursor::TypeAlias);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeProgress, PlatformCursor::TypeProgress);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNoDrop, PlatformCursor::TypeNoDrop);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeCopy, PlatformCursor::TypeCopy);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNone, PlatformCursor::TypeNone);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNotAllowed, PlatformCursor::TypeNotAllowed);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeZoomIn, PlatformCursor::TypeZoomIn);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeZoomOut, PlatformCursor::TypeZoomOut);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeCustom, PlatformCursor::TypeCustom);

COMPILE_ASSERT_MATCHING_ENUM(WebEditingActionTyped, EditorInsertActionTyped);
COMPILE_ASSERT_MATCHING_ENUM(WebEditingActionPasted, EditorInsertActionPasted);
COMPILE_ASSERT_MATCHING_ENUM(WebEditingActionDropped, EditorInsertActionDropped);

COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Text, HTMLInputElement::TEXT);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Password, HTMLInputElement::PASSWORD);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::IsIndex, HTMLInputElement::ISINDEX);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::CheckBox, HTMLInputElement::CHECKBOX);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Radio, HTMLInputElement::RADIO);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Submit, HTMLInputElement::SUBMIT);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Reset, HTMLInputElement::RESET);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::File, HTMLInputElement::FILE);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Hidden, HTMLInputElement::HIDDEN);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Image, HTMLInputElement::IMAGE);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Button, HTMLInputElement::BUTTON);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Search, HTMLInputElement::SEARCH);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Range, HTMLInputElement::RANGE);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Email, HTMLInputElement::EMAIL);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Number, HTMLInputElement::NUMBER);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Telephone, HTMLInputElement::TELEPHONE);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::URL, HTMLInputElement::URL);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Color, HTMLInputElement::COLOR);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Date, HTMLInputElement::DATE);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::DateTime, HTMLInputElement::DATETIME);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::DateTimeLocal, HTMLInputElement::DATETIMELOCAL);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Month, HTMLInputElement::MONTH);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Time, HTMLInputElement::TIME);
COMPILE_ASSERT_MATCHING_ENUM(WebInputElement::Week, HTMLInputElement::WEEK);

COMPILE_ASSERT_MATCHING_ENUM(WebNode::ElementNode, Node::ELEMENT_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::AttributeNode, Node::ATTRIBUTE_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::TextNode, Node::TEXT_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::CDataSectionNode, Node::CDATA_SECTION_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::EntityReferenceNode, Node::ENTITY_REFERENCE_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::EntityNode, Node::ENTITY_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::ProcessingInstructionsNode, Node::PROCESSING_INSTRUCTION_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::CommentNode, Node::COMMENT_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::DocumentNode, Node::DOCUMENT_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::DocumentTypeNode, Node::DOCUMENT_TYPE_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::DocumentFragmentNode, Node::DOCUMENT_FRAGMENT_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::NotationNode, Node::NOTATION_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::XPathNamespaceNode, Node::XPATH_NAMESPACE_NODE);

#if ENABLE(VIDEO)
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::Empty, MediaPlayer::Empty);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::Idle, MediaPlayer::Idle);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::Loading, MediaPlayer::Loading);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::Loaded, MediaPlayer::Loaded);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::FormatError, MediaPlayer::FormatError);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::NetworkError, MediaPlayer::NetworkError);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::DecodeError, MediaPlayer::DecodeError);

COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::HaveNothing, MediaPlayer::HaveNothing);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::HaveMetadata, MediaPlayer::HaveMetadata);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::HaveCurrentData, MediaPlayer::HaveCurrentData);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::HaveFutureData, MediaPlayer::HaveFutureData);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::HaveEnoughData, MediaPlayer::HaveEnoughData);

COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::Unknown, MediaPlayer::Unknown);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::Download, MediaPlayer::Download);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::StoredStream, MediaPlayer::StoredStream);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::LiveStream, MediaPlayer::LiveStream);
#endif

#if ENABLE(NOTIFICATIONS)
COMPILE_ASSERT_MATCHING_ENUM(WebNotificationPresenter::PermissionAllowed, NotificationPresenter::PermissionAllowed);
COMPILE_ASSERT_MATCHING_ENUM(WebNotificationPresenter::PermissionNotAllowed, NotificationPresenter::PermissionNotAllowed);
COMPILE_ASSERT_MATCHING_ENUM(WebNotificationPresenter::PermissionDenied, NotificationPresenter::PermissionDenied);
#endif

COMPILE_ASSERT_MATCHING_ENUM(WebTextAffinityUpstream, UPSTREAM);
COMPILE_ASSERT_MATCHING_ENUM(WebTextAffinityDownstream, DOWNSTREAM);

COMPILE_ASSERT_MATCHING_ENUM(WebTextCaseSensitive, TextCaseSensitive);
COMPILE_ASSERT_MATCHING_ENUM(WebTextCaseInsensitive, TextCaseInsensitive);
