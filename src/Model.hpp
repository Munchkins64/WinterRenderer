#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include "Vertex.hpp"

struct Texture {
    VkImage image;
    VkDeviceMemory imageMemory;
    VkImageView imageView;
    VkSampler sampler;
    uint32_t width;
    uint32_t height;
};

struct Primitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t materialIndex;
};

struct Mesh {
    std::vector<Primitive> primitives;
};

struct Material {
    glm::vec4 baseColorFactor;
    int32_t baseColorTextureIndex;
    float metallicFactor;
    float roughnessFactor;
};

struct Node {
    std::vector<int32_t> children;
    int32_t meshIndex;
    glm::mat4 matrix;
};

class Model {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    glm::vec3 minBounds;
    glm::vec3 maxBounds;

    Model() = default;
    ~Model() = default;

    bool loadFromFile(const std::string& filename,
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkCommandPool commandPool,
        VkQueue graphicsQueue);

    void cleanup(VkDevice device);

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, VkBuffer& buffer,
        VkDeviceMemory& bufferMemory);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createTextureImage(const unsigned char* pixels, uint32_t width,
        uint32_t height, Texture& texture);

    void createTextureImageView(Texture& texture);
    void createTextureSampler(Texture& texture);

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    void transitionImageLayout(VkImage image, VkFormat format,
        VkImageLayout oldLayout, VkImageLayout newLayout);

    void copyBufferToImage(VkBuffer buffer, VkImage image,
        uint32_t width, uint32_t height);
};