#ifdef _WIN32
#include <windows.h>
#endif

#include "GameEditorApp.hpp"
#include <GameApplication.hpp>
#include <Poseidon/Core/Application.hpp>
#include <Poseidon/Core/Game/GameLoop.hpp>
#include <Poseidon/Dev/Debug/DebugOverlay.hpp>
#include <Poseidon/Foundation/Platform/CrashHandler.hpp>
#include <Poseidon/Foundation/Common/ConsoleUtils.hpp>
#include <Poseidon/Foundation/Common/GamePaths.hpp>

// Includes required for bypassing CWR game initialization
#include <Poseidon/Audio/AudioFactory.hpp>
#include <Poseidon/Audio/IAudioSystem.hpp>
#include <Poseidon/Audio/Dummy/SoundSystemDummy.hpp>
#include <Poseidon/Graphics/Rendering/Draw/FontSystem.hpp>
#include <Poseidon/Graphics/Core/Engine.hpp>
#include <Poseidon/World/Scene/Scene.hpp>
#include <Poseidon/Graphics/Rendering/Lighting/Lights.hpp>
#include <Poseidon/Core/Config/EngineConfig.hpp>
#include <Poseidon/World/Scene/Camera/Camera.hpp>
#include <Poseidon/Foundation/Platform/InitBridge.hpp>
#include <SDL3/SDL.h>

class GameEditorApplication : public GameApplication
{
public:
    Poseidon::GameEditorApp editorApp;

    void RegisterGameModules() override {}

    bool ParseCommandLine(const char* commandLine) override
    {
        Poseidon::Foundation::GamePaths::Instance().Initialize("Poseidon", "Poseidon");
        return GameApplication::ParseCommandLine(commandLine);
    }

    bool InitializeSubsystems() override
    {
        // Don't call GameApplication::InitializeSubsystems() because it loads CWR-specific content.
        Poseidon::FontSystem::Instance().Initialize();

        // Force windowed mode for the editor startup
        ENGINE_CONFIG.displayMode = "windowed";
        ENGINE_CONFIG.useWindow = true;

        // Register our Editor UI hook. The Engine's GL33 backend will call this
        // instead of drawing the normal Dev Panel.
        Poseidon::Dev::DebugOverlay::SetEditorUIHook([]() {
            if (Poseidon::GApp && dynamic_cast<GameEditorApplication*>(Poseidon::GApp))
            {
                auto& app = static_cast<GameEditorApplication*>(Poseidon::GApp)->editorApp;
                app.render(ImGui::GetIO().DisplaySize);
            }
        });

        return true;
    }


    void RegisterAudioBackends() override
    {
        Poseidon::RegisterDummyAudioBackend();
    }

    bool InitializeSound() override
    {
        extern void CleanupSoundSystem();
        CleanupSoundSystem();
        Poseidon::GSoundsys = new Poseidon::SoundSystemDummy();
        return true;
    }

    bool InitializeWorld() override
    {
        if (!Poseidon::GScene && Poseidon::GEngine)
        {
            ENGINE_CONFIG.noMenuScene = true;
            Poseidon::GScene = new Poseidon::Scene();
            Poseidon::GScene->Init(Poseidon::GEngine, nullptr);
            Poseidon::LightSun* mainLight = new Poseidon::LightSun();
            mainLight->Recalculate(nullptr);
            Poseidon::GScene->SetMainLight(mainLight);
            Poseidon::GScene->MainLightChanged();
            GPreloadedTextures_Preload(false);
        }
        return true;
    }

    void RunMainLoop() override
    {
        if (!Poseidon::GEngine) return;
        Poseidon::GApp->m_canRender = true;

        Poseidon::Camera camera;

        while (!m_closeRequest)
        {
            Poseidon::GEngine->HandleEvents();
            if (!Poseidon::GEngine->IsOpen()) break;

            const bool* keys = SDL_GetKeyboardState(nullptr);
            if (keys && keys[SDL_SCANCODE_ESCAPE])
            {
                m_closeRequest = true;
                break;
            }

            if (!Poseidon::GEngine->IsAbleToDraw())
                continue;

            // Render a nice dark grey background for the viewport
            Poseidon::GEngine->InitDraw(true, Poseidon::PackedColor(Poseidon::Color(0.18f, 0.18f, 0.20f, 1.0f)));

            // ─── 3D scene rendering pipeline ───
            if (editorApp.isProjectLoaded() && Poseidon::GScene && Poseidon::GScene->GetCamera())
            {
                // Apply camera position/orientation/projection before the draw pipeline
                editorApp.updateCamera();

                // Begin the engine's object draw pipeline
                Poseidon::GScene->BeginObjects();
                Poseidon::GScene->SelectActiveLights(nullptr);

                // Submit editor scene geometry (grid, axes, entities)
                editorApp.drawScene();

                // Process and sort submitted objects
                Poseidon::GScene->EndObjects();

                // Execute the multi-pass draw pipeline
                Poseidon::GScene->DrawObjectsAndShadowsPass1();
                Poseidon::GScene->DrawObjectsAndShadowsPass2();

                // Flush GPU draw queues
                Poseidon::GEngine->FlushQueues();
                Poseidon::GScene->ObjectsDrawn();
                Poseidon::GEngine->FlushQueues();
            }

            // Editor UI Hook is called within FinishDraw() or NextFrame()
            Poseidon::GEngine->FinishDraw();
            Poseidon::GEngine->NextFrame();
        }
    }
};

#ifdef _WIN32
int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    Poseidon::Foundation::InstallCrashHandler(nullptr);
    if (!strstr(szCmdLine, "--check"))
        Poseidon::Foundation::attachParentConsole();
    
    GameEditorApplication app;
    return app.Run(hInst, szCmdLine, sw);
}
#else
int main(int argc, char* argv[])
{
    Poseidon::Foundation::InstallCrashHandler(nullptr);
    GameEditorApplication app;
    return app.Run(argc, argv);
}
#endif
