#include "Urho3DAll.h"

#include <ogg/ogg.h>
#include <theora/theora.h>
#include <theora/theoradec.h>

#pragma comment( lib, "libogg_static.lib" )
#pragma comment( lib, "libtheora_static.lib" )

class URHO3D_API TVComponent : public Component
{
	URHO3D_OBJECT(TVComponent, Component);

public:
	SharedPtr<StaticModel> outputModel;
	SharedPtr<Material> outputMaterial;
	SharedPtr<Texture2D> outputTexture;
	SharedPtr<Image> outputImage;

	unsigned prevTime_, prevFrame_;
	char* frameData_;

	TVComponent(Context* context);
	virtual ~TVComponent();
	static void RegisterObject(Context* context);

	bool OpenFileName(String name);
	bool SetOutputModel(StaticModel* sm);
	void Play();
	void Pause();
	void Loop(bool isLoop = true);
	void Stop();
	unsigned Advance(float timeStep);

	void ScaleModelAccordingVideoRatio();

	int GetFrameWidth(void) const { return frameWidth_; };
	int GetFrameHeight(void) const { return frameHeight_; };
	float GetFramesPerSecond(void) const { return framesPerSecond_; };

	void GetFrameRGB(char* outFrame, int pitch);
	void GetFrameYUV444(char* outFrame, int pitch);

private:
	void HandleUpdate(StringHash eventType, VariantMap& eventData);
	bool OpenFile(String fileName);
	int BufferData(void);
	void DecodeVideoFrame(void);
	bool InitTexture();
	bool InitCopyBuffer();

	File* file_;
	float framesPerSecond_;
	bool isFileOpened_;
	bool isStopped_;
	unsigned frameWidth_;
	unsigned frameHeight_;
	float videoTimer_;
	unsigned lastVideoFrame_;

	/* Ogg stuff */
	ogg_sync_state		m_OggSyncState;
	ogg_page			m_OggPage;
	ogg_packet			m_OggPacket;
	ogg_stream_state	m_VideoStream;

	/* Theora stuff */
	theora_info			m_TheoraInfo;
	theora_comment		m_TheoraComment;
	theora_state		m_TheoraState;
	yuv_buffer			m_YUVFrame;
};