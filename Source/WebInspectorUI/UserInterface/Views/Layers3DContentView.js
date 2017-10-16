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

        this._layers = [];
        this._layerGroupsById = new Map;
        this._selectedLayerGroup = null;
        this._layersChangedWhileHidden = false;

        this._renderer = null;
        this._camera = null;
        this._controls = null;
        this._scene = null;
        this._raycaster = null;
        this._mouse = null;
        this._animationFrameRequestId = null;
    }

    // Public

    get supplementalRepresentedObjects()
    {
        return this._layers;
    }

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

    selectLayerById(layerId)
    {
        let layerGroup = this._layerGroupsById.get(layerId);
        this._updateLayerGroupSelection(layerGroup);
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

        this._scene = new THREE.Scene;
        this._scene.position.set(-this.element.offsetWidth / 2, this.element.offsetHeight / 2, 0);

        this._raycaster = new THREE.Raycaster;
        this._mouse = new THREE.Vector2;
        this._renderer.domElement.addEventListener("mousedown", this._canvasMouseDown.bind(this));

        this.element.appendChild(this._renderer.domElement);

        this._animate();
    }

    layout()
    {
        if (this.layoutReason === WI.View.LayoutReason.Resize)
            return;

        WI.domTreeManager.requestDocument((node) => {
            WI.layerTreeManager.layersForNode(node, (layers) => {
                this._updateLayers(layers);
                this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
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

    _updateLayers(newLayers)
    {
        // FIXME: This should be made into the basic usage of the manager, if not the agent itself.
        //        At that point, we can remove this duplication from the visualization and sidebar.
        let {removals, additions, preserved} = WI.layerTreeManager.layerTreeMutations(this._layers, newLayers);

        for (let layer of removals) {
            let layerGroup = this._layerGroupsById.get(layer.layerId);
            this._scene.remove(layerGroup);
            this._layerGroupsById.delete(layer.layerId);
        }

        if (this._selectedLayerGroup && !this._layerGroupsById.get(this._selectedLayerGroup.userData.layerId))
            this.selectedLayerGroup = null;

        additions.forEach(this._addLayerGroup, this);
        preserved.forEach(this._updateLayerGroupPosition, this);

        this._layers = newLayers;
    }

    _addLayerGroup(layer, index)
    {
        let layerGroup = new THREE.Group;
        layerGroup.userData.layerId = layer.layerId;
        layerGroup.add(this._createLayerMesh(layer.bounds));
        layerGroup.add(this._createLayerMesh(layer.compositedBounds, true));

        this._layerGroupsById.set(layer.layerId, layerGroup);
        this._updateLayerGroupPosition(layer, index);

        this._scene.add(layerGroup);
    }

    _updateLayerGroupPosition(layer, index) {
        let layerGroup = this._layerGroupsById.get(layer.layerId);
        console.assert(layerGroup);

        const zInterval = 25;
        layerGroup.position.set(layer.bounds.x, -layer.bounds.y, index * zInterval);
    }

    _createLayerMesh({width, height}, isOutline = false)
    {
        let geometry = new THREE.Geometry;
        geometry.vertices.push(new THREE.Vector3(0,     0,       0));
        geometry.vertices.push(new THREE.Vector3(width, 0,       0));
        geometry.vertices.push(new THREE.Vector3(width, -height, 0));
        geometry.vertices.push(new THREE.Vector3(0,     -height, 0));

        if (isOutline) {
            let material = new THREE.LineBasicMaterial({color: WI.Layers3DContentView._layerColor.stroke});
            return new THREE.LineLoop(geometry, material);
        }

        geometry.faces.push(new THREE.Face3(0, 1, 3));
        geometry.faces.push(new THREE.Face3(1, 2, 3));

        let material = new THREE.MeshBasicMaterial({
            color: WI.Layers3DContentView._layerColor.fill,
            transparent: true,
            opacity: 0.4,
            side: THREE.DoubleSide,
        });

        return new THREE.Mesh(geometry, material);
    }

    _canvasMouseDown(event)
    {
        this._mouse.x = (event.offsetX / event.target.width) * 2 - 1;
        this._mouse.y = -(event.offsetY / event.target.height) * 2 + 1;
        this._raycaster.setFromCamera(this._mouse, this._camera);

        const recursive = true;
        let intersects = this._raycaster.intersectObjects(this._scene.children, recursive);
        let selection = intersects.length ? intersects[0].object.parent : null;
        if (selection && selection === this._selectedLayerGroup) {
            if (!event.metaKey)
                return;

            selection = null;
        }

        this._updateLayerGroupSelection(selection);

        let layerId = selection ? selection.userData.layerId : null;
        this.dispatchEventToListeners(WI.Layers3DContentView.Event.SelectedLayerChanged, {layerId});
    }

    _updateLayerGroupSelection(layerGroup)
    {
        let setColor = ({fill, stroke}) => {
            let [plane, outline] = this._selectedLayerGroup.children;
            plane.material.color.set(fill);
            outline.material.color.set(stroke);
        };

        if (this._selectedLayerGroup)
            setColor(WI.Layers3DContentView._layerColor);

        this._selectedLayerGroup = layerGroup;

        if (this._selectedLayerGroup)
            setColor(WI.Layers3DContentView._selectedLayerColor);
    }
};

WI.Layers3DContentView._layerColor = {
    fill: "hsl(76, 49%, 75%)",
    stroke: "hsl(79, 45%, 50%)"
};

WI.Layers3DContentView._selectedLayerColor = {
    fill: "hsl(208, 66%, 79%)",
    stroke: "hsl(202, 57%, 68%)"
};

WI.Layers3DContentView.Event = {
    SelectedLayerChanged: "selected-layer-changed"
};
