#include "TVComponent.h"

TVComponent::TVComponent(Context* context) :
	Component(context),
	isFileOpened_(false),
	isStopped_(false),
	frameWidth_(0),
	frameHeight_(0),
	file_(0),
	outputMaterial(0),
	prevTime_(0), 
	prevFrame_(0),
	framePlanarDataY_(0),
	framePlanarDataU_(0),
	framePlanarDataV_(0)

{
	SubscribeToEvent(E_SCENEPOSTUPDATE, URHO3D_HANDLER(TVComponent, HandleUpdate));
}

TVComponent::~TVComponent()
{

	if (framePlanarDataY_)
	{
		delete[] framePlanarDataY_;
		framePlanarDataY_ = 0;
	}

	if (framePlanarDataU_)
	{
		delete[] framePlanarDataU_;
		framePlanarDataU_ = 0;
	}

	if (framePlanarDataV_)
	{
		delete[] framePlanarDataV_;
		framePlanarDataV_ = 0;
	}

}

void TVComponent::RegisterObject(Context* context)
{
	context->RegisterFactory<TVComponent>();
}

bool TVComponent::OpenFileName(String name)
{
	bool ret = false;
	isFileOpened_ = false;
	ogg_sync_init(&m_OggSyncState);
	theora_comment_init(&m_TheoraComment);
	theora_info_init(&m_TheoraInfo);

	if (OpenFile(name))
	{
		bool dataFound = false;
		int theoraPacketsFound = 0;

		while (!dataFound)
		{
			// grab some data from file and put it into the ogg stream
			this->BufferData();

			// grab the ogg page from the stream
			while (ogg_sync_pageout(&m_OggSyncState, &m_OggPage) > 0)
			{
				ogg_stream_state test;

				// check: if this is not headers page, then we finished
				if (!ogg_page_bos(&m_OggPage))
				{
					// all headers pages are finished, now there are only data packets
					dataFound = true;

					// don't leak the page, get it into the video stream
					ogg_stream_pagein(&m_VideoStream, &m_OggPage);
					break;
				}

				// we need to identify the stream

				// 1) Init the test stream with the s/n from our page
				ogg_stream_init(&test, ogg_page_serialno(&m_OggPage));
				// 2) Add this page to this test stream
				ogg_stream_pagein(&test, &m_OggPage);
				// 3) Decode the page into the packet
				ogg_stream_packetout(&test, &m_OggPacket);

				// try to interpret the packet as Theora's data
				if (!theoraPacketsFound && theora_decode_header(&m_TheoraInfo, &m_TheoraComment, &m_OggPacket) >= 0)
				{
					// theora found ! Let's copy the stream
					memcpy(&m_VideoStream, &test, sizeof(test));
					theoraPacketsFound++;
				}
				else
				{
					// non-theora (vorbis maybe)
					ogg_stream_clear(&test);
				}
			}
		}

		// no theora found, maybe this is music file ?
		if (theoraPacketsFound)
		{
			int err;
			// by specification we need 3 header packets for any logical stream (theora, vorbis, etc.)
			while (theoraPacketsFound < 3)
			{
				err = ogg_stream_packetout(&m_VideoStream, &m_OggPacket);
				if (err < 0)
				{
					// some stream errors (maybe stream corrupted?)
					break;
				}
				if (err > 0)
				{
					if (theora_decode_header(&m_TheoraInfo, &m_TheoraComment, &m_OggPacket) >= 0)
						theoraPacketsFound++;
					else
					{
						// another stream corruption ?
						break;
					}
				}

				// if read nothing from packet - just grab more data into packet
				if (!err)
				{
					if (ogg_sync_pageout(&m_OggSyncState, &m_OggPage) > 0)
					{
						// if data arrivet into packet - put it into our logical stream
						ogg_stream_pagein(&m_VideoStream, &m_OggPage);
					}
					else
					{
						// nothing goint from the ogg stream, need to read some data from file
						if (!this->BufferData())
						{
							// f***k ! End of file :(
							break;
						}
					}
				}
			}
		}

		// if we have theora ok
		if (theoraPacketsFound)
		{
			// init decoder
			if (0 == theora_decode_init(&m_TheoraState, &m_TheoraInfo))
			{
				// decoder intialization succeed
				isFileOpened_ = true;

				frameWidth_ = m_TheoraInfo.frame_width;
				frameHeight_ = m_TheoraInfo.frame_height;
				framesPerSecond_ = static_cast<float>(m_TheoraInfo.fps_numerator) / static_cast<float>(m_TheoraInfo.fps_denominator);
				videoTimer_ = 0;
				isStopped_ = false;
			}
		}
	}

	ret = isFileOpened_;
	return ret;
}

