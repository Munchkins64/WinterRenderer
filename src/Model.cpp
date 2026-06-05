#include "Model.hpp"

#include "../external/tiny_gltf.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <stdexcept>
#include <iostream>
#include <cstring>

bool Model::loadFromFile(const std::string& filename,
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue) {
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->commandPool = commandPool;
    this->graphicsQueue = graphicsQueue;

    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool loaded = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filename);

    if (!warn.empty()) {
        std::cout << "GLTF Warning: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << "GLTF Error: " << err << std::endl;
        return false;
    }

    if (!loaded) {
        std::cerr << "Failed to load GLB file: " << filename << std::endl;
        return false;
    }

    minBounds = glm::vec3(std::numeric_limits<float>::max());
    maxBounds = glm::vec3(std::numeric_limits<float>::lowest());

    // Load textures
    for (const auto& image : gltfModel.images) {
        Texture texture{};
        texture.width = image.width;
        texture.height = image.height;

        createTextureImage(image.image.data(), image.width, image.height, texture);
        createTextureImageView(texture);
        createTextureSampler(texture);

        textures.push_back(texture);
    }

    // Create default texture if no textures exist
    if (textures.empty()) {
        Texture defaultTexture{};
        defaultTexture.width = 1;
        defaultTexture.height = 1;
        unsigned char white[] = { 255, 255, 255, 255 };
        createTextureImage(white, 1, 1, defaultTexture);
        createTextureImageView(defaultTexture);
        createTextureSampler(defaultTexture);
        textures.push_back(defaultTexture);
    }

    // Load materials
    for (const auto& mat : gltfModel.materials) {
        Material material{};
        material.baseColorFactor = glm::vec4(1.0f);
        material.baseColorTextureIndex = -1;
        material.metallicFactor = 1.0f;
        material.roughnessFactor = 1.0f;

        // Check for base color factor in pbrMetallicRoughness
        if (mat.pbrMetallicRoughness.baseColorFactor.size() == 4) {
            material.baseColorFactor = glm::vec4(
                static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[0]),
                static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[1]),
                static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[2]),
                static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[3])
            );
        }

        // Check for base color texture
        if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
            if (texIndex < static_cast<int>(gltfModel.textures.size())) {
                material.baseColorTextureIndex = gltfModel.textures[texIndex].source;
            }
        }

        material.metallicFactor = static_cast<float>(mat.pbrMetallicRoughness.metallicFactor);
        material.roughnessFactor = static_cast<float>(mat.pbrMetallicRoughness.roughnessFactor);

        materials.push_back(material);
    }

    // Default material
    if (materials.empty()) {
        Material defaultMat{};
        defaultMat.baseColorFactor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        defaultMat.baseColorTextureIndex = 0;
        materials.push_back(defaultMat);
    }

    // Load meshes
    for (const auto& mesh : gltfModel.meshes) {
        Mesh newMesh{};

        for (const auto& primitive : mesh.primitives) {
            Primitive newPrimitive{};
            newPrimitive.firstIndex = static_cast<uint32_t>(indices.size());
            newPrimitive.materialIndex = primitive.material >= 0 ? primitive.material : 0;

            uint32_t vertexStart = static_cast<uint32_t>(vertices.size());

            // Get accessors
            const float* positionBuffer = nullptr;
            const float* normalBuffer = nullptr;
            const float* texCoordBuffer = nullptr;
            const float* colorBuffer = nullptr;
            size_t vertexCount = 0;
            int colorComponentCount = 0;

            // Positions
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = gltfModel.accessors[posIt->second];
                const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
                positionBuffer = reinterpret_cast<const float*>(
                    &gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
                vertexCount = accessor.count;

                // Update bounds
                if (accessor.minValues.size() >= 3 && accessor.maxValues.size() >= 3) {
                    glm::vec3 minVal(
                        static_cast<float>(accessor.minValues[0]),
                        static_cast<float>(accessor.minValues[1]),
                        static_cast<float>(accessor.minValues[2])
                    );
                    glm::vec3 maxVal(
                        static_cast<float>(accessor.maxValues[0]),
                        static_cast<float>(accessor.maxValues[1]),
                        static_cast<float>(accessor.maxValues[2])
                    );
                    minBounds = glm::min(minBounds, minVal);
                    maxBounds = glm::max(maxBounds, maxVal);
                }
            }

            // Normals
            auto normIt = primitive.attributes.find("NORMAL");
            if (normIt != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = gltfModel.accessors[normIt->second];
                const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
                normalBuffer = reinterpret_cast<const float*>(
                    &gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
            }

            // Texture coordinates
            auto texIt = primitive.attributes.find("TEXCOORD_0");
            if (texIt != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = gltfModel.accessors[texIt->second];
                const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
                texCoordBuffer = reinterpret_cast<const float*>(
                    &gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
            }

            // Vertex colors
            auto colorIt = primitive.attributes.find("COLOR_0");
            if (colorIt != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = gltfModel.accessors[colorIt->second];
                const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
                colorBuffer = reinterpret_cast<const float*>(
                    &gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
                // Determine if it's VEC3 or VEC4
                colorComponentCount = (accessor.type == TINYGLTF_TYPE_VEC3) ? 3 : 4;
            }

            // Create vertices
            for (size_t i = 0; i < vertexCount; i++) {
                Vertex vertex{};

                vertex.position = glm::vec3(
                    positionBuffer[i * 3 + 0],
                    positionBuffer[i * 3 + 1],
                    positionBuffer[i * 3 + 2]);

                if (normalBuffer) {
                    vertex.normal = glm::vec3(
                        normalBuffer[i * 3 + 0],
                        normalBuffer[i * 3 + 1],
                        normalBuffer[i * 3 + 2]);
                }
                else {
                    vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                if (texCoordBuffer) {
                    vertex.texCoord = glm::vec2(
                        texCoordBuffer[i * 2 + 0],
                        texCoordBuffer[i * 2 + 1]);
                }
                else {
                    vertex.texCoord = glm::vec2(0.0f);
                }

                if (colorBuffer) {
                    if (colorComponentCount == 4) {
                        vertex.color = glm::vec4(
                            colorBuffer[i * 4 + 0],
                            colorBuffer[i * 4 + 1],
                            colorBuffer[i * 4 + 2],
                            colorBuffer[i * 4 + 3]);
                    }
                    else {
                        vertex.color = glm::vec4(
                            colorBuffer[i * 3 + 0],
                            colorBuffer[i * 3 + 1],
                            colorBuffer[i * 3 + 2],
                            1.0f);
                    }
                }
                else {
                    vertex.color = glm::vec4(1.0f);
                }

                vertices.push_back(vertex);
            }

            // Indices
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

                newPrimitive.indexCount = static_cast<uint32_t>(accessor.count);

                switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                    const uint32_t* buf = reinterpret_cast<const uint32_t*>(
                        &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                    for (size_t i = 0; i < accessor.count; i++) {
                        indices.push_back(buf[i] + vertexStart);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                    const uint16_t* buf = reinterpret_cast<const uint16_t*>(
                        &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                    for (size_t i = 0; i < accessor.count; i++) {
                        indices.push_back(buf[i] + vertexStart);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                    const uint8_t* buf = reinterpret_cast<const uint8_t*>(
                        &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                    for (size_t i = 0; i < accessor.count; i++) {
                        indices.push_back(buf[i] + vertexStart);
                    }
                    break;
                }
                }
            }

            newMesh.primitives.push_back(newPrimitive);
        }

        meshes.push_back(newMesh);
    }

    // Load nodes
    for (const auto& node : gltfModel.nodes) {
        Node newNode{};
        newNode.meshIndex = node.mesh;

        for (int child : node.children) {
            newNode.children.push_back(child);
        }

        if (node.matrix.size() == 16) {
            // Copy matrix data to a local array first
            float matData[16];
            for (int i = 0; i < 16; i++) {
                matData[i] = static_cast<float>(node.matrix[i]);
            }
            newNode.matrix = glm::make_mat4(matData);
        }
        else {
            glm::vec3 translation(0.0f);
            glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // w, x, y, z
            glm::vec3 scale(1.0f);

            if (node.translation.size() == 3) {
                translation = glm::vec3(
                    static_cast<float>(node.translation[0]),
                    static_cast<float>(node.translation[1]),
                    static_cast<float>(node.translation[2])
                );
            }
            if (node.rotation.size() == 4) {
                // glTF quaternion is (x, y, z, w), glm::quat constructor is (w, x, y, z)
                rotation = glm::quat(
                    static_cast<float>(node.rotation[3]),  // w
                    static_cast<float>(node.rotation[0]),  // x
                    static_cast<float>(node.rotation[1]),  // y
                    static_cast<float>(node.rotation[2])   // z
                );
            }
            if (node.scale.size() == 3) {
                scale = glm::vec3(
                    static_cast<float>(node.scale[0]),
                    static_cast<float>(node.scale[1]),
                    static_cast<float>(node.scale[2])
                );
            }

            glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
            glm::mat4 R = glm::toMat4(rotation);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
            newNode.matrix = T * R * S;
        }

        nodes.push_back(newNode);
    }

    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(vertexBufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer, vertexBufferMemory);

    copyBuffer(stagingBuffer, vertexBuffer, vertexBufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

    createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    vkMapMemory(device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), static_cast<size_t>(indexBufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer, indexBufferMemory);

    copyBuffer(stagingBuffer, indexBuffer, indexBufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    std::cout << "Model loaded: " << vertices.size() << " vertices, "
        << indices.size() << " indices, "
        << textures.size() << " textures" << std::endl;

    return true;
}

void Model::cleanup(VkDevice device) {
    for (auto& texture : textures) {
        vkDestroySampler(device, texture.sampler, nullptr);
        vkDestroyImageView(device, texture.imageView, nullptr);
        vkDestroyImage(device, texture.image, nullptr);
        vkFreeMemory(device, texture.imageMemory, nullptr);
    }

    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
}

void Model::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer,
    VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void Model::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

uint32_t Model::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

VkCommandBuffer Model::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Model::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Model::createTextureImage(const unsigned char* pixels, uint32_t width,
    uint32_t height, Texture& texture) {
    VkDeviceSize imageSize = width * height * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &texture.image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, texture.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &texture.imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(device, texture.image, texture.imageMemory, 0);

    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImage(stagingBuffer, texture.image, width, height);

    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Model::transitionImageLayout(VkImage image, VkFormat format,
    VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::runtime_error("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

void Model::copyBufferToImage(VkBuffer buffer, VkImage image,
    uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void Model::createTextureImageView(Texture& texture) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &texture.imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view!");
    }
}

void Model::createTextureSampler(Texture& texture) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }
}