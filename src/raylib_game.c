/*******************************************************************************************
*
*   raylib gamejam template
*
*   Template originally created with raylib 4.5-dev, last time updated with raylib 5.0
*
*   Template licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2022-2024 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#define PLATFORM_WEB // for IDE quick hack
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"                 // Required for GUI controls

#if defined(PLATFORM_WEB)
    #define CUSTOM_MODAL_DIALOGS            // Force custom modal dialogs usage
    #include <emscripten/emscripten.h>      // Emscripten library - LLVM to JavaScript compiler
#endif

#include "iconset.h"

#include <stdio.h>                          // Required for: printf()
#include <stdlib.h>                         // Required for: 
#include <string.h>                         // Required for:

//----------------------------------------------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------------------------------------------
// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

//----------------------------------------------------------------------------------------------------------------------
// Global constants
//----------------------------------------------------------------------------------------------------------------------
// My color palette
#define COLOR_BLACK	CLITERAL(Color){ 0, 0, 0, 255 }
#define COLOR_GREY	CLITERAL(Color){ 160, 157, 161, 255 }
#define COLOR_WHITE	CLITERAL(Color){ 255, 255, 255, 255 }
#define COLOR_BROWN	CLITERAL(Color){ 148, 82, 0, 255 }
#define COLOR_RED	CLITERAL(Color){ 224, 17, 0, 255 }
#define COLOR_BLUE	CLITERAL(Color){ 59, 133, 220, 255 }
#define COLOR_GREEN	CLITERAL(Color){ 96, 139, 50, 255 }
#define COLOR_YELLOW	CLITERAL(Color){ 255, 198, 83, 255 }

static const int g_ScreenWidth = 1024;
static const int g_ScreenHeight = 800;

static const float g_CameraZoomMin = 4;
static const float g_CameraZoomMax = 30;
static const float g_CameraZoomSpeedFactor = 0.10f;

static const int g_MapGridSize = 64;
static const int g_TileCount = g_MapGridSize * g_MapGridSize;

static const int g_MaxTrains = 64;

static const KeyboardKey g_debugWindowKey = KEY_TAB;
static bool g_debugWindowOn = true; // todo turn off
static bool g_debugSimPauseOn = false;


//----------------------------------------------------------------------------------------------------------------------
// Type Definition
//----------------------------------------------------------------------------------------------------------------------
// Game State
//--------------------------------------------------------------------------------------
typedef enum : uint8_t // c99
{
    APP_STATE_TITLE,
	APP_STATE_MAIN_MENU,
	APP_STATE_GAMEPLAY,
	APP_STATE_GAMEPLAY_PAUSED,
	APP_STATE_GAMEPLAY_ENDED
} AppState;

typedef enum : uint8_t // c99
{
	ACTION_MODE_PAN_VIEW,
	ACTION_MODE_BUILD_RAILS,
	ACTION_MODE_CHANGE_SIGNALS,
	ACTION_MODE_BUILD_BULLDOZER,
	ACTION_MODE_COUNT,
} InteractionMode;

const int g_interactionModeKeyboardShortcuts[ACTION_MODE_COUNT] = { KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N };

const char* g_actionModeButtonLabels[ACTION_MODE_COUNT] =
{
		"#44#View",
		"#171#Rails",
		"#174#Signals",
		"#143#Bulldozer"
};

// Camera
//--------------------------------------------------------------------------------------
typedef enum : uint8_t // c99
{
	CAM_PAN_IDLE,
	CAM_PAN_ACTIVE,
} CameraPanningState;

typedef struct CameraControlValues
{
	Vector3 pivot;
	float zoomDistance;
	float rotation;
} CameraControlValues;

typedef struct CameraPanState
{
	CameraPanningState panningState;
	Vector3 panStartWorldPosition;

} CameraPanState;

// Art Assets
//--------------------------------------------------------------------------------------
typedef enum : uint8_t // c99
{
	MODEL_RAILS_STRAIGHT,
	MODEL_RAILS_CURVE,
	MODEL_RAILS_MERGE,
	MODEL_RAILS_MERGE_MIRROR,
	MODEL_RAILS_CROSS,
	MODEL_FACTORY_A,
	MODEL_TRAIN_LOCOMOTIVE_A,
	MODEL_COUNT
} ModelID;

const char* g_modelFilePaths[MODEL_COUNT] =
{
	"resources/rails_straight_8.obj",
	"resources/rails_curve_8.obj",
	"resources/rails_merge_8.obj",
	"resources/rails_merge_mirror_8.obj",
	"resources/rails_crossing_8.obj",
	"resources/factory_a_8.obj",
	"resources/locomotive_a_8.obj",
};

const char* ModelIdToString(ModelID modelID)
{
	switch (modelID)
	{
		case MODEL_RAILS_STRAIGHT:     	return "Rails Straight";
		case MODEL_RAILS_CURVE:      	return "Rails Curve";
		case MODEL_RAILS_MERGE:      	return "Rails Merge";
		case MODEL_RAILS_MERGE_MIRROR:  return "Rails Merge Mir";
		case MODEL_RAILS_CROSS:      	return "Rails Cross";
		case MODEL_FACTORY_A:     		return "Factory";
		case MODEL_TRAIN_LOCOMOTIVE_A:  return "Locomotive";
		case MODEL_COUNT:     			return "Data";
		default:                 		return "Unknown";
	}
}

static int MAX_MODELS_TO_LOAD_COUNT = MODEL_COUNT;

// Map & tiles
//--------------------------------------------------------------------------------------
typedef enum : uint8_t // c99
{
	TILE_TYPE_EMPTY,
	TILE_TYPE_RAILS,
	TILE_TYPE_TODO,
} TileType;

const char* TileTypeToString(TileType tileType)
{
	switch (tileType)
	{
		case TILE_TYPE_EMPTY:     return "TILE EMPTY";
		case TILE_TYPE_RAILS:      return "TILE RAILS";
		case TILE_TYPE_TODO:     return "Tile Todo";
		default:                 return "UNKNOWN_SECTOR";
	}
}

typedef enum : uint8_t // c99
{
	CONNECTION_NS_SN = 1 << 0,
	CONNECTION_NE_EN = 1 << 1,
	CONNECTION_NW_WN = 1 << 2,
	CONNECTION_ES_SE = 1 << 3,
	CONNECTION_EW_WE = 1 << 4,
	CONNECTION_SW_WS = 1 << 5,
} ConnectionDirection;

const char* ConnectionDirectionToString(ConnectionDirection direction)
{
	switch (direction)
	{
		case CONNECTION_NS_SN:	    return "NS_SN";
		case CONNECTION_NE_EN:      return "NE_EN";
		case CONNECTION_NW_WN:     	return "NW_WN";
		case CONNECTION_ES_SE:      return "ES_SE";
		case CONNECTION_EW_WE: 		return "EW_WE";
		case CONNECTION_SW_WS:      return "SW_WS";
		default: 	                return "UNKNOWN";
	}
}

typedef uint8_t ConnectionsConfig;

typedef struct TileInfo
{
	TileType type;	// uint8_t enum 1 byte
	ModelID modelID; // uint8_t enum 1 byte
	ConnectionsConfig connectionOptions; // uint8 1 byte
	ConnectionsConfig connectionsActive; // uint8 1 byte
	float modelRotationInDegree; // 4 bytes, could be reduced to 2 bytes or even 1 byte if needed
	// todo vehicle IDs and transition progress info etc
} TileInfo;

typedef struct
{
	int x;
	int z;
} TileCoords;

typedef enum
{
	TILE_SECTOR_SE 		= 1 << 0,
	TILE_SECTOR_S  		= 1 << 1,
	TILE_SECTOR_SW 		= 1 << 2,
	TILE_SECTOR_E 		= 1 << 3,
	TILE_SECTOR_CENTER 	= 1 << 4,
	TILE_SECTOR_W 		= 1 << 5,
	TILE_SECTOR_NW 		= 1 << 6,
	TILE_SECTOR_N 		= 1 << 7,
	TILE_SECTOR_NE 		= 1 << 8,
} TileSector;

const char* TileSectorToString(TileSector sector)
{
	switch (sector)
	{
		case TILE_SECTOR_SE:     return "TILE_SECTOR_SE";
		case TILE_SECTOR_S:      return "TILE_SECTOR_S";
		case TILE_SECTOR_SW:     return "TILE_SECTOR_SW";
		case TILE_SECTOR_E:      return "TILE_SECTOR_E";
		case TILE_SECTOR_CENTER: return "TILE_SECTOR_CENTER";
		case TILE_SECTOR_W:      return "TILE_SECTOR_W";
		case TILE_SECTOR_NW:     return "TILE_SECTOR_NW";
		case TILE_SECTOR_N:      return "TILE_SECTOR_N";
		case TILE_SECTOR_NE:     return "TILE_SECTOR_NE";
		default:                 return "UNKNOWN_SECTOR";
	}
}

typedef struct
{
	TileCoords coords;
	TileSector sector;
} TileSectorTrail;

// train stuff
//--------------------------------------------------------------------------------------

typedef enum : uint8_t // c99
{
	TRAIN_STATE_DISABLED = 0, // don't render, don't process
	TRAIN_STATE_HIDDEN, 	 // render differently, don't process
	TRAIN_STATE_BLOCKED, 	// reach a track end or red signal
	TRAIN_STATE_DRIVING,
	TRAIN_STATE_UNLOAD, 	// first unload then load
	TRAIN_STATE_LOAD,
	TRAIN_STATE_DERAILED, 	// non-operational and non-recoverable
} TrainState;

typedef struct TrainInfo
{
	TrainState state;	// uint8_t enum 1 byte
	ModelID modelID; // uint8_t enum 1 byte
	TileCoords tileCurrent;
	TileCoords tilePrevious;
	TileCoords tileNext;
	ConnectionDirection tileConnectionUsed;
	TileSector driveFromSector;
	TileSector driveToSector;
	float pathProgressNormalized;
	float modelRotationInDegree;
	float speedDrive;
	float speedUnload;
	float speedLoad;
	Vector2 pathCurvePoints[4];
	Vector3 modelPosition;
} TrainInfo;

// Game App State
//--------------------------------------------------------------------------------------
static struct
{
	AppState state;
	CameraControlValues cameraControlValues;
	CameraPanState cameraPanState;
	Camera3D camera;
	InteractionMode actionMode;
	Texture assetTexture;
	Model assetModels[MODEL_COUNT];
	TileInfo mapTiles[g_TileCount];
	TileSectorTrail brushSectorTrail[g_MapGridSize * g_MapGridSize];
	int brushSectorTrailLength;
	TrainInfo trains[g_MaxTrains];
} g_game;

//----------------------------------------------------------------------------------------------------------------------
// Functions Forward Declaration [as needed]
//----------------------------------------------------------------------------------------------------------------------
static void AssetsLoad(void);
static void GameAppInitializeState(void);				// prepare all static / global data before starting running the main loop
static void GameplayResetState(void);
static void TickMainLoop(void);							// Update and Draw one frame
static void TickCamera(void);
static void TickTrains(void);
static void CameraUpdateFromControlValues(void);
static void AssetsUnload(void);
static void TileUpdateRailsModel(int x, int z);
static inline void TileAddRailConnection(int x, int y, ConnectionDirection connection);
inline static void TileAddConnectionAndUpdateRailsModel(int x, int z, ConnectionDirection direction);
static inline Vector3 TileGetCenterPosition(TileCoords tileCoords);

//----------------------------------------------------------------------------------------------------------------------
// App Reset management
//----------------------------------------------------------------------------------------------------------------------
// set the initial conditions and resets the game state
void GameAppInitializeState(void)
{
	g_game.state = APP_STATE_TITLE;
	GameplayResetState();
}

// resets gameplay params
void GameplayResetState(void)
{
	// setup camera
	float halfGridSize = (float) g_MapGridSize * 0.5f;
	g_game.cameraControlValues = (CameraControlValues)
	{
		.pivot = (Vector3) {halfGridSize, 0, halfGridSize },
		.zoomDistance = 10,
		.rotation = 0,
	};

	g_game.cameraPanState = (CameraPanState)
	{
		.panningState = CAM_PAN_IDLE,
		.panStartWorldPosition = Vector3Zero()
	};

	CameraUpdateFromControlValues();
	g_game.actionMode = ACTION_MODE_BUILD_RAILS;

	// clear map
	for (int tileIndex = 0; tileIndex < g_MapGridSize * g_MapGridSize; tileIndex++)
	{
		g_game.mapTiles[tileIndex].type = TILE_TYPE_EMPTY;
		g_game.mapTiles[tileIndex].connectionOptions = 0; // none
		g_game.mapTiles[tileIndex].connectionsActive = 0;
	}

	// clear rail paint brush
	g_game.brushSectorTrailLength = 0;

	//////////////////////////////////////////////////////////////////////////////////
	// set starting rail tracks
	// loop track
	TileAddConnectionAndUpdateRailsModel(29, 29, CONNECTION_EW_WE);
	TileAddConnectionAndUpdateRailsModel(30, 29, CONNECTION_NE_EN);
	TileAddConnectionAndUpdateRailsModel(30, 30, CONNECTION_NS_SN);
	TileAddConnectionAndUpdateRailsModel(30, 31, CONNECTION_ES_SE);
	TileAddConnectionAndUpdateRailsModel(29, 31, CONNECTION_EW_WE);
	TileAddConnectionAndUpdateRailsModel(28, 31, CONNECTION_SW_WS);
	TileAddConnectionAndUpdateRailsModel(28, 30, CONNECTION_NS_SN );
	TileAddConnectionAndUpdateRailsModel(28, 29, CONNECTION_NW_WN);
	// loop track exit
	TileAddConnectionAndUpdateRailsModel(30, 29, CONNECTION_EW_WE);
	// straight outside loop fragments
	TileAddConnectionAndUpdateRailsModel(32, 29, CONNECTION_EW_WE);
	TileAddConnectionAndUpdateRailsModel(33, 30, CONNECTION_NS_SN);
	TileAddConnectionAndUpdateRailsModel(31, 31, CONNECTION_EW_WE);
	// turn back towards loop
	TileAddConnectionAndUpdateRailsModel(33, 31, CONNECTION_ES_SE);

	// reset all trains
	for(int i = 0; i < g_MaxTrains; ++i)
	{
		g_game.trains[i].state = TRAIN_STATE_DISABLED;
		g_game.trains[i].modelRotationInDegree = 0;
		g_game.trains[0].modelPosition = (Vector3) {0,0,0};
		g_game.trains[i].modelID = MODEL_TRAIN_LOCOMOTIVE_A;
		g_game.trains[i].tileCurrent = (TileCoords) {0,0};

		g_game.trains[i].speedDrive = 0.5f;
		g_game.trains[i].speedLoad = 3;
		g_game.trains[i].speedUnload = 3;
		g_game.trains[i].pathProgressNormalized = 0;
		g_game.trains[i].pathCurvePoints[0] = Vector2Zero();
		g_game.trains[i].pathCurvePoints[1] = Vector2Zero();
		g_game.trains[i].pathCurvePoints[2] = Vector2Zero();
		g_game.trains[i].pathCurvePoints[3] = Vector2Zero();
		g_game.trains[i].tileConnectionUsed = 0;
		g_game.trains[i].driveFromSector = TILE_SECTOR_CENTER;
		g_game.trains[i].driveToSector = TILE_SECTOR_CENTER;
	}

	// set up starting train
	TileCoords tileCoord = (TileCoords) {30,30};
	g_game.trains[0].state = TRAIN_STATE_DRIVING;
	g_game.trains[0].tilePrevious = (TileCoords) {30,29};
	g_game.trains[0].tileNext = (TileCoords) {30,31};
	g_game.trains[0].tileCurrent = tileCoord;
	g_game.trains[0].modelPosition = TileGetCenterPosition(tileCoord);
	g_game.trains[0].modelRotationInDegree = 0;
	g_game.trains[0].modelID = MODEL_TRAIN_LOCOMOTIVE_A;
	g_game.trains[0].speedDrive = 0.5f;
	g_game.trains[0].speedLoad = 3;
	g_game.trains[0].speedUnload = 3;
	g_game.trains[0].pathProgressNormalized = 0.5f; // start in the middle of the tile track
	g_game.trains[0].tileConnectionUsed = CONNECTION_NS_SN;
	g_game.trains[0].driveFromSector = TILE_SECTOR_S;
	g_game.trains[0].driveToSector = TILE_SECTOR_N;
}

//----------------------------------------------------------------------------------------------------------------------
// Program main entry point
//----------------------------------------------------------------------------------------------------------------------
int main(void)
{
	#if !defined(_DEBUG)
		SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messages
	#endif

	SetConfigFlags(FLAG_MSAA_4X_HINT);

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(g_ScreenWidth, g_ScreenHeight, "raylib gamejam game test");

	GameAppInitializeState();
	AssetsLoad();

	#if defined(PLATFORM_WEB)
	emscripten_set_main_loop(TickMainLoop, 0, 1); // 0 FPS to use animation-request hook as recommended by the warning in the console
	#else
		SetTargetFPS(60);     // Set our game frames-per-second
		//--------------------------------------------------------------------------------------

		// Main game loop
		while (!WindowShouldClose())    // Detect window close button
		{
			LoopTick();
		}
	#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // TODO: Unload all loaded resources at this point
	AssetsUnload();
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
// Asset / Resource Management
//----------------------------------------------------------------------------------------------------------------------
void AssetsLoad(void)
{
	TraceLog(LOG_INFO,"===> starting asset loading ....");
	g_game.assetTexture = LoadTexture("resources/colormap.png");
	for (int i = 0; i < MAX_MODELS_TO_LOAD_COUNT; i++)
	{
		g_game.assetModels[i] = LoadModel(g_modelFilePaths[i]);
		//g_game.assetModels[i].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g_game.assetTexture; // not needed. Texture coming through the obj material library
		TraceLog(LOG_INFO,"===> one step");
	}
	//GuiLoadStyle("resources/ui_style.rgs"); // disabled b/c: font spacing or kerning not working properly when font is not embedded. With font embedded we get an exception and app doesn't work
	//LoadFont("resources/Pixel Intv.otf");
	TraceLog(LOG_INFO,"===> asset loading completed.");
}

void AssetsUnload(void)
{
	for (int i = 0; i < MAX_MODELS_TO_LOAD_COUNT; i++)
	{
		UnloadModel(g_game.assetModels[i]);
	}
	UnloadTexture(g_game.assetTexture);
	TraceLog(LOG_INFO,"===> asset unloading completed.");
}

//----------------------------------------------------------------------------------------------------------------------
// Camera topics
//----------------------------------------------------------------------------------------------------------------------
void inline CameraUpdateFromControlValues(void)
{
	// Extract control values for easier access
	Vector3 pivot = g_game.cameraControlValues.pivot;
	float zoomDistance = g_game.cameraControlValues.zoomDistance;
	float rotation = g_game.cameraControlValues.rotation;
	rotation -= PI * 0.5f; // equivalent to 180 degrees

	// Calculate the new camera position based on rotation around the pivot
	float cameraX = pivot.x + zoomDistance * cosf(rotation);
	float cameraZ = pivot.z + zoomDistance * sinf(rotation);
	float cameraY = pivot.y + zoomDistance * 0.75f; // Adjust this value if you want to modify the height of the camera

	g_game.camera = (Camera3D)
	{
		.position = (Vector3) (Vector3){ cameraX, cameraY, cameraZ },
		.target = g_game.cameraControlValues.pivot,
		.up = (Vector3) {0.0f, 1.0f, 0.0f},
		.fovy = 60.0f,
		.projection = CAMERA_PERSPECTIVE
	};
}

static void TickCamera(void)
{
	bool cameraNeedsUpdating = false;

	//----------------------------------------------------------------------------------
	// Camera Panning
	if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && IsKeyDown(KEY_LEFT_ALT) == false)
	{
		// what's the point coordinate on the world plane?
		Ray mouseRay = GetMouseRay(GetMousePosition(), g_game.camera);
		RayCollision rayCollision = GetRayCollisionQuad
		(
			mouseRay,
			(Vector3) {-100, 0, -100},
			(Vector3) {+100, 0, -100},
			(Vector3) {+100, 0, +100},
			(Vector3) {-100, 0, +100}
		);

		if(rayCollision.hit)
		{
			// if just started, store the starting position
			if(g_game.cameraPanState.panningState == CAM_PAN_IDLE)
			{
				// store the starting point
				g_game.cameraPanState.panningState = CAM_PAN_ACTIVE;
				g_game.cameraPanState.panStartWorldPosition = rayCollision.point;
			}
			else
			{
				// else calculate drag distance
				Vector3 dragOffset = Vector3Subtract(g_game.cameraPanState.panStartWorldPosition, rayCollision.point);
				g_game.cameraControlValues.pivot = Vector3Add(g_game.cameraControlValues.pivot, (Vector3) {dragOffset.x, 0, dragOffset.z});
				cameraNeedsUpdating = true;
			}
		}
	}
	else if(g_game.cameraPanState.panningState == CAM_PAN_ACTIVE)
	{
		g_game.cameraPanState.panningState = CAM_PAN_IDLE;
	}

	Vector3 movement = { 0 };
	if (IsKeyDown(KEY_W)) movement = Vector3Add(movement, (Vector3){ -0.1f, 0, 0 });
	if (IsKeyDown(KEY_S)) movement = Vector3Add(movement, (Vector3){ 0.1f, 0, 0 });
	if (IsKeyDown(KEY_A)) movement = Vector3Add(movement, (Vector3){ 0, 0, 0.1f });
	if (IsKeyDown(KEY_D)) movement = Vector3Add(movement, (Vector3){ 0, 0, -0.1f });
	if (!Vector3Equals(movement, Vector3Zero()))
	{
		g_game.cameraControlValues.pivot = Vector3Add(g_game.cameraControlValues.pivot, movement);
		cameraNeedsUpdating = true;
	}

	//----------------------------------------------------------------------------------
	// Camera rotation
	if(IsKeyDown(KEY_Q))
	{
		g_game.cameraControlValues.rotation += 0.02f;
		cameraNeedsUpdating = true;
	}
	if(IsKeyDown(KEY_E))
	{
		g_game.cameraControlValues.rotation -= 0.02f;
		cameraNeedsUpdating = true;
	}
	if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && IsKeyDown(KEY_LEFT_ALT))
	{
		Vector2 mouseMovementDelta = GetMouseDelta();
		g_game.cameraControlValues.rotation += mouseMovementDelta.x * 0.005f;
		cameraNeedsUpdating = true;
	}

	//----------------------------------------------------------------------------------
	// Camera Zooming
	float wheel = GetMouseWheelMove();
	if(wheel == 0.0f)
	{
		if(IsKeyDown(KEY_R) || IsKeyDown(KEY_PAGE_UP))
		{
			wheel = -0.5f;
		}
		else if(IsKeyDown(KEY_F) || IsKeyDown(KEY_PAGE_DOWN))
		{
			wheel = 0.5f;
		}
	}
	if (wheel != 0)
	{
		// Zoom increment taken from raylib examples
		float scaleFactor = 1.0f + (g_CameraZoomSpeedFactor * fabsf(wheel));
		if (wheel < 0)
		{
			scaleFactor = 1.0f / scaleFactor;
		}
		g_game.cameraControlValues.zoomDistance = Clamp(g_game.cameraControlValues.zoomDistance * scaleFactor, g_CameraZoomMin, g_CameraZoomMax);
		cameraNeedsUpdating= true;
	}

	//----------------------------------------------------------------------------------
	if(cameraNeedsUpdating)
	{
		CameraUpdateFromControlValues();
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Map topics
//----------------------------------------------------------------------------------------------------------------------
void DrawGridAt(int slices, float spacing, float x, float y, float z, Color mainColor, Color axisColor)
{
	int halfSlices = slices/2;
	rlBegin(RL_LINES);
	for (int i = -halfSlices; i <= halfSlices; i++)
	{
		if (i == 0)
		{
			rlColor4ub(axisColor.r, axisColor.g, axisColor.b, axisColor.a);
		}
		else
		{
			rlColor4ub(mainColor.r, mainColor.g, mainColor.b, mainColor.a);
		}

		rlVertex3f((float)i*spacing + x, y, (float)-halfSlices*spacing + z);
		rlVertex3f((float)i*spacing + x, y, (float)halfSlices*spacing + z);

		rlVertex3f((float)-halfSlices*spacing + x, y, (float)i*spacing + z);
		rlVertex3f((float)halfSlices*spacing + x, y, (float)i*spacing + z);
	}
	rlEnd();
}

static inline int TileIndexByTileCoords(int x, int y)
{
	return y * g_MapGridSize + x;
}

static inline TileCoords TileCoordsByIndex(int arrayIndex)
{
	TileCoords coords;

	coords.z = arrayIndex / g_MapGridSize;
	coords.x = arrayIndex % g_MapGridSize;

	return coords;
}

static inline void TileAddConnectionFlag(ConnectionsConfig* target, ConnectionDirection additionalFlag)
{
	*target |= additionalFlag;
}

static inline void MapTileRemoveConnectionFlag(ConnectionsConfig* flags, ConnectionDirection flag)
{
	*flags &= ~flag;
}

static inline bool TileHasConnectionFlag(ConnectionsConfig flags, ConnectionDirection flag)
{
	return (flags & flag) != 0;
}

static inline int TileHConnectionsCount(ConnectionsConfig flags)
{
	int count = 0;
	while (flags)
	{
		flags &= (flags - 1); // Clear the least significant set bit
		count++;
	}
	return count;
}

static inline void TileAddRailConnection(int x, int y, ConnectionDirection connection)
{
	int index = TileIndexByTileCoords(x, y);
	g_game.mapTiles[index].type = TILE_TYPE_RAILS;

	int count = TileHConnectionsCount(g_game.mapTiles[index].connectionOptions);
	if(count == 0)
	{
		// this is the first connection and therefore active by default
		g_game.mapTiles[index].connectionsActive = connection;
	}

	// add connection
	TileAddConnectionFlag(&(g_game.mapTiles[index].connectionOptions), connection);

	if(count == 1)
	{
		// check if it's a crossing and only add it then
		bool isCrossingRails =
		TileHasConnectionFlag(g_game.mapTiles[index].connectionOptions, CONNECTION_NS_SN) &&
		TileHasConnectionFlag(g_game.mapTiles[index].connectionOptions, CONNECTION_EW_WE);

		if(isCrossingRails)
		{
			TileAddConnectionFlag(&(g_game.mapTiles[index].connectionsActive), connection);
		}
	}
}

// Get the grid tile  coordinates
static inline TileCoords TileGetCoordsFromWorldPoint(Vector3 position)
{
	TileCoords tileCoords;

	tileCoords.x = (int) position.x;
	tileCoords.z = (int) position.z;

	// Ensure the indices are within the bounds of the grid
	tileCoords.x = (tileCoords.x < 0) ? 0 : (tileCoords.x >= g_MapGridSize) ? g_MapGridSize - 1 : tileCoords.x;
	tileCoords.z = (tileCoords.z < 0) ? 0 : (tileCoords.z >= g_MapGridSize) ? g_MapGridSize - 1 : tileCoords.z;

	return tileCoords;
}

// Calculate the center position of the grid tile
static inline Vector3 TileGetCenterPosition(TileCoords tileCoords)
{
	Vector3 worldPosition;
	worldPosition.x = (float) tileCoords.x + 0.5f;
	worldPosition.z = (float) tileCoords.z + 0.5f;
	worldPosition.y = 0.01f;
	return worldPosition;
}

static inline TileSector TileSectorGetFromWorldPoint(Vector3 position)
{
	// Get the fractional part of the position within the tile
	float fracX = position.x - (int) position.x;
	float fracZ = position.z - (int) position.z;

	// Determine the sector based on fractional coordinates
	if (fracX < 0.33f)
	{
		if (fracZ < 0.33f) return TILE_SECTOR_SE;
		else if (fracZ < 0.66f) return TILE_SECTOR_E;
		else return TILE_SECTOR_NE;
	}
	else if (fracX < 0.66f)
	{
		if (fracZ < 0.33f) return TILE_SECTOR_S;
		else if (fracZ < 0.66f) return TILE_SECTOR_CENTER;
		else return TILE_SECTOR_N;
	}
	else
	{
		if (fracZ < 0.33f) return TILE_SECTOR_SW;
		else if (fracZ < 0.66f) return TILE_SECTOR_W;
		else return TILE_SECTOR_NW;
	}
}

static inline Vector3 TileSectorGetCenterPosition(TileCoords tileCoords, TileSector sector)
{
	Vector3 worldPosition;

	// Calculate the base tile center position
	worldPosition.x = (float) tileCoords.x;
	worldPosition.z = (float) tileCoords.z;
	worldPosition.y = 0.0f; // Set y-coordinate based on your needs

	// Offset within the tile based on the sector
	float offset = 1.0f / 3.0f;  // One-third offset for a 3x3 grid of sectors
	switch (sector)
	{
		case TILE_SECTOR_SE:
			worldPosition.x += offset * 0.5f;
			worldPosition.z += offset * 0.5f;
			break;
		case TILE_SECTOR_E:
			worldPosition.x += offset * 0.5f;
			worldPosition.z += offset * 1.5f;
			break;
		case TILE_SECTOR_NE:
			worldPosition.x += offset * 0.5f;
			worldPosition.z += offset * 2.5f;
			break;
		case TILE_SECTOR_S:
			worldPosition.x += offset * 1.5f;
			worldPosition.z += offset * 0.5f;
			break;
		case TILE_SECTOR_CENTER:
			worldPosition.x += offset * 1.5f;
			worldPosition.z += offset * 1.5f;
			break;
		case TILE_SECTOR_N:
			worldPosition.x += offset * 1.5f;
			worldPosition.z += offset * 2.5f;
			break;
		case TILE_SECTOR_SW:
			worldPosition.x += offset * 2.5f;
			worldPosition.z += offset * 0.5f;
			break;
		case TILE_SECTOR_W:
			worldPosition.x += offset * 2.5f;
			worldPosition.z += offset * 1.5f;
			break;
		case TILE_SECTOR_NW:
			worldPosition.x += offset * 2.5f;
			worldPosition.z += offset * 2.5f;
			break;
		default:
			// TILE_SECTOR_NONE
			worldPosition = Vector3Zero();
			break;
	}

	return worldPosition;
}

static inline Vector3 TileSectorGetEdgePosition(TileCoords tileCoords, TileSector sector)
{
	// center position for desired sector
	Vector3 worldPosition = TileSectorGetCenterPosition(tileCoords, sector);

	// Offset for the edges of the tile
	float halfSectorOffset = 1.0f / 3.0f * 0.5f;

	switch (sector)
	{
		case TILE_SECTOR_N:
			worldPosition.z += halfSectorOffset;  // Top edge along Z-axis
			break;
		case TILE_SECTOR_S:
			worldPosition.z -= halfSectorOffset;	// Bottom edge along Z-axis
			break;
		case TILE_SECTOR_W:
			worldPosition.x += halfSectorOffset;        // Left edge along X-axis
			break;
		case TILE_SECTOR_E:
			worldPosition.x -= halfSectorOffset;  // Right edge along X-axis
			break;
		default:
			worldPosition = Vector3Zero();
			break;
	}

	return worldPosition;
}

static inline bool TileCoordsAreEqual(TileCoords a, TileCoords b)
{
	return (a.x == b.x) && (a.z == b.z);
}

static inline float DegreesToRadians(float degrees)
{
	return degrees * (PI / 180.0f);
}

static TileSector TileSectorGetExitFromEntryAndConnectionDirection(ConnectionDirection connection, TileSector entry)
{
	if(connection == CONNECTION_NS_SN)
	{
		return entry == TILE_SECTOR_N ? TILE_SECTOR_S : TILE_SECTOR_N;
	}
	else if(connection == CONNECTION_NE_EN)
	{
		return entry == TILE_SECTOR_N ? TILE_SECTOR_E : TILE_SECTOR_N;
	}
	else if(connection == CONNECTION_NW_WN)
	{
		return entry == TILE_SECTOR_N ? TILE_SECTOR_W : TILE_SECTOR_N;
	}
	else if(connection == CONNECTION_SW_WS)
	{
		return entry == TILE_SECTOR_S ? TILE_SECTOR_W : TILE_SECTOR_S;
	}
	else if(connection == CONNECTION_ES_SE)
	{
		return entry == TILE_SECTOR_S ? TILE_SECTOR_E : TILE_SECTOR_S;
	}
	else if(connection == CONNECTION_EW_WE)
	{
		return entry == TILE_SECTOR_E ? TILE_SECTOR_W : TILE_SECTOR_E;
	}

	// shouldn't happen
	return TILE_SECTOR_S;
}

static TileSector TileSectorGetNextEntryByExit(TileSector exitSector)
{
	switch (exitSector)
	{
		case TILE_SECTOR_S:
			return TILE_SECTOR_N;
		case TILE_SECTOR_E:
			return TILE_SECTOR_W;
		case TILE_SECTOR_W:
			return TILE_SECTOR_E;
		case TILE_SECTOR_N:
			return TILE_SECTOR_S;
		default:
			return TILE_SECTOR_CENTER;
	}
}

static TileCoords TileGetNextFromExitSector(TileCoords tileCoords, TileSector exitSector)
{
	// todo check bounds
	TileCoords newCoords = tileCoords;
	switch (exitSector)
	{
		case TILE_SECTOR_S:
			newCoords.z -= newCoords.z > 0 ? 1 : 0;
			break;
		case TILE_SECTOR_E:
			newCoords.x -= newCoords.z > 0 ? 1 : 0;
			break;
		case TILE_SECTOR_W:
			newCoords.x += newCoords.z < g_MapGridSize ? 1 : 0;
			break;
		case TILE_SECTOR_N:
			newCoords.z += newCoords.z < g_MapGridSize ? 1 : 0;
			break;
		default: ; // pass
	}

	return newCoords;
}

static bool TileHasConnectionForEntry(TileCoords tileCoords, TileSector entrySector)
{
	int index = TileIndexByTileCoords(tileCoords.x, tileCoords.z);
	TileInfo tile = g_game.mapTiles[index];
	switch (entrySector)
	{
		case TILE_SECTOR_S:
			return	TileHasConnectionFlag(tile.connectionOptions, CONNECTION_SW_WS) ||
					TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NS_SN) ||
					TileHasConnectionFlag(tile.connectionOptions, CONNECTION_ES_SE);
		case TILE_SECTOR_E:
			return	TileHasConnectionFlag(tile.connectionOptions, CONNECTION_ES_SE) ||
					TileHasConnectionFlag(tile.connectionOptions, CONNECTION_EW_WE) ||
					TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NE_EN);
			break;
		case TILE_SECTOR_W:
			return	TileHasConnectionFlag(tile.connectionOptions, CONNECTION_EW_WE) ||
					TileHasConnectionFlag(tile.connectionOptions, CONNECTION_SW_WS) ||
					TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NW_WN);
			break;
		case TILE_SECTOR_N:
			return	TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NS_SN) ||
					TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NW_WN) ||
					TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NE_EN);
		default:
			return false;
	}
}

static void TileUpdateRailsModel(int x, int z)
{
	int tileIndex = TileIndexByTileCoords(x, z);
	TileInfo tile = g_game.mapTiles[tileIndex];
	int connectionCount = TileHConnectionsCount(tile.connectionOptions);
	if(connectionCount == 1)
	{
		tile.type = TILE_TYPE_RAILS;
		if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NS_SN))
		{
			tile.modelID = MODEL_RAILS_STRAIGHT;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_EW_WE))
		{
			tile.modelID = MODEL_RAILS_STRAIGHT;
			tile.modelRotationInDegree = 90;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_ES_SE))
		{
			tile.modelID = MODEL_RAILS_CURVE;
			tile.modelRotationInDegree = 0;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NE_EN))
		{
			tile.modelID = MODEL_RAILS_CURVE;
			tile.modelRotationInDegree = 90;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NW_WN))
		{
			tile.modelID = MODEL_RAILS_CURVE;
			tile.modelRotationInDegree = 180;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_SW_WS))
		{
			tile.modelID = MODEL_RAILS_CURVE;
			tile.modelRotationInDegree = 270;
		}
		// todo
	}
	else if(connectionCount == 2)
	{
		tile.type = TILE_TYPE_RAILS;
		if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NS_SN) && TileHasConnectionFlag(tile.connectionOptions, CONNECTION_EW_WE))
		{
			tile.modelID = MODEL_RAILS_CROSS;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NS_SN) && TileHasConnectionFlag(tile.connectionOptions, CONNECTION_ES_SE))
		{
			tile.modelID = MODEL_RAILS_MERGE;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NS_SN) && TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NE_EN))
		{
			tile.modelID = MODEL_RAILS_MERGE_MIRROR;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NS_SN) && TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NW_WN))
		{
			tile.modelID = MODEL_RAILS_MERGE;
			tile.modelRotationInDegree = 180;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NS_SN) && TileHasConnectionFlag(tile.connectionOptions, CONNECTION_SW_WS))
		{
			tile.modelID = MODEL_RAILS_MERGE_MIRROR;
			tile.modelRotationInDegree = 180;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_EW_WE) && TileHasConnectionFlag(tile.connectionOptions, CONNECTION_SW_WS))
		{
			tile.modelID = MODEL_RAILS_MERGE;
			tile.modelRotationInDegree = 270;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_EW_WE) && TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NW_WN))
		{
			tile.modelID = MODEL_RAILS_MERGE_MIRROR;
			tile.modelRotationInDegree = 90;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_EW_WE) && TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NE_EN))
		{
			tile.modelID = MODEL_RAILS_MERGE;
			tile.modelRotationInDegree = 90;
		}
		else if(TileHasConnectionFlag(tile.connectionOptions, CONNECTION_EW_WE) && TileHasConnectionFlag(tile.connectionOptions, CONNECTION_ES_SE))
		{
			tile.modelID = MODEL_RAILS_MERGE_MIRROR;
			tile.modelRotationInDegree = 270;
		}
	}
	g_game.mapTiles[tileIndex] = tile;
}

inline static void TileAddConnectionAndUpdateRailsModel(int x, int z, ConnectionDirection direction)
{
	TileAddRailConnection(x, z, direction);
	TileUpdateRailsModel(x, z);
}

void SectorTrailBakeConnection()
{
	if(g_game.brushSectorTrailLength < 2)
	{
		return;
	}

	TileCoords tileCoord = g_game.brushSectorTrail[0].coords;
	// todo get tile info, is there already something build?

	if(g_game.brushSectorTrailLength == 2)
	{
		// todo
	}
	else if(g_game.brushSectorTrailLength == 3)
	{
		TileSector first = g_game.brushSectorTrail[0].sector;
		TileSector last = g_game.brushSectorTrail[g_game.brushSectorTrailLength -1].sector;

		if
		(
			// Straight NS / SN
			(first == TILE_SECTOR_N && last == TILE_SECTOR_S) ||
			(first == TILE_SECTOR_S && last == TILE_SECTOR_N) ||
			(first == TILE_SECTOR_SW && last == TILE_SECTOR_NW) ||
			(first == TILE_SECTOR_NE && last == TILE_SECTOR_SE)
		)
		{
			TileAddRailConnection(tileCoord.x, tileCoord.z, CONNECTION_NS_SN);
		}
		else if
		(
			// Straight EW / WE
			(first == TILE_SECTOR_W && last == TILE_SECTOR_E) ||
			(first == TILE_SECTOR_E && last == TILE_SECTOR_W) ||
			(first == TILE_SECTOR_SE && last == TILE_SECTOR_SW) ||
			(first == TILE_SECTOR_NE && last == TILE_SECTOR_NW)
		)
		{
			TileAddRailConnection(tileCoord.x, tileCoord.z, CONNECTION_EW_WE);
		}
		else if
		(
			// Curve SE / ES
			(first == TILE_SECTOR_S && last == TILE_SECTOR_E) ||
			(first == TILE_SECTOR_E && last == TILE_SECTOR_S)
		)
		{
			TileAddRailConnection(tileCoord.x, tileCoord.z, CONNECTION_ES_SE);
		}
		else if
		(
			// Curve SW / WS
			(first == TILE_SECTOR_S && last == TILE_SECTOR_W) ||
			(first == TILE_SECTOR_W && last == TILE_SECTOR_S)
		)
		{
			TileAddRailConnection(tileCoord.x, tileCoord.z, CONNECTION_SW_WS);
		}
		else if
		(
			// Curve NW / WN
			(first == TILE_SECTOR_N && last == TILE_SECTOR_W) ||
			(first == TILE_SECTOR_W && last == TILE_SECTOR_N)
		)
		{
			TileAddRailConnection(tileCoord.x, tileCoord.z, CONNECTION_NW_WN);
		}
		else if
		(
			// Curve NE / EN
			(first == TILE_SECTOR_N && last == TILE_SECTOR_E) ||
			(first == TILE_SECTOR_E && last == TILE_SECTOR_N)
		)
		{
			TileAddRailConnection(tileCoord.x, tileCoord.z, CONNECTION_NE_EN);
		}
		// todo ----------------- ----------------- ----------------- ----------------- ----------------- -----------------
	}
	TileUpdateRailsModel(tileCoord.x, tileCoord.z);
}

void SectorTrailPaintAt(TileCoords coords, TileSector sector)
{
	// ignore if it's the same as we registered last time
	if(g_game.brushSectorTrailLength > 0 )
	{
		int previousIndex = g_game.brushSectorTrailLength - 1;
		TileCoords previousCoords = g_game.brushSectorTrail[previousIndex].coords;
		bool tileIsTheSame = TileCoordsAreEqual(previousCoords, coords);
		if(tileIsTheSame)
		{
			if(g_game.brushSectorTrail[previousIndex].sector == sector)
			{
				// ignore if it's the same sector
				return;
			}
		}
		else
		{
			// we moved into the next tile
			// create trail todo
			SectorTrailBakeConnection();

			// reset sector trail
			g_game.brushSectorTrailLength = 0;
		}
	}

	// add sector to trail
	int index = g_game.brushSectorTrailLength;
	g_game.brushSectorTrail[index].coords = coords;
	g_game.brushSectorTrail[index].sector = sector;
	g_game.brushSectorTrailLength++;
}

//ConnectionDirection SectorTrail

static inline RayCollision MapMouseRaycast()
{
	Ray mouseRay = GetMouseRay(GetMousePosition(), g_game.camera);
	float cornerValue = (float) g_MapGridSize;
	RayCollision rayCollision = GetRayCollisionQuad
	(
		mouseRay,
		(Vector3) {0, 0, 0},
		(Vector3) {cornerValue, 0, 0},
		(Vector3) {cornerValue, 0, cornerValue},
		(Vector3) {0, 0, cornerValue}
	);
	return rayCollision;
}

//----------------------------------------------------------------------------------------------------------------------
// Debug
//----------------------------------------------------------------------------------------------------------------------

void RenderDebugWindow()
{
	if(IsKeyPressed(g_debugWindowKey))
	{
		g_debugWindowOn = !g_debugWindowOn;
	}

	if(g_debugWindowOn)
	{
		// sim pause button
		if(GuiButton((Rectangle) {15, (float) g_ScreenHeight - 65, 80, 50}, "Pause Sim"))
		{
			g_debugSimPauseOn = !g_debugSimPauseOn;
		}

		// debug panel
		float lineHeight = 20;
		Rectangle rect = (Rectangle) {16, 80, 180, 500};
		GuiDrawRectangle(rect, 2, COLOR_BLACK, COLOR_GREY);
		RayCollision rayCollision = MapMouseRaycast();
		rect.x += 10;
		char textBuffer[64];
		rect.y = -100; // ????
		if(rayCollision.hit)
		{
			TileCoords tileCoords = TileGetCoordsFromWorldPoint(rayCollision.point);
			int tileIndex = TileIndexByTileCoords(tileCoords.x, tileCoords.z);
			TileInfo tile = g_game.mapTiles[tileIndex];

			sprintf(textBuffer, "TileCoords: x=%d, z=%d", tileCoords.x, tileCoords.z);
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			TileSector sector = TileSectorGetFromWorldPoint(rayCollision.point);
			rect.y += lineHeight;
			sprintf(textBuffer, "Sector: %s", TileSectorToString(sector));
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			rect.y += lineHeight;
			sprintf(textBuffer, "Sector: %s", TileTypeToString(tile.type));
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			int connectionCount = TileHConnectionsCount(tile.connectionOptions);
			rect.y += lineHeight;
			sprintf(textBuffer, "Connection Count: %d", connectionCount);
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			int activeConnectionCount = TileHConnectionsCount(tile.connectionsActive);
			rect.y += lineHeight;
			sprintf(textBuffer, "Connections Active Count: %d", activeConnectionCount);
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			rect.y += lineHeight;
			sprintf(textBuffer, "Rotation: %f", tile.modelRotationInDegree);
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			rect.y += lineHeight;
			sprintf(textBuffer, "Model: %s", ModelIdToString(tile.modelID));
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			bool connection_NS = TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NS_SN);
			rect.y += lineHeight;
			sprintf(textBuffer, "Connection NS-SN: %s", connection_NS ? "True" : "False");
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			bool connection_WE = TileHasConnectionFlag(tile.connectionOptions, CONNECTION_EW_WE);
			rect.y += lineHeight;
			sprintf(textBuffer, "Connection EW-WE: %s", connection_WE ? "True" : "False");
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			bool connection_SW = TileHasConnectionFlag(tile.connectionOptions, CONNECTION_SW_WS);
			rect.y += lineHeight;
			sprintf(textBuffer, "Connection SW-WS: %s", connection_SW ? "True" : "False");
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			bool connection_NW = TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NW_WN);
			rect.y += lineHeight;
			sprintf(textBuffer, "Connection NW_WN: %s", connection_NW ? "True" : "False");
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			bool connection_NE = TileHasConnectionFlag(tile.connectionOptions, CONNECTION_NE_EN);
			rect.y += lineHeight;
			sprintf(textBuffer, "Connection NE_EN: %s", connection_NE ? "True" : "False");
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

			bool connection_ES = TileHasConnectionFlag(tile.connectionOptions, CONNECTION_ES_SE);
			rect.y += lineHeight;
			sprintf(textBuffer, "Connection ES_SE: %s", connection_ES ? "True" : "False");
			GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);
		}

		rect.y += lineHeight;
		GuiDrawText("-----------------------------", rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

		TrainInfo train = g_game.trains[0];
		rect.y += lineHeight;
		sprintf(textBuffer, "Train Tile: x%d z%d", train.tileCurrent.x, train.tileCurrent.z);
		GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

		rect.y += lineHeight;
		sprintf(textBuffer, "Train To: %s", TileSectorToString(train.driveToSector));
		GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

		rect.y += lineHeight;
		sprintf(textBuffer, "Train From: %s", TileSectorToString(train.driveFromSector));
		GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

		rect.y += lineHeight;
		sprintf(textBuffer, "Train Connection: %s", ConnectionDirectionToString(train.tileConnectionUsed));
		GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

		rect.y += lineHeight;
		sprintf(textBuffer, "Train Next Tile: %d %d", train.tileNext.x, train.tileNext.z);
		GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

		rect.y += lineHeight;
		sprintf(textBuffer, "Train Next Tile: %d %d", train.tileNext.x, train.tileNext.z);
		GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);

		rect.y += lineHeight;
		sprintf(textBuffer, "Train Rotation: %f", train.modelRotationInDegree);
		GuiDrawText(textBuffer, rect, TEXT_ALIGN_LEFT, COLOR_BLACK);


	}
}


Vector3 Bezier3D(Vector3 start, Vector3 middle, Vector3 end, float t)
{
	// Ensure t is clamped between 0 and 1
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	// Calculate tangents
	Vector3 tangentStart = Vector3Add(start, Vector3Scale(Vector3Subtract(middle, start), 0.5f));
	Vector3 tangentEnd = Vector3Add(end, Vector3Scale(Vector3Subtract(middle, end), 0.5f));

	// Calculate Bezier coefficients
	float u = 1.0f - t;
	float tt = t * t;
	float uu = u * u;
	float uuu = uu * u;
	float ttt = tt * t;

	// Combine the weighted contributions of each control point
	Vector3 p = Vector3Scale(start, uuu); // (1-t)^3 * start
	p = Vector3Add(p, Vector3Scale(tangentStart, 3 * uu * t)); // 3(1-t)^2 * t * tangentStart
	p = Vector3Add(p, Vector3Scale(tangentEnd, 3 * u * tt)); // 3(1-t) * t^2 * tangentEnd
	p = Vector3Add(p, Vector3Scale(end, ttt)); // t^3 * end

	return p;
}

float CalculateLookAtAngle(Vector3 from, Vector3 to)
{
	// Compute the direction vector from 'from' to 'to'
	Vector3 direction = Vector3Subtract(to, from);

	// Calculate the angle using atan2
	// atan2(y, x) returns the angle in radians between the positive X-axis and the point (x, y)
	// We use direction.x and direction.z since we are rotating around the Y-axis
	float angle = atan2f(direction.x, direction.z) * RAD2DEG;

	// Ensure the angle is within the range [0, 360)
	if (angle < 0.0f)
	{
		angle += 360.0f;
	}

	return angle;
}

//----------------------------------------------------------------------------------------------------------------------
// Loop Topics
//----------------------------------------------------------------------------------------------------------------------

void TickToolbarUI(void)
{
	float screenBottomMargin = 20.0f;
	float inBetweenButtonsPadding = 10.0f;
	float buttonHeight = 30.0f;
	float buttonWidth = 100.0f;
	float buttonCount = 5.0f;
	float xStartingPos = (float) g_ScreenWidth * .5f - buttonCount * (inBetweenButtonsPadding + buttonWidth) * 0.5f;
	float yPos = (float) g_ScreenHeight - buttonHeight - screenBottomMargin;
	Rectangle buttonRect = {xStartingPos, yPos, buttonWidth, buttonHeight };

	for(int actionModeId = 0; actionModeId < ACTION_MODE_COUNT; ++actionModeId)
	{
		bool toggleButtonActive = g_game.actionMode == actionModeId;
		GuiToggle(buttonRect, g_actionModeButtonLabels[actionModeId], &toggleButtonActive);
		if(toggleButtonActive || IsKeyPressed(g_interactionModeKeyboardShortcuts[actionModeId]))
		{
			g_game.actionMode = actionModeId;
		}

		buttonRect.x += buttonWidth + inBetweenButtonsPadding;
	}
}


static void TickTrains(void)
{
	if(g_debugSimPauseOn)
	{
		return;
	}

	for(int i = 0; i < g_MaxTrains; ++i)
	{
		if(g_game.trains[i].state != TRAIN_STATE_DISABLED)
		{
			if(g_game.trains[i].state == TRAIN_STATE_DRIVING)
			{
				float deltaTime = GetFrameTime(); // seconds?
				g_game.trains[i].pathProgressNormalized += g_game.trains[i].speedDrive * deltaTime;

				if(g_game.trains[i].pathProgressNormalized >= 1.0f)
				{
					TileCoords tileCoords = g_game.trains[i].tileNext;
					int tileIndex = TileIndexByTileCoords(tileCoords.x, tileCoords.z);

					TileSector entrySectorNeeded = TileSectorGetNextEntryByExit(g_game.trains[i].driveToSector);
					bool hasConnectionToEntry = TileHasConnectionForEntry(tileCoords, entrySectorNeeded);

					// check next tile if it has rails
					if(g_game.mapTiles[tileIndex].type == TILE_TYPE_EMPTY)
					{
						g_game.trains[i].state = TRAIN_STATE_BLOCKED;
					}
					else if(hasConnectionToEntry == false)
					{
						g_game.trains[i].state = TRAIN_STATE_BLOCKED;
					}
					else
					{
						// let's drive the train onto it ...
						// finished the path on the current tile, needs overflow into next tile including updating everything
						float progressNormalized = fmodf(g_game.trains[i].pathProgressNormalized, 1.0f);
						TileCoords previousTileCoords = g_game.trains[i].tileCurrent;
						TileCoords currentTileCoords =  g_game.trains[i].tileNext;

						TileSector entrySector = TileSectorGetNextEntryByExit(g_game.trains[i].driveToSector);

						ConnectionDirection activeConnection;
						int activeConnectionCount = TileHConnectionsCount(g_game.mapTiles[tileIndex].connectionsActive);
						if(activeConnectionCount == 1)
						{
							activeConnection = g_game.mapTiles[tileIndex].connectionsActive;
						}
						else
						{
							// find which one is serving the entry point needed
							// hardcoded for now for the rail crosssection
							if(entrySector == TILE_SECTOR_N || entrySector == TILE_SECTOR_S)
							{
								activeConnection = CONNECTION_NS_SN;
							}
							else
							{
								activeConnection = CONNECTION_EW_WE;
							}
						}

						// while next tile has rails, let's check if it has a fitting entry point where we want to enter
						TileSector exitSector = TileSectorGetExitFromEntryAndConnectionDirection(activeConnection, entrySector);
						TileCoords nextTile = TileGetNextFromExitSector(tileCoords, exitSector);

						// update train to next tile
						g_game.trains[i].pathProgressNormalized = progressNormalized;
						g_game.trains[i].tilePrevious = previousTileCoords;
						g_game.trains[i].tileCurrent = currentTileCoords;

						g_game.trains[i].tileNext = nextTile;
						g_game.trains[i].driveFromSector = entrySector;
						g_game.trains[i].driveToSector = exitSector;
						g_game.trains[i].tileConnectionUsed = activeConnection;
					}
				}

				if(g_game.trains[i].state == TRAIN_STATE_DRIVING)
				{
					// cure progress increase by delta time and train speed
					Vector3 startPosition = TileSectorGetEdgePosition(g_game.trains[i].tileCurrent, g_game.trains[i].driveFromSector);
					Vector3 endPosition = TileSectorGetEdgePosition(g_game.trains[i].tileCurrent, g_game.trains[i].driveToSector);
					Vector3 middlePosition = TileGetCenterPosition(g_game.trains[i].tileCurrent);
					g_game.trains[i].modelPosition = Bezier3D(startPosition, middlePosition, endPosition, g_game.trains[i].pathProgressNormalized);

					// curve alignment by look at a point ahead of the curve
					Vector3 lookAheadPosition = Bezier3D(startPosition, middlePosition, endPosition, g_game.trains[i].pathProgressNormalized + 0.1f);
					g_game.trains[i].modelRotationInDegree = CalculateLookAtAngle(g_game.trains[i].modelPosition, lookAheadPosition);
				}
			}
			else if(g_game.trains[i].state == TRAIN_STATE_BLOCKED)
			{
				TileCoords tileCoords = g_game.trains[i].tileNext;
				int tileIndex = TileIndexByTileCoords(tileCoords.x, tileCoords.z);
				if(g_game.mapTiles[tileIndex].type == TILE_TYPE_RAILS)
				{
					g_game.trains[i].state = TRAIN_STATE_DRIVING;
				}
			}
		}
	}
}

void TickPaintRails(void)
{
	RayCollision rayCollision = MapMouseRaycast();
	if(rayCollision.hit)
	{
		TileCoords tileCoords = TileGetCoordsFromWorldPoint(rayCollision.point);
		Vector3 tileCenterPoint = TileGetCenterPosition(tileCoords);
		TileSector sector = TileSectorGetFromWorldPoint(rayCollision.point);
		Vector3 sectorCenterPoint = TileSectorGetCenterPosition(tileCoords, sector);

		// draw grid cursor
		DrawCube(tileCenterPoint, 1, 0.01f, 1, COLOR_BLUE);

		// draw grid sector cursor
		if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) == false && IsKeyDown(KEY_SPACE) == false && g_game.cameraPanState.panningState == CAM_PAN_IDLE)
		{
			//DrawCube(rayCollision.point, 1, 0.2f, 1, COLOR_YELLOW);
			DrawCube(sectorCenterPoint, 0.33f, 0.1f, 0.33f, COLOR_WHITE);
		}
		else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||IsKeyDown(KEY_SPACE) )
		{

			SectorTrailPaintAt(tileCoords, sector);
			//TileAddRailConnection(tileCoords.x, tileCoords.z, TILE_TYPE_RAILS, true, false, true, false);
		}
		if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsKeyReleased(KEY_SPACE))
		{
			if(g_game.brushSectorTrailLength > 0)
			{
				SectorTrailBakeConnection();
			}
			g_game.brushSectorTrailLength = 0;
		}
	}

	// draw tile sector based paint brush cursor
	for(int tileSectorIndex = 0; tileSectorIndex < g_game.brushSectorTrailLength; tileSectorIndex++)
	{
		TileCoords coords = g_game.brushSectorTrail[tileSectorIndex].coords;
		TileSector sector = g_game.brushSectorTrail[tileSectorIndex].sector;
		Vector3 sectorCenterPoint = TileSectorGetCenterPosition(coords, sector);
		DrawCube(sectorCenterPoint, 0.33f, 0.13f, 0.33f, COLOR_GREY);
	}
}

void TickBulldozer(void)
{
	RayCollision rayCollision = MapMouseRaycast();
	if(rayCollision.hit)
	{
		TileCoords tileCoords = TileGetCoordsFromWorldPoint(rayCollision.point);
		Vector3 tileCenterPoint = TileGetCenterPosition(tileCoords);
		int tileIndex = TileIndexByTileCoords(tileCoords.x, tileCoords.z);
		if(g_game.mapTiles[tileIndex].type == TILE_TYPE_RAILS)
		{
			// can delete cursor

			if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE))
			{
				bool veto = false;
				// any train on that tile?
				for(int trainIndex = 0; trainIndex < g_MaxTrains; ++trainIndex)
				{
					if(g_game.trains[trainIndex].state == TRAIN_STATE_DRIVING)
					{
						int trainX = g_game.trains[trainIndex].tileCurrent.x;
						int trainZ = g_game.trains[trainIndex].tileCurrent.z;

						if(tileCoords.x != trainX && tileCoords.z == trainZ)
						{
							veto = true;
							DrawCube(tileCenterPoint, 1, 0.01f, 1, COLOR_RED);
							break;
						}
					}
				}

				if(veto == false)
				{
					g_game.mapTiles[tileIndex].type = TILE_TYPE_EMPTY;
					g_game.mapTiles[tileIndex].connectionOptions = 0; // none
					g_game.mapTiles[tileIndex].connectionsActive = 0;
					g_game.mapTiles[tileIndex].modelRotationInDegree = 0;
					DrawCube(tileCenterPoint, 1, 0.01f, 1, COLOR_GREEN);
				}
			}
			else
			{
				DrawCube(tileCenterPoint, 1, 0.01f, 1, COLOR_YELLOW);
			}
		}
		else
		{
			// can't do anything - neutral cursor
			DrawCube(tileCenterPoint, 1, 0.01f, 1, COLOR_GREY);
		}
	}
}

// Update and draw frame
void TickMainLoop(void)
{
	//----------------------------------------------------------------------------------
    // Update
	//----------------------------------------------------------------------------------
	TickCamera();
	TickTrains();

	//----------------------------------------------------------------------------------
    // Draw
    //----------------------------------------------------------------------------------

	BeginDrawing();
		ClearBackground(BLACK);
		// Render 3D scene
		BeginMode3D(g_game.camera);

			float width = (float) g_MapGridSize;
			float center = width * 0.5f;
			DrawPlane((Vector3){ center, 0.0f, center }, (Vector2){ width, width }, COLOR_GREEN); // ground plane
			DrawGridAt(g_MapGridSize, 1.0f, center, 0.01f, center, COLOR_BLACK, COLOR_RED);


			if(g_game.actionMode == ACTION_MODE_BUILD_RAILS)
			{
				TickPaintRails();
			}
			else if(g_game.actionMode == ACTION_MODE_BUILD_BULLDOZER)
			{
				TickBulldozer();
			}
			// todo signals
			// todo economy

			const Vector3 vectorUp = (Vector3) {0,1,0};

			////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// draw rails on tiles
			for(int tileIndex = 0; tileIndex < g_TileCount; ++tileIndex)
			{
				TileInfo tile = g_game.mapTiles[tileIndex];
				if(tile.type == TILE_TYPE_RAILS)
				{
					TileCoords tileCoords = TileCoordsByIndex(tileIndex);
					Vector3 tileCenter = TileGetCenterPosition(tileCoords);
					DrawModelEx(g_game.assetModels[tile.modelID], tileCenter, vectorUp, tile.modelRotationInDegree, Vector3One(), COLOR_WHITE);
				}
			}

			////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// draw trains
			for(int i = 0; i < g_MaxTrains; ++i)
			{
				if(g_game.trains[i].state != TRAIN_STATE_DISABLED && g_game.trains[i].state != TRAIN_STATE_HIDDEN)
				{
					TrainInfo train = g_game.trains[i];
					DrawModelEx(g_game.assetModels[train.modelID], train.modelPosition, vectorUp, train.modelRotationInDegree, Vector3One(), WHITE);
				}
			}

		EndMode3D();

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // UI
		int frameThickness = 16;
		DrawRectangle(0,frameThickness, g_ScreenWidth, 30, BLACK); // background
		DrawText("For raylib 2024Q4 NEXT gamejam `Connections`", 200, 15, 30, WHITE);
		DrawRectangleLinesEx((Rectangle){0, 0, (float) g_ScreenWidth, (float) g_ScreenHeight }, (float) frameThickness, COLOR_GREY);

		TickToolbarUI();
		RenderDebugWindow();
		DrawFPS(20, 20);

    EndDrawing();
    //----------------------------------------------------------------------------------  
}
