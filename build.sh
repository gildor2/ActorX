#!/bin/bash

# create directory for placing makefiles
mkdir -p obj

# function for building single target
# usage: build <target-name> [verbose]
function build()
{
	local cfg sdk host year amd64 libdir vctool_opts
	cfg=$1

	# check host application
	if [[ $cfg = Maya* ]]; then
		host="Maya"
		year=${cfg:4:4}
		amd64=${cfg:8}
	elif [[ $cfg = Max* ]]; then
		host="Max"
		year=${cfg:3:4}
		amd64=${cfg:7}
	else
		echo "ERROR: unknown target $cfg"
		exit
	fi

	# x64 and x86 differs in libraries only; x64 libs are either in sdk_x64/lib or sdk/x64/lib
	sdk="SDK/$host$year"
	if ! [ -d "$sdk" ] && [ "$amd64" ]; then
		# no directlry, try _x64 suffix
		sdk=${sdk}_x64
		libdir=$sdk/lib
	elif [ "$amd64" ]; then
		libdir=$sdk/x64/lib
	else
		libdir=$sdk/lib
	fi

	# check SDK for selected platform
#	echo "... testing config: $cfg host=$host year=$year amd64=$amd64 -> $sdk"
	if ! [ -d $libdir ]; then
#		echo "... NOT FOUND $sdk ($libdir)"	#!!!
		if [ "$2" ]; then
			echo ERROR: SDK for $cfg was not found!
		fi
		return
	fi
#	echo "FOUND $sdk ($libdir)"

	echo
	echo "----------------- Building $cfg -----------------"
	echo

	# check bitness (32/64)
	if [ $amd64 ]; then
		vctool_opts="--64"
	else
		vctool_opts=""
	fi

	# generate makefile
	makefile=obj/ActorX_$cfg.mak
	./genmake ActorX_$host.project SDK_INC=$sdk/include SDK_LIB=$libdir CFG=$cfg VER=$year TARGET=vc-win32 > $makefile
	# build
	vc32tools --version=12 $vctool_opts --make $makefile
}


# build specific version if needed (may specify multiple versions)
if [ "$1" ]; then
	for ver in $*; do
		build "$ver" 1
	done
	exit
fi

# build all known targets
for host in "Max" "Maya"; do
	for ver in {2012..2018}; do
		for amd64 in "" "_x64"; do
			cfg=${host}${ver}${amd64}
			build $cfg
		done
	done
done
