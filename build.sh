#!/bin/bash

# create directory for placing makefiles
mkdir -p obj

# function for building single target
# usage: build <target-name> [error-display]
function build()
{
	local cfg sdk host vctool_opts genmake_opts
	cfg=$1

	# check host application
	if [[ $cfg = *Maya* ]]; then
		host="Maya"
		sdk=SDK/$cfg
		year=${cfg:4:4}
	elif [[ $cfg = *Max* ]]; then
		host="Max"
		sdk=SDK/${cfg:0:7}		# Max has single SDK for 32 and 64 bit versions
		year=${cfg:3:4}
	else
		echo "ERROR: unknown target $cfg"
		exit
	fi

#	echo testing config: $cfg sdk=$sdk year=$year

	if [ -d $sdk ]; then

		echo
		echo "----------------- Building $cfg -----------------"
		echo

		# check bitness (32/64)
		if [[ $cfg = *_x64 ]]; then
			vctool_opts="--64"
			genmake_opts="amd64=1"
		else
			vctool_opts=""
			genmake_opts=""
		fi

		# generate makefile
		makefile=obj/ActorX_$cfg.mak
		./genmake ActorX_$host.project $genmake_opts SDK=$sdk CFG=$cfg VER=$year TARGET=vc-win32 > $makefile
		# build
		vc32tools --version=10 $vctool_opts --make $makefile

	elif [ "$2" ]; then

		echo ERROR: SDK for $cfg was not found!

	fi
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
	for ver in {2012..2016}; do
		for amd64 in "" "_x64"; do
			cfg=${host}${ver}${amd64}
			build $cfg
		done
	done
done
