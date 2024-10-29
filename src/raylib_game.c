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

#if defined(PLATFORM_WEB)
    #define CUSTOM_MODAL_DIALOGS            // Force custom modal dialogs usage
    #include <emscripten/emscripten.h>      // Emscripten library - LLVM to JavaScript compiler
#endif

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
// Types and Structures Definition
//----------------------------------------------------------------------------------------------------------------------
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
	CAM_PAN_IDLE,
	CAM_PAN_ACTIVE,
} CameraPanningState;

typedef enum
{
	ACTION_MODE_PANZOOM,
	ACTION_MODE_BUILD,
} InteractionMode;

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

//----------------------------------------------------------------------------------------------------------------------
// Functions Declaration
//----------------------------------------------------------------------------------------------------------------------
static void LoopTick(void);							// Update and Draw one frame
static void InitializeGameAppState(void);			// prepare all static / global data before starting running the main loop
static void ResetGameplayState(void);
static void TickGameCamera(void);
static void UpdateCameraFromControlValues(void);

//----------------------------------------------------------------------------------------------------------------------
// Global Variables Definition & Reset Management
//----------------------------------------------------------------------------------------------------------------------
static const int g_ScreenWidth = 1000;
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
    
    // TODO: Load resources / Initialize variables at this point

	InitializeGameAppState();

	#if defined(PLATFORM_WEB)
		emscripten_set_main_loop(LoopTick, 60, 1);
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
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
// Module functions definition
//----------------------------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------

// Update and draw frame
void LoopTick(void)
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

			DrawGrid(128, 1.0f);

		EndMode3D();

        // UI
		DrawText("Welcome to raylib NEXT gamejam!", 230, 40, 30, WHITE);
		DrawRectangleLinesEx((Rectangle){0, 0, g_ScreenWidth, g_ScreenHeight }, 16, WHITE);

    EndDrawing();
    //----------------------------------------------------------------------------------  
}