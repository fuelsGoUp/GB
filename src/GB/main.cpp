#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <map>
#include <filesystem>
#include <cfloat>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "SceneLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"  

using namespace std;

// ---------- STRUCTS ----------
struct MeshData {
    float* vertices;
    int vertexCount;
};

struct Objeto {
    GLuint VAO;
    int vertexCount;

    glm::vec3 pos;
    glm::vec3 rot;
    glm::vec3 scale;

    bool animated = false;
    glm::vec3 bezierP0;
    glm::vec3 bezierP1;
    glm::vec3 bezierP2;
    glm::vec3 bezierP3;

    // material
    glm::vec3 ka;
    glm::vec3 kd;
    glm::vec3 ks;
    float shininess;
};

// ---------- GLOBAL ----------
std::vector<Objeto> objetos; // Vetor global para armazenar os objetos da cena
int totalObjetos = 0;
int selecionado = 0;

Scene scene;

std::map<std::string, std::pair<GLuint,int>> meshCache;
int windowWidth = 800;
int windowHeight = 600;

GLuint loadTexture(const std::string& filePath) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    namespace fs = std::filesystem;
    fs::path texFile = fs::path(__FILE__).parent_path() / filePath;

    int width, height, nrChannels;
    unsigned char* data = stbi_load(texFile.string().c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        if (nrChannels == 3) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Textura não encontrada: " << texFile.string() << std::endl;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

enum Modo { ROTATE, TRANSLATE, SCALE };
Modo modoAtual = ROTATE;

bool perspective = true;
bool wireframe = false;

// câmera
Camera camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// luz
glm::vec3 lightPos(3.0f, 3.0f, 3.0f);

// ---------- SHADERS ----------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
    FragPos = vec3(model * vec4(position, 1.0));
    Normal = mat3(transpose(inverse(model))) * normal;
    TexCoord = texCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";
// Fragment shader implementando o modelo de iluminação Phong
const char* fragmentShaderSource = R"(
#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 color;

// luz
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;

uniform bool isWireframe;
uniform vec3 wireframeColor;

// material
uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform float shininess;
uniform sampler2D texture1;

void main() {
    if (isWireframe) {
        color = vec4(wireframeColor, 1.0);
        return;
    }

    vec3 ambient = ka * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = kd * diff * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = ks * spec * lightColor;

    vec3 result = ambient + diffuse + specular;
    vec3 texColor = texture(texture1, TexCoord).rgb;
    color = vec4(result * texColor, 1.0);
}
)";

// ---------- LOADER OBJ ----------

// Função para carregar um arquivo OBJ, retornando um MeshData com os vértices e normais intercalados
MeshData loadOBJ(const char* path)
{
    namespace fs = std::filesystem;
    fs::path objFile = fs::path(__FILE__).parent_path() / path;
    ifstream file(objFile);
    if (!file.is_open()) {
        cout << "Erro ao abrir OBJ\n";
        return { nullptr, 0 };
    }
    // Vetores temporários para posições, normais, UVs e índices
    vector<glm::vec3> tempV;
    vector<glm::vec2> tempVT;
    vector<glm::vec3> tempVN;
    vector<int> vIndex;
    vector<int> vtIndex;
    vector<int> vnIndex;
    // Ler o arquivo linha por linha
    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        string type;
        ss >> type;

        if (type == "v") {
            glm::vec3 v;
            ss >> v.x >> v.y >> v.z;
            tempV.push_back(v);
        }
        else if (type == "vt") {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            tempVT.push_back(uv);
        }
        else if (type == "vn") {
            glm::vec3 n;
            ss >> n.x >> n.y >> n.z;
            tempVN.push_back(n);
        }
        else if (type == "f") {
            string v1, v2, v3;
            ss >> v1 >> v2 >> v3;

            vector<string> verts = { v1, v2, v3 };
            // Cada vértice tem formato "vIndex/vtIndex/vnIndex" ou "vIndex//vnIndex"
            for (auto& token : verts) {
                string a, b, c;
                stringstream vs(token);
                getline(vs, a, '/');
                getline(vs, b, '/');
                getline(vs, c, '/');

                vIndex.push_back(stoi(a) - 1);
                vtIndex.push_back(b.empty() ? -1 : stoi(b) - 1);
                vnIndex.push_back(c.empty() ? -1 : stoi(c) - 1);
            }
        }
    }

    int count = vIndex.size();
    float* vertices = new float[count * 8];
    int idx = 0;
    for (int i = 0; i < count; i++) {
        glm::vec3 v = tempV[vIndex[i]];
        glm::vec3 n = (vnIndex[i] >= 0) ? tempVN[vnIndex[i]] : glm::vec3(0, 1, 0);
        glm::vec2 uv = (vtIndex[i] >= 0) ? tempVT[vtIndex[i]] : glm::vec2(0.0f, 0.0f);

        vertices[idx++] = v.x;
        vertices[idx++] = v.y;
        vertices[idx++] = v.z;

        vertices[idx++] = n.x;
        vertices[idx++] = n.y;
        vertices[idx++] = n.z;

        vertices[idx++] = uv.x;
        vertices[idx++] = uv.y;
    }

    return { vertices, count };
}

