#include"Tom'sAudio.h"
#include<fstream>

int main()
{
#pragma region Audio
    //Variables
    int bufferSize = 25;
    bool wait_for_input = true;

    HRESULT hr = S_OK;
    IMMDevice* pSpeakers = NULL;
    IAudioClient* pAudioClient = NULL;
    IAudioCaptureClient* pCapture = NULL;
    WAVEFORMATEX* pFormat = NULL;

    //Prompt user to select AudioRenderDevice
    pSpeakers = GetAudioRenderDevice();

    //Get Device Format
    //Activate selected device, grab AudioClient interface
    hr = pSpeakers->Activate(ID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
    CheckSuccess(hr, "Activate failed!");

    //Get audio format of device
    hr = pAudioClient->GetMixFormat(&pFormat);
    CheckSuccess(hr, "GetMixFormat failed!");

    //initialize wavefile
    WaveFile WaveOut("NewWavFile.wav", pFormat);

    std::cout << "Press SPACE to start recording..." << std::endl;
    while (wait_for_input) if (GetAsyncKeyState(VK_SPACE)) wait_for_input = false;
    wait_for_input = true;

    std::thread RecordToBufferThread([&](auto device, auto buffer, auto time)
        { WaveOut.LoopbackWrite(device, time); }, pAudioClient, bufferSize, SECOND / 4);
    RecordToBufferThread.detach();
    std::cout << "Recording started. Press TAB to stop recording..." << std::endl << std::endl;

    while (wait_for_input)
    {
        if (GetAsyncKeyState(VK_TAB))
        {
            WaveOut.EndWrite();
            wait_for_input = false;
        }
    }

#pragma endregion

    return 0;
}
