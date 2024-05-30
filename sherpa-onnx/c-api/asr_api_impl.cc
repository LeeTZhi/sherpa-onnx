
#include "kws_api.h"
#include "sherpa-onnx/csrc/keyword-spotter.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cassert>
#include <stdlib.h>
#include <vector>


#ifndef ASR_API_VERSION
    #define ASR_API_VERSION "0.2.1"
#endif
///model weights header files

//typedef struct SherpaNcnnRecognizer SherpaNcnnRecognizer;
//typedef struct SherpaNcnnStream SherpaNcnnStream;
//typedef struct SherpaNcnnRecognizerConfig SherpaNcnnRecognizerConfig;

#define NULL_PTR_2_STR(t) (t != nullptr) ? t : ""



/// @brief get device id
/// @param device_id
/// @param device_id_len
/// @return 0 if success, -1 or other value otherwise
namespace asr_api {
    int get_device_id(uint8_t device_id[], int* device_id_len);
    /// @brief verify whether the signature is valid
    /// @param pSignature 
    /// @param sig_len 
    /// @return 0 if the signature is valid, -1 or other value otherwise
    int verify_authtoken(const char* pSignature, const int sig_len);
}


char g_str_error[1024] = {0};

class ASRRecognizer_Impl {
    public:
        ASRRecognizer_Impl() {
            recognizer_ = nullptr;
            stream_ = nullptr;
            encoder_param_buffer_ = nullptr;
            encoder_param_size_ = 0;
            decoder_param_buffer_ = nullptr;
            decoder_param_size_ = 0;
            joiner_param_buffer_ = nullptr;
            joiner_param_size_ = 0;
            tokens_buffer_ = nullptr;

            usage_count_ = 0;
        }
        ~ASRRecognizer_Impl() {
            if (recognizer_ != nullptr) {
                delete recognizer_;
                recognizer_ = nullptr;
            }
            
            if (encoder_param_buffer_ != nullptr) {
                free(encoder_param_buffer_);
                encoder_param_buffer_ = nullptr;
            }
            
            if (decoder_param_buffer_ != nullptr) {
                free(decoder_param_buffer_);
                decoder_param_buffer_ = nullptr;
            }
            
            if (joiner_param_buffer_ != nullptr) {
                free(joiner_param_buffer_);
                joiner_param_buffer_ = nullptr;
            }
            
            if (tokens_buffer_ != nullptr) {
                free(tokens_buffer_);
                tokens_buffer_ = nullptr;
            }
        }

        /// @brief initialize the recognizer and stream
        /// @return Success or error code
        int Init(const KWS_Parameters& asr_config );

        int AcceptWaveform(
            float sampleRate, 
            const int16_t* audioData, 
            int audioDataLen
            ) {
            if (stream_ == nullptr) {
                return -1;
            }
             ///convert the audio data to float
            std::vector<float> audio_data_float(audioDataLen);
            for (int i = 0; i < audioDataLen; i++) {
                audio_data_float[i] = audioData[i]/32768.0f;
            }
            /// accept wave data
            stream_->AcceptWaveform( sampleRate, &audio_data_float[0], audioDataLen);
            
            return 0;
        }

        ///stream recognize
        ///@param audioData: audio data
        ///@param audioDataLen: audio data length
        ///@param isFinalStream: whether is the final streams, if true, the recoginze will be end and reset
        ///@param result: ASR_Result
        ///@param isEndPoint: is end point
        ///If Success, return 0, else return other error code
        int StreamRecognize(
            int isFinalStream,
            KWS_Result* result, 
            int* isEndPoint
            );
        int Reset() {
            return 0;
        }

        void set_auth_token(const char* auth_token, int auth_token_len) {
            if (auth_token != nullptr && auth_token_len > 0) {
                memset(str_auth_token_, 0, sizeof(str_auth_token_));
                memcpy(str_auth_token_, auth_token, std::min(auth_token_len, (int)sizeof(str_auth_token_)));
            }
        }
        
