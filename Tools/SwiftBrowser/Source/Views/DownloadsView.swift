// Copyright (C) 2024 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import SwiftUI

private struct CancelButton: View {
    #if os(macOS)
    private static let systemImage = "xmark.circle.fill"
    #else
    private static let systemImage = "xmark"
    #endif

    let cancel: () async -> Void

    var body: some View {
        Button {
            Task {
                await cancel()
            }
        } label: {
            Label("Cancel", systemImage: Self.systemImage)
                .labelStyle(.iconOnly)
        }
        #if os(macOS)
        .buttonStyle(.plain)
        #endif
    }
}

private struct OpenFileButton: View {
    #if os(macOS)
    private static let systemImage = "magnifyingglass.circle.fill"
    #else
    private static let systemImage = "magnifyingglass"
    #endif

    let url: URL?

    var body: some View {
        Button {
            Task {
                #if os(macOS)
                NSWorkspace.shared.activateFileViewerSelecting([url!])
                #else
                await UIApplication.shared.open(url!)
                #endif
            }
        } label: {
            Label("Show in Finder", systemImage: Self.systemImage)
                .labelStyle(.iconOnly)
        }
        #if os(macOS)
        .buttonStyle(.plain)
        #endif
        .disabled(url == nil)
    }
}

struct DownloadView<D: Downloadable>: View {
    let download: D

    private var formattedSize: String {
        let count = Measurement(value: Double(download.progress.totalUnitCount), unit: UnitInformationStorage.bytes)
        return count.formatted(.byteCount(style: .file))
    }

    var body: some View {
        HStack {
            if download.finished {
                VStack(alignment: .leading) {
                    Text(download.url?.lastPathComponent ?? "File")

                    Text(formattedSize)
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }
            } else {
                ProgressView(download.progress)
            }

            Spacer()

            if download.finished {
                OpenFileButton(url: download.url)
            } else {
                CancelButton {
                    await download.cancel()
                }
            }
        }
        .frame(minHeight: 50)
    }
}

struct DownloadsList<D: Downloadable>: View {
    @Environment(\.dismiss) var dismiss

    let downloads: [D]

    var body: some View {
        NavigationStack {
            List(downloads) {
                DownloadView(download: $0)
            }
            .navigationTitle("Downloads")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                Button("Done") {
                    dismiss()
                }
                .bold()
            }
            #endif
            .overlay {
                if downloads.isEmpty {
                    ContentUnavailableView("No Downloads", systemImage: "square.and.arrow.down")
                }
            }
        }
    }
}

#Preview {
    struct Download: Downloadable {
        let id = UUID()
        let progress: Progress
        let url: URL?
        let finished: Bool

        func cancel() async {
        }
    }

    return DownloadsList(downloads: [
        Download(
            progress: .discreteProgress(totalUnitCount: 1),
            url: .init(string: "file:///sample.pdf"),
            finished: true
        ),
        Download(
            progress: .discreteProgress(totalUnitCount: 1),
            url: .init(string: "https://www.apple.com"),
            finished: false
        )
    ])
}
