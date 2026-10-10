// Modules that WebKit's Swift sources depend on, on iOS.
import AppIntents
import CoreGraphics
import CoreTransferable
import Foundation
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

#if HAVE_MOUSE_DEVICE_OBSERVATION
@_weakLinked import GameController
#endif
#if HAVE_CORE_ANIMATION_SEPARATED_LAYERS
import CoreImage
#endif

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
