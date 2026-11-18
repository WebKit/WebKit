// Modules that WebKit's Swift sources depend on, on iOS.
import AppIntents
import CoreGraphics
import CoreImage
import CoreTransferable
import Foundation
import GameController
import JavaScriptCore
import JavaScriptCore_Private
import Network
import Observation
import UIKit
import UniformTypeIdentifiers
import WebCore_Private
import bmalloc
import os
import wtf

#if USE_APPLE_INTERNAL_SDK
@_weakLinked @_spi(Private) @_spi(ForUIKitOnly) import SwiftUI
#else
import SwiftUI_SPI
#endif

// Not in every SDK; cross-import overlays are disabled, so they are listed explicitly.
#if canImport(_AppIntents_SwiftUI)
import _AppIntents_SwiftUI
#endif
#if canImport(_AppIntents_UIKit)
import _AppIntents_UIKit
#endif
#if canImport(ManagedConfiguration)
import ManagedConfiguration
#endif
