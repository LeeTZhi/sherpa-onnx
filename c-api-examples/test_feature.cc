#include "sherpa-onnx/csrc/keyword-spotter.h"

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
    );

class keyword_spotter_impl {
public:
  keyword_spotter_impl() {
    // do something
    //set 2 null pointers and 4 size_t to 0
    encoder_param_buffer_ = nullptr;
    encoder_param_size_ = 0;
    decoder_param_buffer_ = nullptr;
    decoder_param_size_ = 0;
    joiner_param_buffer_ = nullptr;
    joiner_param_size_ = 0;
    tokens_buffer_ = nullptr;

  }

    ~keyword_spotter_impl() {
        // do something
    }

    int Init(const char* model_path, const char* hotwords_path );

    std::string ToString() const {
        // do something
        return config_.ToString();
    }
protected:
    sherpa_onnx::KeywordSpotterConfig config_;
    std::unique_ptr<sherpa_onnx::KeywordSpotter> recognizer_;
    ///six model weights
    uint8_t* encoder_param_buffer_;
    size_t encoder_param_size_;
    uint8_t* decoder_param_buffer_;
    size_t decoder_param_size_;
    uint8_t* joiner_param_buffer_;
    size_t joiner_param_size_;
    uint8_t* tokens_buffer_;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " model_path" << std::endl;
        return -1;
    }
  keyword_spotter_impl kws;
  kws.Init(argv[1], argv[2]);
  std::string config_string = kws.ToString();
  std::cout << config_string << std::endl;
  return 0;
}


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


int keyword_spotter_impl::Init(const char* model_path, const char* hotwords_path) {
    // model_config, config_ and recognizer_ are defined in recognizer.h
    //set_default_sherpa_ncnn_config(config_);
    config_.model_config.transducer.buffer_flag_ = 1;
    config_.model_config.model_type = "zipformer2";
    config_.model_config.provider = "cpu";
    ///set feature
    /*config_.feat_config.sampling_rate = 16000.0f;
    config_.feat_config.feature_dim = 80;
    config_.feat_config.low_freq = 20.0f;
    config_.feat_config.high_freq = -400.0f;
    config_.feat_config.dither = 0.0f;
    config_.feat_config.normalize_samples = true;
    config_.feat_config.snip_edges = false;
    config_.feat_config.frame_shift_ms = 10.0f;
    config_.feat_config.frame_length_ms = 25.0f;
    config_.feat_config.is_librosa = false;
    config_.feat_config.remove_dc_offset = true;
    config_.feat_config.window_type = "povey";
    config_.feat_config.nemo_normalize_type = "";
    */

    std::string model_name;
    size_t tokens_buffer_size = 0;
    model_name = model_path;
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
    config_.keywords_file = hotwords_path;
    config_.keywords_score = 0.0f;
    
    recognizer_ = std::make_unique<sherpa_onnx::KeywordSpotter>(config_);

    return 0;
}