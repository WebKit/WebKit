/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright 2025 Google LLC
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

import * as BABYLON from "@babylonjs/core";
import "@babylonjs/loaders";
import Ammo from "ammojs-typed";

export function runTest(frames = 10) {
  const classNames = Object.keys(BABYLON);
  BABYLON.Logger.LogLevels = BABYLON.Logger.NoneLogLevel;
  const engine = new BABYLON.NullEngine();
  const scene = createScene(engine);
  for (let i = 0; i < frames; i++) {
    scene.render();
  }
  return {
    classNames,
    cameraRotationLength: scene.cameras[0].rotation.length(),
  };
}

function createScene(engine) {
  const scene = new BABYLON.Scene(engine);
  scene.useConstantAnimationDeltaTime = true;
  const camera = new BABYLON.FreeCamera(
    "camera1",
    new BABYLON.Vector3(0, 5, -10),
    scene
  );
  camera.setTarget(BABYLON.Vector3.Zero());
  const light = new BABYLON.HemisphericLight(
    "light1",
    new BABYLON.Vector3(0, 1, 0),
    scene
  );
  const sphere = BABYLON.MeshBuilder.CreateSphere(
    "sphere1",
    { segments: 16, diameter: 2, sideOrientation: BABYLON.Mesh.FRONTSIDE },
    scene
  );
  sphere.position.y = 1;
  const ground = BABYLON.MeshBuilder.CreateGround(
    "ground1",
    { width: 6, height: 6, subdivisions: 2, updatable: false },
    scene
  );
  return scene;
}

export async function runComplexScene(
  fortData,
  cannonData,
  particleData,
  frames = 10
) {
  const classNames = Object.keys(BABYLON);
  BABYLON.Logger.LogLevels = BABYLON.Logger.NoneLogLevel;
  const engine = new BABYLON.NullEngine({
    deterministicLockstep: true,
  });
  const scene = await createComplexScene(
    engine,
    fortData,
    cannonData,
    particleData
  );
  for (let i = 0; i < frames; i++) {
    scene.animate();
    scene.render();
  }
  // Leak state to the outside.
  return {
    classNames,
    cameraRotationLength: scene.cameras[0].rotation.length(),
  };
}

