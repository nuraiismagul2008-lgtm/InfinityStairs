SCP-087 Infinite Stairs - how to open and build (Visual Studio 2022, Windows, x64)

1. Unzip the whole archive to any folder (the path can contain any letters).
2. Open SCP-087_FinalProject.slnx (or the old .sln, both are the same). Choose configuration "Debug" and platform "x64".
3. Right click the project you want -> Set as Startup Project -> Ctrl+F5.
     SCP-087_FinalProject : the full beta (Scene.cpp)
     CorePipeline         : part 1 only, the base for the team (CorePipeline\Core_Pipeline.cpp)

Notes
- Include and lib paths are relative ($(ProjectDir)Libraries), no absolute paths any more.
- glad.c is now inside the project folder (before it was taken from ..\Assignment2).
- Libraries\lib\glfw3.lib is a Debug x64 build (it uses the debug runtime), so only Debug|x64 is configured.
  For Release you need a Release glfw3.lib (x64, built with the same runtime).
