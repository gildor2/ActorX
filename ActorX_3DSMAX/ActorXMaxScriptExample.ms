----------------------------------------
-- ActorX Copyright 1998-2008 Epic Games, Inc. All Rights Reserved. ----
-- MaxScript Batch Export example ------
-- Rev. 1.0  - created by Erik deNeve --
----------------------------------------

-- All the usual ActorX actions are exposed as MaxScript commands, through the "AX" plug-in object.
-- Use the examples below to create batch scripts to automate repetitive tasks, like 
-- saving out large numbers of animation sequences from several files.  

-- For the sake of batching, using the exporter's commands from script will suppress all the usual 
-- ActorX confirmation dialogs, except for any encountered errors, which will halt the script.

-- The ActorX plug-in, version 2.18 or later needs to be installed for this script file to work.
-- Find the latest plug-ins for the Unreal Engine at http://udn.epicgames.com/Two/ActorX



-- Start by loading any max scene file as follows. Note the double backslashes.

loadMaxFile( "c:\\MyScenes\\MyCharacter.max" )


-- Accessors for the exporter settings. Equivalents of the checkboxes in the GUI,
-- but these will only affect data exported from script commands and won't
-- change the GUI settings.

AX.setallskintype 1
AX.setalltextured 1
AX.settangentuvsplit 0
AX.setbakesmoothing 0
AX.setcullunuseddummies 0
AX.setselectedgeometry 0

-- Set the output path for all animation/mesh files - as well as for the log files.
AX.setoutputpath( "C:\\MyOutputFolder\\" )

-- Digest an animation into the buffer. 
-- digestanim takes four parameters: 
--       sequence name (string)
--       first frame (integer)
--       last frame (integer)
--       forced rate (floating point) Note: use 0 to obtain the rate as defined in the scene.

AX.digestanim "firstsequence"   11 20  0
AX.digestanim "secondsequence"  21 40  0
AX.digestanim "thirdat90fps"  21 40  90.0


-- Note that for batch purposes, you can digest multiple sequences from multiple MAX scenes, 
-- loading new scenes in script as you go, and save the final result with a single 
-- exportanimset command.

-- Save all digested animations to a .psa file.  This also has the effect of clearing 
-- them from the internal buffer.

AX.exportanimset "NewAnimSetFromScript"

-- Save the mesh and reference skeleton .psk file.
AX.exportmesh "NewMeshExportedFromScript"

-- Enable smoothing group baking by splitting vertices.
AX.setbakesmoothing 1 

-- Enable culling of dummies in the hierarchy that don't affect skin or other bones.
AX.setcullunuseddummies 1

-- Save the mesh again with the new settings.
AX.exportmesh "AlternativeNewMeshExportedFromScript"

-- the end
