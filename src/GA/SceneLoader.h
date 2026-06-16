#ifndef SCENELOADER_H
#define SCENELOADER_H

#include <vector>
#include <string>
#include <glm/glm.hpp>

struct AnimationData
{
    bool animated = false;

    glm::vec3 p0;
    glm::vec3 p1;
    glm::vec3 p2;
    glm::vec3 p3;
};

struct SceneObject
{
    std::string model;

    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    AnimationData animation;
};

struct SceneCamera
{
    glm::vec3 position;

    float yaw;
    float pitch;

    float fov;
    float nearPlane;
    float farPlane;
};

struct SceneLight
{
    glm::vec3 position;
    glm::vec3 color;
};

struct Scene
{
    SceneCamera camera;
    SceneLight light;

    std::vector<SceneObject> objects;
};

bool loadScene(
    const std::string& filename,
    Scene& scene);

#endif