#include <stdio.h>
#include <android/log.h>
#include <string.h>
#include "physfs.h"
#include <string>
#include <sys/stat.h>
#include <assert.h>

extern "C" {

#include "BridgeLovr.h"
#include "lovr.h"

static lua_State* L;
static int stepRef;

char *bridgeLovrWritablePath;
BridgeLovrMobileData bridgeLovrMobileData;

int lovr_luaB_print_override (lua_State *L);

// Recursively copy a subdirectory out of PhysFS onto disk
static void physCopyFiles(std::string toDir, std::string fromDir) {
  char **filesOrig = PHYSFS_enumerateFiles(fromDir.c_str());
  char **files = filesOrig;

  if (!files) {
    __android_log_print(ANDROID_LOG_ERROR, "LOVR", "COULD NOT READ DIRECTORY SOMEHOW: [%s]", fromDir.c_str());
    return;
  }

  mkdir(toDir.c_str(), 0777);

  while (*files) {
    std::string fromPath = fromDir + "/" + *files;
    std::string toPath = toDir + "/" + *files;
    PHYSFS_Stat stat;
    PHYSFS_stat(fromPath.c_str(), &stat);

    if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
      __android_log_print(ANDROID_LOG_DEBUG, "LOVR", "DIR:  [%s] INTO: [%s]", fromPath.c_str(), toPath.c_str());
      physCopyFiles(toPath, fromPath);
    } else {
      __android_log_print(ANDROID_LOG_DEBUG, "LOVR", "FILE: [%s] INTO: [%s]", fromPath.c_str(), toPath.c_str());

      PHYSFS_File *fromFile = PHYSFS_openRead(fromPath.c_str());

      if (!fromFile) {
        __android_log_print(ANDROID_LOG_ERROR, "LOVR", "COULD NOT OPEN TO READ:  [%s]", fromPath.c_str());
      
      } else {
        FILE *toFile = fopen(toPath.c_str(), "w");

        if (!toFile) {
          __android_log_print(ANDROID_LOG_ERROR, "LOVR", "COULD NOT OPEN TO WRITE: [%s]", toPath.c_str());

        } else {
#define CPBUFSIZE (1024*8)
          while(1) {
            char buffer[CPBUFSIZE];
            int written = PHYSFS_readBytes(fromFile, buffer, CPBUFSIZE);
            if (written > 0)
              fwrite(buffer, 1, written, toFile);
            if (PHYSFS_eof(fromFile))
              break;
          }
          fclose(toFile);
        }
        PHYSFS_close(fromFile);
      }
    }
    files++;
  }
  PHYSFS_freeList(filesOrig);
}

