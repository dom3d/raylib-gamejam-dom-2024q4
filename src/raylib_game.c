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

static const int g_GridSize = 64;
static const int g_TileCount = g_GridSize * g_GridSize;

//----------------------------------------------------------------------------------------------------------------------
// Type Definition
//----------------------------------------------------------------------------------------------------------------------
// Game State
//--------------------------------------------------------------------------------------
typedef enum
{
    APP_STATE_TITLE,
	APP_STATE_MAIN_MENU,
	APP_STATE_GAMEPLAY,
	APP_STATE_GAMEPLAY_PAUSED,
	APP_STATE_GAMEPLAY_ENDED
} AppState;

typedef enum
{
	ACTION_MODE_INSPECT,
	ACTION_MODE_BUILD_RAILS,
	ACTION_MODE_BUILD_RESOURCER,
	ACTION_MODE_BUILD_PROCESSOR,
	ACTION_MODE_BUILD_CITY,
	ACTION_MODE_BUILD_BULLDOZER,
	ACTION_MODE_COUNT,
} InteractionMode;

const int g_interactionModeKeyboardShortcuts[ACTION_MODE_COUNT] = { KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N };

const char* g_actionModeButtonLabels[ACTION_MODE_COUNT] =
{
		"#21#Inspect",
		"#171#Rails",
		"#210#Resourcing",
		"#209#Processor",
		"#185#City",
		"#143#Bulldozer"
};

// Camera
//--------------------------------------------------------------------------------------
typedef enum
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
typedef enum
{
	MODEL_RAILS_STRAIGHT,
	MODEL_RAILS_CURVE,
	MODEL_FACTORY_A,
	MODEL_TRAIN_LOCOMOTIVE_A,
	MODEL_COUNT
} ModelID;

const char* g_modelFilePaths[MODEL_COUNT] =
{
	"resources/rails_straight_8.obj",
	"resources/rails_curve_8.obj",
	"resources/factory_a_8.obj",
	"resources/locomotive_a_8.obj",
};

static int MAX_MODELS_TO_LOAD_COUNT = MODEL_COUNT;

// Map & tiles
//--------------------------------------------------------------------------------------
typedef enum
{
	TILE_EMPTY,
	TILE_RAILS_STRAIGHT,
	TILE_RAILS_CURVE,
	TILE_RAILS_SPLIT,
	TILE_RAILS_JOINER,
	TILE_RAILS_CROSS,
	TILE_RAILS_HALT_LIGHT,
	TILE_RAILS_HALT_STATION,
} TileType;

typedef enum
{
	DIRECTION_NORTH = 1 << 0,
	DIRECTION_EAST  = 1 << 1,
	DIRECTION_SOUTH = 1 << 2,
	DIRECTION_WEST  = 1 << 3
} DirectionFlag;

typedef uint8_t DirectionsInfo;

typedef struct TileInfo
{
	TileType type;
	DirectionsInfo connectionOptions; // Bitflags for connectionOptions (NORTH, EAST, SOUTH, WEST)
	// todo vehicle IDs and transition progress info etc
} TileInfo;

typedef struct
{
	int x;
	int z;
} TileCoords;

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
} g_game;

//----------------------------------------------------------------------------------------------------------------------
// Functions Declaration
//----------------------------------------------------------------------------------------------------------------------
static void AssetsLoad(void);
static void GameAppInitializeState(void);				// prepare all static / global data before starting running the main loop
static void GameplayResetState(void);
static void TickMainLoop(void);							// Update and Draw one frame
static void CameraTick(void);
static void CameraUpdateFromControlValues(void);
static void AssetsUnload(void);

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
	float halfGridSize = (float) g_GridSize * 0.5f;
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
	g_game.actionMode = ACTION_MODE_INSPECT;

	for (int tileIndex = 0; tileIndex < g_GridSize * g_GridSize; tileIndex++)
	{
		g_game.mapTiles[tileIndex].type = TILE_EMPTY;
		g_game.mapTiles[tileIndex].connectionOptions = 0; // none
	}
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
	rotation += PI; // equivalent to 180 degrees

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

static void CameraTick(void)
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

static inline int MapTileIndexByCoords(int x, int y)
{
	return y * g_GridSize + x;
}

static inline TileCoords MapTileCoordsByIndex(int arrayIndex)
{
	TileCoords coords;

	coords.z = arrayIndex / g_GridSize;
	coords.x = arrayIndex % g_GridSize;

	return coords;
}

static inline void MapDirectionSetFlag(DirectionsInfo* flags, DirectionFlag flag)
{
	*flags |= flag;
}

static inline void MapDirectionClearFlag(DirectionsInfo* flags, DirectionFlag flag)
{
	*flags &= ~flag;
}
static inline int MapDirectionIsFlagSet(DirectionsInfo flags, DirectionFlag flag)
{
	return flags & flag;
}

