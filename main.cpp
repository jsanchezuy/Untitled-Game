#ifdef _WIN32
extern "C"
{
    // http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
    __declspec( dllexport ) unsigned long int NvOptimusEnablement = 0x00000001;
    // https://gpuopen.com/amdpowerxpressrequesthighperformance/
    __declspec( dllexport ) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include <windows.h>
#include <stdio.h>

#include "external/glad.c"
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_mixer.h>

// NOTE: http://casual-effects.com/data/index.html <--- Cool site for test models
#include "shared.h"
#include "platform.cpp"
#include "input.cpp"
#include "renderer.cpp"
#include "sound.cpp"
#include "game.cpp"

// TODO(Jorge): AABB vs Circle Collision
// TODO(Jorge): Make the player look at the current mouse position
// TODO(Jorge): After player looks at mouse, do we need SAT collision?
// TODO(Jorge): Create a bullet, Test it!, Create a Whole lotta bullets
// TODO(Jorge): Make the player fire bullets

// TODO(Jorge): Performance is horrible on intel graphics card, WTF is going on!
// TODO: Test Tweening Functions!

// TODO(Jorge): Create a lot of bullets and render them using instanced rendering
// TODO(Jorge): Colors are different while rendering with nVidia card, and Intel card
// TODO(Jorge): Add License to all files
// TODO(Jorge): Remove unused functions from final version

/*
  TODO:

  Summary: _UNTITLED_ is a bullet hell/shooter game. The Player
  controls a small ship and needs to avoid the bullets the enemies
  throw at him, while shooting at the enemy at the same time. The
  player gains points for every half-second he is alive and 50 points
  for every enemy kill. Enemies keep respawning, so the game is in
  point arcade style.

  Things Needed:

  Menu:
    - The menu is a single screen that shows the keys needed to play the game, escape, set volume, etc..
    - Space Begins.
    - Escape Two times exits.
    - When players loses, space restarts the game.

  Game:
    - Bullets
    - Enemy Collision Box (circle?)
    - Enemy Bullets
    - Player Bullets
    - Player Sprite

 */


enum state
{
    State_Initial,
    State_Game,
    State_Pause,
    State_Gameover,
};


//
// Globals
//
global i32 WindowWidth = 1366;
global i32 WindowHeight = 768;
global b32 ShowDebugText = 0;
global b32 IsRunning = 1;
global keyboard *Keyboard;
global mouse *Mouse;
global clock *Clock;
global window *Window;
global renderer *MainRenderer;
global sound_system *SoundSystem;
global camera *Camera;
global state CurrentState = State_Initial;


// These variables correspond to the FPS counter, TODO: make them not global
global f32 AverageFPS;
global f32 AverageMillisecondsPerFrame;
global f32 FPSTimerSecondsElapsed = 0.0f;
global f32 FPSCounter = 0.0f;

global entity Entity = {};
global entity Enemy = {};

struct rectangle
{
    glm::vec2 Center;
    f32 HalfWidth;
    f32 HalfHeight;
};

b32 Overlapping(f32 MinA, f32 MaxA, f32 MinB, f32 MaxB)
{
    return MinB <= MaxA && MinA <= MaxB;
}

b32 RectanglesCollide(rectangle A, rectangle B)
{
    // NOTE: AABB Collision

    // Compute MinA, MaxA, MinB, MaxB for horizontal plane
    f32 ALeft = A.Center.x - A.HalfWidth;
    f32 ARight = A.Center.x + A.HalfWidth;
    f32 BLeft = B.Center.x - B.HalfWidth;
    f32 BRight = B.Center.x + B.HalfWidth;

    // Compute MinA, MaxA, MinB, MaxB for vertical plane
    f32 ABottom = A.Center.y - A.HalfHeight;
    f32 ATop = A.Center.y + A.HalfHeight;
    f32 BBottom = B.Center.y - B.HalfHeight;
    f32 BTop = B.Center.y + B.HalfHeight;

    return Overlapping(ALeft, ARight, BLeft, BRight) && Overlapping(ABottom, ATop, BBottom, BTop);
}


int main(i32 Argc, char **Argv)
{
    Argc; Argv;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS); // TODO: Init only the subsystems we need!

    Window = P_CreateOpenGLWindow("Untitled", WindowWidth, WindowHeight);
    MainRenderer = R_CreateRenderer(Window);
    Keyboard = I_CreateKeyboard();
    Mouse = I_CreateMouse();
    Clock = P_CreateClock();
    SoundSystem = S_CreateSoundSystem();
    Camera = G_CreateCamera(WindowWidth, WindowHeight);

    texture *Player = R_CreateTexture("textures/Seeker.png");
    texture *EnemyTexture = R_CreateTexture("textures/Black Hole.png");

    font *NovaSquare = R_CreateFont(MainRenderer, "fonts/NovaSquare-Regular.ttf", 60, 0);

    sound_effect *Test = S_CreateSoundEffect("audio/shoot-01.wav");
    sound_music *TestMusic = S_CreateMusic("audio/Music.mp3");
    S_SetMusicVolume(SoundSystem, 0);
    S_PlayMusic(TestMusic);

    Entity.Physics.Position = glm::vec3(0.0f, 0.0f, 0.0f);
    Enemy.Physics.Position = glm::vec3(0.0f, 0.0f, 0.0f);

    f32 Angle = 0.0f;
    SDL_Event Event;
    while(IsRunning)
    {
        Angle += 0.01f;
        P_UpdateClock(Clock);

        { // Compute Average FPS - Average Milliseconds Per Frame
            f32 FramesPerSecondToShow = 2; // How many times per second to calculate fps
            if(FPSTimerSecondsElapsed > (1.0f / FramesPerSecondToShow))
            {
                AverageFPS = FPSCounter / FPSTimerSecondsElapsed;
                AverageMillisecondsPerFrame = (FPSTimerSecondsElapsed / FPSCounter) * 1000.0f;
                FPSCounter = 0;
                FPSTimerSecondsElapsed = 0.0f;
            }
            else
            {
                FPSCounter += 1.0f;
                FPSTimerSecondsElapsed += (f32)Clock->DeltaTime;
            }
        }

        { // SECTION: Input Handling

            while(SDL_PollEvent(&Event))
            {
                switch(Event.type)
                {
                    case SDL_QUIT:
                    {
                        IsRunning = 0;
                        break;
                    }
                }
            }
            I_UpdateKeyboard(Keyboard);
            I_UpdateMouse(Mouse);

            switch(CurrentState)
            {
                case State_Initial:
                {
                    // Do initial input handling here
                    if(I_IsPressed(SDL_SCANCODE_ESCAPE))
                    {
                        IsRunning = 0;
                    }

                    if(I_IsPressed(SDL_SCANCODE_SPACE))
                    {
                        CurrentState = State_Game;
                    }

                    // Handle Window input stuff
                    if(I_IsReleased(SDL_SCANCODE_RETURN) && I_IsPressed(SDL_SCANCODE_LALT))
                    {
                        P_ToggleFullscreen(Window);
                        R_ResizeRenderer(MainRenderer, Window->Width, Window->Height);
                    }
                    break;
                }
                case State_Game:
                {
                    // Game state input handling
                    if(I_IsPressed(SDL_SCANCODE_ESCAPE))
                    {
                        CurrentState = State_Pause;
                    }

                    if(I_IsPressed(SDL_SCANCODE_LSHIFT))
                    {
                        // Camera Stuff
                        if(Mouse->FirstMouse)
                        {
                            Camera->Yaw = -90.0f; // Set the Yaw to -90 so the mouse faces to 0, 0, 0 in the first frame X
                            Camera->Pitch = 0.0f;
                            Mouse->FirstMouse = false;
                        }
                        Camera->Yaw += Mouse->RelX * Mouse->Sensitivity;
                        Camera->Pitch += -Mouse->RelY *Mouse->Sensitivity; // reversed since y-coordinates range from bottom to top
                        if(Camera->Pitch > 89.0f)
                        {
                            Camera->Pitch =  89.0f;
                        }
                        else if(Camera->Pitch < -89.0f)
                        {
                            Camera->Pitch = -89.0f;
                        }
                        glm::vec3 Front;
                        Front.x = cos(glm::radians(Camera->Yaw)) * cos(glm::radians(Camera->Pitch));
                        Front.y = sin(glm::radians(Camera->Pitch));
                        Front.z = sin(glm::radians(Camera->Yaw)) * cos(glm::radians(Camera->Pitch));
                        Camera->Front = glm::normalize(Front);
                    }

                    // Debug Text
                    if(I_IsReleased(SDL_SCANCODE_F1))
                    {
                        ShowDebugText = !ShowDebugText;
                    }

                    // Handle Window input stuff
                    if(I_IsReleased(SDL_SCANCODE_RETURN) && I_IsPressed(SDL_SCANCODE_LALT))
                    {
                        P_ToggleFullscreen(Window);
                        R_ResizeRenderer(MainRenderer, Window->Width, Window->Height);
                    }

                    // Handle Camera Input
                    if(I_IsPressed(SDL_SCANCODE_W) && I_IsPressed(SDL_SCANCODE_LSHIFT))
                    {
                        Camera->Position += Camera->Front * Camera->Speed * (f32)Clock->DeltaTime;
                    }
                    if(I_IsPressed(SDL_SCANCODE_S) && I_IsPressed(SDL_SCANCODE_LSHIFT))
                    {
                        Camera->Position -= Camera->Speed * Camera->Front * (f32)Clock->DeltaTime;
                    }
                    if(I_IsPressed(SDL_SCANCODE_A) && I_IsPressed(SDL_SCANCODE_LSHIFT))
                    {
                        Camera->Position -= glm::normalize(glm::cross(Camera->Front, Camera->Up)) * Camera->Speed * (f32)Clock->DeltaTime;
                    }
                    if(I_IsPressed(SDL_SCANCODE_D) && I_IsPressed(SDL_SCANCODE_LSHIFT))
                    {
                        Camera->Position += glm::normalize(glm::cross(Camera->Front, Camera->Up)) * Camera->Speed * (f32)Clock->DeltaTime;
                    }
                    if(I_IsPressed(SDL_SCANCODE_SPACE) && I_IsPressed(SDL_SCANCODE_LSHIFT))
                    {
                        G_ResetCamera(Camera, WindowWidth, WindowHeight);
                        I_ResetMouse(Mouse);
                    }

                    // Handle Player Input
                    f32 PlayerSpeed = 7.0f;
                    // f32 PlayerSpeed = 3500.0f;
                    if(I_IsPressed(SDL_SCANCODE_W) && I_IsNotPressed(SDL_SCANCODE_LSHIFT))
                    {
                        Entity.Physics.Acceleration.y += PlayerSpeed;
                    }
                    if(I_IsPressed(SDL_SCANCODE_S) && I_IsNotPressed(SDL_SCANCODE_LSHIFT))
                    {
                        Entity.Physics.Acceleration.y -= PlayerSpeed;
                    }
                    if(I_IsPressed(SDL_SCANCODE_A) && I_IsNotPressed(SDL_SCANCODE_LSHIFT))
                    {
                        Entity.Physics.Acceleration.x -= PlayerSpeed;
                    }
                    if(I_IsPressed(SDL_SCANCODE_D) && I_IsNotPressed(SDL_SCANCODE_LSHIFT))
                    {
                        Entity.Physics.Acceleration.x += PlayerSpeed;
                    }
                    if(I_IsPressed(SDL_SCANCODE_SPACE) && I_IsNotPressed(SDL_SCANCODE_LSHIFT))
                    {
                        // Jump?!?!?
                        S_PlaySoundEffect(Test);
                    }

                    if(I_IsPressed(SDL_SCANCODE_UP))
                    {
                        MainRenderer->Exposure += 0.01f;
                    }
                    if(I_IsPressed(SDL_SCANCODE_DOWN))
                    {
                        MainRenderer->Exposure -= 0.01f;
                    }

                    if(I_IsPressed(SDL_SCANCODE_SPACE) && I_IsPressed(SDL_SCANCODE_LSHIFT))
                    {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    }


                    if(I_IsPressed(SDL_SCANCODE_P))
                    {
                        S_SetMusicVolume(SoundSystem, ++SoundSystem->MusicVolume);
                        S_SetEffectsVolume(SoundSystem, ++SoundSystem->EffectsVolume);
                    }
                    if(I_IsPressed(SDL_SCANCODE_N))
                    {
                        S_SetMusicVolume(SoundSystem, --SoundSystem->MusicVolume);
                        S_SetEffectsVolume(SoundSystem, --SoundSystem->EffectsVolume);
                    }
                    break;
                }
                case State_Pause:
                {
                    if(I_IsPressed(SDL_SCANCODE_ESCAPE) && I_WasNotPressed(SDL_SCANCODE_ESCAPE))
                    {
                        IsRunning = 0;
                    }

                    if(I_IsPressed(SDL_SCANCODE_SPACE) && I_WasNotPressed(SDL_SCANCODE_SPACE))
                    {
                        CurrentState = State_Game;
                    }

                    // Handle Window input stuff
                    if(I_IsReleased(SDL_SCANCODE_RETURN) && I_IsPressed(SDL_SCANCODE_LALT))
                    {
                        P_ToggleFullscreen(Window);
                        R_ResizeRenderer(MainRenderer, Window->Width, Window->Height);
                    }
                    break;
                }
                default:
                {
                    // Invalid code path
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Critical Error", "InputHandling invalid code path: default", Window->Handle);
                    exit(0);
                    break;
                }
            }

            {  // SECTION: Update
                switch(CurrentState)
                {
                    case State_Initial:
                    {
                        break;
                    }
                    case State_Game:
                    {
                        ComputeNewtonMotion(&Entity.Physics, (f32)Clock->DeltaTime);

                        if(Entity.Physics.Position.x < -40.0f)
                        {
                            Entity.Physics.Position.x = -40.0f;
                        }
                        else if(Entity.Physics.Position.x > 40.0f)
                        {
                            Entity.Physics.Position.x = 40.0f;
                        }

                        if(Entity.Physics.Position.y < -22.0f)
                        {
                            Entity.Physics.Position.y = -22.0f;
                        }
                        else if(Entity.Physics.Position.y > 22.0f)
                        {
                            Entity.Physics.Position.y = 22.0f;
                        }

                        rectangle RectA = { {Entity.Physics.Position.x, Entity.Physics.Position.y}, 1.0f, 1.0};
                        rectangle RectB = { {0.0f, 0.0f}, 1.0f, 1.0f };

                        break;
                    }
                    case State_Pause:
                    {
                        break;
                    }
                    default:
                    {
                        // Invalid code path
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Critical Error", "UpdateState invalid code path: default", Window->Handle);
                        exit(0);;
                        break;
                    }
                }
            }

            // Update Camera Matrices
            // TODO: Camera Values should go into UBO
            Camera->View = glm::lookAt(Camera->Position, Camera->Position + Camera->Front, Camera->Up);
            Camera->Projection = glm::perspective(glm::radians(Camera->FoV), (f32)WindowWidth / (f32)WindowHeight, Camera->Near, Camera->Far);
            Camera->Ortho = glm::ortho(0.0f, (f32)WindowWidth, 0.0f, (f32)WindowHeight);


            char WindowTitle[60];
            // FPS
            sprintf_s(WindowTitle, sizeof(WindowTitle),"Untitled - FPS: %2.2f", AverageFPS);
            SDL_SetWindowTitle(Window->Handle, WindowTitle);

         } // END: Update

        { // SECTION: Render
            R_BeginFrame(MainRenderer);

            switch(CurrentState)
            {
                case State_Initial:
                {
                    R_DrawText2D(MainRenderer, Camera, "Untitled", NovaSquare, glm::vec2( (1366 / 2) - 240, 768 - 140), glm::vec2(2.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                    R_DrawText2D(MainRenderer, Camera, "Press Space to Begin", NovaSquare, glm::vec2( (1366 / 2) - 300, 768 - 430), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                    break;
                }
                case State_Game:
                {
                    R_DrawTexture(MainRenderer, Camera, Player, Entity.Physics.Position, glm::vec3(1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0);
                    R_DrawTexture(MainRenderer, Camera, EnemyTexture, Enemy.Physics.Position, glm::vec3(1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0);

                    if(ShowDebugText)
                    {
                        i32 FontOriginalSize = NovaSquare->CharacterWidth;
                        i32 FontSize = (i32)(FontOriginalSize * 0.99); // TODO(Jorge); This is wrong, we need a floor, or a ceil function

                        char TextBuffer[60];
                        // FPS
                        sprintf_s(TextBuffer, sizeof(TextBuffer),"FPS: %2.2f", AverageFPS);
                        R_DrawText2D(MainRenderer, Camera, TextBuffer, NovaSquare, glm::vec2(0, WindowHeight - 15), glm::vec2(0.25f), glm::vec3(1.0f, 1.0f, 1.0f));

                        // Exposure
                        sprintf_s(TextBuffer, sizeof(TextBuffer),"Renderer->Exposure: %2.2f", MainRenderer->Exposure);
                        R_DrawText2D(MainRenderer, Camera, TextBuffer, NovaSquare, glm::vec2(0, WindowHeight - 30), glm::vec2(0.25f), glm::vec3(1.0f, 1.0f, 1.0f));

                        // OpenGL Thingys
                        R_DrawText2D(MainRenderer, Camera, (char*)MainRenderer->HardwareVendor, NovaSquare, glm::vec2(0, WindowHeight - 45), glm::vec2(0.2f), glm::vec3(1.0f, 1.0f, 1.0f));
                        R_DrawText2D(MainRenderer, Camera, (char*)MainRenderer->HardwareModel, NovaSquare, glm::vec2(0, WindowHeight - 60), glm::vec2(0.2f), glm::vec3(1.0f, 1.0f, 1.0f));
                        R_DrawText2D(MainRenderer, Camera, (char*)MainRenderer->OpenGLVersion, NovaSquare, glm::vec2(0, WindowHeight - 75), glm::vec2(0.2f), glm::vec3(1.0f, 1.0f, 1.0f));
                        R_DrawText2D(MainRenderer, Camera, (char*)MainRenderer->GLSLVersion, NovaSquare, glm::vec2(0, WindowHeight - 100), glm::vec2(0.2f), glm::vec3(1.0f, 1.0f, 1.0f));

                        // Player Position
                        sprintf_s(TextBuffer, sizeof(TextBuffer),"Player->Position: %2.2f,%2.2f", Entity.Physics.Position.x, Entity.Physics.Position.y);
                        R_DrawText2D(MainRenderer, Camera, TextBuffer, NovaSquare, glm::vec2(0, WindowHeight - 115), glm::vec2(0.2f), glm::vec3(1.0f, 1.0f, 1.0f));

                        // Camera Position
                        sprintf_s(TextBuffer, sizeof(TextBuffer),"Camera->Position: %2.2f,%2.2f,%2.2f", Camera->Position.x, Camera->Position.y, Camera->Position.z);
                        R_DrawText2D(MainRenderer, Camera, TextBuffer, NovaSquare, glm::vec2(0, WindowHeight - 130), glm::vec2(0.2f), glm::vec3(1.0f, 1.0f, 1.0f));

                        // Sound System
                        sprintf_s(TextBuffer, sizeof(TextBuffer),"MusicVolume: %d", SoundSystem->MusicVolume);
                        R_DrawText2D(MainRenderer, Camera, TextBuffer, NovaSquare, glm::vec2(0, WindowHeight - 145), glm::vec2(0.2f), glm::vec3(1.0f, 1.0f, 1.0f));
                        sprintf_s(TextBuffer, sizeof(TextBuffer),"EffectsVolume: %d", SoundSystem->EffectsVolume);
                        R_DrawText2D(MainRenderer, Camera, TextBuffer, NovaSquare, glm::vec2(0, WindowHeight - 160), glm::vec2(0.2f), glm::vec3(1.0f, 1.0f, 1.0f));
                    }

                    break;
                }
                case State_Pause:
                {
                    R_DrawText2D(MainRenderer, Camera, "Untitled", NovaSquare, glm::vec2( (1366 / 2) - 240, 768 - 140), glm::vec2(2.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                    R_DrawText2D(MainRenderer, Camera, "Press Space to Continue", NovaSquare, glm::vec2( (1366 / 2) - 360, 768 - 430), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                    R_DrawText2D(MainRenderer, Camera, "Press Escape to Quit", NovaSquare, glm::vec2( (1366 / 2) - 300, 768 - 600), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                    break;
                }
                default:
                {
                    break;
                }
            }
            R_EndFrame(MainRenderer);


        } // SECTION END: Render
    }

    SDL_GL_DeleteContext(Window->Handle);

    return 0;
}
