// sherpa-onnx/csrc/online-transducer-model-config.h
//
// Copyright (c)  2023  Xiaomi Corporation
#ifndef SHERPA_ONNX_CSRC_ONLINE_TRANSDUCER_MODEL_CONFIG_H_
#define SHERPA_ONNX_CSRC_ONLINE_TRANSDUCER_MODEL_CONFIG_H_

#include <string>

#include "sherpa-onnx/csrc/parse-options.h"

namespace sherpa_onnx {

struct OnlineTransducerModelConfig {
  std::string encoder;
  std::string decoder;
  std::string joiner;

  bool buffer_flag_ = true;
  char* encoder_buffer_;
  size_t encoder_buffer_size_;
  char* decoder_buffer_;
  size_t decoder_buffer_size_;
  char* joiner_buffer_;
  size_t joiner_buffer_size_;

  OnlineTransducerModelConfig() = default;
  OnlineTransducerModelConfig(const std::string &encoder,
                              const std::string &decoder,
                              const std::string &joiner)
      : encoder(encoder), decoder(decoder), joiner(joiner) { 
        encoder_buffer_ = nullptr; decoder_buffer_ = nullptr; joiner_buffer_ = nullptr; }
  OnlineTransducerModelConfig( char* encoder_buffer, size_t encoder_buffer_size,
                              char* decoder_buffer, size_t decoder_buffer_size,
                              char* joiner_buffer, size_t joiner_buffer_size)
      : encoder_buffer_(encoder_buffer), encoder_buffer_size_(encoder_buffer_size),
        decoder_buffer_(decoder_buffer), decoder_buffer_size_(decoder_buffer_size),
        joiner_buffer_(joiner_buffer), joiner_buffer_size_(joiner_buffer_size) { buffer_flag_ = true;}

  void Register(ParseOptions *po);
  bool Validate() const;

  std::string ToString() const;
};

}  // namespace sherpa_onnx

#endif  // SHERPA_ONNX_CSRC_ONLINE_TRANSDUCER_MODEL_CONFIG_H_
