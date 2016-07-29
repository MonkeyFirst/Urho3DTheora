#include "TVComponent.h"

inline int clip(int t)
{
	return ((t < 0) ? (0) : ((t > 255) ? 255 : t));
}

TVComponent::TVComponent(Context* context) :
	Component(context),
	isFileOpened_(false),
	isStopped_(false),
	frameWidth_(0),
	frameHeight_(0),
	file_(0),
	outputImage(0),
	outputMaterial(0),
	outputTexture(0),
	prevTime_(0), 
	prevFrame_(0),
	frameData_(0)
{
	SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(TVComponent, HandleUpdate));
}

TVComponent::~TVComponent()
{

	if (frameData_)
	{
		delete frameData_;
		frameData_ = 0;
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

				// iOrange - we nee to identify the stream

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

		// Create textures & images
		InitTexture();
		InitCopyBuffer();
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
		GetFrameRGB(frameData_, frameWidth_);
		//GetFrameYUV444(frameData_, frameWidth_);
		outputImage->SetData((unsigned char*)frameData_);
		outputTexture->SetData(outputImage);
		
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

	static const int k_SyncBufferSize = 4096;
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

	if (outputTexture)
	{
		outputTexture->ReleaseRef();
		outputTexture = 0;
	}

	if (outputImage)
	{
		outputImage->ReleaseRef();
		outputImage = 0;
	}

	outputTexture = SharedPtr<Texture2D>(new Texture2D(context_));
	outputTexture->SetSize(frameWidth_, frameHeight_, Graphics::GetRGBFormat(), TEXTURE_DYNAMIC);
	outputTexture->SetFilterMode(FILTER_BILINEAR);
	outputTexture->SetNumLevels(1);
	outputTexture->SetAddressMode(TextureCoordinate::COORD_U, TextureAddressMode::ADDRESS_BORDER);
	outputTexture->SetAddressMode(TextureCoordinate::COORD_V, TextureAddressMode::ADDRESS_BORDER);
	

	outputImage = SharedPtr<Image>(new Image(context_));
	outputImage->SetSize(frameWidth_, frameHeight_, 3);
	outputTexture->SetData(outputImage);

	Material* mat = outputModel->GetMaterial(0);
	Technique* t = mat->GetTechnique(0);
	mat->SetTexture(TextureUnit::TU_DIFFUSE, outputTexture);
	

	if (outputTexture && outputImage) ret = true;


	return ret;
	
}

void TVComponent::GetFrameRGB(char* outFrame, int pitch)
{
	for (int y = 0; y < frameHeight_; ++y)
	{
		for (int x = 0; x < frameWidth_; ++x)
		{
			const int off = x + y * pitch;
			const int xx = x >> 1;
			const int yy = y >> 1;

			const int Y = static_cast<int>(m_YUVFrame.y[x + y * m_YUVFrame.y_stride]) - 16;
			const int U = static_cast<int>(m_YUVFrame.u[xx + yy * m_YUVFrame.uv_stride]) - 128;
			const int V = static_cast<int>(m_YUVFrame.v[xx + yy * m_YUVFrame.uv_stride]) - 128;

			outFrame[off * 3 + 0] = clip((298 * Y + 409 * V + 128) >> 8);
			outFrame[off * 3 + 1] = clip((298 * Y - 100 * U - 208 * V + 128) >> 8);
			outFrame[off * 3 + 2] = clip((298 * Y + 516 * U + 128) >> 8);
		}
	}
}

void TVComponent::GetFrameYUV444(char* outFrame, int pitch)
{
	for (int y = 0; y < frameHeight_; ++y)
	{
		for (int x = 0; x < frameWidth_; ++x)
		{
			const int off = x + y * pitch;
			const int xx = x >> 1;
			const int yy = y >> 1;

			outFrame[off * 3 + 0] = m_YUVFrame.y[x + y * m_YUVFrame.y_stride];
			outFrame[off * 3 + 1] = m_YUVFrame.u[xx + yy * m_YUVFrame.uv_stride];
			outFrame[off * 3 + 2] = m_YUVFrame.v[xx + yy * m_YUVFrame.uv_stride];
		}
	}
}


bool TVComponent::InitCopyBuffer()
{
	bool ret = false;

	if (frameData_) 
	{
		delete frameData_;
		frameData_ = 0;
	}

	frameData_ = new char[frameWidth_ * frameHeight_ * 3];
	

	return (ret = frameData_ != 0);
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