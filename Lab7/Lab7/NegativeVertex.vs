struct VS_INPUT
{
    float4 pos : POSITION;
    float2 tex : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = input.pos;
    output.tex = input.tex;
    return output;
}