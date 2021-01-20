/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Example.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Example.h"
#include <vector>
#include <cstdio>
#include <cassert>
#include <fstream>
#include <queue>
#include <boost/optional.hpp>
#include <WebGPU.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

HWND hWnd;
std::unique_ptr<WebGPU::Device> device;
WebGPU::Queue* commandQueue = nullptr;
WebGPU::RenderState* renderState = nullptr;
WebGPU::RenderState* renderStateRTT = nullptr;
WebGPU::ComputeState* computeState = nullptr;
std::unique_ptr<WebGPU::BufferHolder> buffer;
std::unique_ptr<WebGPU::TextureHolder> texture;
std::unique_ptr<WebGPU::BufferHolder> vertexBuffer;
std::unique_ptr<WebGPU::BufferHolder> vertexBufferRTT;
std::unique_ptr<WebGPU::SamplerHolder> sampler;
std::queue<boost::unique_future<std::vector<uint8_t>>> futureQueue;
bool initializedBuffers = false;
float t = 0;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

static std::vector<uint8_t> readFile(const std::string& filename) {
    std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    assert(file.is_open());
    auto size = file.tellg();
    std::vector<uint8_t> result(size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(result.data()), size);
    assert(file.tellg() == size);
    return result;
}

static void drawWebGPU() {
    if (!commandQueue || !renderState || !buffer || !vertexBuffer || !vertexBufferRTT || !renderStateRTT || !texture)
        return;

    WINDOWINFO windowInfo;
    auto success = GetWindowInfo(hWnd, &windowInfo);
    assert(success);
    auto width = windowInfo.rcClient.right - windowInfo.rcClient.left;
    auto height = windowInfo.rcClient.bottom - windowInfo.rcClient.top;

    while (!futureQueue.empty() && futureQueue.front().is_ready() && futureQueue.front().has_value()) {
        const auto& data = futureQueue.front().get();
        float t = *reinterpret_cast<const float*>(data.data());
        std::ostringstream ss;
        ss << "T has value " << t << std::endl;
        OutputDebugStringA(ss.str().c_str());
        futureQueue.pop();
    }

    if (!initializedBuffers) {
        auto hostAccessPass = commandQueue->createHostAccessPass();

        std::vector<uint8_t> bufferContents(sizeof(float));
        for (std::size_t i = 0; i < sizeof(float); ++i)
            bufferContents[i] = reinterpret_cast<uint8_t*>(&t)[i];
        hostAccessPass->overwriteBuffer(buffer->get(), bufferContents);

        std::vector<float> vertexBufferContents = { -1, 1, -1, -1, 1, -1, 1, -1, 1, 1, -1, 1 };
        std::vector<uint8_t> vertexBufferRawContents(sizeof(float) * 12);
        for (std::size_t i = 0; i < vertexBufferContents.size(); ++i) {
            float value = vertexBufferContents[i];
            for (std::size_t j = 0; j < sizeof(float); ++j)
                vertexBufferRawContents[sizeof(float) * i + j] = reinterpret_cast<uint8_t*>(&value)[j];
        }
        hostAccessPass->overwriteBuffer(vertexBuffer->get(), vertexBufferRawContents);

        std::vector<float> vertexBufferContentsRTT = { -0.5, 0.5, 0, -0.5, 0.5, 1.0 };
        std::vector<uint8_t> vertexBufferRawContentsRTT(sizeof(float) * 6);
        for (std::size_t i = 0; i < vertexBufferContentsRTT.size(); ++i) {
            float value = vertexBufferContentsRTT[i];
            for (std::size_t j = 0; j < sizeof(float); ++j)
                vertexBufferRawContentsRTT[sizeof(float) * i + j] = reinterpret_cast<uint8_t*>(&value)[j];
        }
        hostAccessPass->overwriteBuffer(vertexBufferRTT->get(), vertexBufferRawContentsRTT);
        commandQueue->commitHostAccessPass(std::move(hostAccessPass));
        initializedBuffers = true;
    }

    auto computePass = commandQueue->createComputePass();
    computePass->setComputeState(*computeState);
    computePass->setResources(0, { WebGPU::ShaderStorageBufferObjectReference(buffer->get()) });
    computePass->dispatch(1, 1, 1);
    commandQueue->commitComputePass(std::move(computePass));

    std::vector<std::reference_wrapper<WebGPU::Texture>> rtt = { texture->get() };
    auto renderPass = commandQueue->createRenderPass(&rtt);
    renderPass->setRenderState(*renderStateRTT);
    renderPass->setVertexAttributeBuffers({ vertexBufferRTT->get() });
    renderPass->setResources(0, { WebGPU::UniformBufferObjectReference(buffer->get()) });
    renderPass->setViewport(0, 0, width, height);
    renderPass->setScissorRect(0, 0, width, height);
    renderPass->draw(3);
    commandQueue->commitRenderPass(std::move(renderPass));

    auto renderPass2 = commandQueue->createRenderPass(nullptr);
    renderPass2->setRenderState(*renderState);
    renderPass2->setVertexAttributeBuffers({ vertexBuffer->get() });
    renderPass2->setResources(0, { WebGPU::UniformBufferObjectReference(buffer->get()), WebGPU::TextureReference(texture->get()), WebGPU::SamplerReference(sampler->get()) });
    renderPass2->setViewport(0, 0, width, height);
    renderPass2->setScissorRect(0, 0, width, height);
    renderPass2->draw(6);
    commandQueue->commitRenderPass(std::move(renderPass2));

    auto hostAccessPass = commandQueue->createHostAccessPass();
    futureQueue.emplace(hostAccessPass->getBufferContents(buffer->get()));
    commandQueue->commitHostAccessPass(std::move(hostAccessPass));

    commandQueue->present();
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_EXAMPLE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    /*
    #version 460

    layout(location = 0) in vec2 position;

    layout(std140, binding = 0) uniform buf {
        float t;
    };

    void main() {
        float time = t / 10;
        vec2 delta = vec2(cos(time), sin(time));
        delta = delta * 0.5;
        gl_Position = vec4(position + delta, 0.5, 1);
    }
    glslangValidator -g -o "C:\Users\lithe\Documents\VertexShader.spv" -V -H "C:\Users\lithe\Documents\VertexShader.vert" -S vert
    #version 460

    layout (location = 0) out vec4 fragColor;

    void main() {
        fragColor = vec4(1, 0, 0, 1);
    }
    glslangValidator -g -o "C:\Users\lithe\Documents\FragmentShader.spv" -V -H "C:\Users\lithe\Documents\FragmentShader.frag" -S frag
    #version 460

    layout(location = 0) in vec2 position;

    layout(location = 0) out vec2 texCoord;

    layout(std140, binding = 0) uniform buf {
        float t;
    };

    void main() {
        texCoord = position;
        gl_Position = vec4(position, 0.5, 1);
    }
    glslangValidator -g -o "C:\Users\lithe\Documents\VertexShader2.spv" -V -H "C:\Users\lithe\Documents\VertexShader2.vert" -S vert
    struct Result {
        float4 color : SV_Target0;
    };

    Texture2D<float4> inputTexture : register(t1);
    SamplerState inputSampler : register(s2);

    Result main(float2 texCoord : TEXCOORD0)
    {
        Result result;
        float4 sampleResult = inputTexture.Sample(inputSampler, texCoord);
        result.color = float4(sampleResult.rgb, 1.0f);
        return result;
    }
    dxc.exe -spirv -T ps_6_0 "C:\Users\lithe\Documents\PixelShader.hlsl" -Fo "C:\Users\lithe\Documents\FragmentShader2.spv"
    */
    commandQueue = &device->getCommandQueue();
    auto vertexShaderRTT = readFile("C:\\Users\\lithe\\Documents\\VertexShader.spv");
    auto fragmentShaderRTT = readFile("C:\\Users\\lithe\\Documents\\FragmentShader.spv");
    auto computeShader = readFile("C:\\Users\\lithe\\Documents\\ComputeShader.spv");
    auto vertexShader = readFile("C:\\Users\\lithe\\Documents\\VertexShader2.spv");
    auto fragmentShader = readFile("C:\\Users\\lithe\\Documents\\FragmentShader2.spv");

    vertexBufferRTT = std::unique_ptr<WebGPU::BufferHolder>(new WebGPU::BufferHolder(device->getBuffer(static_cast<unsigned int>(sizeof(float) * 6))));
    buffer = std::unique_ptr<WebGPU::BufferHolder>(new WebGPU::BufferHolder(device->getBuffer(static_cast<unsigned int>(sizeof(float)))));
    vertexBuffer = std::unique_ptr<WebGPU::BufferHolder>(new WebGPU::BufferHolder(device->getBuffer(static_cast<unsigned int>(sizeof(float) * 12))));

    sampler = std::unique_ptr<WebGPU::SamplerHolder>(new WebGPU::SamplerHolder(device->getSampler(WebGPU::AddressMode::Repeat, WebGPU::Filter::Linear)));

    WINDOWINFO windowInfo;
    auto success = GetWindowInfo(hWnd, &windowInfo);
    assert(success);
    texture = std::unique_ptr<WebGPU::TextureHolder>(new WebGPU::TextureHolder(device->getTexture(windowInfo.rcClient.right - windowInfo.rcClient.left, windowInfo.rcClient.bottom - windowInfo.rcClient.top, WebGPU::PixelFormat::RGBA8)));

    std::vector<WebGPU::RenderState::VertexAttribute> vertexAttributes = { {WebGPU::RenderState::VertexFormat::Float2, 0, 0} };
    std::vector<WebGPU::PixelFormat> colorPixelFormats = { WebGPU::PixelFormat::RGBA8 };
    renderStateRTT = &device->getRenderState(vertexShaderRTT, "main", fragmentShaderRTT, "main", { 8 }, vertexAttributes, { { WebGPU::ResourceType::UniformBufferObject } }, &colorPixelFormats);
    renderState = &device->getRenderState(vertexShader, "main", fragmentShader, "main", { 8 }, vertexAttributes, { { WebGPU::ResourceType::UniformBufferObject, WebGPU::ResourceType::Texture, WebGPU::ResourceType::Sampler } }, nullptr);
    computeState = &device->getComputeState(computeShader, "main", { { WebGPU::ResourceType::ShaderStorageBufferObject } });
    drawWebGPU();

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_EXAMPLE));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        RedrawWindow(hWnd, nullptr, nullptr, RDW_INTERNALPAINT);
    }
    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EXAMPLE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = nullptr;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   device = WebGPU::Device::create(hInstance, hWnd);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        drawWebGPU();
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
