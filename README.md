# 2025fall_intro2CG_atlas
## Introduction
This is the **NYCU 2025 Fall Introduction to Computer Graphic Final project**. It is the modification of the Github repo [Altas](https://github.com/LiamHz/atlas) 
### perlin-based_atlas 
adds some features and UI based on the original repo. See [Method I](https://github.com/timidtimmm/2025fall_intro2CG_atlas/new/main?filename=README.md#method-i---dynamic-environmentperlin-based_atlas)
### texture_mapping_method
use The AI Workflow and some texture mapping method plus some of the Method I to implement. See  [Method II](https://github.com/timidtimmm/2025fall_intro2CG_atlas/new/main?filename=README.md#method-ii---ai-assisted-terrain-pipelinetexture_mapping_method) 
## Project Objectives
- Visual Continuity & Realism:Smooth biome(生態系) transitions using linear interpolation (removing blocky artifacts).
- High Interactivity: Real-time UI for adjusting Season, Time, and Humidity.
- Radar Minimap for navigation.
- AI-Assisted Workflow: A complete pipeline from Text-to-Image to 3D Terrain.
## Method I - Dynamic Environment(perlin-based_atlas)
1. Dynamic Environment System:
- Seasons: Cherry blossoms in Spring; Snow coverage in Winter...
- Time System: Day, Dusk, Night, and Dawn with dynamic sky color and fog adjustments.
- Humidity: Dynamically affects vegetation density and grass color saturation.
2. Visual Improvements:
- Biome Shading: Replaced hard thresholds with Height Normalization and Linear Interpolation for continuous color transitions.
- Texture Splatting: Multi-layer texturing based on height and slope.
## Method II - AI-Assisted Terrain Pipeline(texture_mapping_method)
1. The AI Workflow: Text Prompt → Stable Diffusion → Depth Estimation → OpenGL
2. Pipeline Steps:
- Satellite Image Generation: Use Stable Diffusion with prompts (e.g., "Volcanic island") to generate realistic top-down views.
- Depth Estimation: Apply Depth Anything V2 to convert the 2D image into a grayscale Heightmap.
- 3D Reconstruction: OpenGL reads the heightmap for Grid Displacement.
## Novelty
1. Radar Minimap and global map to **know where are you**
2. Special Terrain Showcase:
- The Spiral Island: Demonstrates the ability to generate non-natural geometric structures.
- The Volcano: Features a crater lake rendered with our physical water shader.
## Summary:
- Developed an interactive system with Visual Semantics and real-time control.
- Successfully integrated Generative AI into the OpenGL rendering pipeline.
## Future Work:
- Integrate ControlNet for precise local feature control (e.g., river paths).
- Implement Erosion Simulation and LOD (Level of Detail) techniques.
## Demo video
[video]()
## How to execute them?
just execute
```
g++ perlin-based_atlas/main.cpp perlin-based_atlas/lib/glad.c -Iinclude -Llib -lglfw3 -lopengl32 -lgdi32 -luser32 -lkernel32 -lshell32 -lsetupapi -o perlin-based_atlas/build/atlas.exe
.\build\atlas.exe
```
in each of the directory
## Notice 
**perlin-based_atlas** is based from the Github repo [Altas](https://github.com/LiamHz/atlas), and the **texture_mapping_method** is based from it, too. They are different but related. 















