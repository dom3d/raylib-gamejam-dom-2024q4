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
	ACTION_MODE_PANZOOM,
	ACTION_MODE_BUILD,
} InteractionMode;

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

const char* modelFilePaths[MODEL_COUNT] =
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
	DIRECTION_NORTH,
	DIRECTION_EAST,
	DIRECTION_SOUTH,
	DIRECTION_WEST
} Direction;



//----------------------------------------------------------------------------------------------------------------------
// Functions Declaration
//----------------------------------------------------------------------------------------------------------------------
static void LoadAssets(void);
static void InitializeGameAppState(void);				// prepare all static / global data before starting running the main loop
static void ResetGameplayState(void);
static void TickMainLoop(void);							// Update and Draw one frame
static void TickGameCamera(void);
static void UpdateCameraFromControlValues(void);
static void UnloadAssets(void);

//----------------------------------------------------------------------------------------------------------------------
// Global Variables Definition & Reset Management
//----------------------------------------------------------------------------------------------------------------------
static const int g_ScreenWidth = 1024;
static const int g_ScreenHeight = 800;

static const float g_CameraZoomMin = 4;
static const float g_CameraZoomMax = 30;
static const float g_CameraZoomSpeedFactor = 0.10f;

static struct
{
	AppState state;
	CameraControlValues cameraControlValues;
	CameraPanState cameraPanState;
	Camera3D camera;
	InteractionMode actionMode;
	Texture assetTexture;
	Model assetModels[MODEL_COUNT];
} g_game;


// set the initial conditions and resets the game state
void InitializeGameAppState(void)
{
	g_game.state = APP_STATE_TITLE;
	ResetGameplayState();
}

// resets gameplay params
void ResetGameplayState(void)
{
	g_game.cameraControlValues = (CameraControlValues)
	{
		.pivot = Vector3Zero(),
		.zoomDistance = 10,
		.rotation = 0,
	};

	g_game.cameraPanState = (CameraPanState)
	{
		.panningState = CAM_PAN_IDLE,
		.panStartWorldPosition= Vector3Zero()
	};

	UpdateCameraFromControlValues();
	g_game.actionMode = ACTION_MODE_PANZOOM;
}

//----------------------------------------------------------------------------------------------------------------------
// Program main entry point
//----------------------------------------------------------------------------------------------------------------------
int main(void)
{
	#if !defined(_DEBUG)
		SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messages
	#endif

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(g_ScreenWidth, g_ScreenHeight, "raylib gamejam game test");

	InitializeGameAppState();
	LoadAssets();

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
	UnloadAssets();
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
// Module functions definition
//----------------------------------------------------------------------------------------------------------------------
// Asset / Resource Management
//--------------------------------------------------------------------------------------
void LoadAssets(void)
{
	TraceLog(LOG_INFO,"===> starting asset loading ....");
	g_game.assetTexture = LoadTexture("resources/colormap.png");
	for (int i = 0; i < MAX_MODELS_TO_LOAD_COUNT; i++)
	{
		g_game.assetModels[i] = LoadModel(modelFilePaths[i]);
		//g_game.assetModels[i].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g_game.assetTexture; // not needed. Texture coming through the obj material library
		TraceLog(LOG_INFO,"===> one step");
	}
	//GuiLoadStyle("resources/ui_style.rgs"); // disabled b/c: font spacing or kerning not working properly when font is not embedded. With font embedded we get an exception and app doesn't work
	//LoadFont("resources/Pixel Intv.otf");
	TraceLog(LOG_INFO,"===> asset loading completed.");
}

void UnloadAssets(void)
{
	for (int i = 0; i < MAX_MODELS_TO_LOAD_COUNT; i++)
	{
		UnloadModel(g_game.assetModels[i]);
	}
	UnloadTexture(g_game.assetTexture);
	TraceLog(LOG_INFO,"===> asset unloading completed.");
}

void inline UpdateCameraFromControlValues(void)
{
	// Extract control values for easier access
	Vector3 pivot = g_game.cameraControlValues.pivot;
	float zoomDistance = g_game.cameraControlValues.zoomDistance;
	float rotation = g_game.cameraControlValues.rotation;

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

static void TickGameCamera(void)
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
		UpdateCameraFromControlValues();
	}
}

// Loop Topics
//--------------------------------------------------------------------------------------

void TickToolbarUI(void)
{
	float screenBottomMargin = 20.0f;
	float inBetweenButtonsPadding = 10.0f;
	float buttonHeight = 30.0f;
	float buttonWidth = 140.0f;
	float buttonCount = 4.0f;
	float xStartingPos = (float) g_ScreenWidth * .5f - buttonCount * (inBetweenButtonsPadding + buttonWidth) * 0.5f;
	float yPos = (float) g_ScreenHeight - buttonHeight - screenBottomMargin;
	Rectangle buttonRect = {xStartingPos, yPos, buttonWidth, buttonHeight };

	GuiToggle(buttonRect, "#21#Select", false);
	buttonRect.x += buttonWidth + inBetweenButtonsPadding;
	GuiToggle(buttonRect, "#229#Rails Forward", false);
	buttonRect.x += buttonWidth + inBetweenButtonsPadding;
	GuiToggle(buttonRect, "#231#Rails Corner", false);
	buttonRect.x += buttonWidth + inBetweenButtonsPadding;
	GuiToggle(buttonRect, "#143#Bulldozer", false);
}

// Update and draw frame
void TickMainLoop(void)
{
	//----------------------------------------------------------------------------------
    // Update
	//----------------------------------------------------------------------------------
	TickGameCamera();

	//----------------------------------------------------------------------------------
    // Draw
    //----------------------------------------------------------------------------------
	Vector3 cubePosition = { 0.0f, 1.0f, 0.0f };

	BeginDrawing();
		ClearBackground(BLACK);
		// Render 3D scene
		BeginMode3D(g_game.camera);

			DrawCube(cubePosition, 1.0f, 1.0f, 1.0f, GREEN);
			for (int i = 0; i < MAX_MODELS_TO_LOAD_COUNT; i++)
			{
				DrawModel(g_game.assetModels[i], (Vector3) {1.0f * (float) i, 0.0f,1.0f}, 1.0f, WHITE);
			}

			DrawGrid(64, 1.0f);

		EndMode3D();

        // UI
		DrawRectangle(0,0, g_ScreenWidth, 20, BLACK); // background
		DrawText("For raylib NEXT gamejam 2024 Q4", 230, 15, 30, WHITE);
		DrawRectangleLinesEx((Rectangle){0, 0, g_ScreenWidth, g_ScreenHeight }, 16, LIME);

		TickToolbarUI();
		DrawFPS(20, 20);
    EndDrawing();
    //----------------------------------------------------------------------------------  
}