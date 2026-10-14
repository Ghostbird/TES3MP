TES3MP
======

OpenMW is an open-source open-world RPG game engine that supports playing Morrowind by Bethesda Softworks. You need to own the game for OpenMW to play Morrowind.

OpenMW also comes with OpenMW-CS, a replacement for Bethesda's Construction Set.

* Version: 0.52.0
* License: GPLv3 (see [LICENSE](https://gitlab.com/OpenMW/openmw/-/raw/master/LICENSE) for more information)
* Website: https://www.openmw.org
* IRC: #openmw on irc.libera.chat
* Discord: https://discord.gg/bWuqq2e


Font Licenses:
* DejaVuLGCSansMono.ttf: custom (see [files/data/fonts/DejaVuFontLicense.txt](https://gitlab.com/OpenMW/openmw/-/raw/master/files/data/fonts/DejaVuFontLicense.txt) for more information)
* DemonicLetters.ttf: SIL Open Font License (see [files/data/fonts/DemonicLettersFontLicense.txt](https://gitlab.com/OpenMW/openmw/-/raw/master/files/data/fonts/DemonicLettersFontLicense.txt) for more information)
* MysticCards.ttf: SIL Open Font License (see [files/data/fonts/MysticCardsFontLicense.txt](https://gitlab.com/OpenMW/openmw/-/raw/master/files/data/fonts/MysticCardsFontLicense.txt) for more information)

Project status
--------------

The main quests in Morrowind, Tribunal and Bloodmoon are all completable. Some issues with side quests are to be expected (but rare). Check the [bug tracker](https://gitlab.com/OpenMW/openmw/-/issues/?milestone_title=openmw-1.0) for a list of issues we need to resolve before the "1.0" release. Even before the "1.0" release, however, OpenMW boasts some new [features](https://wiki.openmw.org/index.php?title=Features), such as improved graphics and user interfaces.
[Version changelog](https://github.com/TES3MP/TES3MP/blob/master/tes3mp-changelog.md)

As of version 0.8.1, TES3MP is fully playable, providing very extensive player, NPC, world and quest synchronization, as well as state saving and loading, all of which are highly customizable via [serverside Lua scripts](https://github.com/TES3MP/CoreScripts).

Remaining gameplay problems mostly relate to AI and the fact that clientside script variables need to be placed on a synchronization whitelist to avoid packet spam.

TES3MP now also has a [VR branch](https://github.com/TES3MP/TES3MP/tree/0.8.1-vr) that combines its code with that of Mads Buvik Sandvei's [OpenMW VR](https://gitlab.com/madsbuvi/openmw).

Donations
---------------

You can benefit the project by donating on Patreon to our two developers, [David Cernat](https://www.patreon.com/davidcernat) and [Koncord](https://www.patreon.com/Koncord), as well as by supporting [OpenMW](https://openmw.org).

Contributing
---------------

Helping us with documentation, bug hunting and video showcases is always greatly appreciated.

For code contributions, it's best to start out with modestly sized fixes and features and work your way up. There are so many different possible implementations of more major features – many of which would cause undesirable code or vision conflicts with OpenMW – that those should be talked over in advance with the existing developers before effort is spent on them.

Feel free to contact the [team members](https://github.com/TES3MP/TES3MP/blob/master/tes3mp-credits.md) for any questions you might have.

Getting started
---------------

* [Official forums](https://forum.openmw.org/)
* [Installation instructions](https://openmw.readthedocs.io/en/latest/manuals/installation/index.html)
* [Build from source](https://wiki.openmw.org/index.php?title=Development_Environment_Setup)
* [Testing the game](https://wiki.openmw.org/index.php?title=Testing)
* [How to contribute](https://wiki.openmw.org/index.php?title=Contribution_Wanted)
* [Report a bug](https://gitlab.com/OpenMW/openmw/issues) - read the [guidelines](https://wiki.openmw.org/index.php?title=Bug_Reporting_Guidelines) before submitting your first bug!
* [Known issues](https://gitlab.com/OpenMW/openmw/issues?label_name%5B%5D=Bug)

The data path
-------------

The data path tells OpenMW where to find your Morrowind files. If you run the launcher, OpenMW should be able to pick up the location of these files on its own, if both Morrowind and OpenMW are installed properly (installing Morrowind under WINE is considered a proper install).

Command line options
--------------------

    Syntax: openmw <options>
    Allowed options:
      --config arg                          additional config directories
      --replace arg                         settings where the values from the
                                            current source should replace those
                                            from lower-priority sources instead of
                                            being appended
      --user-data arg                       set user data directory (used for
                                            saves, screenshots, etc)
      --resources arg (=resources)          set resources directory
      --help                                print help message
      --version                             print version information and quit
      --data arg (=data)                    set data directories (later directories
                                            have higher priority)
      --data-local arg                      set local data directory (highest
                                            priority)
      --fallback-archive arg (=fallback-archive)
                                            set fallback BSA archives (later
                                            archives have higher priority)
      --start arg                           set initial cell
      --content arg                         content file(s): esm/esp, or
                                            omwgame/omwaddon/omwscripts
      --groundcover arg                     groundcover content file(s): esm/esp,
                                            or omwgame/omwaddon
      --no-sound [=arg(=1)] (=0)            disable all sounds
      --script-all [=arg(=1)] (=0)          compile all scripts (excluding dialogue
                                            scripts) at startup
      --script-all-dialogue [=arg(=1)] (=0) compile all dialogue scripts at startup
      --script-console [=arg(=1)] (=0)      enable console-only script
                                            functionality
      --script-run arg                      select a file containing a list of
                                            console commands that is executed on
                                            startup
      --script-warn [=arg(=1)] (=1)         handling of warnings when compiling
                                            scripts
                                            0 - ignore warnings
                                            1 - show warnings but consider script as
                                            correctly compiled anyway
                                            2 - treat warnings as errors
      --load-savegame arg                   load a save game file on game startup
                                            (specify an absolute filename or a
                                            filename relative to the current
                                            working directory)
      --skip-menu [=arg(=1)] (=0)           skip main menu on game startup
      --new-game [=arg(=1)] (=0)            run new game sequence (ignored if
                                            skip-menu=0)
      --encoding arg (=win1252)             Character encoding used in OpenMW game
                                            messages:

                                            win1250 - Central and Eastern European
                                            such as Polish, Czech, Slovak,
                                            Hungarian, Slovene, Bosnian, Croatian,
                                            Serbian (Latin script), Romanian and
                                            Albanian languages

                                            win1251 - Cyrillic alphabet such as
                                            Russian, Bulgarian, Serbian Cyrillic
                                            and other languages

                                            win1252 - Western European (Latin)
                                            alphabet, used by default
      --fallback arg                        fallback values
      --no-grab [=arg(=1)] (=0)             Don't grab mouse cursor
      --export-fonts [=arg(=1)] (=0)        Export Morrowind .fnt fonts to PNG
                                            image and XML file in current directory
      --activate-dist arg (=-1)             activation distance override
      --random-seed arg (=<impl defined>)   seed value for random number generator
* [Quickstart guide](https://github.com/TES3MP/TES3MP/wiki/Quickstart-guide)
* [Steam group](https://steamcommunity.com/groups/mwmulti) and its [detailed FAQ](https://steamcommunity.com/groups/mwmulti/discussions/1/353916184342480541/)
* [TES3MP section on OpenMW forums](https://forum.openmw.org/viewforum.php?f=45)
* [Discord server](https://discord.gg/ECJk293)
* [Subreddit](https://www.reddit.com/r/tes3mp)
* [Known issues and bug reports](https://github.com/TES3MP/TES3MP/issues)
