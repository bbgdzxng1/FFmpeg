/**
 * Intel Library for Video Super Resolution
 *
 * Copyright (c) 2023 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
const char* gFilterShader =
"#define scale %d\n"
"#define scalef %f\n"
"#define usePixelType %d\n"
"#define patchlength %d\n"
"#define gradientLength %d\n"
"#define numOfAngle %d\n"
"#define numOfStrength %d\n"
"#define numOfCoherence %d\n"
"#define colorRangeMin %d\n"
"#define colorRangeMax %d\n"
"#define STEP 2\n"
"#define PI 3.1415926f\n"
"\n"
"int search_index(float value, float *list, int num) {\n"
"    for (int i = 0; i < num; i++)\n"
"        if (value < list[i])\n"
"            return i;\n"
"    return num;\n"
"}\n"
"\n"
"__kernel\n"
"void filter(__global float *output,\n"
"            __global float *imgGx, __global float *imgGy,\n"
"            __global float *buckets_w, int width, int height, int linesize) {\n"
"    int2 loc = (int2)(get_global_id(0), get_global_id(1)) * STEP;\n"
"    int margin = patchlength / 2;\n"
"    int gradientMargin = gradientLength / 2;\n"
"    float patchGx, patchGy;\n"
"    float m_as[STEP*STEP] = {0}, m_bs[STEP*STEP] = {0}, m_ds[STEP*STEP] = {0};\n"
"    int hashValue;\n"
"    float filteredPixel = 0.0f;\n"
"    int start_x, start_y;\n"
"    float strengthlist[numOfStrength - 1] = { %s };\n"
"    float coherencelist[numOfCoherence - 1] = { %s };\n"
"\n"
"    if (loc.x < margin  || loc.x > width - margin - STEP ||\n"
"        loc.y < margin || loc.y > height - margin - STEP ) {\n"
"        return;\n"
"    }\n"
"    start_y = loc.y - gradientMargin;\n"
"    start_x = loc.x - gradientMargin;\n"
"    __attribute__((opencl_unroll_hint))\n"
"    for (int patch_y = 0; patch_y < gradientLength + STEP - 1; patch_y++)\n"
"        __attribute__((opencl_unroll_hint))\n"
"        for (int patch_x = 0; patch_x < gradientLength + STEP - 1; patch_x++) {\n"
"            int i = 0;\n"
"            patchGx = imgGx[(patch_y + start_y) * linesize + patch_x + start_x];\n"
"            patchGy = imgGy[(patch_y + start_y) * linesize + patch_x + start_x];\n"
"            __attribute__((opencl_unroll_hint))\n"
"            for (int step_y = 0; step_y < STEP; step_y++)\n"
"                __attribute__((opencl_unroll_hint))\n"
"                for (int step_x = 0; step_x < STEP; step_x++)\n"
"                    if (patch_y >= step_y && patch_y < step_y + gradientLength &&\n"
"                        patch_x >= step_x && patch_x < step_x + gradientLength) {\n"
"                            float w;\n"
"                            i = step_y * STEP + step_x;\n"
"                            w = buckets_w[(patch_y - step_y) * gradientLength + patch_x - step_x];\n"
"                            m_ds[i] += patchGx * patchGx * w;\n"
"                            m_bs[i] += patchGx * patchGy * w;\n"
"                            m_as[i] += patchGy * patchGy * w;\n"
"                        }\n"
"        }\n"
"    __attribute__((opencl_unroll_hint))\n"
"    for(int patch_y = 0; patch_y < STEP; patch_y++)\n"
"        __attribute__((opencl_unroll_hint))\n"
"        for(int patch_x = 0; patch_x < STEP; patch_x++) {\n"
"            int i = patch_y * STEP + patch_x;\n"
"            int count = 0;\n"
"            float m_a = m_as[i], m_b = m_bs[i], m_d = m_ds[i];\n"
"            float T = m_a + m_d;\n"
"            float D = m_a * m_d - m_b * m_b;\n"
"            float L1 = T / 2 + sqrt((T * T)/4 - D);\n"
"            float L2 = T / 2 - sqrt((T * T)/4 - D);\n"
"            float angle = 0;\n"
"            if (m_b != 0) {\n"
"                angle = atan2(m_b, L1 - m_d);\n"
"            } else {\n"
"                angle = atan2(0.0f, 1.0f);\n"
"            }\n"
"            if (angle < 0)  angle += PI;\n"
"            float coherence = ( sqrt(L1) - sqrt(L2) ) / ( sqrt(L1) + sqrt(L2) );\n"
"            float strength = L1;\n"
"\n"
"            int angleIdx = (int)(angle / ( PI / numOfAngle ));\n"
"\n"
"            angleIdx = angleIdx > numOfAngle-1 ? numOfAngle-1 : (angleIdx < 0 ? 0 : angleIdx);\n"
"            int strengthIdx = search_index(strength, strengthlist, numOfStrength - 1);\n"
"            strengthIdx = strengthIdx >= numOfStrength ? numOfStrength - 1: strengthIdx;\n"
"            int coherenceIdx = search_index(coherence, coherencelist, numOfCoherence - 1);\n"
"            coherenceIdx = coherenceIdx >= numOfCoherence ? numOfCoherence - 1: coherenceIdx;\n"
"\n"
"            hashValue = angleIdx*numOfStrength*numOfCoherence+\n"
"                        strengthIdx*numOfCoherence+\n"
"                        coherenceIdx;\n"
"            output[(loc.y + patch_y) * linesize + loc.x + patch_x] = hashValue;\n"
"        }\n"
"}\n"
"\n"
"__kernel\n"
"void hash_mul(__global float *input, __global float *input_hash,\n"
"#if usePixelType\n"
"                    __global float filterBuckets[numOfAngle*numOfStrength*numOfCoherence][scale*scale][patchlength*patchlength],\n"
"#else\n"
"                    __global float filterBuckets[numOfAngle*numOfStrength*numOfCoherence][1][patchlength*patchlength],\n"
"#endif\n"
"                    __global float *output, int width, int height, int linesize) { //}, __global double *filteredPixel) {\n"
"    int2 loc = (int2)(get_global_id(0), get_global_id(1)) * STEP;\n"
"    int margin = patchlength / 2;\n"
"    float patch;\n"
"    float filteredPixel[STEP*STEP] = { 0 };\n"
"    int bucketi[STEP*STEP] = { 0 }, pixelType[STEP*STEP] = { 0 };\n"
"    if (loc.x < margin || loc.x > width - margin - STEP ||\n"
"        loc.y < margin || loc.y > height - margin - STEP) {\n"
"        for(int patch_y = 0; patch_y < STEP; patch_y++)\n"
"            for(int patch_x = 0; patch_x < STEP; patch_x++)\n"
"                output[(loc.y + patch_y) * linesize + loc.x + patch_x] =\n"
"                    input[(loc.y + patch_y) * linesize + loc.x + patch_x];\n"
"        return;\n"
"    }\n"
"    int count = 0;\n"
"    __attribute__((opencl_unroll_hint))\n"
"    for(int patch_y = 0; patch_y < STEP; patch_y++)\n"
"        __attribute__((opencl_unroll_hint))\n"
"        for(int patch_x = 0; patch_x < STEP; patch_x++) {\n"
"            bucketi[patch_y * STEP + patch_x] =\n"
"                input_hash[(loc.y + patch_y) * linesize + loc.x + patch_x];\n"
"#if usePixelType\n"
"            pixelType[patch_y * STEP + patch_x] = ((loc.y + patch_y - margin) %% scale) * scale +\n"
"                ((loc.x + patch_x - margin) %% scale);\n"
"#endif\n"
"        }\n"
"    int start_y = loc.y - margin;\n"
"    int start_x = loc.x - margin;\n"
"    __attribute__((opencl_unroll_hint))\n"
"    for (int patch_y = 0; patch_y < patchlength + STEP - 1; patch_y++)\n"
"        __attribute__((opencl_unroll_hint))\n"
"        for (int patch_x = 0; patch_x < patchlength + STEP - 1; patch_x++) {\n"
"            patch = input[(patch_y + start_y) * linesize + patch_x + start_x];\n"
"            __attribute__((opencl_unroll_hint))\n"
"            for (int step_y = 0; step_y < STEP; step_y++)\n"
"                __attribute__((opencl_unroll_hint))\n"
"                for (int step_x = 0; step_x < STEP; step_x++) {\n"
"                    int i = step_y * STEP + step_x;\n"
"                    int offset = (patch_y - step_y) * patchlength + patch_x - step_x;\n"
"                    if (patch_y >= step_y && patch_y < step_y + patchlength &&\n"
"                        patch_x >= step_x && patch_x < step_x + patchlength) {\n"
"                            filteredPixel[i] = filteredPixel[i] + patch*filterBuckets[bucketi[i]][pixelType[i]][offset];\n"
"                    }\n"
"                }\n"
"        }\n"
"    __attribute__((opencl_unroll_hint))\n"
"    for(int patch_y = 0; patch_y < STEP; patch_y++)\n"
"        __attribute__((opencl_unroll_hint))\n"
"        for(int patch_x = 0; patch_x < STEP; patch_x++) {\n"
"            int i = patch_y * STEP + patch_x;\n"
"            if (filteredPixel[i] > colorRangeMax || filteredPixel[i] < colorRangeMin)\n"
"                output[(loc.y + patch_y) * linesize + loc.x + patch_x] = input[(loc.y + patch_y) * linesize + loc.x + patch_x];\n"
"            else\n"
"                output[(loc.y + patch_y) * linesize + loc.x + patch_x] = filteredPixel[i];\n"
"        }\n"
"}\n"
"\n"
"__kernel void gradient(__global float *input,\n"
"                     __global float *imgGx, __global float *imgGy,\n"
"                     int width, int height, int linesize) {\n"
"    int2 loc = (int2)(get_global_id(0), get_global_id(1));\n"
"    float patch[9];\n"
"    int i = 0;\n"
"\n"
"    if (loc.x >= width || loc.x < 0 ||\n"
"        loc.y >= height || loc.y < 0)\n"
"        return;\n"
"\n"
"    for (int y = loc.y - 1; y <= loc.y + 1; y++)\n"
"        for(int x = loc.x - 1; x <= loc.x + 1; x++) {\n"
"            if (x >= width) {\n"
"                patch[i++] = 2 * input[y * linesize + x - 1] - input[y * linesize + x - 2];\n"
"            } else if (x < 0) {\n"
"                patch[i++] = 2 * input[y * linesize] - input[y * linesize + 1];\n"
"            } else if (y >= height) {\n"
"                patch[i++] = 2 * input[(y - 1) * linesize + x] - input[(y - 2) * linesize + x];\n"
"            } else if (y < 0) {\n"
"                patch[i++] = 2 * input[x] - input[linesize + x];\n"
"            } else {\n"
"                patch[i++] = input[y*linesize + x];\n"
"            }\n"
"        }\n"
"    imgGx[loc.y * linesize + loc.x] = patch[5] - patch[3];\n"
"    imgGy[loc.y * linesize + loc.x] = patch[7] - patch[1];\n"
"    return;\n"
"}\n"
"\n"
"__kernel void blend(__global float *input_LR, __global float *input_HR,\n"
"                     __global float *output_blend, int width, int height, int linesize) {\n"
"    int patch_len = 3;\n"
"    int margin = 3 / 2;\n"
"    int2 loc = (int2)(get_global_id(0), get_global_id(1));\n"
"    int census_lr, census_hr;\n"
"    float count = 0;\n"
"    float weight;\n"
"\n"
"\n"
"    if (loc.x < margin || loc.x > width - margin - 1 ||\n"
"        loc.y < margin || loc.y > height - margin - 1) {\n"
"        output_blend[loc.y * linesize + loc.x] = input_HR[loc.y * linesize + loc.x];\n"
"        return;\n"
"    }\n"
"\n"
"    for (int patch_y = loc.y - margin; patch_y <= loc.y + margin; patch_y++)\n"
"        for (int patch_x = loc.x - margin; patch_x <= loc.x + margin; patch_x++) {\n"
"            census_lr = input_LR[patch_y * linesize + patch_x] > input_LR[loc.y * linesize + loc.x] ? 1.0f : 0.0f;\n"
"            census_hr = input_HR[patch_y * linesize + patch_x] > input_HR[loc.y * linesize + loc.x] ? 1.0f : 0.0f;\n"
"            if (census_hr != census_lr)\n"
"                count = count + 1;\n"
"        }\n"
"\n"
"    weight = count / (patch_len * 4.0 - 4.0);\n"
"    output_blend[loc.y * linesize + loc.x] = (1 - weight) * input_HR[loc.y * linesize + loc.x] +\n"
"                                              weight * input_LR[loc.y * linesize + loc.x];\n"
"    return;\n"
"}\n"
"\n"
"__constant sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_LINEAR;\n"
"__kernel void preprocess(__read_only  image2d_t input, int width, int height, int linesize,\n"
"                        __global float *output, int linesize_o, float width_factor, float height_factor,\n"
"                        int bitshift, int nb_components) {\n"
"    int2 loc = (int2)(get_global_id(0), get_global_id(1));\n"
"    if (loc.x >= width || loc.y >= height)\n"
"        return;\n"
"    int data_type = get_image_channel_data_type(input);\n"
"    int normalize_factor = 255;\n"
"    if (data_type == CLK_UNORM_INT16)\n"
"        normalize_factor = 65535;\n"
"    int shift_factor = 1 << bitshift;\n"
"    float2 normalized_loc = (convert_float2(loc) + 0.5f) * (float2)(width_factor, height_factor);\n"
"    if (loc.x == width - 1)\n"
"        normalized_loc.x = loc.x * width_factor;\n"
"    if (loc.y == height - 1)\n"
"        normalized_loc.y = loc.y * height_factor;\n"
"    float4 pixel = read_imagef(input, sampler, normalized_loc) * normalize_factor / shift_factor;\n"
"    if (nb_components == 1)\n"
"        output[loc.y * linesize_o + loc.x] = round(pixel.s0);\n"
"    else if (nb_components == 2) {\n"
"        output[loc.y * linesize_o * 2 + loc.x * 2] = round(pixel.s0);\n"
"        output[loc.y * linesize_o * 2 + loc.x * 2 + 1] = round(pixel.s1);\n"
"    }\n"
"    return;\n"
"}\n"
"\n"
"__kernel void postprocess(__global float *input, int width, int height, int linesize,\n"
"                        __write_only image2d_t output, int linesize_o, int bitshift, int nb_components) {\n"
"    int2 loc = (int2)(get_global_id(0), get_global_id(1));\n"
"\n"
"    if (loc.x >= width || loc.y >= height)\n"
"        return;\n"
"    int data_type = get_image_channel_data_type(output);\n"
"    int normalize_factor = 255;\n"
"    if (data_type == CLK_UNORM_INT16)\n"
"        normalize_factor = 65535;\n"
"    int shift_factor = 1 << bitshift;\n"
"    float4 pixel;\n"
"\n"
"    if (nb_components == 1)\n"
"        pixel.s0 = input[loc.y * linesize + loc.x];\n"
"    else if (nb_components == 2) {\n"
"        pixel.s0 = input[loc.y * linesize * 2 + loc.x * 2];\n"
"        pixel.s1 = input[loc.y * linesize * 2 + loc.x * 2 + 1];\n"
"    }\n"
"    pixel = pixel / normalize_factor * shift_factor;\n"
"    pixel.s3 = 1;\n"
"    write_imagef(output, loc, pixel);\n"
"    return;\n"
"}\n";
