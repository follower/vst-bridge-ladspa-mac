LADSPA / VST Bridge (a.k.a. "VST Bridge" for Audacity)

Written by Leland Lucius
Copyright (C) 2006

Based on VST-Enabler by Dominic Mazzoni
Copyright (C) 2003

Based on vstladspa and vstsdk by Kjetil S. Matheussen / Notam
Copyright (C) 2003

To use with Audacity: just copy vst-bridge.so or vst-bridge.dll
to the Plug-ins folder inside of your Audacity folder.

The Mac version is a universal binary.

This library makes it possible for programs which support
the LADSPA API to load VST plug-ins.  Simply place the library
into any folder where LADSPA plug-ins are loaded, and it will
automatically load all VST plug-ins and make them appear as
LADSPA plug-ins.

VST plugins are searched for in this order:

1)  The current directory
2)  The path specified by the VST_PATH environment variable,
    if any

On Windows:
3)  The first of the following to be found:
    HKEY_CURRENT_USER\Software\VST\VSTPluginsPath
    HKEY_LOCAL_MACHINE\Software\VST\VSTPluginsPath
    C:\Program Files\Steinberg\VSTPlugins

On OSX:
3)  The users ~/Library/Audio/Plug-Ins/VST directory
4)  The system /Library/Audio/Plug-Ins/VST directory

Changes for version 1.1:

   OSX Version:
      Ssupports older (OS9) plugins
      Fixed a couple of minor bugs

   Windows version:
      Searches for VST plugins in standard directories
