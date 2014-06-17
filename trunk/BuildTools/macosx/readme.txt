*****************************************************************
*** Build Instructions for GeoDa.  Current as of GeoDa 1.5.23 ***
*****************************************************************

Overview: We assume the build machine hosts a recently-installed
clean OS.  This build guide contains notes on setting up the compile
environment, obtaining the GeoDa source files and dependent libraries,
compiling libraries and GeoDa, and finally packaging the program
for distribution and installation.

****************************************************
*** Building GeoDa for 64-bit OSX 1.6.8 or later ***
****************************************************

NOTE: This is just basic placeholder for now!  Not currently complete.

Build machine assumptions:
- clean OSX 10.6.8 installation with all OS updates.
- note the current build script will likely only work on OS 10.6.8
  and not later versions of OSX.  It is very difficult to target
  previous versions of OSX, so we have chosen to build on the minimum
  version supported.

1. Install XCode 3.2
 - this will result in Xcode and all c++ compilers as well as svn
 being installed.

2. Use SVN to check out GeoDa trunk:
 - From user's home directory: ~/
 - svn co https://geodacenter.repositoryhosting.com/svn/geodacenter_geoda/trunk trunk
 
3. cd to ~/trunk/BuildTools/macosx

4. run ./build.sh 8 to download and build GeoDa and everything it
depends upon.
 - The number 8 should be changed to however many individual cores the
 build machine has available for parallel compilation.

5. Package GeoDa for distribution / installation.