bool TVComponent::SetOutputModel(StaticModel* sm)
{
	bool ret = false;
	if (sm)
	{
		// Set model surface
		outputModel = sm;
		outputMaterial = sm->GetMaterial(0);

		// Create textures & images
		InitTexture();
		//InitCopyBuffer();
		ScaleModelAccordingVideoRatio();
	}

	return ret;
}

void TVComponent::Play()
{
	isStopped_ = false;
}

void TVComponent::Pause() 
{
	isStopped_ = true;
}

void TVComponent::Loop(bool isLoop)
{

}

void TVComponent::Stop()
{
	isStopped_ = true;
	videoTimer_ = 0;
	prevFrame_ = 0;
	file_->Seek(0);
}

void TVComponent::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
	using namespace Update;
	float timeStep = eventData[P_TIMESTEP].GetFloat();
	//Time* time = GetSubsystem<Time>();

	unsigned frame = Advance(timeStep);
	if (!isStopped_ && prevFrame_ != frame)
	{
		UpdatePlaneTextures();		
	}

	prevFrame_ = frame;
}

bool TVComponent::OpenFile(String fileName)
{
	file_ = new File(context_, fileName, FILE_READ);
	return (file_ != 0);
}

// iOrange - buffer some data from file into Ogg stream
int TVComponent::BufferData(void)
{
	if (!file_) return 0;

	static const int k_SyncBufferSize = 8192;
	//static const int k_SyncBufferSize = 4096;
	
	unsigned s = file_->GetSize();

	// ask some buffer for putting data into stream
	char* buffer = ogg_sync_buffer(&m_OggSyncState, k_SyncBufferSize);
	// read data from file
	int bytes = file_->Read(buffer, k_SyncBufferSize);
	// put readed data into Ogg stream
	ogg_sync_wrote(&m_OggSyncState, bytes);
	return bytes;
}

unsigned TVComponent::Advance(float timeStep)
{
	if (isStopped_)
		return lastVideoFrame_;

	// advance video timer by delta time in milliseconds
	videoTimer_ += timeStep;

	// calculate current frame
	const unsigned curFrame = static_cast<unsigned>(floor(videoTimer_ * framesPerSecond_));


	if (lastVideoFrame_ != curFrame)
	{
		lastVideoFrame_ = curFrame;
		this->DecodeVideoFrame();
	}

	return curFrame;
}

void TVComponent::DecodeVideoFrame()
{
	// first of all - grab some data into ogg packet
	while (ogg_stream_packetout(&m_VideoStream, &m_OggPacket) <= 0)
	{
		// if no data in video stream, grab some data
		if (!BufferData())
		{
			isStopped_ = true;
			return;
		}

		// grab all decoded ogg pages into our video stream
		while (ogg_sync_pageout(&m_OggSyncState, &m_OggPage) > 0)
			ogg_stream_pagein(&m_VideoStream, &m_OggPage);
	}

	// load packet into theora decoder
	if (0 == theora_decode_packetin(&m_TheoraState, &m_OggPacket))
	{
		// if decoded ok - get YUV frame
		theora_decode_YUVout(&m_TheoraState, &m_YUVFrame);
		
	}
	else
		isStopped_ = true;
}

bool TVComponent::InitTexture()
{
	bool ret = false;

	// Try clear if using case of reassingn the movie file
	for (int i = 0; i < YUV_PLANE_MAX_SIZE; ++i)
	{
		if (outputTexture[i])
		{
			outputTexture[i]->ReleaseRef();
			outputTexture[i] = 0;
		}
	}

	// do this for fill m_YUVFrame with properly info about frame
	Advance(0);

	// Planes textures create
	for (int i = 0; i < YUV_PLANE_MAX_SIZE; ++i)
	{
		int texWidth = 0;
		int texHeight = 0;

		switch (i)
		{
		case YUV_PLANE_Y:
			texWidth = m_YUVFrame.y_width;
			texHeight = m_YUVFrame.y_height;
			framePlanarDataY_ = new unsigned char[texWidth * texHeight];
			break;
		
		case YUV_PLANE_U:
			texWidth = m_YUVFrame.uv_width;
			texHeight = m_YUVFrame.uv_height;
			framePlanarDataU_ = new unsigned char[texWidth * texHeight];
			break;

		case YUV_PLANE_V:
			texWidth = m_YUVFrame.uv_width;
			texHeight = m_YUVFrame.uv_height;
			framePlanarDataV_ = new unsigned char[texWidth * texHeight];
			break;
		}

		outputTexture[i] = SharedPtr<Texture2D>(new Texture2D(context_));
		outputTexture[i]->SetSize(texWidth, texHeight, Graphics::GetLuminanceFormat(), TEXTURE_DYNAMIC);
		outputTexture[i]->SetFilterMode(FILTER_BILINEAR);
		outputTexture[i]->SetNumLevels(1);
		outputTexture[i]->SetAddressMode(TextureCoordinate::COORD_U, TextureAddressMode::ADDRESS_MIRROR);
		outputTexture[i]->SetAddressMode(TextureCoordinate::COORD_V, TextureAddressMode::ADDRESS_MIRROR);

	}
	
	// assign planes textures into sepparated samplers for shader
	outputMaterial->SetTexture(TextureUnit::TU_DIFFUSE, outputTexture[YUV_PLANE_Y]);
	outputMaterial->SetTexture(TextureUnit::TU_SPECULAR, outputTexture[YUV_PLANE_U]);
	outputMaterial->SetTexture(TextureUnit::TU_NORMAL, outputTexture[YUV_PLANE_V]);
	
	outputModel->SetMaterial(0, outputMaterial);

	return ret;
	
}

