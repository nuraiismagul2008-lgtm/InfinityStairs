Infinite Stairs: SCP-087 - Computer Graphics Fundamentals, final project (IT-2505)
Team: Nuray Ismagul (rendering pipeline), Kira Parunina (lighting and shadows), Dilyara Bauyrzhan (environment and assets)

PLATFORM
  Windows 10/11, x64, a GPU with OpenGL 3.3 Core. Visual Studio (toolset v143), C++ (MSVC).

LIBRARIES (all are in the archive, folder Libraries\)
  GLFW 3.4 (include\GLFW, lib\glfw3.lib), glad (include\glad, include\KHR, glad.c),
  stb_image.h (loads the PNG), stb_easy_font.h (on-screen text).
       - No GLM, no engine.

SHADERS
  All GLSL code is in the folder shaders\ (loaded at start-up from files, relative to the working directory):
    lighting_shared.glsl  spotlight + shadow mapping (PCF, normal offset, bias) + fog; added in front of scene.frag and dust.frag
    scene.vert / scene.frag    main pass (Phong / Blinn-Phong, flashlight, lamps, shadow test, fog)
    shadow.vert / shadow.frag  depth-only pass into the shadow map (FBO)
    dust.vert / dust.frag      dust particles lit by the flashlight
    face.vert / face.frag      billboard of the SCP face
    text.vert / text.frag      on-screen text
  A shader compile or link error is printed to the console. A missing shader file is printed too.

HOW TO BUILD AND RUN
  1. Unzip the whole archive.
  2. Open SCP-087_FinalProject.slnx, choose Debug | x64.
  3. Press Ctrl+F5 (Start Without Debugging).

  The program loads shaders\*.glsl, assets\textures\scp_face.png and assets\audio\gloomy_demonic_drone.wav
  relative to the working directory. From Visual Studio this is the project folder.
   *If you start the .exe by hand,
   start it from the folder that contains assets\, otherwise the shaders, the face and the sound are missing (a message is
   printed to the console).

HEADLINE TECHNIQUE
  Spotlight flashlight with shadow mapping (depth texture in an FBO, normal offset, bias, PCF) and
  depth-based fog. Shadows on/off: key H. Everything else can be switched too (see the keys).

KEYS (F1 shows the same list on the screen)
  Mouse   | look around                                                                  || Mouse wheel|   field of view
  W A S D | walk                                                                         || Shift      |  run
  Space   | PAUSE the animation (the mouse still looks around)                           || R          | back to the start    
  C       | free-fly camera without collisions (W A S D move, E up, Q down, Shift faster)|| , .        | smaller / bigger shadow bias  
  H       | SHADOWS on / off (headline technique)                                        || O          | shadow map size 512 / 1024 / 2048 / 4096
  P       | PCF: hard / 3x3 / 5x5                                                        || T          | textures  
  B       | random flicker of the lights                                                 ||  - =       | brightness
  F       | flashlight on / off                                                          ||[ ]         | cone angle 12-45 deg      
  G       | fog on / off                                                                 ||1 2 3       |linear / exponential / Gaussian fog
  4       | light in the fog                                                             ||5           |dust
  M       | antialiasing (MSAA)                                                          ||            |
  V       | debug view: final / shadow / light / fog                                     ||F1          | help window on the screen     
  I       | print the numbers of the lighting part to the console                        ||Esc         | exit

ON-SCREEN TEXT
  Top left: topic title, team names, floor, depth, FPS, state of shadows / flashlight / fog / dust.
  The window title also shows the team, the draw calls, triangles and all settings.

EVENTS (walk down the stairs)
  15 m   the lamps go out for 2 s
  48 m   red emergency light and a sound
  51 m   the scene gets dark, the SCP face appears; at distance < 4 m the face attacks (jump scare)
  The world is a loop of 6 floors, so the stairs never end


ASSETS, SOURCES AND LICENSES

Written by the team:
  - All shader code (folder shaders\), the geometry of the stairwell, the lamps and the dust.
  - Wall, metal and stair textures generated procedurally in the code.

External files:
  assets\textures\scp_face.png
    Source: Original visual design generated using OpenAI image generation specifically for this project.
    Description: Custom SCP-inspired face texture created for the game's jump-scare event.

  assets\audio\gloomy_demonic_drone.wav
    Source: Pixabay — Gloomy Demonic Drone.
    URL: https://pixabay.com/sound-effects/film-special-effects-gloomy-demonic-drone-dark-ambient-horror-atmosphere-543162/
    License: Pixabay Content License.

Idea and setting: SCP-087 from the SCP Foundation wiki.
Libraries: GLFW 3.4 (zlib license), glad (public domain / MIT), stb_image.h and stb_easy_font.h (public domain / MIT).
No other libraries are used (no GLM, no engine).