// Copyright (c) Meta Platforms, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef ESP_GFX_SKINNEDDRAWABLE_H_
#define ESP_GFX_SKINNEDDRAWABLE_H_

#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Trade/SkinData.h>

#include "esp/gfx/Drawable.h"
#include "esp/gfx/ShaderManager.h"

namespace esp {
namespace gfx {

class SkinnedDrawable : public Drawable {
 public:
  //! Create a SkinnedDrawable for the given object using shader and mesh.
  //! Adds drawable to given group and uses provided texture, and
  //! color for textured buffer and color shader output respectively
  explicit SkinnedDrawable(
      scene::SceneNode& node,
      Mn::GL::Mesh* mesh,
      Drawable::Flags& meshAttributeFlags,
      ShaderManager& shaderManager,
      const Mn::ResourceKey& lightSetupKey,
      const Mn::ResourceKey& materialDataKey,
      DrawableGroup* group,
      std::shared_ptr<Mn::Trade::SkinData3D> skin,
      std::unordered_map<int, const scene::SceneNode*> jointNodeMap);

  void setLightSetup(const Magnum::ResourceKey& lightSetupKey) override;
  static constexpr const char* SHADER_KEY_TEMPLATE = "Phong-lights={}-flags={}";

 protected:
  void draw(const Magnum::Matrix4& transformationMatrix,
            Magnum::SceneGraph::Camera3D& camera) override;

  void updateShader();
  void updateShaderLightingParameters(
      const Magnum::Matrix4& transformationMatrix,
      Magnum::SceneGraph::Camera3D& camera);

  Magnum::ResourceKey getShaderKey(Magnum::UnsignedInt lightCount,
                                   Magnum::Shaders::PhongGL::Flags flags) const;

  // shader parameters
  ShaderManager& shaderManager_;
  Magnum::Resource<Magnum::GL::AbstractShaderProgram, Magnum::Shaders::PhongGL>
      shader_;
  Magnum::Resource<MaterialData, PhongMaterialData> materialData_;
  Magnum::Resource<LightSetup> lightSetup_;

  Magnum::Shaders::PhongGL::Flags flags_;

  std::shared_ptr<Mn::Trade::SkinData3D> skin_;
  std::unordered_map<int, const scene::SceneNode*> jointNodeMap_;
};

}  // namespace gfx
}  // namespace esp

#endif  // ESP_GFX_SKINNEDDRAWABLE_H_
