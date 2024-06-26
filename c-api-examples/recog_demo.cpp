/*
 2 threads, one for audio input, one for recognition
 the audio input is simulated from a file
*/

#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <chrono>

#include "kws_api.h"

std::mutex queueMutex;
std::condition_variable cv;
std::queue<std::vector<int16_t>> audioQueue;  // Queue to store audio chunks
bool recordingDone = false;

void audioCaptureThread(const std::string& filePath) {
    FILE *fp = fopen(filePath.c_str(), "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", filePath.c_str());
        return ;
    }
    fseek(fp, 44, SEEK_SET);

    const size_t N = static_cast<size_t>(16000*0.2); //200ms
    int16_t buffer[N];

    while (true) {
        size_t n = fread((void *)buffer, sizeof(int16_t), N, fp);
        if (n > 0) {
            std::vector<int16_t> audioChunk(buffer, buffer + n);
            std::lock_guard<std::mutex> lock(queueMutex);
            audioQueue.push(audioChunk);
            cv.notify_one();
        }
        else { // or reset the header and continue
            std::cerr << "End of file reached" << std::endl;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                //recordingDone = true;
                //set to the beginning of the file
                fseek(fp, 44, SEEK_SET);
            }
            cv.notify_one(); // Notify after setting the flag
            //break;
        }
        ///sleep for 200ms
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    fclose(fp);
}

// 函数定义
int findSubstringIndex(const char* str, const char* target) {
    const char* start = str;
    const char* end;
    int index = 0;

    while (*start) {
        end = start;
        // 找到下一个 '\0' 字符
        while (*end && *end != '\0') {
            end++;
        }

        if (strstr(start, target) != NULL) {
            return index;
        }

        // 移动到下一个子字符串的开头
        start = end + 1;
        index++;
    }

    return -1;
}

void audioRecognitionThread(void* recognizer) {
    int32_t segment_id = -1;

    int32_t isFinal = 0;
    int32_t isEnd = 0;
    float sampleRate = 16000.0f;
    KWS_Result results;
    memset(&results, 0, sizeof(results));

    //high resolution to get the elpased time
    //acumulate the time for each recognition
    
    std::chrono::duration<double> time_total = std::chrono::duration<double>::zero();
    int64_t frame_count = 0;
    while (!recordingDone ) { 
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, []{ return !audioQueue.empty() || recordingDone; });
        if (audioQueue.empty() && recordingDone) {
            break; // Exit the loop if queue is empty and recording is done
        }

        std::vector<int16_t> audioChunk;
        if(!audioQueue.empty())
        {
            audioChunk = audioQueue.front();
            audioQueue.pop();
            lock.unlock();
            auto start = std::chrono::high_resolution_clock::now();
            // do recognition, in the stream mode, two threads are needed, one is for audio input, one is for getting the result
            int ret = StreamRecognizeWav(recognizer, audioChunk.data(), audioChunk.size(), sampleRate);
            if (ret != 0) {
                fprintf(stderr, "Failed to recognize audio\n");
                break;
            }
            ret = StreamGetResult(recognizer,  isFinal, &results, &isEnd);
            auto end = std::chrono::high_resolution_clock::now();
            time_total += std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
            frame_count++;
            if (ret != 0) {
                fprintf(stderr, "Failed to get result\n");
                break;
            }
            else {
                if (results.text != NULL) {
                    printf("Partial result: %s\n", results.text);
                }
            }
            DestroyKWSResult(&results);

            if (frame_count % 100 == 0) {
                printf("Average time per frame: %f ms\n", time_total.count() * 1000 / frame_count);
            }
        }
    }

    ResetStreamKWS(recognizer);
}


const char* auth_token = "3046022100fee5357fb8af69edca694256f3f46fae3382039a62ddb7750c3475bb83d79fb4022100e9673a0805b90d7716282c2b1622e71ee7ddceb42a8afc061e41472af911186f";

int main(int32_t argc, char *argv[]) {
    if (argc < 3 ) {
        fprintf(stderr, "usage: %s\n model.bin wav_path\n", argv[0]);
        return -1;
    }

    ///Test for device_id
    ///only at x86 platform
#if defined(__i386__) || defined(__x86_64__)
    uint8_t device_id[64];
    int device_id_len = 64;
    int ret = get_kws_device_sn(device_id, &device_id_len);
    assert(ret == 0 );
    assert(device_id_len == 32 );
    printf("device_id: ");
    for ( int ii = 0; ii < device_id_len; ii++)
    {
        printf("%02x", device_id[ii]);
    }
    printf("\n");
#endif 

    KWS_Parameters config;
    memset(&config, 0, sizeof(config));

    config.version = kFAST;
    config.faster_model_name = argv[1];
    
    char* keywords_file = NULL; //所有热词
    if ( argc >= 4 ) {
        keywords_file = argv[3];
    }
    
    config.hotwords_path = keywords_file;
    config.hotwords_factor = 2.0f;

    
    void *recognizer = CreateStreamKWSObject(&config, auth_token, strlen(auth_token));
    if ( recognizer == NULL ) {
        fprintf(stderr, "Failed to create recognizer\n");
        return -1;
    }

    const char *wav_filename = argv[2];
  
    ///two threads
    std::thread captureThread(audioCaptureThread, wav_filename);
    std::thread recogThread(audioRecognitionThread, recognizer);

    captureThread.join();
    recogThread.join();

    DestroyStreamKWSObject(recognizer);
    return 0;
}