void bridgeLovrInit(BridgeLovrInitData *initData) {
  __android_log_print(ANDROID_LOG_DEBUG, "LOVR", "\n INSIDE LOVR\n");

  // Save writable data directory for LovrFilesystemInit later
  {
    std::string writablePath = std::string(initData->writablePath) + "/data";
    bridgeLovrWritablePath = strdup(writablePath.c_str());
    mkdir(bridgeLovrWritablePath, 0777);
  }

  // This is a bit fancy. We want to run files off disk instead of out of the zip file.
  // This is for two reasons: Because PhysFS won't let us mount "within" a zip;
  // and because if we run the files out of a temp data directory, we can overwrite them
  // with "adb push" to debug.
  // As a TODO, when PHYSFS_mountSubdir lands, this path should change to only run in debug mode.
  std::string programPath = std::string(initData->writablePath) + "/program";
  {
    // We will store the last apk change time in this "lastprogram" file.
    // We will copy all the files out of the zip into the temp dir, but ONLY if they've changed.
    std::string timePath = std::string(initData->writablePath) + "/lastprogram.dat";

    // When did APK last change?
    struct stat apkstat;
    int statfail = stat(initData->apkPath, &apkstat);
    if (statfail) {
      __android_log_print(ANDROID_LOG_ERROR, "LOVR", "CAN'T FIND APK [%s]\n", initData->apkPath);
      assert(0);
    }

    // When did we last do a file copy?
    timespec previoussec;
    FILE *timeFile = fopen(timePath.c_str(), "r");
    bool copyFiles = !timeFile; // If no lastprogram.dat, we've never copied
    if (timeFile) {
      fread(&previoussec.tv_sec, sizeof(previoussec.tv_sec), 1, timeFile);
      fread(&previoussec.tv_nsec, sizeof(previoussec.tv_nsec), 1, timeFile);
      fclose(timeFile);

      copyFiles = apkstat.st_mtim.tv_sec != previoussec.tv_sec || // If timestamp differs, apk changed
               apkstat.st_mtim.tv_nsec != previoussec.tv_nsec;
    }

    if (copyFiles) {
      __android_log_print(ANDROID_LOG_ERROR, "LOVR", "APK CHANGED [%s] WILL UNPACK\n", initData->apkPath);

      // PhysFS hasn't been inited, so we can temporarily use it as an unzip utility if we deinit afterward
      PHYSFS_init("lovr");
      int success = PHYSFS_mount(initData->apkPath, NULL, 1);
      if (!success) {
        __android_log_print(ANDROID_LOG_ERROR, "LOVR", "FAILED TO MOUNT APK [%s]\n", initData->apkPath);
        assert(0);
      } else {
        physCopyFiles(programPath, "/assets");
      }
      PHYSFS_deinit();

      // Save timestamp in a new lastprogram.dat file
      timeFile = fopen(timePath.c_str(), "w");
      fwrite(&apkstat.st_mtim.tv_sec, sizeof(apkstat.st_mtim.tv_sec), 1, timeFile);
      fwrite(&apkstat.st_mtim.tv_nsec, sizeof(apkstat.st_mtim.tv_nsec), 1, timeFile);
      fclose(timeFile);
    }
  }

  // Unpack init data
  bridgeLovrMobileData.displayDimensions = initData->suggestedEyeTexture;

  // Ready to actually go now
  // Load libraries
  L = luaL_newstate(); // FIXME: Just call main?
  luaL_openlibs(L);
  __android_log_print(ANDROID_LOG_DEBUG, "LOVR", "\n OPENED LIB\n");

  // Install custom print
  static const struct luaL_Reg printHack [] = {
    {"print", lovr_luaB_print_override},
    {NULL, NULL} /* end of array */
  };
  lua_getglobal(L, "_G");
  luaL_register(L, NULL, printHack); // "for Lua versions < 5.2"
  //luaL_setfuncs(L, printlib, 0);  // "for Lua versions 5.2 or greater"
  lua_pop(L, 1);

  // Initialize Lovr
  const char *argv[] = {"lovr", programPath.c_str()};
  lovrInit(L, 2, &argv[0]);

  __android_log_print(ANDROID_LOG_DEBUG, "LOVR", "\n LOVRINIT DONE\n");

  // TODO: Merge with emscripten run?
  lua_getglobal(L, "lovr");
  if (!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "load");
    if (!lua_isnil(L, -1)) {
      lua_call(L, 0, 0);
      __android_log_print(ANDROID_LOG_DEBUG, "LOVR", "\n LUA LOADED\n");
    } else {
      lua_pop(L, 1);
    }
  }

  lua_getfield(L, -1, "step");
  stepRef = luaL_ref(L, LUA_REGISTRYINDEX);

  __android_log_print(ANDROID_LOG_DEBUG, "LOVR", "\n BRIDGE INIT COMPLETE\n");
}

void bridgeLovrUpdate(BridgeLovrUpdateData *updateData) {
  // Unpack update data
  bridgeLovrMobileData.lastHeadPose = updateData->lastHeadPose;
  bridgeLovrMobileData.lastHeadVelocity = updateData->lastHeadVelocity;
  memcpy(bridgeLovrMobileData.eyeViewMatrix, updateData->eyeViewMatrix, sizeof(bridgeLovrMobileData.eyeViewMatrix));
  memcpy(bridgeLovrMobileData.projectionMatrix, updateData->projectionMatrix, sizeof(bridgeLovrMobileData.projectionMatrix));

  // Go
  lua_rawgeti(L, LUA_REGISTRYINDEX, stepRef);
  lua_call(L, 0, 0);
}

void lovrOculusMobileDraw(int eye, int framebuffer, int width, int height, float *eyeViewMatrix, float *projectionMatrix);

void bridgeLovrDraw(BridgeLovrDrawData *drawData) {
  int eye = drawData->eye;
  lovrOculusMobileDraw(eye, drawData->framebuffer, bridgeLovrMobileData.displayDimensions.width, bridgeLovrMobileData.displayDimensions.height,
    bridgeLovrMobileData.eyeViewMatrix[eye], bridgeLovrMobileData.projectionMatrix[eye]); // Is this indexing safe?
}

}