static inline void MapDirectionSetAll(DirectionsInfo* flags, bool north, bool east, bool south, bool west)
{
	*flags = 0; // Clear all flags initially
	if (north) *flags |= DIRECTION_NORTH;
	if (south) *flags |= DIRECTION_SOUTH;
	if (east) *flags |= DIRECTION_EAST;
	if (west) *flags |= DIRECTION_WEST;
}

static inline void MapTileSetData(int x, int y, TileType type, bool north, bool east, bool south, bool west)
{
	int index = MapTileIndexByCoords(x, y);
	g_game.mapTiles[index].type = type;
	MapDirectionSetAll(&(g_game.mapTiles[index].connectionOptions), north, east, south, west);
}

// Get the grid tile  coordinates
static inline TileCoords TileGetCoordsFromWorldPoint(Vector3 position)
{
	TileCoords tileCoords;

	tileCoords.x = (int) position.x;
	tileCoords.z = (int) position.z;

	// Ensure the indices are within the bounds of the grid
	tileCoords.x = (tileCoords.x < 0) ? 0 : (tileCoords.x >= g_GridSize) ? g_GridSize - 1 : tileCoords.x;
	tileCoords.z = (tileCoords.z < 0) ? 0 : (tileCoords.z >= g_GridSize) ? g_GridSize - 1 : tileCoords.z;

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

// Update and draw frame
void TickMainLoop(void)
{
	//----------------------------------------------------------------------------------
    // Update
	//----------------------------------------------------------------------------------
	CameraTick();

	//----------------------------------------------------------------------------------
    // Draw
    //----------------------------------------------------------------------------------

	BeginDrawing();
		ClearBackground(BLACK);
		// Render 3D scene
		BeginMode3D(g_game.camera);

			float width = (float) g_GridSize;
			float center = width * 0.5f;
			DrawPlane((Vector3){ center, 0.0f, center }, (Vector2){ width, width }, COLOR_GREEN); // ground plane
			DrawGridAt(g_GridSize, 1.0f, center, 0.01f, center, COLOR_BLACK, COLOR_RED);

			// debug markers
			DrawSphere((Vector3){ 0.0f, 0.0f, 0.0f }, 0.1f, COLOR_RED); // map origin
			DrawSphere((Vector3){width, 0, width}, 0.1f, COLOR_YELLOW); // map far end
//			for (int i = 0; i < MAX_MODELS_TO_LOAD_COUNT; i++)
//			{
//				DrawModel(g_game.assetModels[i], (Vector3) {1.0f * (float) i, 0.0f,1.0f}, 1.0f, WHITE);
//			}


			// remove this from drawing ----------------------------------------------------------------------------------
			// what's the point coordinate on the world plane?
			Ray mouseRay = GetMouseRay(GetMousePosition(), g_game.camera);
			float cornerValue = (float) +g_GridSize;
			RayCollision rayCollision = GetRayCollisionQuad
			(
					mouseRay,
					(Vector3) {0, 0, 0},
					(Vector3) {cornerValue, 0, 0},
					(Vector3) {cornerValue, 0, cornerValue},
					(Vector3) {0, 0, cornerValue}
			);

			if(rayCollision.hit)
			{
				TileCoords tileCoords = TileGetCoordsFromWorldPoint(rayCollision.point);
				Vector3 tileCenterPoint = TileGetCenterPosition(tileCoords);

				// draw grid cursor
				if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) == false && IsKeyDown(KEY_SPACE) == false && g_game.cameraPanState.panningState == CAM_PAN_IDLE)
				{
					//DrawCube(rayCollision.point, 1, 0.2f, 1, COLOR_YELLOW);
					DrawCube(tileCenterPoint, 1, 0.2f, 1, COLOR_BLUE);
				}
				else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) )
				{
					MapTileSetData(tileCoords.x, tileCoords.z, TILE_RAILS_STRAIGHT, true, false, true, false);
				}
			}

			for(int tileIndex = 0; tileIndex < g_TileCount; ++tileIndex)
			{
				if(g_game.mapTiles[tileIndex].type == TILE_RAILS_STRAIGHT)
				{
					TileCoords tileCoords = MapTileCoordsByIndex(tileIndex);
					Vector3 tileCenter = TileGetCenterPosition(tileCoords);
					DrawModel(g_game.assetModels[MODEL_RAILS_STRAIGHT], tileCenter, 1.0f, WHITE);
				}
			}


			//GetTileCoords

		EndMode3D();

        // UI
		int frameThickness = 16;
		DrawRectangle(0,frameThickness, g_ScreenWidth, 30, BLACK); // background
		DrawText("For raylib 2024Q4 NEXT gamejam `Connections`", 200, 15, 30, WHITE);
		DrawRectangleLinesEx((Rectangle){0, 0, (float) g_ScreenWidth, (float) g_ScreenHeight }, (float) frameThickness, COLOR_GREY);

		TickToolbarUI();
		DrawFPS(20, 20);
    EndDrawing();
    //----------------------------------------------------------------------------------  
}