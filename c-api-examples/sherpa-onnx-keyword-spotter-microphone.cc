// kws with microphone

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <list>

#include "portaudio.h"  // NOLINT
#include "c-api/asr_api.h"

bool stop = false;
float mic_sample_rate = 16000;

static int32_t RecordCallback(const void *input_buffer,
                              void * /*output_buffer*/,
                              unsigned long frames_per_buffer,  // NOLINT
                              const PaStreamCallbackTimeInfo * /*time_info*/,
                              PaStreamCallbackFlags /*status_flags*/,
                              void *user_data) {
  void* streamObj = user_data;
  float* audioData_f = static_cast<float*>(input_buffer);
  std::vector<int16_t> audioData[(int)frames_per_buffer];
  for (int i = 0; i < frames_per_buffer; i++) {
    audioData[i] = static_cast<int16_t>(audioData_f[i] * 32768);
  }

  int audioDataLen = frames_per_buffer;
  float sampleRate = mic_sample_rate;
  int ret = StreamRecognizeWav(streamObj, &audioData[0], audioDataLen, sampleRate);

  return stop ? paComplete : paContinue;
}


static void Handler(int32_t sig) {
  stop = true;
  fprintf(stderr, "\nCaught Ctrl + C. Exiting...\n");
}

int32_t main(int32_t argc, char *argv[]) {
  signal(SIGINT, Handler);

  const char *kUsageMessage = R"usage(
This program uses streaming models with microphone for keyword spotting.
Usage:

  ./bin/kws-microphone \
    model_path \
    keyword_file 
  )usage";

  if (argc != 3) {
    fprintf(stderr, "%s\n", kUsageMessage);
    exit(EXIT_FAILURE);
  }

  ASR_Parameters asr_params;
  asr_params.version = FAST;
  asr_params.model_path = argv[1];
  asr_params.hotwords_path = argv[2];

  void* streamASR = CreateStreamASRObject(&asr_params, nullptr, 0);


  PaDeviceIndex num_devices = Pa_GetDeviceCount();
  fprintf(stderr, "Num devices: %d\n", num_devices);

  int32_t device_index = Pa_GetDefaultInputDevice();

  if (device_index == paNoDevice) {
    fprintf(stderr, "No default input device found\n");
    fprintf(stderr, "If you are using Linux, please switch to \n");
    fprintf(stderr, " alsa \n");
    exit(EXIT_FAILURE);
  }

  const char *pDeviceIndex = std::getenv("KWS_MIC_DEVICE");
  if (pDeviceIndex) {
    fprintf(stderr, "Use specified device: %s\n", pDeviceIndex);
    device_index = atoi(pDeviceIndex);
  }

  for (int32_t i = 0; i != num_devices; ++i) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
    fprintf(stderr, " %s %d %s\n", (i == device_index) ? "*" : " ", i,
            info->name);
  }

  PaStreamParameters param;
  param.device = device_index;

  fprintf(stderr, "Use device: %d\n", param.device);

  const PaDeviceInfo *info = Pa_GetDeviceInfo(param.device);
  fprintf(stderr, "  Name: %s\n", info->name);
  fprintf(stderr, "  Max input channels: %d\n", info->maxInputChannels);

  param.channelCount = 1;
  param.sampleFormat = paFloat32;

  param.suggestedLatency = info->defaultLowInputLatency;
  param.hostApiSpecificStreamInfo = nullptr;

  const char *pSampleRateStr = std::getenv("KWS_MIC_SAMPLE_RATE");
  if (pSampleRateStr) {
    fprintf(stderr, "Use sample rate %f for mic\n", mic_sample_rate);
    mic_sample_rate = atof(pSampleRateStr);
  }

  float sample_rate = 16000;

  PaStream *stream;
  PaError err =
      Pa_OpenStream(&stream, &param, nullptr, /* &outputParameters, */
                    mic_sample_rate,
                    0,          // frames per buffer
                    paClipOff,  // we won't output out of range samples
                                // so don't bother clipping them
                    RecordCallback, streamASR);
  if (err != paNoError) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  err = Pa_StartStream(stream);
  fprintf(stderr, "Started\n");

  if (err != paNoError) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  int32_t keyword_index = 0;
  sherpa_onnx::Display display;
  ASR_Result result;
  memset(&result, 0, sizeof(result));

  while (!stop) {
    //get the result
    int ret = GetASRResult(streamASR, &result);
    if (ret != 0) {
      fprintf(stderr, "GetASRResult failed\n");
      break;
    }
    //print the result
    if (result.text != nullptr) {
      fprintf(stderr, "Result: %s\n", result.text);
    }
    DestroyASRResult(&result);

    Pa_Sleep(20);  // sleep for 20ms
  }

  err = Pa_CloseStream(stream);
  if (err != paNoError) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  return 0;
}