    protected:
        sherpa_onnx::KeywordSpotter* recognizer_;
        std::unique_ptr<sherpa_onnx::OnlineStream> stream_;

        sherpa_onnx::KeywordSpotterConfig config_;

        ///six model weights
        uint8_t* encoder_param_buffer_;
        size_t encoder_param_size_;
        uint8_t* decoder_param_buffer_;
        size_t decoder_param_size_;
        uint8_t* joiner_param_buffer_;
        size_t joiner_param_size_;
        uint8_t* tokens_buffer_;

        ///for control the total frames number
        uint64_t usage_count_;
        const uint64_t max_usage_count_ = 10000*600; // 5 times per second, 600 per 2 minutes

        char str_auth_token_[2048];

    protected: //utils
        bool check_recog_frames_number() {

            bool bcount =  usage_count_++ < max_usage_count_;
        #if __aarch64__
        #if 0 //no license check
            //check date, must before 2024.1.1
            time_t now = time(0);
            tm *ltm = localtime(&now);
            bool bdate = ltm->tm_year < 124;
            return bcount && bdate;
        #else 
            (void)bcount;
            if (usage_count_ % 100 == 0) {
                int ret = asr_api::verify_authtoken(str_auth_token_, strlen(str_auth_token_));
                if (ret != 0) {
                    sprintf(g_str_error, "verify auth token failed, error code: %d", ret);
                    return false;
                }
            }
            return true;
        #endif
        #else 
            return bcount;
        #endif
        }

};

static bool load_from_merged_file(
    const std::string& merged_file_name, 
    uint8_t** encoder_param_buffer, 
    size_t& encoder_param_size,
    uint8_t** decoder_param_buffer,
    size_t& decoder_param_size,
    uint8_t** joiner_param_buffer,
    size_t& joiner_param_size,
    uint8_t** tokens_buffer,
    size_t& tokens_buffer_size
    );

static void set_default_sherpa_ncnn_config(sherpa_onnx::KeywordSpotterConfig& config) {
    //Feature config
    config.feat_config.sampling_rate = 16000.0f; //16kHz
    config.feat_config.feature_dim = 80;

    ///model config
    memset(&config.model_config, 0, sizeof(config.model_config));
    config.model_config.transducer.buffer_flag_ = 1;
    config.model_config.num_threads = 2;
    config.model_config.provider = "cpu";
    config.model_config.model_type = "zipformer2";
    //decoder config
    //config.model_config.num_active_paths = 2;
    
    /* 
     for these parameters 
     1. endpoint and vad 
     2. hotwords
    */

    //config.decoder_config.num_active_paths = 1;

    //config.keywords_file = nullptr;
    config.keywords_score = 1.5f;
}

int ASRRecognizer_Impl::Init(const KWS_Parameters& asr_config ) {
    // model_config, config_ and recognizer_ are defined in recognizer.h
    set_default_sherpa_ncnn_config(config_);
    std::string model_name;
    size_t tokens_buffer_size = 0;
    ///load the model weights
    if (asr_config.version == FAST ) {
        model_name = NULL_PTR_2_STR(asr_config.faster_model_name);
    }
    else {
        model_name = NULL_PTR_2_STR(asr_config.larger_model_name);
    }
    /// if model name is empty, return error
    if (model_name.empty()) {
        return -1;
    }
    ///load the model weights
    bool bLoad = load_from_merged_file(
            model_name, 
            &encoder_param_buffer_, 
            encoder_param_size_,
            &decoder_param_buffer_,
            decoder_param_size_,
            &joiner_param_buffer_,
            joiner_param_size_,
            &tokens_buffer_,
            tokens_buffer_size
            );
    if (!bLoad) {
        return -1;
    }

    config_.model_config.transducer.encoder_buffer_ = reinterpret_cast<char*>(encoder_param_buffer_);
    config_.model_config.transducer.encoder_buffer_size_ = encoder_param_size_;
    config_.model_config.transducer.decoder_buffer_ = reinterpret_cast<char*>(decoder_param_buffer_);
    config_.model_config.transducer.decoder_buffer_size_ = decoder_param_size_;
    config_.model_config.transducer.joiner_buffer_ = reinterpret_cast<char*>(joiner_param_buffer_);
    config_.model_config.transducer.joiner_buffer_size_ = joiner_param_size_;

    config_.model_config.tokens_buffer = reinterpret_cast<unsigned char*>(tokens_buffer_);
    config_.model_config.tokens_buffer_size = tokens_buffer_size;
    
    
    ///hotwords
    config_.keywords_file = asr_config.hotwords_path?asr_config.hotwords_path:nullptr;
    config_.keywords_score = asr_config.hotwords_factor;
    
    recognizer_ = new sherpa_onnx::KeywordSpotter( config_ );

    stream_ = recognizer_->CreateStream();
    return 0;
}