// ---------- VAO ----------

// Cria um VAO para o mesh, intercalando posições e normais (6 floats por vértice)
GLuint createVAO(MeshData mesh)
{
    GLuint VAO, VBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 8 * sizeof(float), mesh.vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    return VAO;
}

GLuint getMeshVAO(const std::string& filename, int& vertexCount)
{
    if (meshCache.count(filename)) {
        vertexCount = meshCache[filename].second;
        return meshCache[filename].first;
    }

    MeshData mesh = loadOBJ(filename.c_str());
    if (mesh.vertexCount == 0) {
        std::cout << "Falha ao carregar mesh: " << filename << std::endl;
        return 0;
    }

    GLuint vao = createVAO(mesh);
    meshCache[filename] = std::make_pair(vao, mesh.vertexCount);
    vertexCount = mesh.vertexCount;
    return vao;
}

// ---------- INPUT ----------
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Seleção de objeto e modo
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
        selecionado = (selecionado + 1) % totalObjetos;

    if (key == GLFW_KEY_R && action == GLFW_PRESS)
        modoAtual = ROTATE;

    if (key == GLFW_KEY_T && action == GLFW_PRESS)
        modoAtual = TRANSLATE;

    if (key == GLFW_KEY_E && action == GLFW_PRESS)
        modoAtual = SCALE;

    if (key == GLFW_KEY_P && action == GLFW_PRESS)
        perspective = !perspective;

    if (key == GLFW_KEY_M && action == GLFW_PRESS)
        wireframe = !wireframe;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.processKeyboard("FORWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.processKeyboard("LEFT", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.processKeyboard("BACKWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.processKeyboard("RIGHT", deltaTime);

    float speed = 2.0f * deltaTime;

    if (modoAtual == ROTATE) {
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
            objetos[selecionado].rot.x += speed * 3.14f;
        if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)
            objetos[selecionado].rot.y += speed * 3.14f;
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
            objetos[selecionado].rot.z += speed * 3.14f;
    }

    if (modoAtual == TRANSLATE) {
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
            objetos[selecionado].pos.z -= speed;
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
            objetos[selecionado].pos.z += speed;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
            objetos[selecionado].pos.x -= speed;
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
            objetos[selecionado].pos.x += speed;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
            objetos[selecionado].pos.y += speed;
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
            objetos[selecionado].pos.y -= speed;
    }

    if (modoAtual == SCALE) {
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            objetos[selecionado].scale += glm::vec3(speed);
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            objetos[selecionado].scale -= glm::vec3(speed);
            if (objetos[selecionado].scale.x < 0.1f)
                objetos[selecionado].scale = glm::vec3(0.1f);
        }
    }
}

