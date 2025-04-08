cbuffer FrustumPlanes : register(b0)
{
    float4 frustumPlanes[6];
}
;

struct InstanceData
{
    float4x4 modelMatrix;
    uint textureIndex;
    uint totalInstances;
    float2 padding; // выравнивание
};

StructuredBuffer<InstanceData> instanceBuffer : register(t0);
RWByteAddressBuffer argsBuffer     : register(u0);
RWStructuredBuffer<uint> visibleIds     : register(u1);

bool IsVisible(in float3 aabbCenter, in float halfExtent)
{
    // Проверяем пересечение AABB с каждым из 6 фрустум-плоскостей
    [unroll]
    for (int planeIdx = 0; planeIdx < 6; ++planeIdx)
    {
        float distance = dot(frustumPlanes[planeIdx].xyz, aabbCenter) + frustumPlanes[planeIdx].w;
        float radius = halfExtent * (abs(frustumPlanes[planeIdx].x) + abs(frustumPlanes[planeIdx].y) + abs(frustumPlanes[planeIdx].z));

        if ((distance + radius) < 0)
        {
            return false;
        }
    }
    return true;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint instanceId = dispatchId.x;

if (instanceId >= instanceBuffer[0].totalInstances)
    return;

// Получаем центр AABB как позицию из матрицы модели
float3 centerPosition = instanceBuffer[instanceId].modelMatrix[3].xyz;

// Размер AABB (предположительно куб с масштабом)
const float boxExtent = 0.475f;

if (IsVisible(centerPosition, boxExtent))
{
    uint visibleIndex;
    argsBuffer.InterlockedAdd(4, 1, visibleIndex);
    visibleIds[visibleIndex] = instanceId;
}
}
