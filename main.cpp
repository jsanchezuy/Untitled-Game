#include <stdio.h>

#include "external/glad.c"
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_mixer.h>

#include "shared.h"
#include "platform.cpp"
#include "input.cpp"
#include "renderer.cpp"
#include "sound.cpp"
#include "collision.cpp"
#include "entity.cpp"
#include "random.cpp"

// TODO(Jorge): Make sure all movement uses DeltaTime so movement is independent from framerate
// TODO(Jorge): When the game starts, make sure the windows console does not start. (open the game in windows explorer)
// TODO(Jorge): Once sat algorithm is in a single file header lib, make a blog post detailing how SAT works.
// TODO(Jorge): Add License to all files, use unlicense or CC0. It seems lawyers and github's code like a standard license rather than simplu stating public domain.
// TODO(Jorge): Delete all unused data files
// TODO(Jorge): Textures transparent background is not blending correctly
// TODO(Jorge): Sound system should be able to play two sound effects on top of each other!

enum gamestate
{
    State_Initial,
    State_Game,
    State_Pause,
    State_Gameover,
};

// Platform
global u32 WindowWidth = 1366;
global u32 WindowHeight = 768;

// Application Variables
global b32 IsRunning = 1;
global keyboard     *Keyboard;
global mouse        *Mouse;
global clock        *Clock;
global window       *Window;
global renderer     *Renderer;
global sound_system *SoundSystem;
global camera       *Camera;
global gamestate     CurrentState = State_Game;

// Game Variables
global f32 WorldBottom     = -11.0f;
global f32 WorldTop        = 11.0f;
global f32 WorldLeft       = -20.0f;
global f32 WorldRight      = 20.0f;
global f32 HalfWorldWidth  = WorldRight;
global f32 HalfWorldHeight = WorldTop;
global f32 WorldWidth = WorldRight * 2.0f;
global f32 WorldHeight = WorldTop * 2.0f;

// Debug Variables, might want to turn these off on release
global b32 DrawDebugInformation = 0;

u32 PlayerScore = 0;

f32 AnimationTimer = 0.0f;

