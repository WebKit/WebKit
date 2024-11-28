
let xrButton = null;

function onSessionEnded(event) {
    xrButton.setSession(null);
}

async function onSessionStarted(xrSession) {
    if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined) {
        document.body.className = 'error';
        return;
    }
    
    if (navigator.xr == undefined) {
        document.writeln("WebXR is not supported");
        return;
    }
    try {
        xrButton.setSession(xrSession);
        xrSession.addEventListener('end', onSessionEnded);

        const adapter = await navigator.gpu.requestAdapter({xrCompatible: true});
        const device = await adapter.requestDevice();
        const xrGpuBinding = new XRGPUBinding(xrSession, device);
        const projectionLayer = xrGpuBinding.createProjectionLayer({
          colorFormat: xrGpuBinding.getPreferredColorFormat(),
          depthStencilFormat: 'depth24plus',
        });

        xrSession.updateRenderState({ layers: [projectionLayer] });
        xrSession.requestAnimationFrame(onXRFrame);
        const xrReferenceSpace = await xrSession.requestReferenceSpace("viewer");

        /*** Vertex Buffer Setup ***/
        
        /* Vertex Data */
        const vertexStride = 8 * 4;
        const vertexDataSize = vertexStride * 3;
        
        /* GPUBufferDescriptor */
        const vertexDataBufferDescriptor = {
            size: vertexDataSize,
            usage: GPUBufferUsage.VERTEX
        };
        
        /* GPUBuffer */
        const vertexBuffer = device.createBuffer(vertexDataBufferDescriptor);
        
        /*** Shader Setup ***/
        const wgslSource = `
                         struct Vertex {
                             @builtin(position) Position: vec4<f32>,
                             @location(0) color: vec4<f32>,
                         }
        
                         @vertex fn vsmain(@builtin(vertex_index) VertexIndex: u32, @builtin(instance_index) InstanceID: u32) -> Vertex
                         {
                             let triangle = VertexIndex / 3;
                             let index = VertexIndex - 3 * triangle;
                             let ftriangle = f32(triangle);
                             const k: f32 = 0.25;
                             var pos: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
                                 vec2<f32>( 0.0 + k - 2 * k * ftriangle, -0.5),
                                 vec2<f32>(-0.5 + k - 2 * k * ftriangle,  0.5),
                                 vec2<f32>( 0.5 + k - 2 * k * ftriangle,  0.5)
                             );
                             var vertex_out : Vertex;
                             vertex_out.Position = vec4<f32>(pos[index], 0.5, 1.0);
                             let shiftedIndex = (index + InstanceID / 270) % 3;
                             vertex_out.color = vec4<f32>(pos[u32(shiftedIndex)] + vec2<f32>(0.5, 0.5), 0.0, 1.0);
                             return vertex_out;
                         }
        
                         @fragment fn fsmain(in: Vertex) -> @location(0) vec4<f32>
                         {
                             return in.color;
                         }
        `;
        const shaderModule = device.createShaderModule({ code: wgslSource });
        
        /* GPUPipelineStageDescriptors */
        const vertexStageDescriptor = { module: shaderModule, entryPoint: "vsmain" };
        
        const fragmentStageDescriptor = { module: shaderModule, entryPoint: "fsmain", targets: [ {format: "bgra8unorm-srgb" }, ],  };
        
        /* GPURenderPipelineDescriptor */
        
        const renderPipelineDescriptor = {
            layout: 'auto',
            vertex: vertexStageDescriptor,
            fragment: fragmentStageDescriptor,
            primitive: {topology: "triangle-list" },
            depthStencil: {
              depthWriteEnabled: true,
              depthCompare: 'always',
              format: 'depth24plus-stencil8',
            },
        };
        /* GPURenderPipeline */
        const renderPipeline = device.createRenderPipeline(renderPipelineDescriptor);
        
        /*** Swap Chain Setup ***/
        
        const canvas = document.querySelector("canvas");
        canvas.width = 600;
        canvas.height = 600;
        
        const gpuContext = canvas.getContext("webgpu");
        
        /* GPUCanvasConfiguration */
        const canvasConfiguration = { device: device, format: "bgra8unorm" };
        gpuContext.configure(canvasConfiguration);
        let frameID = 0;
        let offsets = [0, 0, 0];
        let offsetIndex = 0;
        function onXRFrame(time, xrFrame) {
            xrSession.requestAnimationFrame(onXRFrame);
            
            /*** Render Pass Setup ***/
            
            /* Acquire Texture To Render To */
            
            /* GPUColor */
            const darkBlue = { r: 0.15 + offsets[0], g: 0.15 + offsets[1], b: 0.5 + offsets[2], a: 1 };
            
            /*** Rendering ***/
            
            /* GPUCommandEncoder */
            let xrViewerPose = xrFrame.getViewerPose(xrReferenceSpace);
            const commandEncoder = device.createCommandEncoder();

            for (let i = 0; i < xrViewerPose.views.length; ++i) {
                const view = xrViewerPose.views[i];
                const subImage = xrGpuBinding.getViewSubImage(projectionLayer, view);
                
                const passEncoder = commandEncoder.beginRenderPass({
                    colorAttachments: [{
                        view: subImage.colorTexture.createView(subImage.getViewDescriptor()),
                        loadOp: 'clear',
                        clearValue: darkBlue,
                        storeOp: 'store',
                    }],
                    depthStencilAttachment: {
                        view: subImage.depthStencilTexture.createView(subImage.getViewDescriptor()),
                        depthLoadOp: 'clear',
                        depthClearValue: 0.05,
                        depthStoreOp: 'store',
                        stencilLoadOp: 'clear',
                        stencilClearValue: 0,
                        stencilStoreOp: 'store',
                    }});
                
                let vp = subImage.viewport;
                passEncoder.setViewport(vp.x, vp.y, vp.width, vp.height, 0.0, 1.0);
                passEncoder.setPipeline(renderPipeline);

                const vertexBufferSlot = 0;
                passEncoder.setVertexBuffer(vertexBufferSlot, vertexBuffer, 0);
                const instanceID = parseInt(frameID, 10);
                passEncoder.draw(3, 1, i * 3, instanceID); // 3 vertices, 1 instance
                passEncoder.end();
            }

            /* GPUComamndBuffer */
            const commandBuffer = commandEncoder.finish();

            /* GPUQueue */
            const queue = device.queue;
            queue.submit([commandBuffer]);
            ++frameID;
            offsets[offsetIndex] += 0.01;
            if (offsets[offsetIndex] > 1) {
                offsets[offsetIndex] = 0;
                offsetIndex = offsetIndex + 1;
                if (offsetIndex > 2)
                    offsetIndex = 0;
            }
        }

    } catch (error) {
        document.writeln(error);
    }
}

// Called when the user clicks the 'Exit XR' button. In response we end
// the session.
function onEndSession(session) {
session.end();
}

function onRequestSession() {
    return navigator.xr.requestSession('immersive-vr').then(onSessionStarted);
}


function initXR() {
    // Adds a helper button to the page that indicates if any XRDevices are
    // available and let's the user pick between them if there's multiple.
    xrButton = new WebXRButton({
        onRequestSession: onRequestSession,
        onEndSession: onEndSession
    });
    document.querySelector('header').appendChild(xrButton.domElement);

    // Is WebXR available on this UA?
    if (navigator.xr) {
        // If the device allows creation of exclusive sessions set it as the
        // target of the 'Enter XR' button.
        navigator.xr.isSessionSupported('immersive-vr').then((supported) => {
        xrButton.enabled = supported;
    });
    }
}


window.addEventListener("DOMContentLoaded", initXR);
