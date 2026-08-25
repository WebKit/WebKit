# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Background daemons a simulator running layout and API tests does not need. Naming one here keeps it from ever
# launching, which takes roughly 4x less memory per simulator so more of them fit on a host.
#
# Deliberately absent: com.apple.sharingd, which testing needs; com.apple.eligibilityd, which the harness boosts
# before installing WebKitTestRunnerApp; com.apple.sleepd, which SpringBoard retries thousands of times a second
# when it is missing, burning 265% CPU per simulator; com.apple.dmd; com.apple.assistantd, which the keyboard
# asks for dictation availability; and the daemons backing web platform
# features the tests cover.
#
# apple_additions adds the daemons that only exist in Apple's internal builds.
UNNEEDED_DAEMONS = [
    # Widgets & Wallpaper
    'com.apple.chronod',
    'com.apple.liveactivitiesd',
    # Siri & Intelligence
    'com.apple.assistant_cdmd',
    'com.apple.assistant_service',
    'com.apple.siriactionsd',
    'com.apple.siriinferenced',
    'com.apple.siriknowledged',
    'com.apple.voicebankingd',
    'com.apple.speechmodeltrainingd',
    'com.apple.intelligenceplatformd',
    'com.apple.intelligencecontextd',
    'com.apple.intelligenceflowd',
    'com.apple.intelligencetasksd',
    'com.apple.generativeexperiencesd',
    'com.apple.knowledgeconstructiond',
    'com.apple.modelcatalogd',
    'com.apple.modelmanagerd',
    'com.apple.mlhostd',
    'com.apple.mlruntimed',
    'com.apple.suggestd',
    'com.apple.parsecd',
    'com.apple.parsec-fbf',
    'com.apple.proactiveeventtrackerd',
    # Spotlight & Search
    'com.apple.searchd',
    'com.apple.searchtoold',
    'com.apple.spotlightknowledged',
    'com.apple.spotlightknowledged.updater',
    'com.apple.corespotlightservice',
    # iCloud & Apple Account
    'com.apple.appleaccountd',
    'com.apple.appleaccounttransparencyd',
    'com.apple.amsaccountsd',
    'com.apple.amsondevicestoraged',
    'com.apple.cloudd',
    'com.apple.cloudphotod',
    'com.apple.ckdiscretionaryd',
    'com.apple.cloudsettingssyncagent',
    'com.apple.bird',
    'com.apple.syncdefaultsd',
    'com.apple.cdpd',
    'com.apple.sosd',
    'com.apple.SecureBackupDaemon',
    'com.apple.protectedcloudstorage.protectedcloudkeysyncing',
    'com.apple.icloudmailagent',
    'com.apple.icloudsubscriptionoptimizerd',
    'com.apple.communicationtrustd',
    # App Store, Push & Media
    'com.apple.appstorecomponentsd',
    'com.apple.itunescloudd',
    'com.apple.itunesstored',
    'com.apple.storekitd',
    'com.apple.videosubscriptionsd',
    'com.apple.assetsubscriptiond',
    'com.apple.musicd',
    # Mail, Calendar & Contacts
    'com.apple.email.maild',
    'com.apple.exchangesyncd',
    'com.apple.dataaccess.dataaccessd',
    'com.apple.calaccessd',
    'com.apple.remindd',
    'com.apple.contacts.postersyncd',
    'com.apple.peopled',
    # Safari Sync & Web Services
    'com.apple.SafariBookmarksSyncAgent',
    'com.apple.Safari.History',
    'com.apple.Safari.passwordbreachd',
    'com.apple.WebBookmarks.webbookmarksd',
    # Family & Screen Time
    'com.apple.familycircled',
    'com.apple.FamilyControlsAgent',
    'com.apple.familynotificationd',
    'com.apple.askpermissiond',
    'com.apple.asktod',
    'com.apple.ScreenTimeSettingsAgent',
    # Health, Home & Fitness
    'com.apple.healthd',
    'com.apple.healthappd',
    'com.apple.healthcontentd',
    'com.apple.healtheventsd',
    'com.apple.healthrecordsd',
    'com.apple.finhealthd',
    'com.apple.homed',
    'com.apple.homeeventsd',
    'com.apple.fitcore',
    'com.apple.fitcore.session',
    'com.apple.fitnesscoachingd',
    'com.apple.fitnessintelligenced',
    'com.apple.activityawardsd',
    'com.apple.activitysharingd',
    # Photos & Media Analysis
    'com.apple.photoanalysisd',
    'com.apple.photosface',
    'com.apple.mediaanalysisd',
    'com.apple.mediaanalysisd.service',
    'com.apple.mediastream.mstreamd',
    'com.apple.medialibraryd',
    'com.apple.assetsd',
    'com.apple.assetsd.nebulad',
    # News, Weather, Maps & Games
    'com.apple.newsd',
    'com.apple.weatherd',
    'com.apple.Maps.mapssyncd',
    'com.apple.Maps.geocorrectiond',
    'com.apple.maps.destinationd',
    'com.apple.jetpackassetd',
    'com.apple.tipsd',
    'com.apple.gamed',
    'com.apple.gamesaved',
    # Messaging & FaceTime
    'com.apple.identityservicesd',
    'com.apple.ids_simd',
    'com.apple.imautomatichistorydeletionagent',
    'com.apple.imtransferagent',
    'com.apple.facetimemessagestored',
    'com.apple.telephonyutilities.callservicesd',
    # Sharing & Device Connectivity (NB: com.apple.sharingd stays enabled)
    'com.apple.rapportd',
    'com.apple.companiond',
    'com.apple.carkitd',
    'com.apple.wcd',
    'com.apple.tvremoted',
    'com.apple.avatarsd',
    'com.apple.stickersd',
    'com.apple.sociallayerd',
    'com.apple.announced',
    'com.apple.navd',
    'com.apple.findmy.findmylocated',
    # Ads, Diagnostics & Telemetry
    'com.apple.ap.adprivacyd',
    'com.apple.ap.promotedcontentd',
    'com.apple.diagnosticextensionsd',
    'com.apple.feedbackd',
    'com.apple.rtcreportingd',
    'com.apple.securityuploadd',
    'com.apple.geoanalyticsd',
    'com.apple.triald',
    'com.apple.followupd',
    'com.apple.purplebuddy.budd',
    'com.apple.devicecheckd',
    # Other Background Services
    'com.apple.businessservicesd',
    'com.apple.deviceaccessd',
    'com.apple.replicatord',
    'com.apple.linkd',
    'com.apple.ind',
    'com.apple.storagedatad',
    'com.apple.StatusKitAgent',
    'com.apple.countryd',
    'com.apple.mobileassetd',
    'com.apple.managedconfiguration.passcodenagd',
    # Watch companion, on-device learning, device management and store leftovers
    'com.apple.nanotimekitcompaniond',
    'com.apple.nanosystemsettingsd',
    'com.apple.nanomapscd',
    'com.apple.NPKCompanionAgent',
    'com.apple.nanoregistrylaunchd',
    'com.apple.nanoprefsyncd.2',
    'com.apple.nanoappregistryd',
    'com.apple.nanobackupd',
    'com.apple.nanonewscd',
    'com.apple.companionappd',
    'com.apple.appconduitd',
    'com.apple.pairedsyncd',
    'com.apple.pairedunlockd',
    'com.apple.companionfindlocallyd',
    'com.apple.companionmessagesd',
    'com.apple.biomed',
    'com.apple.biomesyncd',
    'com.apple.routined',
    'com.apple.brook.brookcompaniond',
    'com.apple.imagent',
    'com.apple.addressbooksyncd',
    'com.apple.eventkitsyncd',
    'com.apple.cmfsyncagent',
    'com.apple.mobiletimerd',
    'com.apple.donotdisturbd',
    'com.apple.ManagedSettingsAgent',
    'com.apple.appprotectiond',
    'com.apple.fileindexerd',
    'com.apple.amstoold',
    'com.apple.featureaccessd',
    'com.apple.ClipServices.clipserviced',
    'com.apple.managedassetsd',
    'com.apple.GSSCred',
    'com.apple.accessibility.axassetsd',
    'com.apple.geod',
]


def disabled_launchd_jobs():
    """The daemons above in the form launchd's disabled list takes.

    launchd_sim reads that list as it starts, so a daemon named here never launches at all, rather than launching
    during boot and being stopped again afterwards."""
    return {label: True for label in UNNEEDED_DAEMONS}