i32 main(i32 Argc, char **Argv)
{
    // The following makes the compiler not throw a warning for unused
    // variables. I don't really want to turn that warning off, so
    // here it is, a single line of nonsense.
    Argc; Argv;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    Window       = P_CreateOpenGLWindow("Glow", WindowWidth, WindowHeight);
    Renderer     = R_CreateRenderer(Window);
    Keyboard     = I_CreateKeyboard();
    Mouse        = I_CreateMouse();
    Clock        = P_CreateClock();
    SoundSystem  = S_CreateSoundSystem();
    Camera       = R_CreateCamera(Window->Width, Window->Height, glm::vec3(0.0f, 0.0f, 11.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Seed the RNG, GetPerformanceCounter is not the best way, but the results look acceptable
    RandomSeed((u32)SDL_GetPerformanceCounter());

    font *DebugFont = R_CreateFont(Renderer, "fonts/LiberationMono-Regular.ttf", 14, 14);
    font *GameFont  = R_CreateFont(Renderer, "fonts/NovaSquare-Regular.ttf", 100, 100);
    font *UIFont    = R_CreateFont(Renderer, "fonts/NovaSquare-Regular.ttf", 30, 30);

    texture *PlayerTexture      = R_CreateTexture("textures/Player.png");
    texture *BackgroundTexture  = R_CreateTexture("textures/DeepBlue.png");
    texture *WallTexture        = R_CreateTexture("textures/Yellow.png");
    texture *WandererTexture    = R_CreateTexture("textures/Wanderer.png");
    texture *BulletTexture      = R_CreateTexture("textures/Bullet.png");
    texture *SeekerTexture      = R_CreateTexture("textures/Seeker.png");
    texture *PointerTexture     = R_CreateTexture("textures/Pointer.png");
    texture *BlackHoleTexture   = R_CreateTexture("textures/BlackHole.png");
    texture *BouncerTexture     = R_CreateTexture("textures/Bouncer.png");

    f32 BackgroundWidth = WorldWidth + 5.0f;
    f32 BackgroundHeight = WorldHeight + 5.0f;
    entity *Background   = E_CreateEntity(BackgroundTexture, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(BackgroundWidth, BackgroundHeight, 0.0f), 0.0f, 0.0f, 0.0f, Type_None, Collider_Rectangle);

    f32 PlayerSpeed = 3.0f;
    f32 PlayerDrag = 0.8f;
    f32 PlayerRotationSpeed = 0.5f;
    entity *Player       = E_CreateEntity(PlayerTexture, glm::vec3(0.0f, -5.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), 0.0f, PlayerSpeed, PlayerDrag, Type_Player, Collider_Circle);

    // Walls
    entity *LeftWall   = E_CreateEntity(WallTexture, glm::vec3(WorldLeft - 1.0f, 0.0f, 0.0f), glm::vec3(1.0f, BackgroundHeight, 0.0f), 0.0f, 0.0f, 0.0f, Type_Wall, Collider_Rectangle);
    entity *RightWall  = E_CreateEntity(WallTexture, glm::vec3(WorldRight + 1.0f, 0.0f, 0.0f), glm::vec3(1.0f, BackgroundHeight, 0.0f), 0.0f, 0.0f, 0.0f, Type_Wall, Collider_Rectangle);
    entity *TopWall    = E_CreateEntity(WallTexture, glm::vec3(0.0f, WorldTop + 1.0f, 0.0f), glm::vec3(BackgroundWidth, 1.0f, 0.0f), 0.0f, 0.0f, 0.0f, Type_Wall, Collider_Rectangle);
    entity *BottomWall = E_CreateEntity(WallTexture, glm::vec3(0.0f, WorldBottom - 1.0f, 0.0f), glm::vec3(BackgroundWidth, 1.0f, 0.0f), 0.0f, 0.0f, 0.0f, Type_Wall, Collider_Rectangle);

    // These are linked lists that hold enemies and bullets fired by the player
    entity_list *Enemies = E_CreateEntityList(100);
    entity_list *Bullets = E_CreateEntityList(100);

    entity *TestWanderer1 = E_CreateEntity(WandererTexture, glm::vec3(0), glm::vec3(1.0f), 0.0f, 0.0f, 1.0f, Type_Wanderer, Collider_Rectangle);
    entity *TestWanderer2 = E_CreateEntity(WandererTexture, glm::vec3(-2.0f, -4.0f, 0.0f), glm::vec3(1.0f), 0.0f, 0.0f, 1.0f, Type_Wanderer, Collider_Rectangle);
    entity *TestWanderer3 = E_CreateEntity(SeekerTexture, glm::vec3(4.0f, 2.0f, 0.0f), glm::vec3(1.0f), 0.0f, 2.0f, 0.8f, Type_Seeker, Collider_Rectangle);
    entity *TestWanderer4 = E_CreateEntity(SeekerTexture, glm::vec3(-1.0f, 5.0f, 0.0f), glm::vec3(1.0f), 00.0f, 2.0f, 0.8f, Type_Seeker, Collider_Rectangle);
    entity *TestWanderer5 = E_CreateEntity(WandererTexture, glm::vec3(0.0f, -5.0f, 0.0f), glm::vec3(1.0f), 0.0f, 0.0f, 1.0f, Type_Wanderer, Collider_Rectangle);
    entity *BlackHole1    = E_CreateEntity(BlackHoleTexture, glm::vec3(7.0f, -9.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), 0.0f, 0.0f, 1.0f, Type_Pickup, Collider_Rectangle);
    entity *Bouncer1      = E_CreateEntity(BouncerTexture, glm::vec3(4.0f, -5.0f, 0.0f), glm::vec3(1.0f), 0.0f, 0.0f, 1.0f, Type_Bouncer, Collider_Rectangle);

    entity *Pickup1       = E_CreateEntity(BulletTexture, glm::vec3(RandomBetween(WorldLeft, WorldRight), RandomBetween(WorldBottom, WorldTop),0.0f), glm::vec3(3.0f, 1.0f, 0.0f), 0.0f, 0.0f, 1.0f, Type_Pickup, Collider_Rectangle);

    E_PushEntity(Enemies, TestWanderer1);
    E_PushEntity(Enemies, TestWanderer2);
    E_PushEntity(Enemies, TestWanderer3);
    E_PushEntity(Enemies, TestWanderer4);
    E_PushEntity(Enemies, TestWanderer5);
    E_PushEntity(Enemies, BlackHole1);
    E_PushEntity(Enemies, Bouncer1);

    E_PushEntity(Enemies, Pickup1);

    entity *AnimationTest = E_CreateEntity(BouncerTexture, glm::vec3(2.0f, -9.0f, 0.0f), glm::vec3(1.0f), 0.0f, 0.0f, 1.0f, Type_Bouncer, Collider_Rectangle);

    i32 i = 0;
    while(IsRunning)
    {
        P_UpdateClock(Clock);
        R_CalculateFPS(Renderer, Clock);

        { // SECTION: Input Handling
            SDL_Event Event;
            while (SDL_PollEvent(&Event))
            {
                switch (Event.type)
                {
                    case SDL_QUIT:
                    {
                        IsRunning = 0;
                        break;
                    }
                    case SDL_WINDOWEVENT:
                    {
                        if (Event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                        {
                            CurrentState = State_Pause;
                        }
                    }
                }
            }

            I_UpdateKeyboard(Keyboard);
            I_UpdateMouse(Mouse);
            Mouse->WorldPosition.x = Remap((f32)(Mouse->X), 0.0f, (f32)Window->Width, Camera->Position.x - HalfWorldWidth, Camera->Position.x + HalfWorldWidth);
            Mouse->WorldPosition.y = Remap((f32)(Mouse->Y), 0.0f, (f32)Window->Height, Camera->Position.y + HalfWorldHeight, Camera->Position.y - HalfWorldHeight);

            switch (CurrentState)
            {
                case State_Initial:
                {
                    if (I_IsPressed(SDL_SCANCODE_ESCAPE)) { IsRunning = 0; }
                    if (I_IsPressed(SDL_SCANCODE_SPACE))  { CurrentState = State_Game; }
                    if (I_IsReleased(SDL_SCANCODE_RETURN) && I_IsPressed(SDL_SCANCODE_LALT))
                    {
                        P_ToggleFullscreen(Window);
                        R_ResizeRenderer(Renderer, Window->Width, Window->Height);
                    }
                    break;
                }
                case State_Game:
                {
                    // Game state input handling
                    if (I_IsPressed(SDL_SCANCODE_ESCAPE))
                    {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        CurrentState = State_Pause;
                    }

                    // DrawDebugInformation
                    if(I_IsPressed(SDL_SCANCODE_F1) && I_WasNotPressed(SDL_SCANCODE_F1)) { DrawDebugInformation = !DrawDebugInformation; }

                    if (I_IsPressed(SDL_SCANCODE_LSHIFT))
                    {
                        // Camera Stuff
                        if (Mouse->FirstMouse)
                        {
                            Camera->Yaw = -90.0f; // Set the Yaw to -90 so the mouse faces to 0, 0, 0 in the first frame X
                            Camera->Pitch = 0.0f;
                            Mouse->FirstMouse = false;
                        }
                        Camera->Yaw += Mouse->RelX * Mouse->Sensitivity;
                        Camera->Pitch += -Mouse->RelY * Mouse->Sensitivity; // reversed since y-coordinates range from bottom to top
                        if (Camera->Pitch > 89.0f)
                        {
                            Camera->Pitch = 89.0f;
                        }
                        else if (Camera->Pitch < -89.0f)
                        {
                            Camera->Pitch = -89.0f;
                        }
                        glm::vec3 Front;
                        Front.x = cos(glm::radians(Camera->Yaw)) * cos(glm::radians(Camera->Pitch));
                        Front.y = sin(glm::radians(Camera->Pitch));
                        Front.z = sin(glm::radians(Camera->Yaw)) * cos(glm::radians(Camera->Pitch));
                        Camera->Front = glm::normalize(Front);
                    }

                    // Camera Input
                    if (I_IsPressed(SDL_SCANCODE_W) && I_IsPressed(SDL_SCANCODE_LSHIFT)) { Camera->Position += Camera->Front * Camera->Speed * (f32)Clock->DeltaTime; }
                    if (I_IsPressed(SDL_SCANCODE_S) && I_IsPressed(SDL_SCANCODE_LSHIFT)) { Camera->Position -= Camera->Speed * Camera->Front * (f32)Clock->DeltaTime; }
                    if (I_IsPressed(SDL_SCANCODE_A) && I_IsPressed(SDL_SCANCODE_LSHIFT)) { Camera->Position -= glm::normalize(glm::cross(Camera->Front, Camera->Up)) * Camera->Speed * (f32)Clock->DeltaTime; }
                    if (I_IsPressed(SDL_SCANCODE_D) && I_IsPressed(SDL_SCANCODE_LSHIFT)) { Camera->Position += glm::normalize(glm::cross(Camera->Front, Camera->Up)) * Camera->Speed * (f32)Clock->DeltaTime; }
                    if (I_IsPressed(SDL_SCANCODE_SPACE) && I_IsPressed(SDL_SCANCODE_LSHIFT))
                    {
                        R_ResetCamera(Camera, Window->Width, Window->Height, glm::vec3(0.0f, 0.0f, 11.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                        I_ResetMouse(Mouse);
                    }

                    // Player Input
                    if (I_IsPressed(SDL_SCANCODE_W) && I_IsNotPressed(SDL_SCANCODE_LSHIFT)) { Player->Acceleration.y += Player->Speed; }
                    if (I_IsPressed(SDL_SCANCODE_A) && I_IsNotPressed(SDL_SCANCODE_LSHIFT)) { Player->Acceleration.x -= Player->Speed; }
                    if (I_IsPressed(SDL_SCANCODE_S) && I_IsNotPressed(SDL_SCANCODE_LSHIFT)) { Player->Acceleration.y -= Player->Speed; }
                    if (I_IsPressed(SDL_SCANCODE_D) && I_IsNotPressed(SDL_SCANCODE_LSHIFT)) { Player->Acceleration.x += Player->Speed; }

                    // Fire Bullet
                    if(I_IsMouseButtonPressed(SDL_BUTTON_LEFT) && I_WasMouseButtonNotPressed(SDL_BUTTON_LEFT))
                    {
                        f32 DeltaX = Player->Position.x - Mouse->WorldPosition.x;
                        f32 DeltaY = Player->Position.y - Mouse->WorldPosition.y;
                        f32 RotationAngle = (((f32)atan2(DeltaY, DeltaX) * (f32)180.0f) / 3.14159265359f) + 180.0f;
                        f32 BulletSpeed = 20.0f;
                        glm::vec3 BulletDirection = glm::normalize(Mouse->WorldPosition - Player->Position);
                        f32 ScalingFactor = 3.5f;
                        entity *NewBullet = E_CreateEntity(BulletTexture, Player->Position, glm::vec3(0.31f * ScalingFactor, 0.11f * ScalingFactor, 0.0f), RotationAngle, BulletSpeed, 1.0f, Type_Bullet, Collider_Rectangle);
                        NewBullet->Acceleration += BulletDirection * BulletSpeed;
                        E_PushEntity(Bullets, NewBullet);
                    }

                    // Enemy AI
                    // Set "inputs" according to enemy type, i can't figure out a better place to put the enemy AI and i'm not gonna think too much about it
                    for(entity_node *Node = Enemies->Head;
                        Node != NULL;
                        Node = Node->Next)
                    {
                        entity *Entity = Node->Entity;

                        // TODO: Create pickup bullets, and only let the player fire if they have a bullet in his inventory
                        switch(Entity->Type)
                        {
                            case Type_Seeker:
                            {
                                // Get Angle to player, and move towards the player
                                f32 DeltaX = Entity->Position.x - Player->Position.x;
                                f32 DeltaY = Entity->Position.y - Player->Position.y;
                                f32 RotationAngle = (((f32)atan2(DeltaY, DeltaX) * (f32)180.0f) / 3.14159265359f) + 180.0f;
                                Entity->Angle = RotationAngle;
                                glm::vec3 SeekerDirection = Direction(Entity->Position, Player->Position);
                                Entity->Acceleration += SeekerDirection * Entity->Speed;
                            } break;
                            case Type_Wanderer:
                            {
                                f32 X = Cosf((f32)Clock->SecondsElapsed);
                                f32 Y = Sinf((f32)Clock->SecondsElapsed);
                                Entity->Position.x += X * (f32)Clock->DeltaTime * 3.0f;
                                Entity->Position.y += Y * (f32)Clock->DeltaTime * 3.0f;
                                Entity->Angle += 0.4f;
                            } break;

                            case Type_Pickup:
                            {
                                // TODO(Jorge): Change size to make the grow and shrink
                            } break;
                            case Type_Bouncer:
                            {
                                Entity->Acceleration += glm::vec3(1.0f, 1.0f, 0.0f);
                            } break;
                            case Type_Bullet:
                            case Type_None:
                            case Type_Wall:
                            case Type_Player:
                            default:
                            {
                                InvalidCodePath;
                                break;
                            }
                        }
                    }

                    // Handle Window resize Alt+Enter
                    if (I_IsReleased(SDL_SCANCODE_RETURN) && I_IsPressed(SDL_SCANCODE_LALT))
                    {
                        P_ToggleFullscreen(Window);
                        R_ResizeRenderer(Renderer, Window->Width, Window->Height);
                    }

                    break;
                }
                case State_Pause:
                {
                    if (I_IsPressed(SDL_SCANCODE_ESCAPE) && I_WasNotPressed(SDL_SCANCODE_ESCAPE)) { IsRunning = 0; }
                    if (I_IsPressed(SDL_SCANCODE_SPACE) && I_WasNotPressed(SDL_SCANCODE_SPACE)) { CurrentState = State_Game; }
                    if (I_IsReleased(SDL_SCANCODE_RETURN) && I_IsPressed(SDL_SCANCODE_LALT))
                    {
                        P_ToggleFullscreen(Window);
                        R_ResizeRenderer(Renderer, Window->Width, Window->Height);
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
        } // SECTION END: Input Handling

        {  // SECTION: Update
            switch(CurrentState)
            {
                case State_Initial:
                {
                    break;
                }
                case State_Game:
                {
                    // Rotate player according to mouse world position
                    f32 DeltaX = Player->Position.x - Mouse->WorldPosition.x;
                    f32 DeltaY = Player->Position.y - Mouse->WorldPosition.y;
                    Player->Angle = (((f32)atan2(DeltaY, DeltaX) * (f32)180.0f) / 3.14159265359f) + 180.0f;

                    // Update Player
                    E_Update(Player, (f32)Clock->DeltaTime);

                    // Update Enemies
                    for(entity_node *Node = Enemies->Head;
                        Node != NULL;
                        Node = Node->Next)
                    {
                        E_Update(Node->Entity, (f32)Clock->DeltaTime);
                    }

                    // Update Player Bullets
                    for(entity_node *Node = Bullets->Head;
                        Node != NULL;
                        Node = Node->Next)
                    {
                        E_Update(Node->Entity, (f32)Clock->DeltaTime);

                        // If the bullet is no longer near the play
                        // area, delete this. Maybe later just checked
                        // square distances to avoid a sqrt.
                        if(Magnitude(Node->Entity->Position) > 30.0f)
                        {
                            E_ListFreeNode(Bullets, Node);
                        }
                    }

                    // Entities are updated, now let's do collision
                    glm::vec2 ResolutionDirection;
                    f32 ResolutionOverlap;

                    // Collision Player vs Walls
                    if(E_EntitiesCollide(Player, LeftWall, &ResolutionDirection, &ResolutionOverlap))
                    {
                        glm::vec2 I = ResolutionDirection * ResolutionOverlap;
                        Player->Position.x -= I.x;
                        Player->Position.y -= I.y;
                    }
                    if(E_EntitiesCollide(Player, RightWall, &ResolutionDirection, &ResolutionOverlap))
                    {
                        glm::vec2 I = ResolutionDirection * ResolutionOverlap;
                        Player->Position.x -= I.x;
                        Player->Position.y -= I.y;
                    }
                    if(E_EntitiesCollide(Player, TopWall, &ResolutionDirection, &ResolutionOverlap))
                    {
                        glm::vec2 I = ResolutionDirection * ResolutionOverlap;
                        Player->Position.x -= I.x;
                        Player->Position.y -= I.y;
                    }
                    if(E_EntitiesCollide(Player, BottomWall, &ResolutionDirection, &ResolutionOverlap))
                    {
                        glm::vec2 I = ResolutionDirection * ResolutionOverlap;
                        Player->Position.x -= I.x;
                        Player->Position.y -= I.y;
                    }

                    // Collision Player vs Enemies
                    for(entity_node *Node = Enemies->Head;
                        Node != NULL;
                        Node = Node->Next)
                    {
                        if(E_EntitiesCollide(Player, Node->Entity, &ResolutionDirection, &ResolutionOverlap))
                        {
                            E_ListFreeNode(Enemies, Node);
                        }
                    }

                    // Enemies vs Player Bullets,  note: this is a n^m loop
                    for(entity_node *Enemy = Enemies->Head;
                        Enemy != NULL;
                        Enemy = Enemy->Next)
                    {
                        for(entity_node *Bullet = Bullets->Head;
                            Bullet != NULL;
                            Bullet = Bullet->Next)
                        {
                            if(E_EntitiesCollide(Enemy->Entity, Bullet->Entity, &ResolutionDirection, &ResolutionOverlap))
                            {
                                PlayerScore += 1;
                                E_ListFreeNode(Enemies, Enemy);
                                E_ListFreeNode(Bullets, Bullet);
                            }
                        }
                    }
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

            R_UpdateCamera(Renderer, Camera);

        } // SECTION END: Update

        { // SECTION: Render
            R_BeginFrame(Renderer);

            switch(CurrentState)
            {
                case State_Initial:
                {
                    Renderer->BackgroundColor = MenuBackgroundColor;
                    R_SetActiveShader(Renderer->Shaders.Texture);
                    break;
                }
                case State_Game:
                {
                    Renderer->BackgroundColor = BackgroundColor;

                    R_SetActiveShader(Renderer->Shaders.Texture);

                    R_DrawEntity(Renderer, Background);
                    R_DrawEntity(Renderer, Player);
                    R_DrawEntity(Renderer, AnimationTest);

                    R_DrawEntityList(Renderer, Enemies);
                    R_DrawEntityList(Renderer, Bullets);

                    // Draw Mouse Pointer. The Position needs
                    // adjustment since R_DrawTexture draws
                    // centered. Could also create a crosshair image
                    // and it would be perfect.
                    glm::vec3 CursorSize = glm::vec3(0.44f, 0.62f, 0.0f);
                    glm::vec3 CorrectedCursorPosition = glm::vec3(Mouse->WorldPosition.x - (CursorSize.x / 2.0f),
                                                                  Mouse->WorldPosition.y - (CursorSize.y / 2.0f),
                                                                  0.1f);
                    R_DrawTexture(Renderer, PointerTexture, CorrectedCursorPosition, CursorSize, glm::vec3(0.0f), 0.0f);

                    // Draw player score
                    char PlayerScoreString[80];
                    sprintf_s(PlayerScoreString, "Score: %d", PlayerScore);
                    R_DrawText2D(Renderer, PlayerScoreString, UIFont, glm::vec2(Window->Width - UIFont->Width * 5 , Window->Height-UIFont->Height), glm::vec2(1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f));

                    if(DrawDebugInformation)
                    {
                        // String buffer used for snprintf
                        char String[100] = {};

                        // GPU and OpenGL stuff
                        f32 LeftMargin = 4.0f;
                        R_DrawText2D(Renderer, "GPU:", DebugFont, glm::vec2(LeftMargin, Window->Height - DebugFont->Height), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                        R_DrawText2D(Renderer, (char*)Renderer->HardwareVendor, DebugFont, glm::vec2(LeftMargin * 2, Window->Height - DebugFont->Height * 2), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                        R_DrawText2D(Renderer, (char*)Renderer->HardwareModel, DebugFont, glm::vec2(LeftMargin * 2, Window->Height - DebugFont->Height * 3), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                        snprintf(String, sizeof(char) * 99,"OpenGL Version: %s", Renderer->OpenGLVersion);
                        R_DrawText2D(Renderer, String, DebugFont, glm::vec2(LeftMargin * 2, Window->Height - DebugFont->Height * 4), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                        snprintf(String, sizeof(char) * 99,"GLSL Version: %s", Renderer->GLSLVersion);
                        R_DrawText2D(Renderer, String, DebugFont, glm::vec2(LeftMargin * 2, Window->Height - DebugFont->Height * 5), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));

                        // CPU
                        R_DrawText2D(Renderer, "CPU:", DebugFont, glm::vec2(LeftMargin, Window->Height - DebugFont->Height * 6), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                        snprintf(String, sizeof(char) * 99,"Cache Line Size: %d", SDL_GetCPUCacheLineSize());
                        R_DrawText2D(Renderer, String, DebugFont, glm::vec2(LeftMargin * 2, Window->Height - DebugFont->Height * 7), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                        snprintf(String, sizeof(char) * 99,"Core Count: %d", SDL_GetCPUCount());
                        R_DrawText2D(Renderer, String, DebugFont, glm::vec2(LeftMargin * 2, Window->Height - DebugFont->Height * 8), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));

                        // FPS Min Ms/Max Ms/ Avg Ms
                        snprintf(String, sizeof(char) * 99,"FPS: %.4f", Renderer->FPS);
                        R_DrawText2D(Renderer, String, DebugFont, glm::vec2(LeftMargin, Window->Height - DebugFont->Height * 9), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                        snprintf(String, sizeof(char) * 99,"Average Ms Per Frame: %.5f", Renderer->AverageMsPerFrame);
                        R_DrawText2D(Renderer, String, DebugFont, glm::vec2(LeftMargin, Window->Height - DebugFont->Height * 10), glm::vec2(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));

                        // Mouse World Position
                    }

                    break;
                }
                case State_Pause:
                {
                    Renderer->BackgroundColor = MenuBackgroundColor;
                    R_SetActiveShader(Renderer->Shaders.Texture);

                    f32 HardcodedFontWidth = 38.0f * 1.5f;
                    f32 XPos = (Window->Width / 2.0f) - HardcodedFontWidth * 2.5f;
                    R_DrawText2D(Renderer, "Pause", GameFont, glm::vec2(XPos, Window->Height / 2.0f + 50.0f), glm::vec2(1.5f, 1.5f), glm::vec3(1.0f, 1.0f, 1.0f));
                    R_DrawText2D(Renderer, "press space to continue", GameFont, glm::vec2(Window->Width / 2.0f - 38.0f * 11.5f * 0.7f, Window->Height / 2.0f - 100.0f), glm::vec2(0.7, 0.7), glm::vec3(1.0f, 1.0f, 1.0f));
                    R_DrawText2D(Renderer, "press escape to exit", GameFont, glm::vec2(Window->Width / 2.0f - 38.0f * 10.0f * 0.7f, Window->Height / 2.0f - 200.0f), glm::vec2(0.7, 0.7), glm::vec3(1.0f, 1.0f, 1.0f));

                    break;
                }
                default:
                {
                    InvalidCodePath;
                    break;
                }
            }

            R_EndFrame(Renderer);
        } // SECTION END: Render
        SDL_GL_DeleteContext(Window->Handle);
    }

    return 0;
}