static size_t calculateLengthWithKnownNulls(const char* str, int knownNulls) {
    size_t length = 0;
    int nullCount = 0;

    while (nullCount <= knownNulls) {
        if (*str == '\0') {
            nullCount++;
            if (nullCount > knownNulls) {
                break;
            }
        }
        length++;
        str++;
    }

    return length;
}

int ASRRecognizer_Impl::StreamRecognize(
    int isFinalStream,
    KWS_Result* result, 
    int* isEndPoint
    ) {
    
    result->count = 0;
    result->text = nullptr;
    result->timestamps = nullptr;
    *isEndPoint = 0;
    
    if (result == nullptr) {
        return -1;
    }
    if (isEndPoint == nullptr) {
        return -1;
    }

    ///check the frames number
    if (!check_recog_frames_number()) {
        return -2;
    }
    
    ///if ready decode 
    //int ret = IsReady( recognizer_, stream_);
    while ( recognizer_->IsReady( stream_.get()) ) {
        ///decode
        recognizer_->DecodeStream(stream_.get());
    } 
    
    auto results = recognizer_->GetResult(stream_.get());
    if (results.tokens.size() > 0) {
        ///copy the result to outputs
        result->count = 1;
        std::string json = results.AsJsonString();
        result->text = (char*)malloc(json.size());
        memcpy(result->text, json.c_str(), json.size());
        result->timestamps = nullptr;
    }

    return 0;
}

