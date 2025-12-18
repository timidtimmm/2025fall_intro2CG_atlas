#include <cmath>
#include <string>
#include <random>
#include <iostream>
#include <math.h>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cstdio>

#include "include/glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "lib/tiny_obj_loader.h"
#include "shader.h"
#include "camera.h"
#include "perlin.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const GLint WIDTH = 1920, HEIGHT = 1080;

// ---- Text renderer globals ----
GLuint g_textVAO = 0, g_textVBO = 0;
GLuint g_fontTex = 0;
Shader* g_textShader = nullptr;

static const int FONT_ATLAS_W = 1024;
static const int FONT_ATLAS_H = 1024;
int g_fontW = 0;
int g_fontH = 0;
static const int FONT_GRID = 16;

// ---- Environment params ----
enum class Season { SPRING = 0, SUMMER = 1, AUTUMN = 2, WINTER = 3 };
enum class Weather { CLEAR = 0, RAINY = 1, SNOWY = 2 };
enum class TimeOfDay { DAY = 0, DUSK = 1, NIGHT = 2, DAWN = 3 };
TimeOfDay gTimeOfDay = TimeOfDay::DAY;

Season gSeason = Season::SUMMER;
Weather gWeather = Weather::CLEAR;
float gHumidity = 0.3f;

std::vector<std::vector<float>> g_chunkVertices;

// ----------------- Structs -----------------
struct plant {
    std::string type;
    float xpos;
    float ypos;
    float zpos;
    int xOffset;
    int yOffset;

    plant(std::string _type, float _xpos, float _ypos, float _zpos, int _xOffset, int _yOffset) {
        type = _type;
        xpos = _xpos;
        ypos = _ypos;
        zpos = _zpos;
        xOffset = _xOffset;
        yOffset = _yOffset;
    }
};

// ---- UI stuff ----
enum class UIButtonType {
    SEASON_SPRING,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    TIME_DAY,
    TIME_DUSK,
    TIME_NIGHT,
    TIME_DAWN,
    HUMIDITY_UP,
    HUMIDITY_DOWN
};

struct UIButton {
    UIButtonType type;
    glm::vec2 center;
    glm::vec2 halfSize;
    glm::vec4 color;
};

GLuint g_uiVAO = 0, g_uiVBO = 0;
std::vector<UIButton> g_buttons;
bool g_uiMode = false;

// ----------------- Global state -----------------
GLFWwindow *window = nullptr;

// Map params
float WATER_HEIGHT = 0.1f;
int chunk_render_distance = 3;
int xMapChunks = 10;
int yMapChunks = 10;
int chunkWidth = 127;
int chunkHeight = 127;
int gridPosX = 0;
int gridPosY = 0;
float originX = (chunkWidth * xMapChunks) / 2 - chunkWidth / 2;
float originY = (chunkHeight * yMapChunks) / 2 - chunkHeight / 2;

// Noise params
int octaves = 5;
float meshHeight = 32.0f;
float noiseScale = 64.0f;
float persistence = 0.5f;
float lacunarity = 2.0f;

// Model params
float MODEL_SCALE = 3.0f;
float MODEL_BRIGHTNESS = 6.0f;

// FPS
double lastTime = 0.0;
int nbFrames = 0;

// ---- Model minY tracking ----
float g_treeMinY = 0.0f;
float g_flowerMinY = 0.0f;
bool g_modelMinYInitialized = false; // kept for compatibility (no longer required)

// ---- FIX: Per-chunk terrain GL buffers (for updating colors) ----
std::vector<GLuint> g_mapPosVBO;
std::vector<GLuint> g_mapNormalVBO;
std::vector<GLuint> g_mapColorVBO;
std::vector<GLuint> g_mapEBO;

// ---- FIX: Per-chunk instancing buffers & counts ----
std::vector<GLuint> g_treeInstanceVBO;
std::vector<GLuint> g_flowerInstanceVBO;
std::vector<GLsizei> g_treeInstanceCount;
std::vector<GLsizei> g_flowerInstanceCount;

// ---- FIX: Model vertex counts (avoid hard-coded 10192/1300) ----
int g_treeVertexCount   = 0;
int g_flowerVertexCount = 0;

// ---- FIX: Model minY loaded flags ----
bool g_treeMinYSet   = false;
bool g_flowerMinYSet = false;

// Camera
Camera camera(glm::vec3(originX, 20.0f, originY));
bool firstMouse = true;
float lastX = WIDTH / 2;
float lastY = HEIGHT / 2;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float currentFrame = 0.0f;

// Chunk & plant globals
std::vector<GLuint> g_map_chunks;
std::vector<GLuint> g_tree_chunks;
std::vector<GLuint> g_flower_chunks;
GLuint g_treeVAO = 0, g_flowerVAO = 0;
std::vector<plant> g_plants;

// ----------------- Forward declarations -----------------
int init();
void processInput(GLFWwindow *window, Shader &shader);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

void render(std::vector<GLuint> &map_chunks, Shader &shader,
            glm::mat4 &view, glm::mat4 &model, glm::mat4 &projection,
            int &nIndices, std::vector<GLuint> &tree_chunks,
            std::vector<GLuint> &flower_chunks, Shader &uiShader);

std::vector<int> generate_indices();
std::vector<float> generate_noise_map(int xOffset, int yOffset);
std::vector<float> generate_vertices(const std::vector<float> &noise_map);
std::vector<float> generate_normals(const std::vector<int> &indices, const std::vector<float> &vertices);
std::vector<float> generate_biome(
    const std::vector<float> &vertices,
    std::vector<plant> &plants,
    int xOffset, int yOffset,
    Season season,
    Weather weather,
    float humidity
);
void generate_map_chunk(GLuint &VAO, int xOffset, int yOffset, std::vector<plant> &plants);

float get_terrain_height_at(float worldX, float worldZ,
                            const std::vector<float>& vertices,
                            int chunkWidth, int chunkHeight);

float load_model(GLuint &VAO, std::string filename, int* outVertexCount);
void setup_instancing(GLuint &VAO, std::vector<GLuint> &plant_chunk, std::string plant_type,
                      std::vector<plant> &plants, std::string filename);

void rebuild_world();
void update_terrain_colors_only();

// UI helpers
void init_ui_geometry();
void init_ui_buttons();
void on_button_clicked(UIButtonType type);
void handle_ui_click(double mouseX, double mouseY);
void draw_ui(Shader &uiShader);

// Text rendering helpers
void init_text_renderer();
void draw_text(Shader& sh, const std::string& text, float xNDC, float yNDC, float sizeNDC, glm::vec3 color);

glm::vec3 gSky = {0.53f, 0.81f, 0.92f};
Shader* gObjectShader = nullptr;

