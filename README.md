This is a repository for building LovrApp, a standalone Android app which is based on the [LÖVR](https://lovr.org) VR API.

# Usage

Most users do not need to build LovrApp themselves. For running your own Lua files you can sideload the LovrTest app, which you can find on the "Releases" section of this github repo, or [this page with instructions](https://mcclure.github.io/mermaid-lovr).

## To build (Mac instructions):

* The submodule cmakelib/lovr has submodules. If you did not initially clone this repo with --recurse-submodules, you will need to run `(cd cmakelib/lovr && git submodule init && git submodule update)` before doing anything else.

* Install Android Studio

* Open Android Studio, go into Preferences, search in the box for "SDK". Use the "Android SDK" pane and the "SDK Platforms" tab to download Android API level 21. Next, install the NDK from the "SDK Tools" tab of the same pane. Now quit Android Studio (in my testing it is broken and will break your project).

* Run:

      PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH adb devices

    ...to get the ID number for the device.  (If you do not see your device in the list, see "notes" below.)

* Plug the id number from adb into [https://dashboard.oculus.com/tools/osig-generator/]

* Copy the file downloaded from osig-generator into `LovrApp/assets`

* You need to build the gradle script in `cmakelib`, then run the installDebug target of the gradle script in `LovrApp/Projects/Android`. You can do this with the `gradlew` script in the root, but it will need the Android tools in `PATH` and the sdk install location in `ANDROID_HOME`. You can just run this at the Bash prompt from the repository root to do all of this:

      (export PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH ANDROID_HOME=~/Library/Android/sdk GRADLE=`pwd`/gradlew; (cd cmakelib && $GRADLE build) && (cd LovrApp/Projects/Android && $GRADLE installDebug)) && say "Done"

Notes:
* You have to have turned on developer mode on your headset before deploying.
* You also have to enable USB debugging for the device.  For the Oculus Go, you can do this by plugging in the device, putting it on, and using the controller to accept the "Allow USB Debugging" popup.
* If you get a message about "signatures do not match the previously installed version", run this and try again:

        PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH adb uninstall org.lovr.appsample

* If all you have done is changed the assets, you can upload those by running only the final `installDebug` gradlew task. For example:

      (export PATH="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home/bin":~/Library/Android/sdk/platform-tools:$PATH ANDROID_HOME=~/Library/Android/sdk GRADLE=`pwd`/gradlew; (cd LovrApp/Projects/Android && $GRADLE installDebug))

* To see all the things gradlew can do in a particular directory run it with "tasks" as the argument.
* The reason for the long PATH/ANDROID_HOME line is to get the java and android tools into scope for that line. You could also just modify the env vars in your bashrc.

Help would be appreciated if you know how to do any of the following: Make the build work in Android Studio; make the build work in Windows; make the build work in a single gradle pass without having to invoke `gradlew` three times.

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

LovrApp is set up so that a "debug" build uses Oculus's recommended settings for development and a "release" build uses the appropriate settings for store submission. There are some required properties for the AndroidManifest.xml file which are not set by LovrApp becuase they differ between Oculus Go and Oculus Quest. See [this page](https://developer.oculus.com/distribute/latest/concepts/publish-mobile-manifest/) for the remaining Oculus Store configuration requirements.

At ship time, it is also probably a good idea to search all gradle files for instances of:

    abiFilters 'armeabi-v7a','arm64-v8a'

and replace it with:

    abiFilters 'arm64-v8a'

and then do another build. This will reduce your binary size. `armeabi-v7a` is only needed if your application is intended to run on Samsung Gear; it is not useful on Oculus Go or Oculus Quest.

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
- Display masks are not supported, but could be
- Microphones are not supported, but could be
- It would be nice to have Windows build instructions above
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