extern "C" {

ASR_API_EXPORT void* CreateStreamKWSObject(
    const KWS_Parameters* asr_config, 
    const char* authToken,
    const int authTokenLen
    ) {    
    ASRRecognizer_Impl* asr_recognizer = new ASRRecognizer_Impl();

#if __aarch64__
    int ret = asr_api::verify_authtoken(authToken, authTokenLen);
    if (ret != 0) {
        delete asr_recognizer;
        sprintf(g_str_error, "verify auth token failed, error code: %d", ret);
        return nullptr;
    }
    asr_recognizer->set_auth_token(authToken, authTokenLen);
#endif

    if (asr_recognizer->Init(*asr_config) != 0) {
        delete asr_recognizer;
        sprintf(g_str_error, "init asr recognizer failed");
        return nullptr;
    }
    return (void*)asr_recognizer;
}

ASR_API_EXPORT  void DestroyStreamKWSObject(void* asr_object) {
    if (asr_object == nullptr) {
        return;
    }
    ASRRecognizer_Impl* asr_recognizer = (ASRRecognizer_Impl*)asr_object;
    delete asr_recognizer;
}

ASR_API_EXPORT  int ResetStreamKWS(void* asr_object) {
    if (asr_object == nullptr) {
        return -1;
    }
    ASRRecognizer_Impl* asr_recognizer = (ASRRecognizer_Impl*)asr_object;
    return asr_recognizer->Reset();
}

ASR_API_EXPORT  int StreamGetResult(
    void* streamASR, 
    int isFinalStream,
    KWS_Result* result, 
    int* isEndPoint) {
    
    if (streamASR == nullptr) {
        return -1;
    }

    ASRRecognizer_Impl* asr_recognizer = (ASRRecognizer_Impl*)streamASR;
    return asr_recognizer->StreamRecognize(isFinalStream, result, isEndPoint);
}

ASR_API_API int StreamRecognizeWav(
    void* streamASR, 
    const int16_t* audioData, 
    int audioDataLen, 
    float sampleRate
    ) {
    if (streamASR == nullptr) {
        return -1;
    }
    ASRRecognizer_Impl* asr_recognizer = (ASRRecognizer_Impl*)streamASR;
    return asr_recognizer->AcceptWaveform(sampleRate, audioData, audioDataLen);
}

ASR_API_EXPORT int DestroyKWSResult(KWS_Result* result){
    ///TODO
    if ( result == nullptr) {
        return 0;
    }
    if(result->text != nullptr)
        free(result->text);
    result->text = nullptr;
    if (result->timestamps != nullptr)
        free(result->timestamps);
    result->timestamps = nullptr;
    result->count = 0;
    return 0;
}

ASR_API_EXPORT const char* get_kws_last_error_message() {
    return g_str_error;
}

ASR_API_EXPORT const char* GetKWSSDKVersion() {
    return ASR_API_VERSION;
}

ASR_API_EXPORT int get_kws_device_sn(uint8_t sn[], int* sn_len) {
    if (sn == nullptr || sn_len == nullptr || *sn_len < 32) {
        //last error message
        sprintf(g_str_error, "sn or sn_len is nullptr");
        return -1;
    }
    int ret = asr_api::get_device_id(sn, sn_len);
    if (ret != 0) {
        sprintf(g_str_error, "get device sn failed, error code: %d", ret);
    }
    return ret;
}

} //extern "C"

//1 byte alignment
#pragma pack(1)
typedef struct file_header {
  char file_id[2];
  uint32_t file_size;
} file_header;
//end 1 byte alignment
#pragma pack()

static void process_buffer_with_magic_number(uint8_t* buffer, uint32_t buffer_size, uint32_t magic_number) {
    for (int i = 0; i < buffer_size; ++i) {
        buffer[i] ^= (magic_number >> ((i % 4) * 8)) & 0xFF;
    }
}

#define SURE_NEW(p) {if (p == nullptr) {std::cout << "Memory allocation failed at: " << __LINE__ <<std::endl; return false;}}

#define SURE_READ(is, cnt) do { \
    if (is) \
      std::cout << "all characters read successfully."<<std::endl; \
    else    \
      {     \
        std::cout << "read "<< (cnt) <<" but error: only " << is.gcount() << " could be read"<<std::endl; \
        return false; \
        } \
} while(0)
#define ALIGN_SIZE(size, alignment) (((size) + (alignment) - 1) / (alignment) * (alignment))

static uint8_t* _aligned_alloc(size_t alignment, size_t size) {
    void* ptr = nullptr;
    size = ALIGN_SIZE(size, alignment);
    int ret = posix_memalign(&ptr, alignment, size);
    if (ret != 0) {
        std::cout << "Failed to allocate memory with alignment: " << alignment << ", size: " << size << std::endl;
        return nullptr;
    }
    return static_cast<uint8_t*>(ptr);
}

