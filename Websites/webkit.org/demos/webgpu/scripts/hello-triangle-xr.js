async function helloTriangle() {
    if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined) {
        document.body.className = 'error';
        return;
    }
    
    if (navigator.xr == undefined) {
        document.writeln("WebXR is not supported");
        return;
    }
    try {
        const xrSession = await navigator.xr.requestSession('immersive-vr');
        const adapter = await navigator.gpu.requestAdapter({xrCompatible: true});
        const device = await adapter.requestDevice();
        const xrGpuBinding = new XRGPUBinding(xrSession, device);
        const projectionLayer = xrGpuBinding.createProjectionLayer({
          colorFormat: xrGpuBinding.getPreferredColorFormat(),
          depthStencilFormat: 'depth24plus',
        });

        xrSession.updateRenderState({ layers: [projectionLayer] });
        xrSession.requestAnimationFrame(onXRFrame);

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
        
                         @vertex fn vsmain(@builtin(vertex_index) VertexIndex: u32) -> Vertex
                         {
                             var pos: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
                                 vec2<f32>( 0.0,  0.5),
                                 vec2<f32>(-0.5, -0.5),
                                 vec2<f32>( 0.5, -0.5)
                             );
                             var vertex_out : Vertex;
                             vertex_out.Position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
                             vertex_out.color = vec4<f32>(pos[VertexIndex] + vec2<f32>(0.5, 0.5), 0.0, 1.0);
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
        
        const fragmentStageDescriptor = { module: shaderModule, entryPoint: "fsmain", targets: [ {format: "bgra8unorm" }, ],  };
        
        /* GPURenderPipelineDescriptor */
        
        const renderPipelineDescriptor = {
            layout: 'auto',
            vertex: vertexStageDescriptor,
            fragment: fragmentStageDescriptor,
            primitive: {topology: "triangle-list" },
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

        function onXRFrame(time, xrFrame) {
            xrSession.requestAnimationFrame(onXRFrame);
            
            /*** Render Pass Setup ***/
            
            /* Acquire Texture To Render To */
            
            /* GPUColor */
            const darkBlue = { r: 0.15, g: 0.15, b: 0.5, a: 1 };
            
            /*** Rendering ***/
            
            /* GPUCommandEncoder */
            const commandEncoder = device.createCommandEncoder();
            
            for (const view in xrViewerPose.views) {
                const subImage = xrGpuBinding.getViewSubImage(layer, view);
                
                const passEncoder = commandEncoder.beginRenderPass({
                    colorAttachments: [{
                        attachment: subImage.colorTexture.createView(subImage.viewDescriptor),
                        loadOp: 'clear',
                        clearValue: [0,0,0,1],
                    }],
                    depthStencilAttachment: {
                        attachment: subImage.depthStencilTexture.createView(subImage.viewDescriptor),
                        depthLoadOp: 'clear',
                        depthClearValue: 1.0,
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
                passEncoder.draw(3, 1, 0, 0); // 3 vertices, 1 instance, 0th vertex, 0th instance.
                passEncoder.end();
            }

            /* GPUComamndBuffer */
            const commandBuffer = commandEncoder.finish();
            
            /* GPUQueue */
            const queue = device.queue;
            queue.submit([commandBuffer]);
        }

    } catch (error) {
        document.writeln(error);
    }
}

window.addEventListener("DOMContentLoaded", helloTriangle);
