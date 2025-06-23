#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <random>
#include <spdlog/spdlog.h>
#include <vector>

// #define ALL_BUFFERS_READWRITE

static constexpr size_t k_BufferSize = 1024 * 1024;

int main(int argc, char** args) {
    const bool debugMode = true;
    const char* preferredGpu = nullptr;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GPUDevice* gpu = SDL_CreateGPUDevice(
#if defined(__APPLE__)
        SDL_GPU_SHADERFORMAT_MSL,
#elif defined(_WIN32)
        SDL_GPU_SHADERFORMAT_DXIL,
#endif
        debugMode,
        preferredGpu
    );
    if (gpu == nullptr) {
        spdlog::error("Could not create GPU.");
    }
    // In a compute-only sample, we don't want to create windows.
    spdlog::info("Created GPU with driver {}", SDL_GetGPUDeviceDriver(gpu));

    const auto shaderFormats = SDL_GetGPUShaderFormats(gpu);
#if defined(__APPLE__)
    if (!(shaderFormats & SDL_GPU_SHADERFORMAT_MSL)) {
        spdlog::error("This GPU doesn't support Metal.");
    }
#elif defined(_WIN32)
    if (!(shaderFormats & SDL_GPU_SHADERFORMAT_DXIL)) {
        spdlog::error("This GPU doesn't support DXIL.");
    }
#endif

    size_t shaderSize;
#if defined(__APPLE__)
    void* shaderCode = SDL_LoadFile("cs.metal", &shaderSize);
#elif defined(_WIN32)
    void* shaderCode = SDL_LoadFile("cs.dxil", &shaderSize);
#endif

    SDL_GPUComputePipelineCreateInfo computePipelineInfo = {0};
    computePipelineInfo.code = reinterpret_cast<Uint8*>(shaderCode);
    computePipelineInfo.code_size = shaderSize;
#if defined(__APPLE__)
    computePipelineInfo.entrypoint = "CSMain";
    computePipelineInfo.format = SDL_GPU_SHADERFORMAT_MSL;
#elif defined(_WIN32)
    computePipelineInfo.entrypoint = "CSMain";
    computePipelineInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
#endif
    computePipelineInfo.num_readonly_storage_textures = 0;
    computePipelineInfo.num_readwrite_storage_textures = 0;
#if defined(ALL_BUFFERS_READWRITE)
    computePipelineInfo.num_readonly_storage_buffers = 0;
    computePipelineInfo.num_readwrite_storage_buffers = 3;
#else
    computePipelineInfo.num_readonly_storage_buffers = 2;
    computePipelineInfo.num_readwrite_storage_buffers = 1;
#endif
    computePipelineInfo.num_samplers = 0;
    computePipelineInfo.num_uniform_buffers = 0;
    computePipelineInfo.threadcount_x = 512;
    computePipelineInfo.threadcount_y = 1;
    computePipelineInfo.threadcount_z = 1;

    SDL_GPUComputePipeline* computePipeline = SDL_CreateGPUComputePipeline(gpu, &computePipelineInfo);

    if (computePipeline == nullptr) {
        spdlog::error("Failed to create compute pipeline!");
    }
    else {
        spdlog::info("Pipeline created.");
    }

    SDL_GPUBufferCreateInfo roBufferInfo;
#if defined(ALL_BUFFERS_READWRITE)
    roBufferInfo.usage = 0
        | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
        | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE
    ;
#else
    roBufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
#endif
    roBufferInfo.size = k_BufferSize;
    SDL_GPUBuffer* input1 = SDL_CreateGPUBuffer(gpu, &roBufferInfo);
    SDL_GPUBuffer* input2 = SDL_CreateGPUBuffer(gpu, &roBufferInfo);

    SDL_GPUBufferCreateInfo rwBufferInfo = roBufferInfo;
#if defined(ALL_BUFFERS_READWRITE)
    rwBufferInfo.usage = 0
        | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
        | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE
    ;
#else
    rwBufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
#endif
    SDL_GPUBuffer* output = SDL_CreateGPUBuffer(gpu, &rwBufferInfo);

    SDL_GPUTransferBufferCreateInfo txBufferInfo;
    txBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    txBufferInfo.size = k_BufferSize;
    SDL_GPUTransferBuffer* txBuffer1 = SDL_CreateGPUTransferBuffer(gpu, &txBufferInfo);
    SDL_GPUTransferBuffer* txBuffer2 = SDL_CreateGPUTransferBuffer(gpu, &txBufferInfo);

    SDL_GPUTransferBufferCreateInfo rxBufferInfo = txBufferInfo;
    rxBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    SDL_GPUTransferBuffer* rxBuffer = SDL_CreateGPUTransferBuffer(gpu, &rxBufferInfo);

    auto makeRandomVector = [](size_t size) {
        std::vector<float> randomVector(k_BufferSize / sizeof(float));
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
        std::generate(randomVector.begin(), randomVector.end(), std::bind(dis, gen));

        return randomVector;
    };

    const auto cpuBufferInput2 = makeRandomVector(k_BufferSize / sizeof(float));
    const auto cpuBufferInput1 = makeRandomVector(k_BufferSize / sizeof(float));
    std::vector<float> cpuBufferOutput(k_BufferSize / sizeof(float));
    std::fill(cpuBufferOutput.begin(), cpuBufferOutput.end(), .0f);

    auto* txBuffer1Data = reinterpret_cast<float*>(SDL_MapGPUTransferBuffer(gpu, txBuffer1, false));
    std::copy(cpuBufferInput1.begin(), cpuBufferInput1.end(), txBuffer1Data);
    SDL_UnmapGPUTransferBuffer(gpu, txBuffer1);

    auto* txBuffer2Data = reinterpret_cast<float*>(SDL_MapGPUTransferBuffer(gpu, txBuffer2, false));
    std::copy(cpuBufferInput2.begin(), cpuBufferInput2.end(), txBuffer2Data);
    SDL_UnmapGPUTransferBuffer(gpu, txBuffer2);

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpu); {
        SDL_GPUCopyPass* uploadPass = SDL_BeginGPUCopyPass(cmdBuf); {
            SDL_GPUTransferBufferLocation cpuLoc1;
                cpuLoc1.offset = 0;
                cpuLoc1.transfer_buffer = txBuffer1;
            SDL_GPUBufferRegion gpuLoc1;
                gpuLoc1.buffer = input1;
                gpuLoc1.offset = 0;
                gpuLoc1.size = k_BufferSize;
            SDL_UploadToGPUBuffer(uploadPass, &cpuLoc1, &gpuLoc1, false);

            SDL_GPUTransferBufferLocation cpuLoc2 = cpuLoc1;
                cpuLoc2.transfer_buffer = txBuffer2;
            SDL_GPUBufferRegion gpuLoc2 = gpuLoc1;
                gpuLoc2.buffer = input2;
                gpuLoc2.offset = 0;
                gpuLoc2.size = k_BufferSize;
            SDL_UploadToGPUBuffer(uploadPass, &cpuLoc2, &gpuLoc2, false);
        } SDL_EndGPUCopyPass(uploadPass);

#if defined(ALL_BUFFERS_READWRITE)
        SDL_GPUStorageBufferReadWriteBinding bufferBindings[3];
        bufferBindings[0].buffer = input1;
        bufferBindings[1].buffer = input2;
        bufferBindings[2].buffer = output;
        bufferBindings[0].cycle = false;
        bufferBindings[1].cycle = false;
        bufferBindings[2].cycle = false;

        SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuf, nullptr, 0, bufferBindings, 3);