/*
Load six buffers from the merged file, memory allocate in this function
@param merged_file_name: merged file name
@param encoder_param_buffer: encoder param buffer
@param encoder_bin_buffer: encoder bin buffer
@param decoder_param_buffer: decoder param buffer
@param decoder_bin_buffer: decoder bin buffer
@param joint_param_buffer: joint param buffer
@param joint_bin_buffer: joint bin buffer
if load successfully, return true, otherwise return false
*/
bool load_from_merged_file(
    const std::string& merged_file_name, 
    uint8_t** encoder_param_buffer, 
    size_t& encoder_param_size,
    uint8_t** decoder_param_buffer,
    size_t &decoder_param_size,
    uint8_t** joiner_param_buffer,
    size_t &joiner_param_size,
    uint8_t** tokens_buffer,
    size_t& tokens_buffer_size
    ) {
    
    ///set all point to nullptr
    *encoder_param_buffer = nullptr;
    encoder_param_size = 0;
    *decoder_param_buffer = nullptr;
    decoder_param_size = 0;
    *joiner_param_buffer = nullptr;
    joiner_param_size = 0;
    *tokens_buffer = nullptr;

    size_t aligned_size = 8;

    std::ifstream merged_file(merged_file_name.c_str(), std::ios::binary);
    if (!merged_file) {
        std::cout << "Failed to open file: " << merged_file_name << std::endl;
        return false;
    }
    uint32_t magic_number = 0;
    merged_file.read((char*)&magic_number, sizeof(magic_number));
    SURE_READ(merged_file, sizeof(magic_number));
    
    //read EP file header and data
    file_header fh;
    merged_file.read((char*)&fh, sizeof(fh));
    SURE_READ(merged_file, sizeof(fh));
    assert(memcmp(fh.file_id, "EP", 2) == 0);
    *encoder_param_buffer = _aligned_alloc(aligned_size, fh.file_size);
    SURE_NEW(*encoder_param_buffer);

    //*encoder_param_buffer = static_cast<uint8_t*>(aligned_alloc(4, fh.file_size));
    //SURE_NEW(*encoder_param_buffer);
    merged_file.read((char*)*encoder_param_buffer, fh.file_size);
    SURE_READ(merged_file, fh.file_size);
    encoder_param_size = fh.file_size;
    ///magic number xor
    process_buffer_with_magic_number(*encoder_param_buffer, fh.file_size, magic_number);

    //read DP file header and data
    merged_file.read((char*)&fh, sizeof(fh));
    SURE_READ(merged_file, sizeof(fh));
    assert(memcmp(fh.file_id, "DP", 2) == 0);
    *decoder_param_buffer = _aligned_alloc(aligned_size, fh.file_size);;
    SURE_NEW(*decoder_param_buffer);
    merged_file.read((char*)*decoder_param_buffer, fh.file_size);
    SURE_READ(merged_file, fh.file_size);
    decoder_param_size = fh.file_size;
    ///magic number xor
    process_buffer_with_magic_number(*decoder_param_buffer, fh.file_size, magic_number);

    //read JP file header and data
    merged_file.read((char*)&fh, sizeof(fh));
    SURE_READ(merged_file, sizeof(fh));
    assert(memcmp(fh.file_id, "JP", 2) == 0);
    *joiner_param_buffer = _aligned_alloc(aligned_size, fh.file_size);
    SURE_NEW(*joiner_param_buffer);
    merged_file.read((char*)*joiner_param_buffer, fh.file_size);
    SURE_READ(merged_file, fh.file_size);
    joiner_param_size = fh.file_size;
    ///magic number xor
    process_buffer_with_magic_number(*joiner_param_buffer, fh.file_size, magic_number);

    //read tokens file header and data
    merged_file.read((char*)&fh, sizeof(fh));
    SURE_READ(merged_file, sizeof(fh));
    assert(memcmp(fh.file_id, "TK", 2) == 0);
    *tokens_buffer = _aligned_alloc(aligned_size, fh.file_size);
    SURE_NEW(*tokens_buffer);
    merged_file.read((char*)*tokens_buffer, fh.file_size);
    SURE_READ(merged_file, fh.file_size);
    ///magic number xor
    process_buffer_with_magic_number(*tokens_buffer, fh.file_size, magic_number);
    tokens_buffer_size = fh.file_size;

    merged_file.close();
    return true;
}