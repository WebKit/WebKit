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

        this._compositingBordersButtonNavigationItem = new WI.ActivateButtonNavigationItem("layer-borders", WI.UIString("Show compositing borders"), WI.UIString("Hide compositing borders"), "Images/LayerBorders.svg", 13, 13);
        this._compositingBordersButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleCompositingBorders, this);
        this._compositingBordersButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;

        WI.showPaintRectsSetting.addEventListener(WI.Setting.Event.Changed, this._showPaintRectsSettingChanged, this);
        this._paintFlashingButtonNavigationItem = new WI.ActivateButtonNavigationItem("paint-flashing", WI.UIString("Enable paint flashing"), WI.UIString("Disable paint flashing"), "Images/Paint.svg", 16, 16);
        this._paintFlashingButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._togglePaintFlashing, this);
        this._paintFlashingButtonNavigationItem.enabled = !!PageAgent.setShowPaintRects;
        this._paintFlashingButtonNavigationItem.activated = PageAgent.setShowPaintRects && WI.showPaintRectsSetting.value;
        this._paintFlashingButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;

        this._layers = [];
        this._layerGroupsById = new Map;
        this._selectedLayerGroup = null;

        this._renderer = null;
        this._camera = null;
        this._controls = null;
        this._scene = null;
        this._boundingBox = null;
        this._raycaster = null;
        this._mouse = null;
        this._animationFrameRequestId = null;
        this._documentNode = null;
    }

    // Public

    get navigationItems()
    {
        return [this._compositingBordersButtonNavigationItem, this._paintFlashingButtonNavigationItem];
    }

    get supplementalRepresentedObjects()
    {
        return this._layers;
    }

    shown()
    {
        super.shown();

        this._updateCompositingBordersButtonState();

        this.updateLayout();
        WI.layerTreeManager.addEventListener(WI.LayerTreeManager.Event.LayerTreeDidChange, this._layerTreeDidChange, this);
        
        if (this.didInitialLayout)
            this._animate();
    }

    hidden()
    {
        WI.layerTreeManager.removeEventListener(WI.LayerTreeManager.Event.LayerTreeDidChange, this._layerTreeDidChange, this);
        this._stopAnimation();

        super.hidden();
    }

    closed()
    {
        WI.showPaintRectsSetting.removeEventListener(WI.Setting.Event.Changed, this._showPaintRectsSettingChanged, this);

        super.closed();
    }

    selectLayerById(layerId)
    {
        let layerGroup = this._layerGroupsById.get(layerId);
        this._updateLayerGroupSelection(layerGroup);
        this._centerOnSelection();
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this._renderer = new THREE.WebGLRenderer({antialias: true});
        this._renderer.setClearColor("white");
        this._renderer.setSize(this.element.offsetWidth, this.element.offsetHeight);

        this._camera = new THREE.PerspectiveCamera(45, this.element.offsetWidth / this.element.offsetHeight, 1, 100000);

        this._controls = new THREE.OrbitControls(this._camera, this._renderer.domElement);
        this._controls.enableDamping = true;
        this._controls.enableKeys = false;
        this._controls.zoomSpeed = 0.5;
        this._controls.minDistance = 1000;
        this._controls.rotateSpeed = 0.5;
        this._controls.minAzimuthAngle = -Math.PI / 2;
        this._controls.maxAzimuthAngle = Math.PI / 2;
        this._renderer.domElement.addEventListener("contextmenu", (event) => { event.stopPropagation(); });

        this._scene = new THREE.Scene;
        this._boundingBox = new THREE.Box3;

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
            let documentWasUpdated = this._updateDocument(node);

            WI.layerTreeManager.layersForNode(node, (layers) => {
                this._updateLayers(layers);
                if (documentWasUpdated)
                    this._resetCamera();
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
        this.needsLayout();
    }

    _animate()
    {
        this._controls.update();
        this._restrictPan();
        this._renderer.render(this._scene, this._camera);
        this._animationFrameRequestId = requestAnimationFrame(() => { this._animate(); });
    }

    _stopAnimation()
    {
        cancelAnimationFrame(this._animationFrameRequestId);
        this._animationFrameRequestId = null;
    }

    _updateDocument(documentNode)
    {
        if (documentNode === this._documentNode)
            return false;

        this._scene.children.length = 0;
        this._layerGroupsById.clear();
        this._layers.length = 0;

        this._documentNode = documentNode;

        return true;
    }

    _updateLayers(newLayers)
    {
        // FIXME: This should be made into the basic usage of the manager, if not the agent itself.
        //        At that point, we can remove this duplication from the visualization and sidebar.
        let {removals, additions} = WI.layerTreeManager.layerTreeMutations(this._layers, newLayers);

        for (let layer of removals) {
            let layerGroup = this._layerGroupsById.get(layer.layerId);
            this._scene.remove(layerGroup);
            this._layerGroupsById.delete(layer.layerId);
        }

        if (this._selectedLayerGroup && !this._layerGroupsById.get(this._selectedLayerGroup.userData.layer.layerId))
            this.selectedLayerGroup = null;

        for (let layer of additions) {
            let layerGroup = this._createLayerGroup(layer);
            this._layerGroupsById.set(layer.layerId, layerGroup);
            this._scene.add(layerGroup);
        }

        // FIXME: Update the backend to provide a literal "layer tree" so we can decide z-indices less naively.
        const zInterval = 25;
        newLayers.forEach((layer, index) => {
            let layerGroup = this._layerGroupsById.get(layer.layerId);
            layerGroup.position.set(layer.bounds.x, -layer.bounds.y, index * zInterval);
        });

        this._boundingBox.setFromObject(this._scene);
        this._controls.maxDistance = this._boundingBox.max.z + WI.Layers3DContentView._zPadding;

        this._layers = newLayers;
        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _createLayerGroup(layer) {
        let layerGroup = new THREE.Group;
        layerGroup.userData.layer = layer;
        layerGroup.add(this._createLayerMesh(layer.bounds), this._createLayerMesh(layer.compositedBounds, true));
        return layerGroup;
    }

    _createLayerMesh({width, height}, isOutline = false)
    {
        let geometry = new THREE.Geometry;
        geometry.vertices.push(
            new THREE.Vector3(0,     0,       0),
            new THREE.Vector3(0,     -height, 0),
            new THREE.Vector3(width, -height, 0),
            new THREE.Vector3(width, 0,       0),
        );

        if (isOutline) {
            let material = new THREE.LineBasicMaterial({color: WI.Layers3DContentView._layerColor.stroke});
            return new THREE.LineLoop(geometry, material);
        }

        geometry.faces.push(new THREE.Face3(0, 1, 3), new THREE.Face3(1, 2, 3));

        let material = new THREE.MeshBasicMaterial({
            color: WI.Layers3DContentView._layerColor.fill,
            transparent: true,
            opacity: 0.4,
            side: THREE.DoubleSide,
            depthWrite: false,
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

        let layerId = selection ? selection.userData.layer.layerId : null;
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

    _centerOnSelection()
    {
        if (!this._selectedLayerGroup)
            return;

        let {x, y, width, height} = this._selectedLayerGroup.userData.layer.bounds;
        this._controls.target.set(x + (width / 2), -y - (height / 2), 0);
        this._camera.position.set(x + (width / 2), -y - (height / 2), this._selectedLayerGroup.position.z + WI.Layers3DContentView._zPadding / 2);
    }

    _resetCamera()
    {
        let {x, y, width, height} = this._layers[0].bounds;
        this._controls.target.set(x + (width / 2), -y - (height / 2), 0);
        this._camera.position.set(x + (width / 2), -y - (height / 2), this._controls.maxDistance - WI.Layers3DContentView._zPadding / 2);
    }

    _restrictPan()
    {
        let delta = this._boundingBox.clampPoint(this._controls.target).setZ(0).sub(this._controls.target);
        this._controls.target.add(delta);
        this._camera.position.add(delta);
    }

    _showPaintRectsSettingChanged(event)
    {
        console.assert(PageAgent.setShowPaintRects);

        this._paintFlashingButtonNavigationItem.activated = WI.showPaintRectsSetting.value;
        PageAgent.setShowPaintRects(this._paintFlashingButtonNavigationItem.activated);
    }

    _togglePaintFlashing(event)
    {
        WI.showPaintRectsSetting.value = !WI.showPaintRectsSetting.value;
    }

    _updateCompositingBordersButtonState()
    {
        // This value can be changed outside of Web Inspector.
        // FIXME: Have PageAgent dispatch a change event instead?
        PageAgent.getCompositingBordersVisible((error, compositingBordersVisible) => {
            this._compositingBordersButtonNavigationItem.activated = error ? false : compositingBordersVisible;
            this._compositingBordersButtonNavigationItem.enabled = error !== "unsupported";
        });
    }

    _toggleCompositingBorders(event)
    {
        this._compositingBordersButtonNavigationItem.activated = !this._compositingBordersButtonNavigationItem.activated;
        PageAgent.setCompositingBordersVisible(this._compositingBordersButtonNavigationItem.activated);
    }
};

WI.Layers3DContentView._zPadding = 3000;

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
