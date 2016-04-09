ActorX - Unreal Actor Exporter
==============================

Building the source code
------------------------

We are using own build system to compile ActorX. You may find a Perl script in `./genmake`. This script
generates makefiles from some human-friendly project format. After that you may build generated makefile
using `nmake` for Visual Studio or `make` for gcc.

There's a build script which iterates over all available SDKs and issues build for all available SDK platofrms.
This script could be found in `./build.sh`

ActorX is compiled using Visual Studio. Currently build is performed with Visual C++ 2010, but in theory
almost all Visual Studio versions should be supported (perhaps except Visual C++ 6.0 and Visual C++ 2001).

Build system utilizes GNU Tools for building, in particular - Bash and Perl. I've packaged Windows versions
of such tools which was a part of [MinGW/MSYS project](http://www.mingw.org/). You get may everything you need
for a build [here](https://github.com/gildor2/UModel/releases). This page contains **BuildTools.zip**. You should
download it and extract into some directory, let's say to *C:\BuildTools*. After that, put *C:\BuildTools\bin*
to the system *PATH* variable. Also it is possible to create a batch file which will temporarily tune *PATH* and
then execute build script. Here's an example of such file:

    @echo off
    set PATH=%PATH%;C:\BuildTools\bin
    bash build.sh

To launch a build process without a batch, simply execute

    bash build.sh


This command line will issue building ActorX plugin for all available supported SDKs. If you want to build a particular
version, use this command line format:

    bash build.sh <SDK_Ver>

(see description of SDK_Ver format below)


SDK
---

SDK are not distributed for legal reasons. You should download them from corresponding developer's sites and place
under /SDK directory (relative to the project's root). Directory layout for x86/x64 mixed platofrms is:

    /SDK
      /<SDK_Ver>
         /include
         /lib
         /x64
           /lib

`<SDK_Ver> ::= (Max|Maya)(Year)[_x64]`

All includes goes to `<SDK_Ver>/include`, x86 libraries to `<SDK_Ver>/lib`, x64 libraries to `<SDK_Ver>/x64/lib`.
For x64-only SDK's, use the following layout:

    /SDK
      /Max2014_x64
         /include
         /lib

All libraries should go to the `<SDK_Ver>/lib` directory. Suffix `_x64` of SDK's directory name is required.

Build script is intended to work with Max and Maya versions starting from 2012 year, but it should work well with older
versions too.

Web resources
-------------

[Plugin announcements and discussion thread on Gildor.org](http://www.gildor.org/smf/index.php/topic,1221.0.html)

[Original plugin home page](http://udn.epicgames.com/Three/ActorX.html)

License
-------

ActorX plugin is licensed under the [BSD license](LICENSE.txt).
