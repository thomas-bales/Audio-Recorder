#include"Tom'sAudio.h"

const CLSID ID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const CLSID ID_IMMDevicEnumerator = __uuidof(IMMDeviceEnumerator);
const IID ID_IAudioClient = __uuidof(IAudioClient);
const IID ID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

#pragma region WaveFile
//constructor
WaveFile::WaveFile(std::string file_path, WAVEFORMATEX* format_in)
{
	wav.open(file_path, std::ios::binary);
	if (!wav)
	{
		std::cout << "ERROR: Could not open wav file!" << std::endl << std::endl;
	}

	pFormat = format_in;

	if (format_in->wFormatTag == 1)
	{
		Subchunk1Size = 16;
		AudioFormat = 1;
		ExtraSize = 0;
	}
	else if (format_in->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		WAVEFORMATEXTENSIBLE* formatEx_in = (WAVEFORMATEXTENSIBLE*)format_in;
		AudioFormat = WAVE_FORMAT_EXTENSIBLE;
		ExtraSize = 22;
		Subchunk1Size = 18 + ExtraSize;

		ValidBitsPerSample = formatEx_in->Samples.wValidBitsPerSample;
		ChannelMask = formatEx_in->dwChannelMask;
		SubFormat = formatEx_in->SubFormat;

	}

	NumChannels = format_in->nChannels;
	SampleRate = format_in->nSamplesPerSec;
	BitsPerSample = format_in->wBitsPerSample;

	FormatFile();
}

//destructor
WaveFile::~WaveFile()
{
	wav.close();
}

void WaveFile::FormatFile()
{
	//RIFF Chunk
	wav << ChunkID;
	wav << ChunkSize;
	wav << Format;
	//fmt Chunk
	wav << Subchunk1ID;
	WriteAsBytes(wav, Subchunk1Size, 4);
	WriteAsBytes(wav, AudioFormat, 2);
	WriteAsBytes(wav, NumChannels, 2);
	WriteAsBytes(wav, SampleRate, 4);
	WriteAsBytes(wav, ByteRate, 4);
	WriteAsBytes(wav, BlockAlign, 2);
	WriteAsBytes(wav, BitsPerSample, 2);
	//extra data chunk
	if (AudioFormat == WAVE_FORMAT_EXTENSIBLE)
	{
		WriteAsBytes(wav, ExtraSize, 2);
		WriteAsBytes(wav, ValidBitsPerSample, 2);
		WriteAsBytes(wav, ChannelMask, 4);
		//SubFormat
		WriteAsBytes(wav, SubFormat.Data1, 4);
		WriteAsBytes(wav, SubFormat.Data2, 2);
		WriteAsBytes(wav, SubFormat.Data3, 2);
		for (int i = 0; i < 8; i++)
			WriteAsBytes(wav, SubFormat.Data4[i], 1);
	}
	//data chunk
	wav << Subchunk2ID;
	wav << Subchunk2Size;
}

void WaveFile::LoopbackWrite(IAudioClient* AudioClient_in, REFERENCE_TIME req_buffer_time_in)
{
	pAudioClient = AudioClient_in;
	req_buffer_time = req_buffer_time_in;
	bWriteToBuffer = false;

	//Set stream to loopback mode
	hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
		req_buffer_time, 0, pFormat, NULL);
	CheckSuccess(hr, "Initialize failed!");

	//Grab AudioCaptureClient Interface
	hr = pAudioClient->GetService(ID_IAudioCaptureClient, (void**)&pCapture);
	CheckSuccess(hr, "GetService failed!");

	WriteFile();
}
void WaveFile::LoopbackWrite(IAudioClient* AudioClient_in, int bufferSize_in, REFERENCE_TIME req_buffer_time_in)
{
	pAudioClient = AudioClient_in;
	req_buffer_time = req_buffer_time_in;
	bufferSize = bufferSize_in;
	bWriteToBuffer = true;
	bBuffer = true;

	//Set stream to loopback mode
	hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
		req_buffer_time, 0, pFormat, NULL);
	CheckSuccess(hr, "Initialize failed!");

	//Grab AudioCaptureClient Interface
	hr = pAudioClient->GetService(ID_IAudioCaptureClient, (void**)&pCapture);
	CheckSuccess(hr, "GetService failed!");

	WriteFile();
}
void WaveFile::WriteFile()
{
	int start_audio;
	int end_audio;
	bool bLocalWriteToBuffer = bWriteToBuffer;
	bRecording = true;

	UINT32 packet_size;
	BYTE* pPacket = NULL;
	UINT32 frames_available;
	DWORD packet_flags;

	//Start recording
	hr = pAudioClient->Start();
	CheckSuccess(hr, "Start failed!");

	//write to buffer, otherwise write to file
	if (bLocalWriteToBuffer)
	{
		if (bBuffer)
		{
			while (bRecording)
			{
				//Sleep for half req_buffer_time (lets buffer partially fill up with audio, introduces latency)
				Sleep((req_buffer_time / MILLISECOND) / 2);

				//Get next packet size
				hr = pCapture->GetNextPacketSize(&packet_size);
				CheckSuccess(hr, "GetNextPacketSize failed!");


				//Grab packet from buffer, process packet data, return packet to buffer
				while (packet_size != 0)
				{
					hr = pCapture->GetBuffer(&pPacket, &frames_available, &packet_flags, NULL, NULL);
					CheckSuccess(hr, "GetBuffer failed!");

					for (UINT32 i = 0; i < frames_available; i++)
					{
						//buffer.push(*(reinterpret_cast<float*>(pPacket + i * 8)));
						buffer.push(*(reinterpret_cast<float*>(pPacket + i * 8 + 4)));
						//WriteAsBytes(buffer, *(reinterpret_cast<int*>(pPacket + i * 8)), 4);
						//WriteAsBytes(buffer, *(reinterpret_cast<int*>(pPacket + i * 8 + 4)), 4);
					}


					//if there is too much data in sink
					while (buffer.size() > bufferSize)
						buffer.pop();

					hr = pCapture->ReleaseBuffer(frames_available);
					CheckSuccess(hr, "ReleaseBuffer failed!");

					hr = pCapture->GetNextPacketSize(&packet_size);
					CheckSuccess(hr, "GetNextPacketSize failed!");
				}
			}
		}

	}
	else
	{
		start_audio = wav.tellp();

		while (bRecording)
		{
			//Sleep for half req_buffer_time (lets buffer partially fill up with audio, introduces latency)
			Sleep((req_buffer_time / MILLISECOND) / 2);

			//Get next packet size
			hr = pCapture->GetNextPacketSize(&packet_size);
			CheckSuccess(hr, "GetNextPacketSize failed!");


			//Grab packet from buffer, process packet data, return packet to buffer
			while (packet_size != 0)
			{
				hr = pCapture->GetBuffer(&pPacket, &frames_available, &packet_flags, NULL, NULL);
				CheckSuccess(hr, "GetBuffer failed!");

				for (UINT32 i = 0; i < frames_available; i++)
				{
					WriteAsBytes(wav, *(reinterpret_cast<int*>(pPacket + i * 8)), 4);
					WriteAsBytes(wav, *(reinterpret_cast<int*>(pPacket + i * 8 + 4)), 4);
				}

				hr = pCapture->ReleaseBuffer(frames_available);
				CheckSuccess(hr, "ReleaseBuffer failed!");

				hr = pCapture->GetNextPacketSize(&packet_size);
				CheckSuccess(hr, "GetNextPacketSize failed!");
			}
		}

		end_audio = wav.tellp();
	}

	hr = pAudioClient->Stop();
	CheckSuccess(hr, "Stop failed!");

	//finish formatting wave file
	if (!bLocalWriteToBuffer)
	{
		//rewrite subchunk 2 size
		wav.seekp(start_audio - 4);
		WriteAsBytes(wav, end_audio - start_audio, 4);
		//rewrite chunk size
		wav.seekp(4, std::ios::beg);
		WriteAsBytes(wav, end_audio - 8, 4);

		wav.close();
	}

	std::cout << std::endl << "Recording Stopped" << std::endl;
}

