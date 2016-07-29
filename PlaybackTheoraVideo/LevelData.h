#include "Urho3DAll.h"

class TVComponent;

class CLevelData : public Object
{
	URHO3D_OBJECT(CLevelData, Object);
public:
	CLevelData(Context* context);

	SharedPtr<Scene> scene;
	SharedPtr<Camera> camera;
	SharedPtr<Node> cameraNode;
	SharedPtr<Node> playerNode;
	SharedPtr<TVComponent> tvc;

	float yaw_;
	float pitch_;


	bool InitScene(String sceneName = "Scene.xml");
	bool InitCameraFromSceneData(String camName = "Camera");
	bool InitTVComponentForSceneNode(String nodeName = "TV");
	

	virtual ~CLevelData();


private:
	bool LoadSceneFromXMLFile(String sceneFileName);
	void ViewRotate(float timeStep);
	void ViewMove(float timeStep);
	void HandleUpdate(StringHash eventType, VariantMap& eventData);

};