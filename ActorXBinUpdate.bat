
@echo "Copying Max Files...."

xcopy ActorX_3DSMAX\Win32\Release_Max9.0\ActorX.dlu ..\..\..\..\Binaries\ActorX\Max90\ /y 
xcopy ActorX_3DSMAX\x64\Release_Max9.0\ActorX.dlu ..\..\..\..\Binaries\ActorX\Max90_x64\ /y 
xcopy ActorX_3DSMAX\Win32\Release_Max2008\ActorX.dlu ..\..\..\..\Binaries\ActorX\Max2008\ /y 
xcopy ActorX_3DSMAX\x64\Release_Max2008\ActorX.dlu ..\..\..\..\Binaries\ActorX\Max2008_x64\ /y 
xcopy ActorX_3DSMAX\Win32\Release_Max2009\ActorX.dlu ..\..\..\..\Binaries\ActorX\Max2009\ /y 
xcopy ActorX_3DSMAX\x64\Release_Max2009\ActorX.dlu ..\..\..\..\Binaries\ActorX\Max2009_x64\ /y 
xcopy ActorX_3DSMAX\Win32\Release_Max2010\ActorX.dlu ..\..\..\..\Binaries\ActorX\Max2010\ /y 
xcopy ActorX_3DSMAX\x64\Release_Max2010\ActorX.dlu ..\..\..\..\Binaries\ActorX\Max2010_x64\ /y 

@echo "Copying Maya Files...."

xcopy ActorX_MAYA\Win32\Release_Maya8.5\ActorX.mll ..\..\..\..\Binaries\ActorX\Maya85\ /y 
xcopy ActorX_MAYA\x64\Release_Maya8.5\ActorX.mll ..\..\..\..\Binaries\ActorX\Maya85_x64\ /y 
xcopy ActorX_MAYA\Win32\Release_Maya2008\ActorX.mll ..\..\..\..\Binaries\ActorX\Maya2008\ /y 
xcopy ActorX_MAYA\x64\Release_Maya2008\ActorX.mll ..\..\..\..\Binaries\ActorX\Maya2008_x64\ /y 
xcopy ActorX_MAYA\Win32\Release_Maya2009\ActorX.mll ..\..\..\..\Binaries\ActorX\Maya2009\ /y 
xcopy ActorX_MAYA\x64\Release_Maya2009\ActorX.mll ..\..\..\..\Binaries\ActorX\Maya2009_x64\ /y 
xcopy ActorX_MAYA\Win32\Release_Maya2010\ActorX.mll ..\..\..\..\Binaries\ActorX\Maya2010\ /y 
xcopy ActorX_MAYA\x64\Release_Maya2010\ActorX.mll ..\..\..\..\Binaries\ActorX\Maya2010_x64\ /y 

pause