void WaveFile::EndWrite()
{
	bRecording = false;
}

#pragma endregion class for creating wav files

//use for error-handling
void CheckSuccess(HRESULT hr_in, std::string error_message)
{
	if (FAILED(hr_in))
	{
		std::cout << error_message << " Exiting..." << std::endl;
		exit;
	}
}

//use to write integers with a given amount of Bytes
void WriteAsBytes(std::ofstream& fout, int value, int byte_size)
{
	fout.write(reinterpret_cast<const char*>(&value), byte_size);
}

//Prompts the user to choose a connected endpoint audio device and returns a pointer to that device
IMMDevice* GetAudioRenderDevice()
{
	//variables
	HRESULT hr = S_OK;																			//initilize for error-handling
	IMMDeviceEnumerator* pDeviceEnum = NULL;													//initilize pointers for storing interfaces
	IMMDeviceCollection* pDeviceColl = NULL;
	IMMDevice* pDevice = NULL;
	IPropertyStore* pProperties = NULL;
	LPWSTR device_ID = NULL;
	UINT num_of_devices = 0;
	int device_selection = 0;

	//Audio Thread?
	hr = CoInitialize(NULL);																	//initialize COM Library, parameter must be NULL for some reason
	CheckSuccess(hr, "CoInitialize failed!");

	hr = CoCreateInstance(ID_MMDeviceEnumerator, NULL, CLSCTX_ALL, ID_IMMDevicEnumerator,		//create device enumerator
		(void**)&pDeviceEnum);
	CheckSuccess(hr, "CoCreateInstance failed!");

	hr = pDeviceEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDeviceColl);           //create device collection of all active audio render devices
	CheckSuccess(hr, "EnumAudioEndpoints failed!");

	hr = pDeviceColl->GetCount(&num_of_devices);                                                //get number of devices in collection                                                
	CheckSuccess(hr, "GetCount failed!");

	for (UINT i = 0; i < num_of_devices; i++)
	{
		hr = pDeviceColl->Item(i, &pDevice);                                                    //get interface to the i'th device in collection              
		CheckSuccess(hr, "Item failed!");

		hr = pDevice->GetId(&device_ID);                                                        //get device ID
		CheckSuccess(hr, "GetID failed!");

		hr = pDevice->OpenPropertyStore(STGM_READ, &pProperties);                               //get interface to device's properties
		CheckSuccess(hr, "OpenPropertyStore failed!");

		PROPVARIANT device_name;                                                                //get device properties and print to screen
		PROPVARIANT device_adapter_name;
		PROPVARIANT device_description;
		PropVariantInit(&device_name);
		PropVariantInit(&device_adapter_name);
		PropVariantInit(&device_description);
		hr = pProperties->GetValue(PKEY_Device_FriendlyName, &device_name);

		printf("Endpoint %d: \"%S\" (%S)\n", i, device_name.pwszVal, device_ID);
	}

	do
	{
		std::cout << "Please select a valid device: ";
		std::cin >> device_selection;
	} while (device_selection >= (int)num_of_devices || device_selection < 0);

	hr = pDeviceColl->Item(device_selection, &pDevice);                                                    //get interface to the i'th device in collection              
	CheckSuccess(hr, "Item failed!");

	return pDevice;
}