// ============ applySeasonParams() ============
void applySeasonParams(Shader& sh){
    glm::vec3 sky, amb, dif, spc;
    float fogStart, fogEnd;

    switch (gSeason){
    case Season::SPRING:
        sky = {0.60f, 0.86f, 0.98f};
        amb = {0.26f, 0.26f, 0.26f};
        dif = {0.45f, 0.45f, 0.45f};
        spc = {1.00f, 1.00f, 1.00f};
        fogStart = 120.f; fogEnd = 260.f;
        break;
    case Season::SUMMER:
        sky = {0.35f, 0.70f, 0.98f};
        amb = {0.22f, 0.22f, 0.22f};
        dif = {0.55f, 0.55f, 0.55f};
        spc = {1.00f, 1.00f, 1.00f};
        fogStart = 160.f; fogEnd = 340.f;
        break;
    case Season::AUTUMN:
        sky = {0.55f, 0.70f, 0.82f};
        amb = {0.18f, 0.16f, 0.14f};
        dif = {0.35f, 0.30f, 0.26f};
        spc = {0.90f, 0.85f, 0.80f};
        fogStart = 90.f;  fogEnd = 200.f;
        break;
    case Season::WINTER:
        sky = {0.65f, 0.78f, 0.88f};
        amb = {0.24f, 0.26f, 0.30f};
        dif = {0.45f, 0.48f, 0.55f};
        spc = {1.10f, 1.10f, 1.10f};
        fogStart = 70.f;  fogEnd = 170.f;
        break;
    }

    switch (gTimeOfDay) {
    case TimeOfDay::DAY:
        break;
    case TimeOfDay::DUSK:
        sky = glm::vec3(0.8f, 0.5f, 0.3f) * 0.7f;
        amb *= 0.5f;
        dif *= 0.6f;
        fogStart *= 0.7f;
        fogEnd *= 0.7f;
        break;
    case TimeOfDay::NIGHT:
        sky = glm::vec3(0.05f, 0.08f, 0.15f);
        amb *= 0.15f;
        dif *= 0.20f;
        spc *= 0.8f;
        fogStart *= 0.5f;
        fogEnd *= 0.5f;
        break;
    case TimeOfDay::DAWN:
        sky = glm::vec3(0.7f, 0.5f, 0.7f) * 0.6f;
        amb *= 0.6f;
        dif *= 0.7f;
        fogStart *= 0.8f;
        fogEnd *= 0.8f;
        break;
    }

    gSky = sky;

    sh.use();
    sh.setInt("u_season", (int)gSeason);
    sh.setInt("u_timeOfDay", (int)gTimeOfDay);
    sh.setFloat("u_humidity", gHumidity);
    sh.setVec3("u_skyColor", gSky);
    sh.setFloat("u_meshHeight", meshHeight);
    sh.setFloat("u_fogStart", fogStart);
    sh.setFloat("u_fogEnd", fogEnd);

    sh.setVec3("light.ambient",  amb.x, amb.y, amb.z);
    sh.setVec3("light.diffuse",  dif.x, dif.y, dif.z);
    sh.setVec3("light.specular", spc.x, spc.y, spc.z);
}

// ----------------- Text rendering implementation -----------------
void init_text_renderer() {
    if (g_textVAO != 0) return;

    glGenVertexArrays(1, &g_textVAO);
    glGenBuffers(1, &g_textVBO);

    glBindVertexArray(g_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_textVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * 1024, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static inline void push_quad(std::vector<float>& v,
                             float x0, float y0, float x1, float y1,
                             float u0, float v0, float u1, float v1)
{
    v.insert(v.end(), { x0,y0,u0,v0, x1,y0,u1,v0, x1,y1,u1,v1 });
    v.insert(v.end(), { x0,y0,u0,v0, x1,y1,u1,v1, x0,y1,u0,v1 });
}

float measure_text_width_ndc(const std::string& text, float sizeNDC) {
    int currentWidth, currentHeight;
    glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
    float aspectFix = float(currentHeight) / float(currentWidth);

    float glyphW = sizeNDC * aspectFix;
    float advance = glyphW * 0.70f;

    int n = 0;
    for (unsigned char c : text) {
        if (c == '\n') break;
        n++;
    }
    if (n <= 0) return 0.0f;
    return (n - 1) * advance + glyphW;
}

void draw_text_centered_in_button(const UIButton& btn,
                                  const std::string& label,
                                  float sizeNDC,
                                  glm::vec3 color)
{
    float w = measure_text_width_ndc(label, sizeNDC);
    float x = btn.center.x - 0.5f * w;
    float y = btn.center.y - 0.5f * sizeNDC;
    draw_text(*g_textShader, label, x, y, sizeNDC, color);
}

void draw_text(Shader& sh, const std::string& text, float xNDC, float yNDC, float sizeNDC, glm::vec3 color)
{
    if (g_textVAO == 0 || g_fontTex == 0) return;

    int currentWidth, currentHeight;
    glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
    float aspectFix = float(currentHeight) / float(currentWidth);

    float glyphH = sizeNDC;
    float glyphW = sizeNDC * aspectFix;
    float advance = glyphW * 0.70f;

    float epsU = 0.5f / float(g_fontW > 0 ? g_fontW : 1024);
    float epsV = 0.5f / float(g_fontH > 0 ? g_fontH : 1024);

    std::vector<float> verts;
    verts.reserve(text.size() * 6 * 4);

    float penX = xNDC;
    float penY = yNDC;

    for (unsigned char c : text) {
        if (c == '\n') { penX = xNDC; penY -= glyphH * 1.2f; continue; }
        if (c == ' ') { penX += advance; continue; }

        unsigned char uc = (unsigned char)c;
        int col = uc & 15;
        int row = uc >> 4;

        float u0 = (col + 0.0f) / float(FONT_GRID) + epsU;
        float u1 = (col + 1.0f) / float(FONT_GRID) - epsU;

        float vTop = 1.0f - (row + 0.0f) / float(FONT_GRID);
        float vBottom = 1.0f - (row + 1.0f) / float(FONT_GRID);
        float v0 = vBottom + epsV;
        float v1 = vTop - epsV;

        float x0 = penX;
        float y0 = penY;
        float x1 = penX + glyphW;
        float y1 = penY + glyphH;

        push_quad(verts, x0, y0, x1, y1, u0, v0, u1, v1);
        penX += advance;
    }

    sh.use();
    sh.setVec3("textColor", color);
    sh.setInt("uFont", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_fontTex);

    glBindVertexArray(g_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_textVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 4));
    glBindVertexArray(0);
}

// load texture 2d
GLuint load_texture_2d(const char* path) {
    int w, h, n;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &n, 4);
    if (!data) {
        std::cout << "Failed to load texture: " << path << "\n";
        return 0;
    }
    g_fontW = w;
    g_fontH = h;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    return tex;
}