async function createComplexScene(engine, fortData, cannonData, particleData) {
  // Source:
  // - https://www.babylonjs.com/featureDemos/
  // - https://playground.babylonjs.com/#C21DGD#2
  const scene = new BABYLON.Scene(engine);
  scene.useConstantAnimationDeltaTime = true;
  scene.clearColor = new BABYLON.Color3(0.31, 0.48, 0.64);
  let camera = new BABYLON.ArcRotateCamera(
    "camera",
    BABYLON.Tools.ToRadians(125),
    BABYLON.Tools.ToRadians(70),
    25,
    new BABYLON.Vector3(0, 3, 0),
    scene
  );
  camera.lowerRadiusLimit = 10;
  const ammo = await Ammo();
  scene.enablePhysics(
    new BABYLON.Vector3(0, -9.8, 0),
    new BABYLON.AmmoJSPlugin(true, ammo)
  );
  let cannonAnimationPairings = {};
  let cannonReadyToPlay = {};

  //create a cannonBall template to clone from, set it's visibility to off.
  let cannonBall = BABYLON.MeshBuilder.CreateSphere(
    "cannonBall",
    { diameter: 0.3 },
    scene
  );
  let cannonBallMat = new BABYLON.StandardMaterial("cannonBallMaterial", scene);
  cannonBallMat.diffuseColor = BABYLON.Color3.Black();
  cannonBallMat.specularPower = 256;
  cannonBall.material = cannonBallMat;
  cannonBall.visibility = false;

  //create a large box far underneath the tower, that will act as a trigger to destroy the cannonballs.
  let killBox = BABYLON.MeshBuilder.CreateBox(
    "killBox",
    { width: 400, depth: 400, height: 4 },
    scene
  );
  killBox.position = new BABYLON.Vector3(0, -50, 0);
  killBox.visibility = 0;

  const pirateFortImport = await BABYLON.ImportMeshAsync(fortData, scene, {
    pluginExtension: ".glb",
  });
  pirateFortImport.meshes[0].name = "pirateFort";
  scene.getMeshByName("sea").material.needDepthPrePass = true;
  scene.getLightByName("Sun").intensity = 12;

  //Load the cannon model and create clones
  const cannonImportResult = await BABYLON.ImportMeshAsync(cannonData, scene, {
    pluginExtension: ".glb",
  });
  //remove the top level root node
  let cannon = cannonImportResult.meshes[0].getChildren()[0];
  cannon.setParent(null);
  cannonImportResult.meshes[0].dispose();

  //set the metadata of each mesh to filter on later
  let cannonMeshes = cannon.getChildMeshes();
  for (let i = 0; i < cannonMeshes.length; i++) {
    cannonMeshes[i].metadata = "cannon";
  }

  const importedAnimGroups = cannonImportResult.animationGroups;

  //loop through all imported animation groups and copy the animation curve data to an array.
  let animations = [];
  for (let i = 0; i < importedAnimGroups.length; i++) {
    importedAnimGroups[i].stop();
    animations.push(importedAnimGroups[i].targetedAnimations[0].animation);
    importedAnimGroups[i].dispose();
  }

  //create a new animation group and add targeted animations based on copied curve data from the "animations" array.
  let cannonAnimGroup = new BABYLON.AnimationGroup("cannonAnimGroup");
  cannonAnimGroup.addTargetedAnimation(
    animations[0],
    cannon.getChildMeshes()[1]
  );
  cannonAnimGroup.addTargetedAnimation(
    animations[1],
    cannon.getChildMeshes()[0]
  );

  //create a box for particle emission, position it at the muzzle of the cannon, turn off visibility and parent it to the cannon mesh
  let particleEmitter = BABYLON.MeshBuilder.CreateBox(
    "particleEmitter",
    { size: 0.05 },
    scene
  );
  particleEmitter.position = new BABYLON.Vector3(0, 0.76, 1.05);
  particleEmitter.rotation.x = BABYLON.Tools.ToRadians(78.5);
  particleEmitter.isVisible = false;
  particleEmitter.setParent(cannon.getChildMeshes()[1]);

  //load particle system from the snippet server and set the emitter to the particleEmitter. Set its stopDuration.
  const smokeBlast = BABYLON.ParticleSystem.Parse(particleData, scene, "");
  smokeBlast.emitter = particleEmitter;
  smokeBlast.targetStopDuration = 0.2;

  //position and rotation data for the placement of the cannon clones
  let cannonPositionArray = [
    [
      new BABYLON.Vector3(0.97, 5.52, 1.79),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
    [
      new BABYLON.Vector3(1.08, 2.32, 3.05),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
    [
      new BABYLON.Vector3(1.46, 2.35, -0.73),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(90),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
    [
      new BABYLON.Vector3(1.45, 5.52, -1.66),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(90),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
    [
      new BABYLON.Vector3(1.49, 8.69, -0.35),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(90),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
    [
      new BABYLON.Vector3(-1.37, 8.69, -0.39),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(-90),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
    [
      new BABYLON.Vector3(0.58, 4, -2.18),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(180),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
    [
      new BABYLON.Vector3(1.22, 8.69, -2.5),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(180),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
    [
      new BABYLON.Vector3(-1.31, 2.33, -2.45),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(180),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
    [
      new BABYLON.Vector3(-3.54, 5.26, -2.12),
      new BABYLON.Vector3(
        BABYLON.Tools.ToRadians(0),
        BABYLON.Tools.ToRadians(-90),
        BABYLON.Tools.ToRadians(180)
      ),
    ],
  ];

  //create 10 cannon clones, each with unique position/rotation data. Note that particle systems are cloned with parent meshes
  //also create 10 new animation groups with targeted animations applied to the newly cloned meshes
  for (let i = 0; i < 10; i++) {
    let cannonClone = cannon.clone("cannonClone" + i);
    cannonClone.position = cannonPositionArray[i][0];
    cannonClone.rotation = cannonPositionArray[i][1];
    let cannonAnimGroupClone = new BABYLON.AnimationGroup(
      "cannonAnimGroupClone" + i
    );
    cannonAnimGroupClone.addTargetedAnimation(
      cannonAnimGroup.targetedAnimations[0].animation,
      cannonClone.getChildMeshes()[1]
    );
    cannonAnimGroupClone.addTargetedAnimation(
      cannonAnimGroup.targetedAnimations[1].animation,
      cannonClone.getChildMeshes()[0]
    );

    //store a key/value pair of each clone name and the name of the associated animation group name.
    cannonAnimationPairings[cannonClone.name] = cannonAnimGroupClone.name;
    //store key/value pair for the cannon name and it's readyToPlay status as 1;
    cannonReadyToPlay[cannonClone.name] = 1;
  }

  //dispose of the original cannon, animation group, and particle system
  cannon.dispose();
  cannonAnimGroup.dispose();
  smokeBlast.dispose();

  return scene;
}
