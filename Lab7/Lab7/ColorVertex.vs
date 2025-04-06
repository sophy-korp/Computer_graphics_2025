static const uint MAX_INSTANCES = 23;

struct InstanceData
{
    float4x4 model;
    uint texInd;
};

cbuffer ModelBufferInst : register(b0)
{
    InstanceData modelBuffer[MAX_INSTANCES];
};

cbuffer CameraBuffer : register(b1)
{
    matrix vp;
    float3 CameraPos;
};

struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float3 Tangent : TEXCOORD3;
    float3 Bitangent : TEXCOORD4;
    float3 CameraPos : TEXCOORD5;
    uint TexInd : TEXCOORD6;
};

PS_INPUT main(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    PS_INPUT output;
    
    float4 worldPos = mul(float4(input.Pos, 1.0f), modelBuffer[instanceID].model);
    output.WorldPos = worldPos.xyz;
    output.Pos = mul(worldPos, vp);
    output.Normal = mul(input.Normal, (float3x3)modelBuffer[instanceID].model);
    output.TexCoord = input.TexCoord;
    output.CameraPos = CameraPos;

    float3 tangent;
    if (abs(input.Normal.z) > 0.999f)
    {
        tangent = float3(1.0f, 0.0f, 0.0f);
    }
    else
    {
        tangent = normalize(cross(input.Normal, float3(0, 0, 1)));
    }

    float3 bitangent = cross(input.Normal, tangent);
    output.Tangent = mul(tangent, (float3x3)modelBuffer[instanceID].model);
    output.Bitangent = mul(bitangent, (float3x3)modelBuffer[instanceID].model);
    output.TexInd = modelBuffer[instanceID].texInd;
    return output;
}