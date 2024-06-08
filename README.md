This is a repository of the modified version of the unrealgt plugin

Installation
The UnrealGT plugin should be built from Source in an existing project. This requires you to build your project from source manually. (Similar to the Airsim Plugin build process)

1.	Clone this repository into your projects plugin Directory.

2.	Generate the project files (Cmake, Visual Studio Solution, Vscode workspace…) files by right clicking your projects .uproject file on Windows/macOS or by with GenerateProjectFiles.sh on linux.

3.	Build & Run the generated solution/cmake project from your IDE or the CLI.
4.	Select DebugGameEditor or DevelopmentGameEditor as Target.

5.	After the Editor has launched goto Edit > Plugins and enable the UnrealGT plugin.


Adding the Camera Module
1.	Find the ‘Recording_camera5.uasset’ file in the repo folder /Content/Asset
2.	Create a folder in the Unreal Engine editor ‘content Drawer’ 
3.	Copy and paste the uasset file in the folder
*Note, if you rename the uasset file to something else, the SIG-ATRID script should be modified accordingly


Documentation & Info: https://unrealgt.github.io/

*build plugin according to: https://dev.epicgames.com/community/learning/tutorials/qz93/unreal-engine-building-plugins