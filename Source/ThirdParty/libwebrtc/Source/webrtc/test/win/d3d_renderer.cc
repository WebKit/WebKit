/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/win/d3d_renderer.h"

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_TEX1)

struct D3dCustomVertex {
  float x, y, z;
  float u, v;
};

const char kD3DClassName[] = "d3d_renderer";

VideoRenderer* VideoRenderer::CreatePlatformRenderer(const char* window_title,
                                                     size_t width,
                                                     size_t height) {
  return D3dRenderer::Create(window_title, width, height);
}

D3dRenderer::D3dRenderer(size_t width, size_t height)
    : width_(width),
      height_(height),
      hwnd_(NULL),
      d3d_(nullptr),
      d3d_device_(nullptr),
      texture_(nullptr),
      vertex_buffer_(nullptr) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
}

D3dRenderer::~D3dRenderer() {
  Destroy();
}

LRESULT WINAPI D3dRenderer::WindowProc(HWND hwnd,
                                       UINT msg,
                                       WPARAM wparam,
                                       LPARAM lparam) {
  if (msg == WM_DESTROY || (msg == WM_CHAR && wparam == VK_RETURN)) {
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProcA(hwnd, msg, wparam, lparam);
}

void D3dRenderer::Destroy() {
  texture_ = nullptr;
  vertex_buffer_ = nullptr;
  d3d_device_ = nullptr;
  d3d_ = nullptr;

  if (hwnd_ != NULL) {
    DestroyWindow(hwnd_);
    RTC_DCHECK(!IsWindow(hwnd_));
    hwnd_ = NULL;
  }
}

bool D3dRenderer::Init(const char* window_title) {
  hwnd_ = CreateWindowA(kD3DClassName, window_title, WS_OVERLAPPEDWINDOW, 0, 0,
                        static_cast<int>(width_), static_cast<int>(height_),
                        NULL, NULL, NULL, NULL);

  if (hwnd_ == NULL) {
    Destroy();
    return false;
  }

  d3d_ = Direct3DCreate9(D3D_SDK_VERSION);
  if (d3d_ == nullptr) {
    Destroy();
    return false;
  }

  D3DPRESENT_PARAMETERS d3d_params = {};

  d3d_params.Windowed = TRUE;
  d3d_params.SwapEffect = D3DSWAPEFFECT_COPY;

  IDirect3DDevice9* d3d_device;
  if (d3d_->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd_,
                         D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3d_params,
                         &d3d_device) != D3D_OK) {
    Destroy();
    return false;
  }
  d3d_device_ = d3d_device;
  d3d_device->Release();

  IDirect3DVertexBuffer9* vertex_buffer;
  const int kRectVertices = 4;
  if (d3d_device_->CreateVertexBuffer(kRectVertices * sizeof(D3dCustomVertex),
                                      0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED,
                                      &vertex_buffer, NULL) != D3D_OK) {
    Destroy();
    return false;
  }
  vertex_buffer_ = vertex_buffer;
  vertex_buffer->Release();

  d3d_device_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  d3d_device_->SetRenderState(D3DRS_LIGHTING, FALSE);
  Resize(width_, height_);

  ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
  d3d_device_->Present(NULL, NULL, NULL, NULL);

  return true;
}

D3dRenderer* D3dRenderer::Create(const char* window_title,
                                 size_t width,
                                 size_t height) {
  static ATOM wc_atom = 0;
  if (wc_atom == 0) {
    WNDCLASSA wc = {};

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
    wc.lpszClassName = kD3DClassName;

    wc_atom = RegisterClassA(&wc);
    if (wc_atom == 0)
      return nullptr;
  }

  D3dRenderer* d3d_renderer = new D3dRenderer(width, height);
  if (!d3d_renderer->Init(window_title)) {
    delete d3d_renderer;
    return nullptr;
  }

  return d3d_renderer;
}

void D3dRenderer::Resize(size_t width, size_t height) {
  width_ = width;
  height_ = height;
  IDirect3DTexture9* texture;

  d3d_device_->CreateTexture(static_cast<UINT>(width_),
                             static_cast<UINT>(height_), 1, 0, D3DFMT_A8R8G8B8,
                             D3DPOOL_MANAGED, &texture, NULL);
  texture_ = texture;
  texture->Release();

  // Vertices for the video frame to be rendered to.
  static const D3dCustomVertex rect[] = {
      {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f},
      {-1.0f, 1.0f, 0.0f, 0.0f, 0.0f},
      {1.0f, -1.0f, 0.0f, 1.0f, 1.0f},
      {1.0f, 1.0f, 0.0f, 1.0f, 0.0f},
  };

  void* buf_data;
  if (vertex_buffer_->Lock(0, 0, &buf_data, 0) != D3D_OK)
    return;

  memcpy(buf_data, &rect, sizeof(rect));
  vertex_buffer_->Unlock();
}

void D3dRenderer::OnFrame(const webrtc::VideoFrame& frame) {
  if (static_cast<size_t>(frame.width()) != width_ ||
      static_cast<size_t>(frame.height()) != height_) {
    Resize(static_cast<size_t>(frame.width()),
           static_cast<size_t>(frame.height()));
  }

  D3DLOCKED_RECT lock_rect;
  if (texture_->LockRect(0, &lock_rect, NULL, 0) != D3D_OK)
    return;

  ConvertFromI420(frame, VideoType::kARGB, 0,
                  static_cast<uint8_t*>(lock_rect.pBits));
  texture_->UnlockRect(0);

  d3d_device_->BeginScene();
  d3d_device_->SetFVF(D3DFVF_CUSTOMVERTEX);
  d3d_device_->SetStreamSource(0, vertex_buffer_.get(), 0,
                               sizeof(D3dCustomVertex));
  d3d_device_->SetTexture(0, texture_.get());
  d3d_device_->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  d3d_device_->EndScene();

  d3d_device_->Present(NULL, NULL, NULL, NULL);
}
}  // namespace test
}  // namespace webrtc
