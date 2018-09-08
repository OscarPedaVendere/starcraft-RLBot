# jFlap SC:BW Reinforcement Learning Agent

This bot was made by jFlap. This bot is released under GNU GPL-3.0 License.
Sep 2018.
My personal Starcraft BW Reinforcement Learning Agent:
This is an implemented environment to test the capacity of RL TD Learning under Small-scale combat in the RTS Game Starcraft:Brood War.
Although the bot has been entirely written by myself, it is an implementation of a research paper called *Applying Reinforcement Learning to Small Scale Combat in the Real-Time Strategy Game StarCraft:Broodwar*.
You can find the paper in this repository.
The bot was written in C++, as it is mandatory to develp a bot in SC:BW using native BWAPI and not an alternative mirror.


## Getting Started Developing


## Prerequisites
This bot was developed using VS C++ 2017 Community and BWAPI 4.2.0 (latest release). C++ Boost libraries also are needed in order to serialize data from and to files allowing the agent to store his learned memory.


## VS Community 2017
You need a working version of **Visual Studio 2017** to build and improve the bot.
This project has been developed with the Platform Toolset v141. You should have it as it comes with Visual Studio 2017.
Some settings are needed to make the bot working:
* On the project settings window you need to set
* * `General->Project Defaults->Configuration Type` to `Dynamic Library (.dll)`
* * `General->Platform Toolset` to `Visual Studio 2017 (v141)`
* * In `Linker->Input->Additional Dependecies` you have to include the `libboost_serialization-vc141-mt-x32-1_67.lib` and `libboost_serialization-vc141-mt-gd-x32-1_67.lib` built libraries that come with this project


## BWAPI 4.2.0

**BWAPI Version 4.2.0** is needed in order to build properly the bot.
The installer or release binaries can be found on the official github page:
https://github.com/bwapi/bwapi/releases/tag/v4.2.0


## Boost Libraries
**Boost Libraries** are needed to serialize the agent's memory comfortably into a file without worrying about how the file is itself written.
Although you can find any version of Boost Libraries online on the official site, the ones that come with this repo are builded with the same platform toolset of the project.
So if you download the whole project and overwrite the include folder you should be ready to go.


## Configuring: RELEASE and DEBUG targets

### RELEASE target
The default target is Release (Win32) that is the main target you want to have as you commit changes in your project.

### DEBUG target
The Debug target can be used but debugging in BWAPI is not straightforward as it could seem.
To know how to debug in BWAPI refer to the official wiki:
https://github.com/calebthompson/bwapi/blob/master/wiki/Debugging.wiki

## About the Project

### Training the Agent

### Documentation
Working on it..


# Improve this bot
