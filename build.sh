#!/bin/bash

mkdir obj

makefile=obj/ActorX_MAYA.mak
./genmake ActorX.project TARGET=vc-win32 > $makefile

vc32tools --version=10 --64 --make $makefile
