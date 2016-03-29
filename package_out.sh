#!/bin/bash
archive="ActorX_All.zip"

rm -f $archive
cd out

cp ../readme.txt ../LICENSE.txt .
echo pkzipc -add -recurse -directories ../$archive -level=9 "*.*"
pkzipc -add -recurse -directories ../$archive -level=9 "*.*"
rm readme.txt LICENSE.txt

cd ..