void TVComponent::UpdatePlaneTextures() 
{
	Graphics* graphics = GetSubsystem<Graphics>();

	// Convert non-planar YUV-frame into separated planar raw-textures 
	for (int y = 0; y < m_YUVFrame.uv_height; ++y)
	{
		for (int x = 0; x < m_YUVFrame.uv_width; ++x)
		{

			const int offsetUV = x + y * m_YUVFrame.uv_width;

			framePlanarDataU_[offsetUV] = m_YUVFrame.u[x + y * m_YUVFrame.uv_stride];
			framePlanarDataV_[offsetUV] = m_YUVFrame.v[x + y * m_YUVFrame.uv_stride];

			// 2x2 more work for Y

			const int offsetLUTile = x + y * m_YUVFrame.y_width;
			framePlanarDataY_[offsetLUTile] = m_YUVFrame.y[x + y * m_YUVFrame.y_stride];

			const int offsetRUTile = m_YUVFrame.uv_width + x + y * m_YUVFrame.y_width;
			framePlanarDataY_[offsetRUTile] = m_YUVFrame.y[m_YUVFrame.uv_width + x + y * m_YUVFrame.y_stride];

			const int offsetLBTile = x + (y + m_YUVFrame.uv_height) * m_YUVFrame.y_width;
			framePlanarDataY_[offsetLBTile] = m_YUVFrame.y[x + ((y + m_YUVFrame.uv_height) * m_YUVFrame.y_stride)];

			const int offsetRBTile = (m_YUVFrame.uv_width + x) + (y + m_YUVFrame.uv_height) * m_YUVFrame.y_width;
			framePlanarDataY_[offsetRBTile] = m_YUVFrame.y[(m_YUVFrame.uv_width + x) + ((y + m_YUVFrame.uv_height) * m_YUVFrame.y_stride)];

		}
	}

	// Fill textures with new data
	for (int i = 0; i < YUV_PLANE_MAX_SIZE; ++i)
	{
		switch (i)
		{
		case YUVP420PlaneType::YUV_PLANE_Y:
			outputTexture[i]->SetSize(m_YUVFrame.y_width, m_YUVFrame.y_height, Graphics::GetLuminanceFormat(), TEXTURE_DYNAMIC);
			outputTexture[i]->SetData(0, 0, 0, m_YUVFrame.y_width, m_YUVFrame.y_height, (const void*)framePlanarDataY_);			
			break;

		case YUVP420PlaneType::YUV_PLANE_U:
			outputTexture[i]->SetSize(m_YUVFrame.uv_width, m_YUVFrame.uv_height, Graphics::GetLuminanceFormat(), TEXTURE_DYNAMIC);
			outputTexture[i]->SetData(0, 0, 0, m_YUVFrame.uv_width, m_YUVFrame.uv_height, (const void*)framePlanarDataU_);
			break;

		case YUVP420PlaneType::YUV_PLANE_V:
			outputTexture[i]->SetSize(m_YUVFrame.uv_width, m_YUVFrame.uv_height, Graphics::GetLuminanceFormat(), TEXTURE_DYNAMIC);
			outputTexture[i]->SetData(0, 0, 0, m_YUVFrame.uv_width, m_YUVFrame.uv_height, (const void*)framePlanarDataV_);
			break;
		}
	}
}

void TVComponent::ScaleModelAccordingVideoRatio()
{
	if (outputModel)
	{
		Node* node = outputModel->GetNode();
		float ratioW = (float)frameWidth_ / (float)frameHeight_;
		float ratioH = (float)frameHeight_ / (float)frameWidth_;

		Vector3 originalScale = node->GetScale();
		node->SetScale(Vector3(originalScale.x_, originalScale.x_ * ratioH, originalScale.z_ * ratioH));
	}
}
