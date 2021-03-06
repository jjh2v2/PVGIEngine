#include "RayTracingUtil.hlsl"

Texture2D    DiffuseMetallicGBuffer  : register(t0);
Texture2D	 NormalRoughnessGBuffer  : register(t1);
Texture2D	 PositionDepthGBuffer	 : register(t2);
Texture2D	 ShadowPosHGBuffer		 : register(t3);
Texture2D	 MainTex				 : register(t4);

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentU : TANGENT;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vin.TexC.x - 1.0f, 1.0f - 2.0f * vin.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	int3 TexC = int3(pin.PosH.xy, 0);

	float4 position = PositionDepthGBuffer.Load(TexC);
	float4 normal = NormalRoughnessGBuffer.Load(TexC);
	float4 albedo = DiffuseMetallicGBuffer.Load(TexC);
	float4 lighting = MainTex.Load(TexC);
	
	float3 view = normalize(gEyePosW - position.xyz);
	
	float3 indirectSpecular = CalculateSpecularIndirectLighting(position.xyz, normal.xyz, view, (normal.w * normal.w), albedo);
	
	lighting.rgb += indirectSpecular;
	
	return lighting;
}