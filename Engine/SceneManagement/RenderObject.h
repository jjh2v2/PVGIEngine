#pragma once

#include "../Utilities/d3dApp.h"
#include "../Utilities/FrameResource.h"
#include "MeshGeometry.h"
#include "Material.h"

using namespace DirectX;

class RenderObject
{
public:
	RenderObject() = default;
	
	void Draw(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*, 
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, UINT, UINT, UINT, bool);

	void InitializeAsQuad(MeshGeometry*, UINT);
	
	int GetNumFramesDirty();
	XMFLOAT4X4* GetWorldMatrixPtr();
	UINT GetObjCBIndex();

	void SetObjCBIndex(UINT);
	void SetMat(Material*);
	void SetGeo(MeshGeometry*);
	void SetPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY);
	void SetIndexCount(UINT);
	void SetStartIndexLocation(UINT);
	void SetBaseVertexLocation(int);
	void SetWorldMatrix(XMMATRIX*);
	void SetIsPostProcessingQuad(bool);
	
	void DecrementNumFramesDirty();

	~RenderObject() = default;

private:

	void DrawQuad(ID3D12GraphicsCommandList*, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>);
	
	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = 3 so that each frame resource gets the update.
	int NumFramesDirty = 3;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	bool isPostProcessingQuad = false;
};