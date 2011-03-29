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

#ifndef WebAccessibilityRole_h
#define WebAccessibilityRole_h

#include "WebCommon.h"

namespace WebKit {

// These values must match WebCore::AccessibilityRole values
enum WebAccessibilityRole {
    WebAccessibilityRoleUnknown = 1,
    WebAccessibilityRoleButton,
    WebAccessibilityRoleRadioButton,
    WebAccessibilityRoleCheckBox,
    WebAccessibilityRoleSlider,
    WebAccessibilityRoleTabGroup,
    WebAccessibilityRoleTextField,
    WebAccessibilityRoleStaticText,
    WebAccessibilityRoleTextArea,
    WebAccessibilityRoleScrollArea,
    WebAccessibilityRolePopUpButton,
    WebAccessibilityRoleMenuButton,
    WebAccessibilityRoleTable,
    WebAccessibilityRoleApplication,
    WebAccessibilityRoleGroup,
    WebAccessibilityRoleRadioGroup,
    WebAccessibilityRoleList,
    WebAccessibilityRoleScrollBar,
    WebAccessibilityRoleValueIndicator,
    WebAccessibilityRoleImage,
    WebAccessibilityRoleMenuBar,
    WebAccessibilityRoleMenu,
    WebAccessibilityRoleMenuItem,
    WebAccessibilityRoleColumn,
    WebAccessibilityRoleRow,
    WebAccessibilityRoleToolbar,
    WebAccessibilityRoleBusyIndicator,
    WebAccessibilityRoleProgressIndicator,
    WebAccessibilityRoleWindow,
    WebAccessibilityRoleDrawer,
    WebAccessibilityRoleSystemWide,
    WebAccessibilityRoleOutline,
    WebAccessibilityRoleIncrementor,
    WebAccessibilityRoleBrowser,
    WebAccessibilityRoleComboBox,
    WebAccessibilityRoleSplitGroup,
    WebAccessibilityRoleSplitter,
    WebAccessibilityRoleColorWell,
    WebAccessibilityRoleGrowArea,
    WebAccessibilityRoleSheet,
    WebAccessibilityRoleHelpTag,
    WebAccessibilityRoleMatte,
    WebAccessibilityRoleRuler,
    WebAccessibilityRoleRulerMarker,
    WebAccessibilityRoleLink,
    WebAccessibilityRoleDisclosureTriangle,
    WebAccessibilityRoleGrid,
    WebAccessibilityRoleCell,
    WebAccessibilityRoleColumnHeader,
    WebAccessibilityRoleRowHeader,

    WebAccessibilityRoleWebCoreLink,
    WebAccessibilityRoleImageMapLink,
    WebAccessibilityRoleImageMap,
    WebAccessibilityRoleListMarker,
    WebAccessibilityRoleWebArea,
    WebAccessibilityRoleHeading,
    WebAccessibilityRoleListBox,
    WebAccessibilityRoleListBoxOption,
    WebAccessibilityRoleTableHeaderContainer,
    WebAccessibilityRoleDefinitionListTerm,
    WebAccessibilityRoleDefinitionListDefinition,
    WebAccessibilityRoleAnnotation,
    WebAccessibilityRoleSliderThumb,
    WebAccessibilityRoleIgnored,
    WebAccessibilityRolePresentational,
    WebAccessibilityRoleTab,
    WebAccessibilityRoleTabList,
    WebAccessibilityRoleTabPanel,
    WebAccessibilityRoleTreeRole,
    WebAccessibilityRoleTreeGrid,
    WebAccessibilityRoleTreeItemRole,
    WebAccessibilityRoleDirectory,
    WebAccessibilityRoleEditableText,

    WebAccessibilityRoleListItem,
    WebAccessibilityRoleMenuListPopup,
    WebAccessibilityRoleMenuListOption,

    WebAccessibilityRoleParagraph,
    WebAccessibilityRoleLabel,
    WebAccessibilityRoleDiv,
    WebAccessibilityRoleForm,

    WebAccessibilityRoleLandmarkApplication,
    WebAccessibilityRoleLandmarkBanner,
    WebAccessibilityRoleLandmarkComplementary,
    WebAccessibilityRoleLandmarkContentInfo,
    WebAccessibilityRoleLandmarkMain,
    WebAccessibilityRoleLandmarkNavigation,
    WebAccessibilityRoleLandmarkSearch,

    WebAccessibilityRoleApplicationAlert,
    WebAccessibilityRoleApplicationAlertDialog,
    WebAccessibilityRoleApplicationDialog,
    WebAccessibilityRoleApplicationLog,
    WebAccessibilityRoleApplicationMarquee,
    WebAccessibilityRoleApplicationStatus,
    WebAccessibilityRoleApplicationTimer,

    WebAccessibilityRoleDocument,
    WebAccessibilityRoleDocumentArticle,
    WebAccessibilityRoleDocumentMath,
    WebAccessibilityRoleDocumentNote,
    WebAccessibilityRoleDocumentRegion,

    WebAccessibilityRoleUserInterfaceTooltip
};

} // namespace WebKit

#endif
