#pragma once

//TODO
#include <Magnum/GL/Texture.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Trade/SkinData.h>
#include <esp/scene/SceneNode.h>
#include <memory>
#include <unordered_map>

#include "esp/core/Esp.h"

namespace esp
{
namespace gfx
{
struct SkinData {
    /** @brief Pointer to loaded skin data for the instance. */
    std::shared_ptr<Magnum::Trade::SkinData3D> skin{};
    /** @brief Map between skin joint IDs and articulated object nodes. */
    std::unordered_map<int, const scene::SceneNode*> jointIdToArticulatedObjectNode{};
    /** @brief Map between skin joint IDs and scaled articulated object nodes. */
    std::unordered_map<int, const scene::SceneNode*> jointIdToScaledNode{};
    /** @brief Skin joint ID of the root node. */
    int rootJointId{ID_UNDEFINED};
    /** @brief TODO */
    int perVertexJointCount{4};

    std::unordered_map<int, Magnum::Matrix4> localTransforms;
    
};
}
}