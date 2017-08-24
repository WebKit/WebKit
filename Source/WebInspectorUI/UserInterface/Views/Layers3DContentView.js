/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.Layers3DContentView = class Layers3DContentView extends WI.ContentView
{
    constructor()
    {
        super();

        this.element.classList.add("layers-3d");

        WI.layerTreeManager.addEventListener(WI.LayerTreeManager.Event.LayerTreeDidChange, this._layerTreeDidChange, this);

        this._layersChangedWhileHidden = false;
        this._renderer = null;
        this._camera = null;
        this._controls = null;
        this._boundsGroup = null;
        this._compositedBoundsGroup = null;
        this._scene = null;
        this._animationFrameRequestId = null;
    }

    // Public

    shown()
    {
        super.shown();

        if (this._layersChangedWhileHidden) {
            this._layersChangedWhileHidden = false;

            this.updateLayout();
        }

        if (this.didInitialLayout)
            this._animate();
    }

    hidden()
    {
        this._stopAnimation();

        super.hidden();
    }

    closed()
    {
        WI.layerTreeManager.removeEventListener(WI.LayerTreeManager.Event.LayerTreeDidChange, this._layerTreeDidChange, this);

        super.closed();
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this._renderer = new THREE.WebGLRenderer({antialias: true});
        this._renderer.setClearColor("white");
        this._renderer.setSize(this.element.offsetWidth, this.element.offsetHeight);

        this._camera = new THREE.PerspectiveCamera(45, this.element.offsetWidth / this.element.offsetHeight, 1, 100000);
        this._camera.position.set(0, 0, 4000);
        this._camera.lookAt(new THREE.Vector3(0, 0, 0));

        this._controls = new THREE.OrbitControls(this._camera, this._renderer.domElement);
        this._controls.enableDamping = true;
        this._controls.enableZoom = false;
        this._controls.enablePan = false;
        this._controls.minAzimuthAngle = -Math.PI / 2;
        this._controls.maxAzimuthAngle = Math.PI / 2;

        this._boundsGroup = new THREE.Group();
        this._compositedBoundsGroup = new THREE.Group();

        this._scene = new THREE.Scene();
        this._scene.position.set(-this.element.offsetWidth / 2, this.element.offsetHeight / 2, 0);
        this._scene.add(this._boundsGroup);
        this._scene.add(this._compositedBoundsGroup);

        this.element.appendChild(this._renderer.domElement);

        this._animate();
    }

    layout()
    {
        if (this.layoutReason === WI.View.LayoutReason.Resize)
            return;

        WI.domTreeManager.requestDocument((node) => {
            WI.layerTreeManager.layersForNode(node, (layerForNode, childLayers) => {
                this._clearLayers();
                for (let i = 0; i < childLayers.length; i++)
                    this._addLayer(childLayers[i], i);
            });
        });
    }

    sizeDidChange()
    {
        super.sizeDidChange();

        this._stopAnimation();
        this._camera.aspect = this.element.offsetWidth / this.element.offsetHeight;
        this._camera.updateProjectionMatrix();
        this._renderer.setSize(this.element.offsetWidth, this.element.offsetHeight);
        this._animate();
    }

    // Private

    _layerTreeDidChange(event)
    {
        if (!this.visible) {
            this._layersChangedWhileHidden = true;
            return;
        }

        this.needsLayout();
    }

    _animate()
    {
        this._controls.update();
        this._renderer.render(this._scene, this._camera);
        this._animationFrameRequestId = requestAnimationFrame(() => { this._animate(); });
    }

    _stopAnimation()
    {
        cancelAnimationFrame(this._animationFrameRequestId);
        this._animationFrameRequestId = null;
    }

    _clearLayers()
    {
        this._boundsGroup.children.length = 0;
        this._compositedBoundsGroup.children.length = 0;
    }

    _addLayer(layer, index)
    {
        let compositedBounds = {
            x: layer.bounds.x,
            y: layer.bounds.y,
            width: layer.compositedBounds.width,
            height: layer.compositedBounds.height,
        };

        this._boundsGroup.add(this._createLayerMesh(layer.bounds, index));
        this._compositedBoundsGroup.add(this._createLayerMesh(compositedBounds, index, true));
    }

    _createLayerMesh(rect, index, isOutline = false)
    {
        const zInterval = 25;

        let geometry = new THREE.Geometry();
        geometry.vertices.push(new THREE.Vector3(rect.x,              -rect.y,               index * zInterval));
        geometry.vertices.push(new THREE.Vector3(rect.x + rect.width, -rect.y,               index * zInterval));
        geometry.vertices.push(new THREE.Vector3(rect.x + rect.width, -rect.y - rect.height, index * zInterval));
        geometry.vertices.push(new THREE.Vector3(rect.x,              -rect.y - rect.height, index * zInterval));

        if (isOutline) {
            let material = new THREE.LineBasicMaterial({color: "hsl(79, 45%, 50%)"});
            return new THREE.LineLoop(geometry, material);
        }

        geometry.faces.push(new THREE.Face3(0, 1, 3));
        geometry.faces.push(new THREE.Face3(1, 2, 3));

        let material = new THREE.MeshBasicMaterial({
            color: "hsl(76, 49%, 75%)",
            transparent: true,
            opacity: 0.4,
            side: THREE.DoubleSide,
        });

        return new THREE.Mesh(geometry, material);
    }
};
