#define ALL_BUFFERS_READWRITE

#if defined(ALL_BUFFERS_READWRITE)
    RWStructuredBuffer<float> inputA : register(u0, space1);
    RWStructuredBuffer<float> inputB : register(u1, space1);
    RWStructuredBuffer<float> output : register(u2, space1);
#else
    StructuredBuffer<float> inputA : register(t0, space0);
    StructuredBuffer<float> inputB : register(t1, space0);
    RWStructuredBuffer<float> output : register(u0, space1);
#endif

[numthreads(512, 1, 1)]
void main(uint3 tid : SV_DispatchThreadId) {
    const float a = inputA[tid.x];
    const float b = inputB[tid.x];

    output[tid.x] = a + b;
}
