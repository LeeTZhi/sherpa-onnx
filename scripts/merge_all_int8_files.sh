#!/bin/bash
# merge all files into one file
./merge_model $1/encoder_jit_trace-pnnx.ncnn.param $1/encoder_jit_trace-pnnx.ncnn.bin \
        $1/decoder_jit_trace-pnnx.ncnn.param $1/decoder_jit_trace-pnnx.ncnn.bin \
        $1/joiner_jit_trace-pnnx.ncnn.param $1/joiner_jit_trace-pnnx.ncnn.bin \
        $1/tokens.txt \
        $2