// ----------------- UI implementation -----------------
void init_ui_geometry() {
    if (g_uiVAO != 0) return;

    float quadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenVertexArrays(1, &g_uiVAO);
    glGenBuffers(1, &g_uiVBO);

    glBindVertexArray(g_uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void init_ui_buttons() {
    g_buttons.clear();

    float btnHalfH = 0.04f;
    float btnHalfW = 0.09f;
    float gap = 0.02f;

    auto addBtn = [&](UIButtonType type, float x, float y, glm::vec4 color) {
        UIButton b;
        b.type = type;
        b.center = glm::vec2(x, y);
        b.halfSize = glm::vec2(btnHalfW, btnHalfH);
        b.color = color;
        g_buttons.push_back(b);
    };

    float topY = 0.85f;
    float startX = -0.9f;

    addBtn(UIButtonType::SEASON_SPRING, startX + btnHalfW, topY, glm::vec4(0.7f, 1.0f, 0.7f, 0.6f));
    startX += btnHalfW * 2.0f + gap;
    addBtn(UIButtonType::SEASON_SUMMER, startX + btnHalfW, topY, glm::vec4(0.3f, 0.8f, 0.3f, 0.6f));
    startX += btnHalfW * 2.0f + gap;
    addBtn(UIButtonType::SEASON_AUTUMN, startX + btnHalfW, topY, glm::vec4(0.9f, 0.6f, 0.2f, 0.6f));
    startX += btnHalfW * 2.0f + gap;
    addBtn(UIButtonType::SEASON_WINTER, startX + btnHalfW, topY, glm::vec4(0.7f, 0.8f, 1.0f, 0.6f));

    float timeY = 0.75f;
    startX = -0.9f;

    addBtn(UIButtonType::TIME_DAY, startX + btnHalfW, timeY, glm::vec4(1.0f, 1.0f, 0.5f, 0.6f));
    startX += btnHalfW * 2.0f + gap;
    addBtn(UIButtonType::TIME_DUSK, startX + btnHalfW, timeY, glm::vec4(1.0f, 0.6f, 0.3f, 0.6f));
    startX += btnHalfW * 2.0f + gap;
    addBtn(UIButtonType::TIME_NIGHT, startX + btnHalfW, timeY, glm::vec4(0.2f, 0.2f, 0.5f, 0.6f));
    startX += btnHalfW * 2.0f + gap;
    addBtn(UIButtonType::TIME_DAWN, startX + btnHalfW, timeY, glm::vec4(0.8f, 0.5f, 0.8f, 0.6f));

    float humidityBtnHalfW = 0.05f;
    float humidityBtnHalfH = 0.04f;
    float humidityGap = 0.03f;

    UIButton down, up;

    down.type = UIButtonType::HUMIDITY_DOWN;
    down.center = glm::vec2(0.72f, 0.85f);
    down.halfSize = glm::vec2(humidityBtnHalfW, humidityBtnHalfH);
    down.color = glm::vec4(1.0f, 0.5f, 0.3f, 0.6f);
    g_buttons.push_back(down);

    up.type = UIButtonType::HUMIDITY_UP;
    up.center = glm::vec2(0.72f + humidityBtnHalfW * 2.0f + humidityGap, 0.85f);
    up.halfSize = glm::vec2(humidityBtnHalfW, humidityBtnHalfH);
    up.color = glm::vec4(0.2f, 0.8f, 1.0f, 0.6f);
    g_buttons.push_back(up);

    std::cout << "[DEBUG] Button layout:" << std::endl;
    for (size_t i = 0; i < g_buttons.size(); i++) {
        std::cout << "  [" << i << "] type=" << (int)g_buttons[i].type
                  << " center=(" << g_buttons[i].center.x << ", " << g_buttons[i].center.y << ")"
                  << " bounds=[" << (g_buttons[i].center.x - g_buttons[i].halfSize.x) << " to "
                  << (g_buttons[i].center.x + g_buttons[i].halfSize.x) << "]" << std::endl;
    }
}

// ----------------- FIX: terrain chunk destroy helper -----------------
static inline void destroy_map_chunk(int pos) {
    if (pos < 0 || pos >= (int)g_map_chunks.size()) return;

    if (g_map_chunks[pos]) glDeleteVertexArrays(1, &g_map_chunks[pos]);
    if (!g_mapPosVBO.empty() && g_mapPosVBO[pos]) glDeleteBuffers(1, &g_mapPosVBO[pos]);
    if (!g_mapNormalVBO.empty() && g_mapNormalVBO[pos]) glDeleteBuffers(1, &g_mapNormalVBO[pos]);
    if (!g_mapColorVBO.empty() && g_mapColorVBO[pos]) glDeleteBuffers(1, &g_mapColorVBO[pos]);
    if (!g_mapEBO.empty() && g_mapEBO[pos]) glDeleteBuffers(1, &g_mapEBO[pos]);

    g_map_chunks[pos] = 0;
    if (!g_mapPosVBO.empty()) g_mapPosVBO[pos] = 0;
    if (!g_mapNormalVBO.empty()) g_mapNormalVBO[pos] = 0;
    if (!g_mapColorVBO.empty()) g_mapColorVBO[pos] = 0;
    if (!g_mapEBO.empty()) g_mapEBO[pos] = 0;
}

// ----------------- FIX: only update terrain colors -----------------
void update_terrain_colors_only() {
    for (int y = 0; y < yMapChunks; y++) {
        for (int x = 0; x < xMapChunks; x++) {
            int pos = x + y * xMapChunks;
            if (pos < 0 || pos >= (int)g_map_chunks.size()) continue;
            if (g_map_chunks[pos] == 0) continue;
            if (pos >= (int)g_chunkVertices.size()) continue;

            const std::vector<float>& verts = g_chunkVertices[pos];
            if (verts.empty()) continue;

            std::vector<plant> dummy_plants;
            std::vector<float> colors = generate_biome(verts, dummy_plants, x, y, gSeason, gWeather, gHumidity);

            if (pos < (int)g_mapColorVBO.size() && g_mapColorVBO[pos] != 0) {
                glBindBuffer(GL_ARRAY_BUFFER, g_mapColorVBO[pos]);
                glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }
    }
}

// ----------------- FIX: on_button_clicked -----------------
void on_button_clicked(UIButtonType type) {
    enum class UpdateKind { NONE, COLORS_ONLY, FULL_REBUILD };
    UpdateKind upd = UpdateKind::NONE;

    std::cout << "[DEBUG] Button clicked, type = " << (int)type << std::endl;

    switch (type) {
    case UIButtonType::SEASON_SPRING:
        if (gSeason != Season::SPRING) { gSeason = Season::SPRING; upd = UpdateKind::FULL_REBUILD; }
        break;
    case UIButtonType::SEASON_SUMMER:
        if (gSeason != Season::SUMMER) { gSeason = Season::SUMMER; upd = UpdateKind::FULL_REBUILD; }
        break;
    case UIButtonType::SEASON_AUTUMN:
        if (gSeason != Season::AUTUMN) { gSeason = Season::AUTUMN; upd = UpdateKind::FULL_REBUILD; }
        break;
    case UIButtonType::SEASON_WINTER:
        if (gSeason != Season::WINTER) { gSeason = Season::WINTER; upd = UpdateKind::FULL_REBUILD; }
        break;

    case UIButtonType::HUMIDITY_UP:
        gHumidity = std::min(1.0f, gHumidity + 0.1f);
        upd = UpdateKind::COLORS_ONLY;
        break;
    case UIButtonType::HUMIDITY_DOWN:
        gHumidity = std::max(0.0f, gHumidity - 0.1f);
        upd = UpdateKind::COLORS_ONLY;
        break;

    case UIButtonType::TIME_DAY:   gTimeOfDay = TimeOfDay::DAY;   upd = UpdateKind::NONE; break;
    case UIButtonType::TIME_DUSK:  gTimeOfDay = TimeOfDay::DUSK;  upd = UpdateKind::NONE; break;
    case UIButtonType::TIME_NIGHT: gTimeOfDay = TimeOfDay::NIGHT; upd = UpdateKind::NONE; break;
    case UIButtonType::TIME_DAWN:  gTimeOfDay = TimeOfDay::DAWN;  upd = UpdateKind::NONE; break;
    }

    std::cout << "[UI] Final state - Season=" << (int)gSeason
              << " Humidity=" << gHumidity
              << " Time=" << (int)gTimeOfDay << std::endl;

    if (gObjectShader) applySeasonParams(*gObjectShader);

    if (upd == UpdateKind::FULL_REBUILD) rebuild_world();
    else if (upd == UpdateKind::COLORS_ONLY) update_terrain_colors_only();
}

void handle_ui_click(double mouseX, double mouseY) {
    if (!g_uiMode) return;

    int currentWidth, currentHeight;
    glfwGetFramebufferSize(window, &currentWidth, &currentHeight);

    float ndcX = 2.0f * float(mouseX) / float(currentWidth) - 1.0f;
    float ndcY = -2.0f * float(mouseY) / float(currentHeight) + 1.0f;

    std::cout << "[CLICK] Window size: " << currentWidth << "x" << currentHeight
              << " | Mouse: (" << mouseX << ", " << mouseY << ") -> NDC: ("
              << ndcX << ", " << ndcY << ")" << std::endl;

    for (int i = 0; i < (int)g_buttons.size(); i++) {
        const auto& btn = g_buttons[i];
        glm::vec2 min = btn.center - btn.halfSize;
        glm::vec2 max = btn.center + btn.halfSize;

        if (ndcX >= min.x && ndcX <= max.x &&
            ndcY >= min.y && ndcY <= max.y) {
            std::cout << "  Button[" << i << "] type=" << (int)btn.type << " HIT!" << std::endl;
            on_button_clicked(btn.type);
            return;
        }
    }

    std::cout << "[CLICK] No button hit" << std::endl;
}

void mouse_button_callback(GLFWwindow* window_, int button, int action, int mods) {
    (void)window_; (void)mods;
    if (!g_uiMode) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        handle_ui_click(mx, my);
    }
}

void draw_ui(Shader &uiShader) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uiShader.use();
    glBindVertexArray(g_uiVAO);

    for (auto &btn : g_buttons) {
        uiShader.setVec4("uColor", btn.color);
        uiShader.setVec2("uPos", btn.center);
        uiShader.setVec2("uSize", btn.halfSize);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// ----------------- World generation helpers -----------------
void rebuild_world() {
    g_plants.clear();

    for (int y = 0; y < yMapChunks; y++) {
        for (int x = 0; x < xMapChunks; x++) {
            int pos = x + y * xMapChunks;
            generate_map_chunk(g_map_chunks[pos], x, y, g_plants);
        }
    }

    setup_instancing(g_treeVAO, g_tree_chunks, "tree", g_plants, "obj/CommonTree_1.obj");
    setup_instancing(g_flowerVAO, g_flower_chunks, "flower", g_plants, "obj/Flowers.obj");
}

// ----------------- main -----------------
int main() {
    glm::mat4 view;
    glm::mat4 model;
    glm::mat4 projection;

    if (init() != 0)
        return -1;

    Shader objectShader("shaders/objectShader.vert", "shaders/objectShader.frag");
    Shader uiShader("shaders/uiShader.vert", "shaders/uiShader.frag");

    objectShader.use();
    objectShader.setBool("isFlat", true);

    objectShader.setVec3("light.ambient", 0.2f, 0.2f, 0.2f);
    objectShader.setVec3("light.diffuse", 0.3f, 0.3f, 0.3f);
    objectShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
    objectShader.setVec3("light.direction", -0.2f, -1.0f, -0.3f);

    objectShader.setInt("u_season", (int)gSeason);

    int chunkN = xMapChunks * yMapChunks;
    g_map_chunks.resize(chunkN);
    g_tree_chunks.resize(chunkN);
    g_flower_chunks.resize(chunkN);
    g_chunkVertices.resize(chunkN);

    // ---- FIX: allocate terrain buffers ----
    g_mapPosVBO.assign(chunkN, 0);
    g_mapNormalVBO.assign(chunkN, 0);
    g_mapColorVBO.assign(chunkN, 0);
    g_mapEBO.assign(chunkN, 0);

    // ---- FIX: allocate instancing buffers/count ----
    g_treeInstanceVBO.assign(chunkN, 0);
    g_flowerInstanceVBO.assign(chunkN, 0);
    g_treeInstanceCount.assign(chunkN, 0);
    g_flowerInstanceCount.assign(chunkN, 0);

    gObjectShader = &objectShader;
    applySeasonParams(objectShader);
    rebuild_world();

    int nIndices = chunkWidth * chunkHeight * 6;

    lastTime = glfwGetTime();
    nbFrames = 0;

    while (!glfwWindowShouldClose(window)) {
        objectShader.use();
        projection = glm::perspective(glm::radians(camera.Zoom),
                                      (float)WIDTH / (float)HEIGHT,
                                      0.1f,
                                      (float)chunkWidth * (chunk_render_distance - 1.2f));
        view = camera.GetViewMatrix();
        objectShader.setMat4("u_projection", projection);
        objectShader.setMat4("u_view", view);
        objectShader.setVec3("u_viewPos", camera.Position);

        render(g_map_chunks, objectShader, view, model, projection,
               nIndices, g_tree_chunks, g_flower_chunks, uiShader);
    }

    // cleanup
    for (int i = 0; i < (int)g_map_chunks.size(); i++) {
        if (g_tree_chunks[i]) glDeleteVertexArrays(1, &g_tree_chunks[i]);
        if (g_flower_chunks[i]) glDeleteVertexArrays(1, &g_flower_chunks[i]);
    }

    for (int i = 0; i < (int)g_map_chunks.size(); i++) {
        destroy_map_chunk(i);
    }

    for (auto b : g_treeInstanceVBO) if (b) glDeleteBuffers(1, &b);
    for (auto b : g_flowerInstanceVBO) if (b) glDeleteBuffers(1, &b);

    if (g_textVAO) glDeleteVertexArrays(1, &g_textVAO);
    if (g_textVBO) glDeleteBuffers(1, &g_textVBO);
    if (g_fontTex) glDeleteTextures(1, &g_fontTex);
    if (g_uiVAO) glDeleteVertexArrays(1, &g_uiVAO);
    if (g_uiVBO) glDeleteBuffers(1, &g_uiVBO);

    delete g_textShader;
    glfwTerminate();
    return 0;
}

// ----------------- instancing & render -----------------
void setup_instancing(GLuint &VAO, std::vector<GLuint> &plant_chunk, std::string plant_type,
                      std::vector<plant> &plants, std::string filename) {
    (void)VAO;

    const int chunkN = xMapChunks * yMapChunks;
    if ((int)plant_chunk.size() != chunkN) plant_chunk.resize(chunkN, 0);

    std::vector<GLuint>* instVBOs = nullptr;
    std::vector<GLsizei>* instCnt = nullptr;

    if (plant_type == "tree") {
        instVBOs = &g_treeInstanceVBO;
        instCnt  = &g_treeInstanceCount;
    } else {
        instVBOs = &g_flowerInstanceVBO;
        instCnt  = &g_flowerInstanceCount;
    }

    if ((int)instVBOs->size() != chunkN) instVBOs->assign(chunkN, 0);
    if ((int)instCnt->size()  != chunkN) instCnt->assign(chunkN, 0);

    // load model VAO for each chunk (only once), create instance VBOs
    for (int i = 0; i < chunkN; i++) {
        if (plant_chunk[i] == 0) {
            int vcount = 0;
            float minY = load_model(plant_chunk[i], filename, &vcount);

            if (plant_type == "tree" && !g_treeMinYSet) {
                g_treeMinY = minY; g_treeMinYSet = true; g_treeVertexCount = vcount;
                std::cout << "[INFO] Tree minY=" << g_treeMinY << " vtx=" << g_treeVertexCount << "\n";
            }
            if (plant_type == "flower" && !g_flowerMinYSet) {
                g_flowerMinY = minY; g_flowerMinYSet = true; g_flowerVertexCount = vcount;
                std::cout << "[INFO] Flower minY=" << g_flowerMinY << " vtx=" << g_flowerVertexCount << "\n";
            }
        }
        if ((*instVBOs)[i] == 0) {
            glGenBuffers(1, &((*instVBOs)[i]));
        }
    }

    float modelMinY = (plant_type == "tree") ? g_treeMinY : g_flowerMinY;

    // collect instances per chunk
    std::vector<std::vector<float>> chunkInstances(chunkN);

    for (int i = 0; i < (int)plants.size(); i++) {
        if (plants[i].type != plant_type) continue;

        float xPos = plants[i].xpos / MODEL_SCALE;
        float yPos = plants[i].ypos / MODEL_SCALE + (-modelMinY);
        float zPos = plants[i].zpos / MODEL_SCALE;

        int pos = plants[i].xOffset + plants[i].yOffset * xMapChunks;
        if (pos < 0 || pos >= chunkN) continue;

        chunkInstances[pos].push_back(xPos);
        chunkInstances[pos].push_back(yPos);
        chunkInstances[pos].push_back(zPos);
    }

    // upload instance buffers
    for (int y = 0; y < yMapChunks; y++) {
        for (int x = 0; x < xMapChunks; x++) {
            int pos = x + y * xMapChunks;

            glBindVertexArray(plant_chunk[pos]);
            glBindBuffer(GL_ARRAY_BUFFER, (*instVBOs)[pos]);

            auto& data = chunkInstances[pos];
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float),
                         data.empty() ? nullptr : data.data(),
                         GL_STATIC_DRAW);

            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glVertexAttribDivisor(3, 1);

            (*instCnt)[pos] = (GLsizei)(data.size() / 3);
        }
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void render(std::vector<GLuint> &map_chunks, Shader &shader,
            glm::mat4 &view, glm::mat4 &model, glm::mat4 &projection,
            int &nIndices, std::vector<GLuint> &tree_chunks,
            std::vector<GLuint> &flower_chunks, Shader &uiShader) {

    currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(window, shader);

    glClearColor(gSky.x, gSky.y, gSky.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gridPosX = (int)(camera.Position.x - originX) / chunkWidth + xMapChunks / 2;
    gridPosY = (int)(camera.Position.z - originY) / chunkHeight + yMapChunks / 2;

    for (int y = 0; y < yMapChunks; y++) {
        for (int x = 0; x < xMapChunks; x++) {
            if (std::abs(gridPosX - x) <= chunk_render_distance &&
                (y - gridPosY) <= chunk_render_distance) {

                int idx = x + y * xMapChunks;

                // ---- terrain ----
                model = glm::mat4(1.0f);
                model = glm::translate(model,
                    glm::vec3(-chunkWidth / 2.0f + (chunkWidth - 1) * x,
                              0.0f,
                              -chunkHeight / 2.0f + (chunkHeight - 1) * y));
                shader.setMat4("u_model", model);
                shader.setBool("u_isPlant", false);
                shader.setInt("u_plantKind", 0);
                shader.setInt("u_season", (int)gSeason);

                glBindVertexArray(map_chunks[idx]);
                glDrawElements(GL_TRIANGLES, nIndices, GL_UNSIGNED_INT, 0);

                // ---- plants ----
                model = glm::mat4(1.0f);
                model = glm::translate(model,
                    glm::vec3(-chunkWidth / 2.0f + (chunkWidth - 1) * x,
                              0.0f,
                              -chunkHeight / 2.0f + (chunkHeight - 1) * y));
                model = glm::scale(model, glm::vec3(MODEL_SCALE));

                shader.setMat4("u_model", model);
                shader.setBool("u_isPlant", true);
                shader.setInt("u_season", (int)gSeason);

                glEnable(GL_CULL_FACE);

                // flowers (FIX: use real instance count + real vertex count)
                shader.setInt("u_plantKind", 1);
                GLsizei fcnt = (idx < (int)g_flowerInstanceCount.size()) ? g_flowerInstanceCount[idx] : 0;
                if (fcnt > 0) {
                    glBindVertexArray(flower_chunks[idx]);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, g_flowerVertexCount, fcnt);
                }

                // trees (FIX)
                shader.setInt("u_plantKind", 2);
                GLsizei tcnt = (idx < (int)g_treeInstanceCount.size()) ? g_treeInstanceCount[idx] : 0;
                if (tcnt > 0) {
                    glBindVertexArray(tree_chunks[idx]);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, g_treeVertexCount, tcnt);
                }

                glDisable(GL_CULL_FACE);

                shader.setBool("u_isPlant", false);
                shader.setInt("u_plantKind", 0);
                shader.setInt("u_season", (int)gSeason);
            }
        }
    }

    draw_ui(uiShader);

    // ---- text ----
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (g_textShader) {
        g_textShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_fontTex);

        float seasonSize = g_buttons[0].halfSize.y * 2.0f * 0.60f;
        draw_text_centered_in_button(g_buttons[0], "SPRING", seasonSize, {1,1,1});
        draw_text_centered_in_button(g_buttons[1], "SUMMER", seasonSize, {1,1,1});
        draw_text_centered_in_button(g_buttons[2], "AUTUMN", seasonSize, {1,1,1});
        draw_text_centered_in_button(g_buttons[3], "WINTER", seasonSize, {1,1,1});

        draw_text_centered_in_button(g_buttons[4], "DAY", seasonSize, {1,1,1});
        draw_text_centered_in_button(g_buttons[5], "DUSK", seasonSize, {1,1,1});
        draw_text_centered_in_button(g_buttons[6], "NIGHT", seasonSize, {1,1,1});
        draw_text_centered_in_button(g_buttons[7], "DAWN", seasonSize, {1,1,1});

        float pmSize = g_buttons[8].halfSize.y * 2.0f * 0.80f;
        draw_text_centered_in_button(g_buttons[8], "-", pmSize, {1,1,1});
        draw_text_centered_in_button(g_buttons[9], "+", pmSize, {1,1,1});

        std::string seasonText =
            (gSeason == Season::SPRING) ? "Spring" :
            (gSeason == Season::SUMMER) ? "Summer" :
            (gSeason == Season::AUTUMN) ? "Autumn" : "Winter";

        std::string timeText =
            (gTimeOfDay == TimeOfDay::DAY) ? "Day" :
            (gTimeOfDay == TimeOfDay::DUSK) ? "Dusk" :
            (gTimeOfDay == TimeOfDay::NIGHT) ? "Night" : "Dawn";

        draw_text(*g_textShader, "Season: " + seasonText, -0.95f, -0.80f, 0.06f, {1,1,0});
        draw_text(*g_textShader, "Time: " + timeText, -0.95f, -0.86f, 0.06f, {1,0.8f,0.5f});
        draw_text(*g_textShader, "Humidity: " + std::to_string(int(gHumidity * 100)) + "%",
                  0.55f, -0.86f, 0.06f, {0.7f,0.9f,1.0f});
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    double currentTime = glfwGetTime();
    nbFrames++;
    if (currentTime - lastTime >= 1.0) {
        printf("%f ms/frame\n", 1000.0 / double(nbFrames));
        nbFrames = 0;
        lastTime += 1.0;
    }

    glfwPollEvents();
    glfwSwapBuffers(window);
}

// ? 多點水域檢查
static inline bool is_underwater_footprint(
    float plantX, float plantZ,
    const std::vector<float>& vertices,
    int chunkWidth_, int chunkHeight_,
    float waterLevel,
    float radius)
{
    const float dx[9] = {0, radius, -radius, 0, 0, radius, radius, -radius, -radius};
    const float dz[9] = {0, 0, 0, radius, -radius, radius, -radius, radius, -radius};

    for (int i = 0; i < 9; i++) {
        float h = get_terrain_height_at(plantX + dx[i], plantZ + dz[i],
                                       vertices, chunkWidth_, chunkHeight_);
        if (h <= waterLevel + 1.0f) return true;
    }
    return false;
}

// ----------------- Model loading & terrain -----------------
float load_model(GLuint &VAO, std::string filename, int* outVertexCount) {
    std::vector<float> vertices;
    float minY = 1e9f;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str());

    if (!warn.empty()) std::cout << warn << std::endl;
    if (!err.empty())  std::cerr << err << std::endl;
    if (!ok) {
        std::cerr << "[ERR] LoadObj failed: " << filename << std::endl;
        if (outVertexCount) *outVertexCount = 0;
        return 0.0f;
    }

    for (size_t s = 0; s < shapes.size(); s++) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            int fv = shapes[s].mesh.num_face_vertices[f];

            int matId = -1;
            if (f < shapes[s].mesh.material_ids.size()) matId = shapes[s].mesh.material_ids[f];

            glm::vec3 diffuse(1.0f, 1.0f, 1.0f);
            if (matId >= 0 && matId < (int)materials.size()) {
                diffuse = glm::vec3(materials[matId].diffuse[0],
                                    materials[matId].diffuse[1],
                                    materials[matId].diffuse[2]);
            }

            for (int v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                float px = attrib.vertices[3 * idx.vertex_index + 0];
                float py = attrib.vertices[3 * idx.vertex_index + 1];
                float pz = attrib.vertices[3 * idx.vertex_index + 2];

                minY = std::min(minY, py);

                glm::vec3 n(0,1,0);
                if (idx.normal_index >= 0 && (3 * idx.normal_index + 2) < (int)attrib.normals.size()) {
                    n = glm::vec3(attrib.normals[3 * idx.normal_index + 0],
                                  attrib.normals[3 * idx.normal_index + 1],
                                  attrib.normals[3 * idx.normal_index + 2]);
                }

                vertices.push_back(px);
                vertices.push_back(py);
                vertices.push_back(pz);
                vertices.push_back(n.x);
                vertices.push_back(n.y);
                vertices.push_back(n.z);
                vertices.push_back(diffuse.x * MODEL_BRIGHTNESS);
                vertices.push_back(diffuse.y * MODEL_BRIGHTNESS);
                vertices.push_back(diffuse.z * MODEL_BRIGHTNESS);
            }
            index_offset += fv;
        }
    }

    if (outVertexCount) *outVertexCount = (int)vertices.size() / 9;

    GLuint VBO;
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(float),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return (minY == 1e9f) ? 0.0f : minY;
}

