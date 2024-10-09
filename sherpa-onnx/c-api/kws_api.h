#ifndef KWS_API_H
#define KWS_API_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#if defined(_WIN32)
#if defined(ASR_API_BUILD_SHARED_LIBS)
#define ASR_API_EXPORT __declspec(dllexport)
#define ASR_API_IMPORT __declspec(dllimport)
#else
#define ASR_API_EXPORT
#define ASR_API_IMPORT
#endif
#else  // WIN32
//define the linux so export, visibility hidden 
#define ASR_API_EXPORT __attribute__((visibility("default")))
#define ASR_API_IMPORT ASR_API_EXPORT
#endif

#if defined(ASR_API_BUILD_MAIN_LIB)
#define ASR_API_API ASR_API_EXPORT
#else
#define ASR_API_API ASR_API_IMPORT
#endif

enum KWS_Version { kFAST=0x01, kACCURATE=0x02 };

///struct 
typedef struct KWS_Parameters
{
    int32_t size;
    int version; //ASR_Version

    // 更大的模型名称
    const char* larger_model_name;

    // 更快的模型名称
    const char* faster_model_name;

    /// 用于VAD，请参阅docs/endpoint_readme.md
    /// 0为禁用，1为启用
    int32_t enable_endpoint;

    /// 如果即使没有解码任何内容，尾部静默时间（以秒为单位）大于此值，则检测到端点。
    /// 仅在enable_endpoint不为0时使用。
    float rule1_min_threshold;

    /// 如果在解码了不是空白的内容之后，尾部静默时间（以秒为单位）大于此值，则检测到端点。
    /// 仅在enable_endpoint不为0时使用。
    float rule2_min_threshold;

    /// 如果话语时间（以秒为单位）大于此值，则检测到端点。
    /// 仅在enable_endpoint不为0时使用。
    float rule3_min_threshold;

    /// 热词文件路径，每行是一个热词，如果语言是CJK之类的东西，则通过空格分隔，如果语言是英语之类的东西，则通过bpe模型分隔。
    const char *hotwords_path;

    /// 热词的比例，仅在hotwords_file不为空时使用
    float hotwords_factor;

    int num_threads; // 线程数
    char reserved[252];
} KWS_Parameters;

typedef struct KWS_Result
{
    int32_t size;
    int32_t result_size;
    int32_t result_capacity;
    char* text;
    // 识别结果的时间戳，单位为秒
    float* timestamps;
    //识别结果数目
    int32_t count;
    char reserved[248];
} KWS_Result;

///API
/* get version and build time
    * @return: version and build time
*/

ASR_API_EXPORT const char* GetSDKVersion();

/*
 * CreateStreamASRObject, create the stream ASR object
    * @param parameters: ASR_Parameters
    * @param authToken: auth token
    * @param authTokenLen: auth token length
    * @return: handle of stream ASR object if success, else return nullptr
*/
ASR_API_EXPORT void* CreateStreamKWSObject(
    const KWS_Parameters* parameters,
    const char* authToken,
    const int authTokenLen
    );

/* 
    * DestroyStreamASRObject
    * @param streamASR: void*
    * @return: void
*/
ASR_API_EXPORT void DestroyStreamKWSObject(void* streamASR);

/*
 * Streaming StreamGetResult, if isEndPoint is true, the results will be stored in result, after get the results, 
 * you should call DestroyASRResult to free the memory
    * @param streamASR: void*  
    * @param isFinalStream: whether is the final streams, if true, the recoginze will be end and reset
    * @param result: ASR_Result
    * @param isEndPoint: is end point
    * If Success, return 0, else return other error code   
*/
ASR_API_EXPORT int StreamGetResult(
    void* streamASR, 
    int isFinalStream,
    KWS_Result* result, 
    int* isEndPoint
    );


/*
 *Accept the streaming wav data 
    * @param streamASR: void*
    * @param audioData: audio data
    * @param audioDataLen: audio data length
    * @param sampleRate: sample rate
    * If Success, return 0, else return other error code
*/
ASR_API_EXPORT int StreamRecognizeWav(
    void* streamASR, 
    const int16_t* audioData, 
    int audioDataLen, 
    float sampleRate
    );
/*
    * release the memory of ASR_Result
    * @param result: ASR_Result
    * If Success, return 0, else return other error code   
*/
ASR_API_EXPORT int DestroyKWSResult(KWS_Result* result);

/* reset the stream recognize
    * @param streamASR: void*
    * @return: If Success, return 0, else return other error code   
*/
ASR_API_EXPORT int ResetStreamKWS(void* streamASR);


ASR_API_EXPORT const char* get_kws_last_error_message();

/* get the sn of the device
    * @param sn: sn buffer, allocated by caller
    * @param sn_len: sn buffer length, return the actual length of sn
    * @return: If Success, return 0, else return other error code
*/
ASR_API_EXPORT int get_kws_device_sn(unsigned char sn[], int* sn_len);

/* Verify the license(auth token)
    * @param authToken: auth token
    * @param authTokenLen: auth token length
    * @return: If Success, return 0, else return other error code
*/
ASR_API_EXPORT int VerifyLicenseKWS(const char* authToken, const int authTokenLen);

#ifdef __cplusplus
}
#endif 

#ifdef __cplusplus
class KWSClass {
public:
    KWSClass() {
        streamASRObject = nullptr;
    }

    int Init(const KWS_Parameters* parameters, const char* authToken, const int authTokenLen) {
        

        streamASRObject = CreateStreamKWSObject(parameters, authToken, authTokenLen);
        if ( streamASRObject == nullptr ) {
            return -1;
        }
    }

    ~KWSClass() {
        if ( streamASRObject != nullptr ) {
            DestroyStreamKWSObject(streamASRObject);
            streamASRObject = nullptr;
        }
    }

    int StreamRecognizeGetResult(        
        int isFinalStream,
        KWS_Result* result, 
        int* isEndPoint
        ) {
        return ::StreamGetResult(streamASRObject,  isFinalStream, result, isEndPoint);
    }

    int StreamRecognizeWav(
        const int16_t* audioData, 
        int audioDataLen, 
        float sampleRate
        ) {
        return ::StreamRecognizeWav(streamASRObject, audioData, audioDataLen, sampleRate);
    }
    
    int ResetStreamKWS_() {
        return ::ResetStreamKWS(streamASRObject);
    }

protected:
    void* asrObject;
    void* streamASRObject;
};
#endif

#endif // ASR_API_H