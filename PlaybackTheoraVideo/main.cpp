#include "Urho3DAll.h"
#include "LevelData.h"

using namespace Urho3D;

class MyApp : public Application
{
public:
	CLevelData* level;

    MyApp(Context* context) :
        Application(context)
    {
    }
    virtual void Setup()
    {
        // Called before engine initialization. engineParameters_ member variable can be modified here
		engineParameters_["WindowWidth"] = 1280;
		engineParameters_["WindowHeight"] = 720;
		engineParameters_["FullScreen"] = false;
		engineParameters_["VSync"] = true;
		engineParameters_["FrameLimiter "] = true;
    }
    virtual void Start()
    {
		level = new CLevelData(context_);
		
		if (level->InitScene())
		{
			level->InitCameraFromSceneData();
			level->InitTVComponentForSceneNode("TV");
			
		} 
		else 
		{

			engine_->Exit();
		}

        // Called after engine initialization. Setup application & subscribe to events here
        SubscribeToEvent(E_KEYDOWN, URHO3D_HANDLER(MyApp, HandleKeyDown));
    }
    virtual void Stop()
    {
		if (level) delete level;

		// Perform optional cleanup after main loop has terminated
    }
    void HandleKeyDown(StringHash eventType, VariantMap& eventData)
    {
        using namespace KeyDown;
        // Check for pressing ESC. Note the engine_ member variable for convenience access to the Engine object
        int key = eventData[P_KEY].GetInt();
        if (key == KEY_ESCAPE)
            engine_->Exit();
    }
};
URHO3D_DEFINE_APPLICATION_MAIN(MyApp)
