This is a repository for building LovrApp, a standalone Android app which is based on the [LÖVR](lovr.org) VR API.

# Usage

Most users do not need to build LovrApp themselves. For running your own Lua files you can sideload the LovrTest app, which you can find on the "Releases" section of this github repo, or [this page with instructions](https://mcclure.github.io/mermaid-lovr).

## To build (Mac instructions):

* The submodule cmakelib/lovr has submodules. If you did not initially clone this repo with --recurse-submodules, you will need to run `(cd cmakelib/lovr && git submodule init && git submodule update)` before doing anything else.

* Install Android Studio

* Open Android Studio, go into Preferences, search in the box for "SDK". Use the "Android SDK" pane and the "SDK Platforms" tab to download Android API level 21. Next, install the NDK from the "SDK Tools" tab of the same pane. Now quit Android Studio (in my testing it is broken and will break your project).

* Run:

      PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH adb devices

    To get the ID number for the device.  If you do not see your device in the list, you may need to enable USB debugging for the device.  For the Oculus Go, you can do this by turning on the device, putting it on, and using the controller to accept the "Allow USB Debugging" popup.

* Plug the id number from adb into [https://dashboard.oculus.com/tools/osig-generator/]

* Copy the file downloaded from osig-generator into `LovrApp/assets`

* You need to build the gradle script in `cmakelib`, then run the installDebug target of the gradle script in `LovrApp/Projects/Android`. You can do this with the `gradlew` script in the root, but it will need the Android tools in `PATH` and the sdk install location in `ANDROID_HOME`. You can just run this at the Bash prompt from the repository root to do all of this:

      (export PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH ANDROID_HOME=~/Library/Android/sdk GRADLE=`pwd`/gradlew; (cd cmakelib && $GRADLE build) && (cd LovrApp/Projects/Android && $GRADLE installDebug)) && say "Done"

Notes:
* You have to have turned on developer mode on your headset before deploying.
* If it gets stuck complaining about "unauthorized", try putting on the headset and see if there's a permissions popup.
* If you get a message about "signatures do not match the previously installed version", run this and try again:

        PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH adb uninstall org.lovr.appsample

* If all you have done is changed the assets, you can upload those by running only the final `installDebug` gradlew task. For example:

      (export PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH ANDROID_HOME=~/Library/Android/sdk GRADLE=`pwd`/gradlew; (cd LovrApp/Projects/Android && $GRADLE installDebug))

* To see all the things gradlew can do in a particular directory run it with "tasks" as the argument.
* The reason for the long PATH/ANDROID_HOME line is to get the java and android tools into scope for that line. You could also just modify the env vars in your bashrc.

Help would be appreciated if you know how to do any of the following: Make the build work in Android Studio; make the build work in Windows; make the build work in a single gradle pass without having to invoke `gradlew` three times.

## Adding game code:

The game code should be put in `LovrApp/assets`. Alternately, you can place your own directory somewhere and put the path to it on the `assets.srcDirs = ['../../assets', '../../../your/path/here']` line of `LovrApp/Projects/Android/build.gradle`. This will be uploaded when LovrApp's installDebug runs.

## To ship your own app:

Decide on a name for your app and also an identifier (this is something like "com.companyname.gametitle").

Edit `LovrApp/Projects/build.gradle`. Change "project.archivesBaseName" and "applicationId" to reflect your identifier.

Edit `LovrApp/Projects/Android/AndroidManifest.xml`. Change "package=" at the top to your identifier and change "android:label=" partway down (right after YOUR NAME HERE) to your name.

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
- Currently uses Lua rather than LuaJIT (this is nontrivial to fix-- the problem is LuaJIT builds two helper tools during its own build, but this does not work in a cross compile environment without weird tricks)
- Currently draws each eye separately (no stereo rendering)
- `getEyePose` in oculus mobile driver is wrong (just redirects to getPose)
- Avatar SDK support (to display controller model) should be added
- "Focus" events should be issued on pause and resume
- lovr.conf MSAA setting is ignored
- Can 32-bit build be removed?

# License

This repository is based on the Oculus Mobile SDK, which Oculus has made available under the terms in [LICENSE-OCULUS.txt](LICENSE-OCULUS.txt), and Lovr, which is available under the terms in "cmakelib/lovr/LICENSE".

LovrApp (the additional glue built on top) is by Andi McClure <<andi.m.mcclure@gmail.com>> and is made available under the license below.

> Copyright 2018 Andi McClure
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
