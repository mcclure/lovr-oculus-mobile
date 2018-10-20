// Functions on the Lovr side of the wall, called from the Oculus side of the wall.

// What's going on here:
// At the moment, it's not easy to statically link LovrApp_NativeActivity with lovr.
// In order to prevent NativeActivity and Lovr from having to start including each other's
// headers and maybe run into linking problems, all communication between the two
// happens through the functions and data structures in this file.

typedef struct {
	int width;
	int height;
} BridgeLovrDimensions;

typedef struct {
	float x; float y; float z; float q[4];
} BridgeLovrPose;

typedef struct {
	float x; float y; float z; float ax; float ay; float az;
} BridgeLovrVel;

// Data passed from BridgeLovr to oculus_mobile
typedef struct {
	BridgeLovrDimensions displayDimensions;
	BridgeLovrPose lastHeadPose;
	BridgeLovrVel lastHeadVelocity; // Ignore angle
	float eyeViewMatrix[2][16];
	float projectionMatrix[2][16];
} BridgeLovrMobileData;
extern BridgeLovrMobileData bridgeLovrMobileData;

// Data passed from Lovr_NativeActivity to BridgeLovr at init time
typedef struct {
	const char *writablePath;
	const char *apkPath;
	BridgeLovrDimensions suggestedEyeTexture;
} BridgeLovrInitData;

void bridgeLovrInit(BridgeLovrInitData *initData);

// Data passed from Lovr_NativeActivity to BridgeLovr at update time
typedef struct {
	BridgeLovrPose lastHeadPose;
	BridgeLovrVel lastHeadVelocity; // Ignore angle
	float eyeViewMatrix[2][16];
	float projectionMatrix[2][16];
} BridgeLovrUpdateData;

void bridgeLovrUpdate(BridgeLovrUpdateData *updateData);

typedef struct {
	int eye;
	int framebuffer;
} BridgeLovrDrawData;

void bridgeLovrDraw(BridgeLovrDrawData *drawData);

extern char *bridgeLovrWritablePath;