// FIXED generate_map_chunk (pos + store vertices at correct time + keep VBO handles)
void generate_map_chunk(GLuint &VAO, int xOffset, int yOffset, std::vector<plant> &plants) {
    int pos = xOffset + yOffset * xMapChunks;

    if (pos >= 0 && pos < (int)g_map_chunks.size() && g_map_chunks[pos] != 0) {
        destroy_map_chunk(pos);
    }

    std::vector<int> indices = generate_indices();
    std::vector<float> noise_map = generate_noise_map(xOffset, yOffset);
    std::vector<float> verts = generate_vertices(noise_map);
    std::vector<float> normals = generate_normals(indices, verts);
    std::vector<float> colors = generate_biome(verts, plants, xOffset, yOffset, gSeason, gWeather, gHumidity);

    if (pos >= 0 && pos < (int)g_chunkVertices.size()) {
        g_chunkVertices[pos] = verts;
    }

    GLuint VBOpos, VBOnrm, VBOcol, EBO;
    glGenBuffers(1, &VBOpos);
    glGenBuffers(1, &VBOnrm);
    glGenBuffers(1, &VBOcol);
    glGenBuffers(1, &EBO);
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBOpos);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, VBOnrm);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, VBOcol);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    if (pos >= 0 && pos < (int)g_mapPosVBO.size())    g_mapPosVBO[pos] = VBOpos;
    if (pos >= 0 && pos < (int)g_mapNormalVBO.size()) g_mapNormalVBO[pos] = VBOnrm;
    if (pos >= 0 && pos < (int)g_mapColorVBO.size())  g_mapColorVBO[pos] = VBOcol;
    if (pos >= 0 && pos < (int)g_mapEBO.size())       g_mapEBO[pos] = EBO;

    if (pos >= 0 && pos < (int)g_map_chunks.size())   g_map_chunks[pos] = VAO;
}