#else
        SDL_GPUStorageBufferReadWriteBinding outputBufferBinding;
        outputBufferBinding.buffer = output;
        outputBufferBinding.cycle = false;

        SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuf, nullptr, 0, &outputBufferBinding, 1);
#endif   
        {
            SDL_BindGPUComputePipeline(computePass, computePipeline);
#if !defined(ALL_BUFFERS_READWRITE)
            SDL_GPUBuffer* inputBuffers[2] = {
                input1,
                input2
            };
            SDL_BindGPUComputeStorageBuffers(computePass, 0, inputBuffers, 2);
#endif
            SDL_DispatchGPUCompute(computePass, k_BufferSize / (512 * sizeof(float)), 1, 1);
        } SDL_EndGPUComputePass(computePass);

        SDL_GPUCopyPass* downloadPass = SDL_BeginGPUCopyPass(cmdBuf); {
            SDL_GPUTransferBufferLocation cpuOutput;
                cpuOutput.offset = 0;
                cpuOutput.transfer_buffer = rxBuffer;
            SDL_GPUBufferRegion gpuOutput;
                gpuOutput.buffer = output;
                gpuOutput.offset = 0;
                gpuOutput.size = k_BufferSize;
            SDL_DownloadFromGPUBuffer(downloadPass, &gpuOutput, &cpuOutput);
        } SDL_EndGPUCopyPass(downloadPass);
    } SDL_GPUFence* doneFence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuf);

    // Now, compare with CPU results.
    for (int idx = 0; idx < cpuBufferOutput.size(); ++idx) {
        cpuBufferOutput[idx] = cpuBufferInput1[idx] + cpuBufferInput2[idx];
    }
    spdlog::info("Ticks before WaitForGPUFences: {}", SDL_GetPerformanceCounter());
    SDL_WaitForGPUFences(gpu, true, &doneFence, 1);
    spdlog::info("Ticks after  WaitForGPUFences: {}", SDL_GetPerformanceCounter());
    SDL_ReleaseGPUFence(gpu, doneFence);

    auto* rxBufferData = reinterpret_cast<float*>(
    SDL_MapGPUTransferBuffer(gpu, rxBuffer, false));
    int wrongCount = 0;
    for (int idx = 0; idx < cpuBufferOutput.size(); ++idx) {
        if (rxBufferData[idx] != cpuBufferOutput[idx]) {
            spdlog::warn("Idx {}: CPU is {}, GPU is {}", idx, cpuBufferOutput[idx], rxBufferData[idx]);
            wrongCount++;
            if (wrongCount > 10) {
                break;
            }
        }
    }
    if (wrongCount == 0) {
        spdlog::info("Completed check; all pass.");
    }
    SDL_UnmapGPUTransferBuffer(gpu, rxBuffer);
    SDL_ReleaseGPUComputePipeline(gpu, computePipeline);

    SDL_ReleaseGPUTransferBuffer(gpu, txBuffer1);
    SDL_ReleaseGPUTransferBuffer(gpu, txBuffer2);
    SDL_ReleaseGPUTransferBuffer(gpu, rxBuffer);
    SDL_ReleaseGPUBuffer(gpu, input1);
    SDL_ReleaseGPUBuffer(gpu, input2);
    SDL_ReleaseGPUBuffer(gpu, output);

    SDL_free(shaderCode);
    SDL_DestroyGPUDevice(gpu);
    SDL_Quit();
    return 0;
}
