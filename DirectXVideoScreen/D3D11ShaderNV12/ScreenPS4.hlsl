//--------------------------------------------------------------------------------------
// ScreenPS2.hlsl
//--------------------------------------------------------------------------------------
//Texture2D txInput : register(t0);
Texture2D txInput : register(t0);

SamplerState GenericSampler : register(s0);

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float2 PS(PS_INPUT input) : SV_Target
{
	float2 output = 0;

	Texture2D tex = txInput;
	uint2 size = 0;
	float miplevels;
	tex.GetDimensions(0, size.x, size.y, miplevels); // Whole texture size, not UV plane size

	uint2 uv_pos = floor(input.Tex*size);

	uint plane_idx = uv_pos.y < size.y / 2 ? 0 : 1;

	uint pick_sample1_1D = (uv_pos.x + (uv_pos.y - plane_idx*size.y/2) * size.x) * 2;
	uint pick_sample2_1D = pick_sample1_1D + 1;
	float2 pick_sample1_pos_tex = float2((float)(pick_sample1_1D%size.x) / size.x, (float)(pick_sample1_1D / size.x) / size.y);
	float2 pick_sample2_pos_tex = float2((float)(pick_sample2_1D%size.x) / size.x, (float)(pick_sample2_1D / size.x) / size.y);

	//output.x = size.x % 256 / 256.0f; //(float)(pick_sample1_1D%size.x % 256)/256.0f; // txInput.Sample(GenericSampler, pick_sample1_pos_tex)[plane_idx];
	//output.y = size.y % 256 / 256.0f; //(float)(pick_sample2_1D%size.x % 256)/256.0f; // txInput.Sample(GenericSampler, pick_sample2_pos_tex)[plane_idx];

	output.x = txInput.Sample(GenericSampler, pick_sample1_pos_tex)[plane_idx];
	output.y = txInput.Sample(GenericSampler, pick_sample2_pos_tex)[plane_idx];


	return output;
}