glm::vec3 get_color(int r, int g, int b) {
    return glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f);
}

std::vector<float> generate_noise_map(int offsetX, int offsetY) {
    std::vector<float> noiseValues;
    std::vector<float> normalizedNoiseValues;
    std::vector<int> p = get_permutation_vector();

    float amp = 1.0f;
    float freq = 1.0f;
    float maxPossibleHeight = 0.0f;

    for (int i = 0; i < octaves; i++) {
        maxPossibleHeight += amp;
        amp *= persistence;
    }

    for (int y = 0; y < chunkHeight; y++) {
        for (int x = 0; x < chunkWidth; x++) {
            amp = 1.0f;
            freq = 1.0f;
            float noiseHeight = 0.0f;
            for (int i = 0; i < octaves; i++) {
                float xSample = (x + offsetX * (chunkWidth - 1)) / noiseScale * freq;
                float ySample = (y + offsetY * (chunkHeight - 1)) / noiseScale * freq;

                float perlinValue = perlin_noise(xSample, ySample, p);
                noiseHeight += perlinValue * amp;

                amp *= persistence;
                freq *= lacunarity;
            }

            noiseValues.push_back(noiseHeight);
        }
    }

    for (int y = 0; y < chunkHeight; y++) {
        for (int x = 0; x < chunkWidth; x++) {
            normalizedNoiseValues.push_back((noiseValues[x + y * chunkWidth] + 1.0f) / maxPossibleHeight);
        }
    }

    return normalizedNoiseValues;
}