// ---------- SHADER ----------
GLuint createShader()
{
    // Criar e compilar vertex shader usando a string vertexShaderSource
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL);
    glCompileShader(fs);
    
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    return prog;
}

glm::vec3 bezier(
    glm::vec3 p0,
    glm::vec3 p1,
    glm::vec3 p2,
    glm::vec3 p3,
    float t)
{
    float u = 1.0f - t;

    return
        u*u*u*p0 +
        3*u*u*t*p1 +
        3*u*t*t*p2 +
        t*t*t*p3;
}

glm::mat4 getProjectionMatrix()
{
    if (perspective) {
        return glm::perspective(glm::radians(scene.camera.fov), (float)windowWidth / (float)windowHeight, scene.camera.nearPlane, scene.camera.farPlane);
    }
    float scale = 10.0f;
    return glm::ortho(-scale, scale, -scale * 0.75f, scale * 0.75f, scene.camera.nearPlane, scene.camera.farPlane);
}

bool raySphereIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& center, float radius, float& t)
{
    glm::vec3 oc = rayOrigin - center;
    float a = glm::dot(rayDir, rayDir);
    float b = 2.0f * glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return false;
    float sqrtDisc = sqrt(discriminant);
    float t0 = (-b - sqrtDisc) / (2.0f * a);
    float t1 = (-b + sqrtDisc) / (2.0f * a);
    if (t0 > 0.0f) { t = t0; return true; }
    if (t1 > 0.0f) { t = t1; return true; }
    return false;
}

glm::vec3 screenPointToWorldRay(double mouseX, double mouseY, const glm::mat4& view, const glm::mat4& projection)
{
    float x = (2.0f * (float)mouseX) / windowWidth - 1.0f;
    float y = 1.0f - (2.0f * (float)mouseY) / windowHeight;
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);

    glm::vec4 rayEye = glm::inverse(projection) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec4 rayWorld = glm::inverse(view) * rayEye;
    return glm::normalize(glm::vec3(rayWorld));
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = getProjectionMatrix();
    glm::vec3 rayDir = screenPointToWorldRay(mouseX, mouseY, view, projection);
    glm::vec3 rayOrigin = camera.position;

    float closestT = FLT_MAX;
    int hitObject = -1;
    for (int i = 0; i < totalObjetos; ++i) {
        float radius = glm::max(objetos[i].scale.x, glm::max(objetos[i].scale.y, objetos[i].scale.z)) * 1.2f;
        float t;
        if (raySphereIntersect(rayOrigin, rayDir, objetos[i].pos, radius, t) && t < closestT) {
            closestT = t;
            hitObject = i;
        }
    }

    if (hitObject >= 0)
        selecionado = hitObject;
}

