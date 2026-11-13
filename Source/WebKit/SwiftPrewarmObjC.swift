// SDK modules used by WebKitSwift and _WebKit_SwiftUI, which share importer flags (no C++ interop).
import AppIntents
import Combine
import Foundation
import OSLog
import Security
import Spatial
import SwiftUI
import os
import simd
#if canImport(IdentityDocumentServices)
import IdentityDocumentServices
#endif
#if canImport(WritingTools)
import WritingTools
#endif

#if WTF_PLATFORM_MAC
import AppKit
import AVFoundation
import GroupActivities
import RealityKit
#else
import UIKit
#if canImport(IdentityDocumentServicesUI)
import IdentityDocumentServicesUI
#endif
#if canImport(MarketplaceKit)
import MarketplaceKit
#endif
#endif