static inline glm::vec3 lerp3(const glm::vec3& a, const glm::vec3& b, float t) {
    t = std::fmax(0.0f, std::fmin(1.0f, t));
    return a * (1.0f - t) + b * t;
}

struct terrainColor {
    terrainColor(float _height, glm::vec3 _color) {
        height = _height;
        color = _color;
    }
    float height;
    glm::vec3 color;
};

float get_terrain_height_at(float worldX, float worldZ, const std::vector<float>& vertices,
                            int chunkWidth_, int chunkHeight_) {
    int gridX = (int)worldX;
    int gridZ = (int)worldZ;

    gridX = std::max(0, std::min(gridX, chunkWidth_ - 2));
    gridZ = std::max(0, std::min(gridZ, chunkHeight_ - 2));

    float fracX = worldX - (float)gridX;
    float fracZ = worldZ - (float)gridZ;

    fracX = std::max(0.0f, std::min(1.0f, fracX));
    fracZ = std::max(0.0f, std::min(1.0f, fracZ));

    int idx00 = (gridX + gridZ * chunkWidth_) * 3;
    int idx10 = ((gridX + 1) + gridZ * chunkWidth_) * 3;
    int idx01 = (gridX + (gridZ + 1) * chunkWidth_) * 3;
    int idx11 = ((gridX + 1) + (gridZ + 1) * chunkWidth_) * 3;

    float h00 = vertices[idx00 + 1];
    float h10 = vertices[idx10 + 1];
    float h01 = vertices[idx01 + 1];
    float h11 = vertices[idx11 + 1];

    float h0 = h00 * (1.0f - fracX) + h10 * fracX;
    float h1 = h01 * (1.0f - fracX) + h11 * fracX;
    float height = h0 * (1.0f - fracZ) + h1 * fracZ;

    return height;
}

