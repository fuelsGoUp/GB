#include "SceneLoader.h"
#include <fstream>
#include <filesystem>
#include "json.hpp"

using json = nlohmann::json;

bool loadScene(
    const std::string& filename,
    Scene& scene)
{
    namespace fs = std::filesystem;
    fs::path sceneFile = fs::path(__FILE__).parent_path() / filename;
    std::ifstream file(sceneFile);

    if (!file.is_open())
        return false;

    json j;
    file >> j;

    auto cam = j["camera"];

    scene.camera.position =
    {
        cam["position"][0],
        cam["position"][1],
        cam["position"][2]
    };

    scene.camera.yaw = cam["yaw"];
    scene.camera.pitch = cam["pitch"];

    scene.camera.fov = cam["fov"];
    scene.camera.nearPlane = cam["near"];
    scene.camera.farPlane = cam["far"];

    auto light = j["light"];

    scene.light.position =
    {
        light["position"][0],
        light["position"][1],
        light["position"][2]
    };

    scene.light.color =
    {
        light["color"][0],
        light["color"][1],
        light["color"][2]
    };

    for(auto& obj : j["objects"])
    {
        SceneObject o;

        o.model = obj["model"];

        o.position =
        {
            obj["position"][0],
            obj["position"][1],
            obj["position"][2]
        };

        o.rotation =
        {
            obj["rotation"][0],
            obj["rotation"][1],
            obj["rotation"][2]
        };

        o.scale =
        {
            obj["scale"][0],
            obj["scale"][1],
            obj["scale"][2]
        };

        if(obj.contains("animation"))
        {
            auto a = obj["animation"];

            o.animation.animated = true;

            o.animation.p0 =
            {
                a["p0"][0],
                a["p0"][1],
                a["p0"][2]
            };

            o.animation.p1 =
            {
                a["p1"][0],
                a["p1"][1],
                a["p1"][2]
            };

            o.animation.p2 =
            {
                a["p2"][0],
                a["p2"][1],
                a["p2"][2]
            };

            o.animation.p3 =
            {
                a["p3"][0],
                a["p3"][1],
                a["p3"][2]
            };
        }

        scene.objects.push_back(o);
    }

    return true;
}