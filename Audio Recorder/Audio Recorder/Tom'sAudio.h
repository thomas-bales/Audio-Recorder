#pragma once

#include<iostream>
#include<fstream>
#include<cmath>
#include<queue>
#include<thread>
#include<complex>
#include<stdlib.h>
#include<mmdeviceapi.h>
#include<Audioclient.h>
#include<audiopolicy.h>
#include<objbase.h>
#include<winerror.h>
#include<functiondiscoverykeys.h>
#include"Tom'sAudio.h"

//Constants
#define SECOND 10000000
#define MILLISECOND 1000000

extern const CLSID ID_MMDeviceEnumerator;
extern const CLSID ID_IMMDevicEnumerator;
extern const IID ID_IAudioClient;
extern const IID ID_IAudioCaptureClient;

class WaveFile
{
private:
	HRESULT hr = S_OK;
	std::ofstream wav;
	int bufferSize;

	//audioDevice Interface variables
	IAudioClient* pAudioClient = NULL;
	WAVEFORMATEX* pFormat = NULL;
	IAudioCaptureClient* pCapture = NULL;
	REFERENCE_TIME req_buffer_time;

	//flags
	bool bBuffer = false;
	bool bWriteToBuffer = false;
	bool bRecording;
	//helper function for constructor
	void FormatFile();
	//helper function for audio capture functions
	void WriteFile();
public:
	std::queue<float> buffer;

	//RIFF Chunk
	const std::string ChunkID = "RIFF";
	std::string ChunkSize = "----";
	const std::string Format = "WAVE";

	//fmt chunk
	const std::string Subchunk1ID = "fmt ";
	int Subchunk1Size;
	int AudioFormat;
	int NumChannels;
	int SampleRate;
	int BitsPerSample; //located after ByteRate and BlockAlign in actual file
	int ByteRate = SampleRate * NumChannels * (BitsPerSample / 8);
	int BlockAlign = NumChannels * (BitsPerSample / 8);
	//extra data for WAVEEXTENSIBLE
	int ExtraSize = 0;
	int ValidBitsPerSample;
	int ChannelMask;
	GUID SubFormat;

	//data chunk
	const std::string Subchunk2ID = "data";
	std::string Subchunk2Size = "----";

	//constructor: initilizes object and formats input file
	WaveFile(std::string file_path, WAVEFORMATEX* format_in);
	//destructor: use to release pointer variables
	//May need to use SAFE_RELEASE to deallocate memory
	~WaveFile();

	//takes in an audio render device in loopback mode and writes the output
	//to another file by default, or to a specified buffer (overloaded function);
	//the recording is started on a new detached thread
	void LoopbackWrite(IAudioClient* pAudioClient_in, REFERENCE_TIME req_buffer_time_in);
	void LoopbackWrite(IAudioClient* pAudioClient_in, int bufferSize_in, REFERENCE_TIME req_buffer_time_in);
	void EndWrite();
};

void CheckSuccess(HRESULT hr_in, std::string error_message);

void WriteAsBytes(std::ofstream& fout, int value, int byte_size);

IMMDevice* GetAudioRenderDevice();