static inline float compute_plant_ground_height(
    float plantX, float plantZ,
    const std::vector<float>& vertices,
    int chunkWidth_, int chunkHeight_)
{
    float sampleRadius = 0.2f;
    float lift = 0.01f;

    float minH = get_terrain_height_at(plantX, plantZ, vertices, chunkWidth_, chunkHeight_);

    minH = std::min(minH, get_terrain_height_at(plantX + sampleRadius, plantZ, vertices, chunkWidth_, chunkHeight_));
    minH = std::min(minH, get_terrain_height_at(plantX - sampleRadius, plantZ, vertices, chunkWidth_, chunkHeight_));
    minH = std::min(minH, get_terrain_height_at(plantX, plantZ + sampleRadius, vertices, chunkWidth_, chunkHeight_));
    minH = std::min(minH, get_terrain_height_at(plantX, plantZ - sampleRadius, vertices, chunkWidth_, chunkHeight_));

    return minH + lift;
}

std::vector<float> generate_biome(const std::vector<float> &vertices,
                                  std::vector<plant> &plants,
                                  int xOffset, int yOffset,
                                  Season season,
                                  Weather weather,
                                  float humidity) {
    std::vector<float> colors;
    std::vector<terrainColor> biomeColors;
    glm::vec3 color;

    biomeColors.push_back(terrainColor(WATER_HEIGHT * 0.5f, get_color(60, 95, 190)));
    biomeColors.push_back(terrainColor(WATER_HEIGHT, get_color(60, 100, 190)));
    biomeColors.push_back(terrainColor(0.15f, get_color(210, 215, 130)));
    biomeColors.push_back(terrainColor(0.30f, get_color(95, 165, 30)));
    biomeColors.push_back(terrainColor(0.40f, get_color(65, 115, 20)));
    biomeColors.push_back(terrainColor(0.55f, get_color(90, 65, 60)));
    biomeColors.push_back(terrainColor(0.75f, get_color(75, 60, 55)));
    biomeColors.push_back(terrainColor(2.00f, get_color(70, 55, 50)));

    float snowLineHeight = 0.0f;

    switch (season) {
    case Season::SPRING:
        biomeColors[3].color *= 1.2f;
        biomeColors[4].color *= 1.1f;
        snowLineHeight = 0.0f;
        break;

    case Season::SUMMER:
        biomeColors[3].color *= 1.1f;
        snowLineHeight = 0.0f;
        break;

    case Season::AUTUMN:
        biomeColors[3].color = get_color(190, 150, 60);
        biomeColors[4].color = get_color(160, 110, 50);
        snowLineHeight = 0.70f;
        biomeColors[6].height = 0.70f;
        biomeColors[7].height = 0.75f;
        biomeColors[7].color = get_color(95, 80, 75);
        biomeColors.push_back(terrainColor(0.80f, get_color(160, 170, 180)));
        biomeColors.push_back(terrainColor(0.90f, get_color(210, 220, 230)));
        biomeColors.push_back(terrainColor(2.00f, get_color(240, 245, 250)));
        break;

    case Season::WINTER:
        snowLineHeight = 0.45f;
        biomeColors[5].height = 0.45f;
        biomeColors[6].height = 0.50f;
        biomeColors[7].height = 0.55f;
        biomeColors[6].color = get_color(120, 125, 135);
        biomeColors[7].color = get_color(180, 190, 200);
        biomeColors.push_back(terrainColor(0.60f, get_color(210, 220, 230)));
        biomeColors.push_back(terrainColor(0.70f, get_color(230, 235, 242)));
        biomeColors.push_back(terrainColor(0.85f, get_color(245, 248, 252)));
        biomeColors.push_back(terrainColor(2.00f, get_color(252, 254, 255)));
        break;
    }

    float plantSpawnBase = 5.0f;
    float plantSpawnScale = 1.0f + (humidity - 0.5f) * 2.0f;

    glm::vec3 grassDry = get_color(180, 180, 100);
    glm::vec3 grassNormal = get_color(95, 165, 30);
    glm::vec3 grassWet = get_color(50, 140, 40);

    glm::vec3 grass2Dry = get_color(150, 140, 90);
    glm::vec3 grass2Normal = get_color(65, 115, 20);
    glm::vec3 grass2Wet = get_color(40, 100, 30);

    if (season != Season::WINTER) {
        if (humidity < 0.5f) {
            float t = humidity * 2.0f;
            biomeColors[3].color = lerp3(grassDry, grassNormal, t);
            biomeColors[4].color = lerp3(grass2Dry, grass2Normal, t);
        } else {
            float t = (humidity - 0.5f) * 2.0f;
            biomeColors[3].color = lerp3(grassNormal, grassWet, t);
            biomeColors[4].color = lerp3(grass2Normal, grass2Wet, t);
        }
    }

    glm::vec3 sandDry = get_color(230, 225, 140);
    glm::vec3 sandWet = get_color(190, 195, 110);
    biomeColors[2].color = lerp3(sandDry, sandWet, humidity);

    if (humidity > 0.6f && season != Season::WINTER) {
        float wetness = (humidity - 0.6f) * 2.5f;
        biomeColors[5].color = lerp3(biomeColors[5].color, biomeColors[5].color * 0.85f, wetness);
        if (biomeColors.size() > 6) {
            biomeColors[6].color = lerp3(biomeColors[6].color, biomeColors[6].color * 0.85f, wetness);
        }
    }

    std::string plantType;

    for (int i = 1; i < (int)vertices.size(); i += 3) {
        float worldHeight = vertices[i];
        float normalizedHeight = worldHeight / meshHeight;

        normalizedHeight = std::fmax(0.0f, std::fmin(normalizedHeight, 1.5f));

        int nBands = (int)biomeColors.size();

        int k1 = nBands - 1;
        for (int k = 0; k < nBands - 1; k++) {
            if (normalizedHeight < biomeColors[k + 1].height) {
                k1 = k + 1;
                break;
            }
        }
        int k0 = std::max(0, k1 - 1);

        float h0 = biomeColors[k0].height;
        float h1 = biomeColors[k1].height;

        if (h1 - h0 < 0.001f) {
            color = biomeColors[k1].color;
        } else {
            float t = (normalizedHeight - h0) / (h1 - h0);
            t = std::fmax(0.0f, std::fmin(1.0f, t));
            t = t * t * (3.0f - 2.0f * t);
            color = lerp3(biomeColors[k0].color, biomeColors[k1].color, t);
        }

        bool isSnowRegion = (snowLineHeight > 0.0f && normalizedHeight >= snowLineHeight);

        if (normalizedHeight >= 0.25f && normalizedHeight <= 0.45f && !isSnowRegion) {
            float spawnThreshold = plantSpawnBase * plantSpawnScale;

            if (season == Season::WINTER) {
                spawnThreshold *= 0.3f;
            }

            if (rand() % 1000 < spawnThreshold) {
                if (rand() % 100 < 70) plantType = "flower";
                else plantType = "tree";

                float plantX = vertices[i - 1];
                float plantZ = vertices[i + 1];

                float offsetX_ = (rand() % 100 - 50) / 100.0f * 0.8f;
                float offsetZ_ = (rand() % 100 - 50) / 100.0f * 0.8f;
                plantX += offsetX_;
                plantZ += offsetZ_;

                float finalHeight = compute_plant_ground_height(plantX, plantZ, vertices,
                                                               chunkWidth, chunkHeight);
                float normalizedFinal = finalHeight / meshHeight;

                float waterLevel = WATER_HEIGHT * meshHeight;
                float footprintRadius = (plantType == "tree") ? 0.8f : 0.35f;

                if (is_underwater_footprint(plantX, plantZ, vertices, chunkWidth, chunkHeight,
                                           waterLevel, footprintRadius)) {
                    colors.push_back(color.r);
                    colors.push_back(color.g);
                    colors.push_back(color.b);
                    continue;
                }

                if (finalHeight <= waterLevel + 1.0f) { colors.push_back(color.r); colors.push_back(color.g); colors.push_back(color.b); continue; }
                if (normalizedFinal < WATER_HEIGHT + 0.08f) { colors.push_back(color.r); colors.push_back(color.g); colors.push_back(color.b); continue; }
                if (snowLineHeight > 0.0f && normalizedFinal >= snowLineHeight) { colors.push_back(color.r); colors.push_back(color.g); colors.push_back(color.b); continue; }

                plants.push_back(plant{
                    plantType,
                    plantX,
                    finalHeight,
                    plantZ,
                    xOffset, yOffset
                });
            }
        }

        colors.push_back(color.r);
        colors.push_back(color.g);
        colors.push_back(color.b);
    }

    return colors;
}

