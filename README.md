Mali Offline Compiler Plugin for Unreal Engine 4
================================================

Using the Plugin
----------------

Inside the **Material Editor** and the **Material Instance Editor**, just click the **Offline Compiler** button to
bring up the **Mali Offline Compiler** tab. This tab can be docked anywhere you want it. Then, choose which Mali GPU
you want to see statistics for, and click the **Compile** button. This will begin compilation of your material.

When compilation is complete, you should see a compilation report. First, there will be a **Statistics Summary**.
This shows a short list of the representative shaders for your material, and how they are expected to run on the Mali
GPU you chose. With these statistics, you can experiment and see how changing the material affects performance
without having to run your application on a real device.

Below the summary, there will be a number of expandable drop downs. Each of these drop downs corresponds to one or
more of the **Usage** options selected in the **Details** tab of the **Material Editor**. Inside, you can see each of
the shaders compiled for the material, for each and every situation that material might be displayed in.

Shader statistics are unsupported when editing **Material Functions**.

Building from Source
--------------------

First, check out Unreal Engine 4 using git. You can do this by following the instructions at [UE4 on GitHub](
https://www.unrealengine.com/ue4-on-github).

Once you've got the engine checked out locally, navigate to the root folder, then, from your git shell, run:

```bash
git submodule add -f https://github.com/ARM-software/malioc-ue4.git Engine/Plugins/MaliOfflineCompiler
```

This will add the plugin as a [submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules) to Unreal Engine.

You might need to go into **Engine/Plugins/MaliOfflineCompiler** and checkout a specific branch for the version of Unreal
Engine you are using.

Finally, follow the instructions at [Building Unreal Engine 4 from Source](
https://docs.unrealengine.com/latest/INT/Programming/Development/BuildingUnrealEngine/index.html) to build the engine
and the plugin on your platform.

Building for Launcher Distributions
-----------------------------------

The easiest, but still somewhat convoluted, method of compiling the Offline Compiler Plugin in a way that is
compatible with
builds of Unreal Engine 4 distributed through the Epic Launcher is as follows:

* Using the Epic Launcher, download the version of Unreal Engine that you want to build the plugin for.
* Launch this version of Unreal Engine. In the **Unreal Project Browser**, select the **New Project** tab, select the
**C++** tab, and select the **Basic Code** template. Name the project **MaliOCProject**, and click **Create
Project**. Make note of where the project was created.
* The Unreal Editor and the IDE for your platform will automatically open after the initial compilation is complete.
Close both.
* Go into the **MaliOCProject** folder, where the project you just created is located.
* Clone the **Offline Compiler** repository into **MaliOCProject/Plugins/MaliOfflineCompiler**.
* Launch Unreal Engine again, select **MaliOCProject**, and click **Open**. A dialog will pop up asking you to rebuild
missing modules. Click **Yes**.
* The Unreal Editor will automatically open after the modules have been rebuilt. Close it again.
* Go into the **MaliOCProject/Plugins/MaliOfflineCompiler** folder, and delete the following files and folders:
  * .git
  * Intermediate
  * Scripts
  * .gitignore
* Archive the entire **Plugins** folder.
* Finally, to test that the build was successful, copy the **Plugins** folder into
<PathToWhereTheLauncherInstalledUnrealEngine>/Engine. There should already be a **Plugins** folder in here, so select
**Yes** when asked to merge folders.
* Launch Unreal Engine via the launcher and validate that the plugin loaded successfully and works correctly.