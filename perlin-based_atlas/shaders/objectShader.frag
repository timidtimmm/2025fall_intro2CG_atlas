#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vBaseColor;
in vec2 vSeedXZ;
in float vLocalY;

out vec4 FragColor;

struct Light {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Light light;
uniform vec3  u_viewPos;

uniform bool  isFlat;
uniform bool  u_isPlant;
uniform int   u_plantKind; // 0 terrain, 1 flower, 2 tree
uniform int   u_season;    // 0 spring 1 summer 2 autumn 3 winter
uniform int   u_timeOfDay; // 0 day 1 dusk 2 night 3 dawn
uniform float u_humidity;

uniform vec3  u_skyColor;
uniform float u_meshHeight;
uniform float u_fogStart;
uniform float u_fogEnd;

// ---------- utils ----------
float hash12(vec2 p){
    // stable-ish random
    float h = dot(p, vec2(127.1, 311.7));
    return fract(sin(h) * 43758.5453123);
}

vec3 applyHumidity(vec3 c){
    // 更細緻的濕度影響 - 平滑連續變化
    
    // 乾燥效果 (0.0 ~ 0.5): 增加黃色調，降低飽和度
    float dryAmount = smoothstep(0.5, 0.0, u_humidity);
    vec3 dryTint = vec3(1.15, 1.05, 0.80);  // 偏黃
    float dryDesaturate = 0.15 * dryAmount;  // 降低飽和度
    
    // 濕潤效果 (0.5 ~ 1.0): 增加綠色和藍色調，提高飽和度
    float wetAmount = smoothstep(0.5, 1.0, u_humidity);
    vec3 wetTint = vec3(0.85, 1.10, 0.90);  // 偏綠
    float wetSaturate = 0.20 * wetAmount;   // 提高飽和度
    
    // 應用乾燥效果
    c = mix(c, c * dryTint, dryAmount * 0.5);
    
    // 降低/提高飽和度
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(c, vec3(lum), dryDesaturate);
    c = mix(vec3(lum), c, 1.0 + wetSaturate);
    
    // 應用濕潤效果
    c = mix(c, c * wetTint, wetAmount * 0.4);
    
    // 極端濕度：水面反光效果
    if (u_humidity > 0.85) {
        float shine = (u_humidity - 0.85) * 6.67;  // 0~1
        c = mix(c, c * 1.15, shine * 0.3);
    }
    
    // 極端乾燥：塵土效果
    if (u_humidity < 0.15) {
        float dust = (0.15 - u_humidity) * 6.67;  // 0~1
        vec3 dustColor = vec3(0.85, 0.80, 0.70);
        c = mix(c, c * dustColor, dust * 0.25);
    }
    
    return c;
}

vec3 applySeasonTone(vec3 c){
    if (u_season == 0) {           // spring
        c *= vec3(1.00, 1.10, 1.00);
    } else if (u_season == 1) {    // summer
        c *= vec3(0.98, 1.08, 0.98);
    } else if (u_season == 2) {    // autumn
        c = vec3(c.r*1.10 + c.g*0.10,
                 c.g*0.70 + c.r*0.10,
                 c.b*0.85) * 0.95;
    } else {                        // winter
        float l = dot(c, vec3(0.299, 0.587, 0.114));
        c = mix(vec3(l), c, 0.20);
        c *= vec3(0.95, 1.00, 1.05) * 1.05;
    }
    
    // 時間色調調整
    if (u_timeOfDay == 1) {        // dusk
        c *= vec3(1.3, 0.9, 0.7);  // 增加紅橘色調
    } else if (u_timeOfDay == 2) { // night
        // 夜晚：降低飽和度，增加藍色
        float lum = dot(c, vec3(0.299, 0.587, 0.114));
        c = mix(vec3(lum), c, 0.4);  // 大幅降低飽和度
        c *= vec3(0.6, 0.7, 1.0) * 0.3;  // 暗藍色調
    } else if (u_timeOfDay == 3) { // dawn
        c *= vec3(1.1, 0.95, 1.1);  // 增加粉紫色調
    }
    
    return c;
}

float snowMask(){
    if (u_season != 3) return 0.0;
    float h = smoothstep(0.45*u_meshHeight, 0.70*u_meshHeight, vWorldPos.y);
    float up = smoothstep(0.25, 0.85, max(vNormal.y, 0.0));
    float m = h * up;
    if (u_isPlant) m *= 0.45;
    return clamp(m, 0.0, 1.0);
}

vec3 lighting(vec3 base, vec3 N, vec3 P){
    vec3 L = normalize(-light.direction);
    float diff = max(dot(N, L), 0.0);
    
    // 計算 specular
    vec3 V = normalize(u_viewPos - P);
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), 16.0);
    
    // ✅ 修復 1: 大幅降低 specular 強度
    // ✅ 修復 2: 只在植物上應用明顯的高光
    float specStrength = u_isPlant ? 0.15 : 0.02;  // 地形幾乎沒有高光
    vec3 spc = light.specular * spec * specStrength;
    
    // ✅ 修復 3: 確保高光不會讓顏色超過 1.0
    vec3 result = base * (light.ambient + light.diffuse * diff) + spc;
    return clamp(result, 0.0, 1.0);
}

// ---------- sakura effect ----------
vec3 applySakuraIfNeeded(vec3 base){
    // only: spring + tree instances
    if (!(u_season == 0 && u_isPlant && u_plantKind == 2)) return base;

    float r = hash12(vSeedXZ * 0.173);        // per-tree random
    float isSakura = step(r, 0.35);           // 35% trees bloom

    // canopy mask: make blossom mostly on upper part of model
    float canopy = smoothstep(0.30, 1.20, vLocalY);

    // blossom color
    // 接近真實櫻花的顏色
    vec3 sakuraPink = vec3(1.00, 0.60, 0.80);
    vec3 tint = mix(base, sakuraPink, 0.95);

    // apply only where canopy, and only on some trees
    float bloom = isSakura * canopy;
    return mix(base, tint, bloom);
}

void main() {
    vec3 base = vBaseColor;

    base = applyHumidity(base);
    base = applySeasonTone(base);  // 現在包含時間色調

    // spring sakura trees
    base = applySakuraIfNeeded(base);

    // winter snow
    float s = snowMask();
    vec3 snowCol = vec3(0.95, 0.97, 1.00);
    base = mix(base, snowCol, s);

    vec3 col = lighting(base, normalize(vNormal), vWorldPos);

    // 夜晚添加星光效果（可選）
    if (u_timeOfDay == 2 && !u_isPlant) {
        float stars = hash12(vWorldPos.xz * 0.5);
        if (stars > 0.995) {
            col += vec3(0.3, 0.3, 0.4) * (stars - 0.995) * 200.0;
        }
    }

    float d = length(u_viewPos - vWorldPos);
    float fog = clamp((d - u_fogStart) / max(u_fogEnd - u_fogStart, 0.001), 0.0, 1.0);
    col = mix(col, u_skyColor, fog * 0.6);

    FragColor = vec4(col, 1.0);
}