std::vector<float> generate_normals(const std::vector<int> &indices, const std::vector<float> &vertices) {
    int nVerts = (int)vertices.size() / 3;
    std::vector<glm::vec3> acc(nVerts, glm::vec3(0.0f));
    std::vector<float> normals(vertices.size(), 0.0f);

    for (int i = 0; i < (int)indices.size(); i += 3) {
        int i0 = indices[i], i1 = indices[i+1], i2 = indices[i+2];

        glm::vec3 v0(vertices[i0*3+0], vertices[i0*3+1], vertices[i0*3+2]);
        glm::vec3 v1(vertices[i1*3+0], vertices[i1*3+1], vertices[i1*3+2]);
        glm::vec3 v2(vertices[i2*3+0], vertices[i2*3+1], vertices[i2*3+2]);

        glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        acc[i0] += n; acc[i1] += n; acc[i2] += n;
    }

    for (int v = 0; v < nVerts; v++) {
        glm::vec3 n = glm::normalize(acc[v]);
        normals[v*3+0] = n.x;
        normals[v*3+1] = n.y;
        normals[v*3+2] = n.z;
    }
    return normals;
}

std::vector<float> generate_vertices(const std::vector<float> &noise_map) {
    std::vector<float> v;

    for (int y = 0; y < chunkHeight; y++) {
        for (int x = 0; x < chunkWidth; x++) {
            v.push_back((float)x);
            float easedNoise = std::pow(noise_map[x + y * chunkWidth] * 1.1f, 3.0f);
            v.push_back(std::fmax(easedNoise * meshHeight, WATER_HEIGHT * 0.5f * meshHeight));
            v.push_back((float)y);
        }
    }
    return v;
}

std::vector<int> generate_indices() {
    std::vector<int> indices;

    for (int y = 0; y < chunkHeight; y++) {
        for (int x = 0; x < chunkWidth; x++) {
            int pos = x + y * chunkWidth;

            if (x == chunkWidth - 1 || y == chunkHeight - 1) {
                continue;
            } else {
                indices.push_back(pos + chunkWidth);
                indices.push_back(pos);
                indices.push_back(pos + chunkWidth + 1);

                indices.push_back(pos + 1);
                indices.push_back(pos + 1 + chunkWidth);
                indices.push_back(pos);
            }
        }
    }
    return indices;
}

// ----------------- GLFW / input -----------------
int init() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Terrain Generator", nullptr, nullptr);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    int screenWidth, screenHeight;
    glfwGetFramebufferSize(window, &screenWidth, &screenHeight);

    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, screenWidth, screenHeight);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    init_ui_geometry();
    init_ui_buttons();
    init_text_renderer();

    g_fontTex = load_texture_2d("assets/font.png");
    g_textShader = new Shader("shaders/text.vert", "shaders/text.frag");
    g_textShader->use();
    g_textShader->setInt("uFont", 0);

    return 0;
}

void processInput(GLFWwindow *window_, Shader &shader) {
    if (glfwGetKey(window_, GLFW_KEY_Q) == GLFW_PRESS)
        glfwSetWindowShouldClose(window_, true);

    if (glfwGetKey(window_, GLFW_KEY_F) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    if (glfwGetKey(window_, GLFW_KEY_G) == GLFW_PRESS) {
        shader.use();
        shader.setBool("isFlat", false);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (glfwGetKey(window_, GLFW_KEY_H) == GLFW_PRESS) {
        shader.use();
        shader.setBool("isFlat", true);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    static bool bWasPressed = false;
    int bState = glfwGetKey(window_, GLFW_KEY_B);

    if (bState == GLFW_PRESS && !bWasPressed) {
        g_uiMode = !g_uiMode;
        if (g_uiMode) {
            glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    bWasPressed = (bState == GLFW_PRESS);
}

void mouse_callback(GLFWwindow *window_, double xpos, double ypos) {
    if (g_uiMode) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        return;
    }

    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;

    lastX = (float)xpos;
    lastY = (float)ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window_, double xoffset, double yoffset) {
    (void)window_; (void)xoffset;
    camera.ProcessMouseScroll((float)yoffset);
}