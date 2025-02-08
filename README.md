## Summary
This project adds some crude TAS tools to A Hat in Time. I made this back in 2020 and don't plan on continuing development on it as such, but I'll look at any PRs made. Supports game manifest 7770543545116491859.

Understandable made a setup tutorial here: https://www.youtube.com/watch?v=PH0gWy6cIrY.
More info can be found in the Boop Troop Discord server.

## Building
Should be standard CMake.

## Usage
#### PREREQUISITES for demo.py:
- Python 3 (https://www.python.org/downloads/, "Latest Python 3 Release")
- windows-curses ("pip install windows-curses" in command prompt after installing Python and ensuring that pip is in your path, as well as that the PATH has updated)

#### HOW TO USE:
0a. Ensure you have the "TAS patch" of the game. You can find instructions here: https://docs.google.com/document/d/1yJRN3S7sMddmvfz8LA-1N3xxc8_wauo56s-CfHkWuxY/edit.

0b. Ensure you have a (possibly virtual) Xbox 360 controller connected. x360ce (https://github.com/x360ce/x360ce/releases/latest) usually works fine.

1. Open kidloader.exe. It might say untrusted source, because I don't know how to get verified by Microsoft.
2. If all goes well, it should just be able to run in the background -- the program will then inject kiddll.dll into the game when launching, providing the TAS tools.
3. (optional) check if the TAS tools are loaded in the game by hitting right shift with the game focused. It should start frame advance mode. Hit left control to exit frame advance mode.
4. Open demo.py, either by double clicking, or if that doesn't work, running it from a command prompt like "python demo.py".

#### demo.py:
If all goes well, you should see a terminal window with a list of modes; "None", "Record", and a list of .json, .htas, or .list files.
In-game, the active mode will change to what you choose here (with up and down arrow keys) whenever a level is loaded/reloaded.

**None**: this does nothing, you can play like normal.

**Record**: this enters recording mode. The program will record all of your inputs in the last level, which can then be saved by hitting the "S" key while focused on the command prompt. Note that you save the last level's inputs, meaning that if you e.g. do a Gallery IL, you should first hit "S" once you're back in the hub, or have used RestartIL or similarly reloaded the level.

**\<anything else>**: this enters playback mode. The program will, upon the next level load, start playing back inputs from .json, .htas, or .list files (the one you're focused on). .json is usually from a recording, and .htas is usually an actual TAS. .list can be multiple of either.

#### TROUBLESHOOTING:
Make sure you open kidloader.exe BEFORE the game, because it has to update some stuff in the game very early in its startup process.

Always open the game BEFORE demo.py.

If you close the game OR demo.py, you will need to re-launch both to have it work. You do not need to relaunch kidloader.exe.

If your game "crashes" when frame advancing, there's a registry edit you can do to increase the time an application can be unresponsive before Windows kills it. If the key does not exist, create it and logout/reboot your computer. The key is HKEY_CURRENT_USER\Control Panel\Desktop\HungAppTimeout. The value should be a string specifying the delay in milliseconds (e.g. 3600000 for one hour).

If the game freezes during TAS playback in Spaceship, make sure that RAND is set to at least two values from the first frame onwards. Rumbi's game logic will keep calling rand() until it gets a different result from last time, causing an infinite loop when RNG is fixed.