// ---------- MAIN ----------
int main()
{
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Phong Viewer", NULL, NULL);
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    GLuint shader = createShader();
    glUseProgram(shader);

    GLuint textureID = loadTexture("image.png");
    glUniform1i(glGetUniformLocation(shader, "texture1"), 0);

    if(!loadScene("scene.json", scene)) {
        cout << "Erro ao carregar cena\n";
        return -1;
    }

    totalObjetos = (int)scene.objects.size();
    objetos.resize(totalObjetos);

    for (int i = 0; i < totalObjetos; ++i) {
        const SceneObject& o = scene.objects[i];
        objetos[i].VAO = getMeshVAO(o.model, objetos[i].vertexCount);
        objetos[i].pos = o.position;
        objetos[i].rot = o.rotation;
        objetos[i].scale = o.scale;
        objetos[i].animated = o.animation.animated;
        objetos[i].bezierP0 = o.animation.p0;
        objetos[i].bezierP1 = o.animation.p1;
        objetos[i].bezierP2 = o.animation.p2;
        objetos[i].bezierP3 = o.animation.p3;

        objetos[i].ka = glm::vec3(0.2f);
        objetos[i].kd = glm::vec3(0.7f);
        objetos[i].ks = glm::vec3(1.0f);
        objetos[i].shininess = 32.0f;
    }

    // Imprimir controles
    cout << "\n=== CONTROLES ===" << endl;
    cout << "WASD: Mover câmera" << endl;
    cout << "TAB: Mudar objeto selecionado" << endl;
    cout << "R: Modo Rotação (X/Y/Z para rotacionar)" << endl;
    cout << "T: Modo Translação (I/K/J/L/U/O para transladar)" << endl;
    cout << "E: Modo Escala (UP/DOWN para aumentar/diminuir)" << endl;
    cout << "P: Alternar perspectiva Ortográfica/Câmera" << endl;
    cout << "M: Alternar Wireframe" << endl;
    cout << "Clique esquerdo: selecionar objeto" << endl;
    cout << "ESC: Sair" << endl;
    cout << "================\n" << endl;

    lightPos = scene.light.position;
    camera = Camera(scene.camera.position, glm::vec3(0.0f, 1.0f, 0.0f), scene.camera.yaw, scene.camera.pitch);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        processInput(window);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // mover luz
        float sp = 2*deltaTime;
        if (glfwGetKey(window, GLFW_KEY_1)) lightPos.x += sp;
        if (glfwGetKey(window, GLFW_KEY_2)) lightPos.x -= sp;

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = getProjectionMatrix();

        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,GL_FALSE,glm::value_ptr(projection));

        glUniform3fv(glGetUniformLocation(shader,"lightPos"),1,glm::value_ptr(scene.light.position));
        glUniform3fv(glGetUniformLocation(shader,"viewPos"),1,glm::value_ptr(camera.position));
        glUniform3fv(glGetUniformLocation(shader,"lightColor"),1,glm::value_ptr(scene.light.color));

        for (int i = 0; i < totalObjetos; ++i)
        {
            if (objetos[i].animated) {
                float t = fmod(currentFrame * 0.2f, 1.0f);
                objetos[i].pos = bezier(objetos[i].bezierP0, objetos[i].bezierP1, objetos[i].bezierP2, objetos[i].bezierP3, t);
            }

            glm::mat4 model(1);
            model = glm::translate(model, objetos[i].pos);
            model = glm::rotate(model, objetos[i].rot.x, glm::vec3(1,0,0));
            model = glm::rotate(model, objetos[i].rot.y, glm::vec3(0,1,0));
            model = glm::rotate(model, objetos[i].rot.z, glm::vec3(0,0,1));
            model = glm::scale(model, objetos[i].scale);

            glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,GL_FALSE,glm::value_ptr(model));
            // Enviar material para o shader
            glUniform3fv(glGetUniformLocation(shader,"ka"),1,glm::value_ptr(objetos[i].ka));
            glUniform3fv(glGetUniformLocation(shader,"kd"),1,glm::value_ptr(objetos[i].kd));
            glUniform3fv(glGetUniformLocation(shader,"ks"),1,glm::value_ptr(objetos[i].ks));
            glUniform1f(glGetUniformLocation(shader,"shininess"),objetos[i].shininess);
            glUniform1i(glGetUniformLocation(shader,"isWireframe"), GL_FALSE);

            glBindVertexArray(objetos[i].VAO);

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES, 0, objetos[i].vertexCount);

            if (wireframe) {
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1.0f, -1.0f);
                glLineWidth(2.0f);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glUniform1i(glGetUniformLocation(shader,"isWireframe"), GL_TRUE);
                glUniform3f(glGetUniformLocation(shader,"wireframeColor"), 0.0f, 0.0f, 0.0f);
                glDrawArrays(GL_TRIANGLES, 0, objetos[i].vertexCount);
                glDisable(GL_POLYGON_OFFSET_LINE);
            }
        }
        glfwSwapBuffers(window);
    }

    glfwTerminate();
}