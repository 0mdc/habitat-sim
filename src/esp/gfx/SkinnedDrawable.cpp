// Copyright (c) Meta Platforms, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "SkinnedDrawable.h"

#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>

#include <Corrade/Containers/Pair.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <memory>
#include <string>
#include "Magnum/Magnum.h"
#include "Magnum/Math/Tags.h"
#include "Magnum/Shaders/PhongGL.h"
#include "Magnum/Trade/Trade.h"
#include "esp/core/Logging.h"
#include "esp/gfx/Drawable.h"
#include "esp/gfx/SkinData.h"
#include "esp/scene/SceneNode.h"

namespace Mn = Magnum;

namespace esp {
namespace gfx {

SkinnedDrawable::SkinnedDrawable(scene::SceneNode& node,
                                 Mn::GL::Mesh* mesh,
                                 Drawable::Flags& meshAttributeFlags,
                                 ShaderManager& shaderManager,
                                 const Mn::ResourceKey& lightSetupKey,
                                 const Mn::ResourceKey& materialDataKey,
                                 DrawableGroup* group,
                                 SkinData skinData)
    : Drawable{node, mesh, DrawableType::Generic, group},
      shaderManager_{shaderManager},
      lightSetup_{shaderManager.get<LightSetup>(lightSetupKey)},
      materialData_{
          shaderManager.get<MaterialData, PhongMaterialData>(materialDataKey)},
      skinData_(skinData) {
  flags_ = Mn::Shaders::PhongGL::Flag::ObjectId;

  /* If texture transformation is specified, enable it only if the material is
     actually textured -- it's an error otherwise */
  if (materialData_->textureMatrix != Mn::Matrix3{} &&
      (materialData_->ambientTexture || materialData_->diffuseTexture ||
       materialData_->specularTexture || materialData_->specularTexture ||
       materialData_->textureObjectId)) {
    flags_ |= Mn::Shaders::PhongGL::Flag::TextureTransformation;
  }
  if (materialData_->ambientTexture) {
    flags_ |= Mn::Shaders::PhongGL::Flag::AmbientTexture;
  }
  if (materialData_->diffuseTexture) {
    flags_ |= Mn::Shaders::PhongGL::Flag::DiffuseTexture;
  }
  if (materialData_->specularTexture) {
    flags_ |= Mn::Shaders::PhongGL::Flag::SpecularTexture;
  }

  if (materialData_->normalTexture) {
    if (meshAttributeFlags & Drawable::Flag::HasTangent) {
      flags_ |= Mn::Shaders::PhongGL::Flag::NormalTexture;
      if (meshAttributeFlags & Drawable::Flag::HasSeparateBitangent) {
        flags_ |= Mn::Shaders::PhongGL::Flag::Bitangent;
      }
    } else {
      ESP_WARNING() << "Mesh does not have tangents and Magnum cannot generate "
                       "them yet, ignoring a normal map";
    }
  }
  if (materialData_->perVertexObjectId) {
    flags_ |= Mn::Shaders::PhongGL::Flag::InstancedObjectId;
  }
  if (materialData_->textureObjectId) {
    flags_ |= Mn::Shaders::PhongGL::Flag::ObjectIdTexture;
  }
  if (meshAttributeFlags & Drawable::Flag::HasVertexColor) {
    flags_ |= Mn::Shaders::PhongGL::Flag::VertexColor;
  }

  // update the shader early here to to avoid doing it during the render loop
  if (glMeshExists()) {
    updateShader();
  }
}

void SkinnedDrawable::setLightSetup(const Mn::ResourceKey& resourceKey) {
  lightSetup_ = shaderManager_.get<LightSetup>(resourceKey);

  // update the shader early here to to avoid doing it during the render loop
  updateShader();
}

// TODO: Share that code with GenericDrawable
void SkinnedDrawable::updateShaderLightingParameters(
    const Mn::Matrix4& transformationMatrix,
    Mn::SceneGraph::Camera3D& camera) {
  const Mn::Matrix4 cameraMatrix = camera.cameraMatrix();

  std::vector<Mn::Vector4> lightPositions;
  lightPositions.reserve(lightSetup_->size());
  std::vector<Mn::Color3> lightColors;
  lightColors.reserve(lightSetup_->size());
  std::vector<Mn::Color3> lightSpecularColors;
  lightSpecularColors.reserve(lightSetup_->size());
  constexpr float dummyRange = Mn::Constants::inf();
  std::vector<float> lightRanges(lightSetup_->size(), dummyRange);
  const Mn::Color4 ambientLightColor = getAmbientLightColor(*lightSetup_);

  for (Mn::UnsignedInt i = 0; i < lightSetup_->size(); ++i) {
    const auto& lightInfo = (*lightSetup_)[i];
    lightPositions.emplace_back(getLightPositionRelativeToCamera(
        lightInfo, transformationMatrix, cameraMatrix));

    const auto& lightColor = (*lightSetup_)[i].color;
    lightColors.emplace_back(lightColor);

    // In general, a light's specular color should match its base color.
    // However, negative lights have zero (black) specular.
    constexpr Mn::Color3 blackColor(0.0, 0.0, 0.0);
    bool isNegativeLight = lightColor.x() < 0;
    lightSpecularColors.emplace_back(isNegativeLight ? blackColor : lightColor);
  }

  // See documentation in src/deps/magnum/src/Magnum/Shaders/Phong.h
  (*shader_)
      .setAmbientColor(materialData_->ambientColor * ambientLightColor)
      .setDiffuseColor(materialData_->diffuseColor)
      .setSpecularColor(materialData_->specularColor)
      .setShininess(materialData_->shininess)
      .setLightPositions(lightPositions)
      .setLightColors(lightColors)
      .setLightRanges(lightRanges);
}

void SkinnedDrawable::draw(const Mn::Matrix4& transformationMatrix,
                           Mn::SceneGraph::Camera3D& camera) {
  CORRADE_ASSERT(glMeshExists(),
                 "SkinnedDrawable::draw() : GL mesh doesn't exist", );

  updateShader();
  updateShaderLightingParameters(transformationMatrix, camera);

  (*shader_)
      // e.g., semantic mesh has its own per vertex annotation, which has been
      // uploaded to GPU so simply pass 0 to the uniform "objectId" in the
      // fragment shader
      .setObjectId(
          static_cast<RenderCamera&>(camera).useDrawableIds() ? drawableId_
          : (materialData_->perVertexObjectId || materialData_->textureObjectId)
              ? 0
              : node_.getSemanticId())
      .setTransformationMatrix(transformationMatrix)
      .setProjectionMatrix(camera.projectionMatrix())
      .setNormalMatrix(transformationMatrix.normalMatrix());

  if ((flags_ & Mn::Shaders::PhongGL::Flag::TextureTransformation) &&
      materialData_->textureMatrix != Mn::Matrix3{}) {
    shader_->setTextureMatrix(materialData_->textureMatrix);
  }

  if (flags_ & Mn::Shaders::PhongGL::Flag::AmbientTexture) {
    shader_->bindAmbientTexture(*(materialData_->ambientTexture));
  }
  if (flags_ & Mn::Shaders::PhongGL::Flag::DiffuseTexture) {
    shader_->bindDiffuseTexture(*(materialData_->diffuseTexture));
  }
  if (flags_ & Mn::Shaders::PhongGL::Flag::SpecularTexture) {
    shader_->bindSpecularTexture(*(materialData_->specularTexture));
  }
  if (flags_ & Mn::Shaders::PhongGL::Flag::NormalTexture) {
    shader_->bindNormalTexture(*(materialData_->normalTexture));
  }
  if (flags_ >= Mn::Shaders::PhongGL::Flag::ObjectIdTexture) {
    shader_->bindObjectIdTexture(*(materialData_->objectIdTexture));
  }

  // Gather joint transformations
  auto& skin = skinData_.skin;
  auto& jointIdToArticulatedObjectNodes =
      skinData_.jointIdToArticulatedObjectNode;
  auto& scaledNodes = skinData_.jointIdToScaledNode;
  Cr::Containers::Array<Mn::Matrix4> jointTransformations{
      Cr::NoInit, skin->joints().size()};

  // Undo root node transform
  auto invRootTransform = jointIdToArticulatedObjectNodes[skinData_.rootJointId]
                              ->absoluteTransformationMatrix()
                              .inverted();

  Mn::Matrix4 lastTransform = Mn::Matrix4{Magnum::Math::IdentityInit};
  for (std::size_t i = 0; i != jointTransformations.size(); ++i) {
    auto jointNodeIt = scaledNodes.find(skin->joints()[i]);
    if (jointNodeIt != scaledNodes.end()) {
      jointTransformations[i] =
          invRootTransform *
          jointNodeIt->second->absoluteTransformationMatrix() *
          skin->inverseBindMatrices()[i];
      lastTransform = jointTransformations[i];
    } else {
      jointTransformations[i] = lastTransform;
    }
  }
  shader_->setJointMatrices(jointTransformations);

  shader_->draw(getMesh());
}

void SkinnedDrawable::updateShader() {
  Mn::UnsignedInt lightCount = lightSetup_->size();
  Mn::UnsignedInt jointCount = skinData_.skin->joints().size();
  Mn::UnsignedInt perVertexJointCount = skinData_.perVertexJointCount;

  if (!shader_ || shader_->lightCount() != lightCount ||
      shader_->flags() != flags_) {
    // if the number of lights or flags have changed, we need to fetch a
    // compatible shader
    shader_ =
        shaderManager_.get<Mn::GL::AbstractShaderProgram, Mn::Shaders::PhongGL>(
            getShaderKey(lightCount, flags_));

    // if no shader with desired number of lights and flags exists, create one
    if (!shader_) {
      shaderManager_.set<Mn::GL::AbstractShaderProgram>(
          shader_.key(),
          new Mn::Shaders::PhongGL{
              Mn::Shaders::PhongGL::Configuration{}
                  .setFlags(flags_)
                  .setLightCount(lightCount)
                  .setJointCount(jointCount, perVertexJointCount)},
          Mn::ResourceDataState::Final, Mn::ResourcePolicy::ReferenceCounted);
    }

    CORRADE_INTERNAL_ASSERT(shader_ && shader_->lightCount() == lightCount &&
                            shader_->flags() == flags_);
  }
}

Mn::ResourceKey SkinnedDrawable::getShaderKey(
    Mn::UnsignedInt lightCount,
    Mn::Shaders::PhongGL::Flags flags) const {
  return Corrade::Utility::formatString(
      SHADER_KEY_TEMPLATE, lightCount,
      static_cast<Mn::Shaders::PhongGL::Flags::UnderlyingType>(flags));
}

}  // namespace gfx
}  // namespace esp
