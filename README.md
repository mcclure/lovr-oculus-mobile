This is a repository for building LovrApp, a standalone Android app which is based on the [LÖVR](https://lovr.org) VR API.

# Usage

Most users do not need to build LovrApp themselves. For running your own Lua files you can sideload the LovrTest app, which you can find on the "Releases" section of this github repo, or [this page with instructions](https://lovr.org/docs/Getting_Started_(Android)).

## To build (all platforms):

* The submodule cmakelib/lovr has submodules. If you did not initially clone this repo with --recurse-submodules, you will need to run `(cd cmakelib/lovr && git submodule init && git submodule update)` before doing anything else.

* Install Android Studio

* Open Android Studio, go into Preferences, search in the box for "SDK" (or from the "Welcome to Android Studio" box, choose "Configure"->"SDK Manager"). Use the "Android SDK" pane and the "SDK Platforms" tab to download Android API level 23. Next, navigate to the "SDK Tools" tab of the same pane and check "Show Package Details". Under "NDK (Side by Side)" select "21.0.6113669", and under "CMake" check "3.6.4111459" (or whichever CMake is newest). Hit "Apply". Now quit Android Studio (we'll be doing the next part at the command line).

* Follow the additional platform steps below:

### To build (Macintosh)

* You need to build the gradle script in `cmakelib`, then run the installDebug target of the gradle script in `LovrApp/Projects/Android`. You can do this with the `gradlew` script in the root, but it will need the Android tools in `PATH` and the sdk install location in `ANDROID_HOME`. You can just run this at the Bash prompt from the repository root to do all of this:

      (export PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH ANDROID_HOME=~/Library/Android/sdk GRADLE=`pwd`/gradlew; (cd cmakelib && $GRADLE build) && (cd LovrApp/Projects/Android && $GRADLE installDebug)) && say "Done"


### To build (Windows)

* Unfortunately this is the only way I can get the build to work on Windows right now: Edit cmakelib/lovr/CMakeLists.txt and change LOVR_USE_LUAJIT and LOVR_ENABLE_AUDIO near the top from ON to OFF. This will make things run slightly slower and also disable audio.

* Follow the instructions under "creating a signing key" below in this README. (This is done automatically on Mac, but not on Windows.)

* Run the following from a cmd.exe window:

        set ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk
        set JAVA_HOME=C:\Program Files\Android\Android Studio\jre
        set PATH=%PATH%;%CD%

        pushd cmakelib
        gradlew build
        popd

        pushd LovrApp/Projects/Android
        gradlew installDebug
        popd

### To build (additional notes)

* You have to have turned on developer mode on your headset before deploying.
* You also have to enable USB debugging for the device.  For the Oculus Go, you can do this by plugging in the device, putting it on, and using the controller to accept the "Allow USB Debugging" popup.
* If you get a message about "signatures do not match the previously installed version", run this and try again:

        PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH adb uninstall org.lovr.appsample

* If all you have done is changed the assets, you can upload those by running only the final `installDebug` gradlew task. For example:

      (export PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH ANDROID_HOME=~/Library/Android/sdk GRADLE=`pwd`/gradlew; (cd LovrApp/Projects/Android && $GRADLE installDebug))

* To see all the things gradlew can do in a particular directory run it with "tasks" as the argument.
* The reason for the long `(export PATH`/ANDROID_HOME line is to get the java and android tools into scope for that line. It would also work to modify the env vars in your bashrc. But you *do* have to set the environment variables somehow or else you could run the wrong version of Java and get confusing errors like "Could not determine java version from '13'". 

Help would be appreciated if you know how to do any of the following: Make the build work in Android Studio, build on Windows with audio, or make the build work in a single gradle pass without having to invoke `gradlew` twice.

## Adding game code:

You have three options:

1. Put the game code in `LovrApp/assets`.
2. Create a directory on your hard drive with your code in it, and create a file `LovrApp/local_assets` (or `LovrApp/local_assets.txt`) containing the **path** to the code.
3. Create a directory and add the path to it to the `assets.srcDirs = ['../../assets', '../../../your/path/here']` line of `LovrApp/Projects/Android/build.gradle`.

Files in any of these directories will be uploaded when LovrApp's installDebug runs.

Option #2 is best if you are building from git, since local_assets is in gitignore.

## To create your app:

Decide on a name for your app and also an identifier (this is something like "com.companyname.gametitle").

Edit `LovrApp/Projects/build.gradle`. Change "project.archivesBaseName" and "applicationId" to reflect your identifier.

Edit `LovrApp/Projects/Android/AndroidManifest.xml`. Change "package=" at the top to your identifier and change "android:label=" partway down (right after YOUR NAME HERE) to your name.

# To ship your app:

LovrApp is set up so that a "debug" build uses Oculus's recommended settings for development and a "release" build uses the appropriate settings for store submission on Quest. The store submssion requirements do change from time to time, so you may find the store backend asks for additional configuration changes at upload time. See [this page](https://developer.oculus.com/distribute/latest/concepts/publish-mobile-manifest/) for the most up to date Oculus Store configuration requirements.

## Creating a signing key

Before uploading anything to the Oculus Store, you should make sure you are using the right keystore. If you build without specifying a keystore, gradle will create one for you. It is very important you back this up in a safe place **before** you upload anything to the Oculus dashboard, as you cannot change it later so losing your keystore means getting locked out from updating your own app.

Better than just backing it up would be creating your own keystore, since you can input your business name and a password:

    keytool -genkey -v -keystore YOURNAME-key.jks -keyalg RSA -keysize 2048 -validity 10000 -alias YOURNAME

The easiest way to install your new keystore is to put a copy at the exact path `LovrApp/Projects/Android/android.debug.keystore`

## If you want to ship on Oculus Go

The build scripts in this repo create release builds for Quest. As of this writing, there are some steps you must perform if you want to submit a release build to the Go store. First, edit `VrApp.gradle`. There are two instances of `v2SigningEnabled true`. Change both of these to say

    v1SigningEnabled true

Next, edit `LovrApp/Projects/Android/AndroidManifest.xml`. **Delete** this line:

  <uses-feature
      android:name="android.hardware.vr.headtracking"
      android:version="1"
      android:required="false"
      />

## If you want to run on Samsung Gear VR

The build scripts in this repo are set up for Oculus Go and Quest. If you want to ship on Samsung Gear there are additional steps. You must set up an "osig" file as described on the Oculus website. You also may want to search all gradle files for instances of:

    abiFilters 'arm64-v8a'

and replace it with:

    abiFilters 'armeabi-v7a','arm64-v8a'

and then do another build. This will allow your build to run on 32-bit phones.

## To build the autoloader test app:

When built without any changes, this repo produces an "org.lovr.appsample" app that prints a "game not found" message. If you look in the Github "releases" section, however, you'll find a "org.lovr.test" app that loads a game from the SD card where you can easily copy it using `adb sync`. Run the app for full instructions.

To build the `org.lovr.test` app yourself:

* `git clone` a copy of [[https://github.com/mcclure/lodr]]. Save the path to the `lodr` directory.
* In the lovr-oculus-mobile repo, in the `LovrApp` directory, create a file `local_assets.txt` containing the path to the `lodr` directory.
* Edit the file `LovrApp/Projects/build.gradle` and change the "archivesBaseName" to `test`.

The command to upload a Lua project to the SD card so `org.lovr.test` can run it is:

    adb push --sync . /sdcard/Android/data/org.lovr.test/files/.lodr

## Debugging:

If you get a crash, select the crash report in `adb logcat`, then run this to extract the crash report and look up line numbers:

    pbpaste | ~/Library/Android/sdk/ndk-bundle/prebuilt/darwin-x86_64/bin/ndk-stack -sym ./LovrApp/Projects/Android/build/intermediates/transforms/mergeJniLibs/debug/0/lib/arm64-v8a

# Contributing

## Upgrading

This repository has a branch "oculus-original-sdk". As commits, this branch has versions of some versions of official Oculus Mobile SDK releases (with those samples which are not necessary, notably the ones with large embedded video samples, deleted).

If you need to upgrade the version of Oculus Mobile SDK used by this release, you should do so by checking out the "oculus-original-sdk" branch, unpacking the latest ovr_sdk_mobile package into the repository, committing, and then merging back into the master branch. This will ensure that changes to the SDK are cleanly incorporated.

# TODO

Known limitations and planned improvement for LovrApp and Lovr for Oculus Mobile:

- No Gear controller (headset button) support
- No gamepad support
- `getEyePose` in oculus mobile driver is wrong (just redirects to getPose)
- Avatar SDK support (to display controller model) should be added
- "Focus" events should be issued on pause and resume
- lovr.conf MSAA setting is ignored
- The play bounds feature is not supported, but could be
- Display masks are not supported, but could be
- Microphones are not supported, but could be
- It would be nice to have Windows build instructions above

# License

This repository is based on the Oculus Mobile SDK, which Oculus has made available under the terms in [LICENSE-OCULUS.txt](LICENSE-OCULUS.txt), and Lovr, which is available under the terms in "cmakelib/lovr/LICENSE".

LovrApp (the additional glue built on top) is by Andi McClure <<andi.m.mcclure@gmail.com>> and is made available under the license below.

> Copyright 2020 Andi McClure
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
