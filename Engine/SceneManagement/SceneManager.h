#pragma once

#include "RenderObject.h"
#include "MeshLoader.h"
#include "Texture.h"

struct SceneObject
{
	SceneObject() = default;

	std::string					meshName;
	std::string					diffuseOpacityTextureName;
	std::string					normalRoughnessTextureName;
	DirectX::XMFLOAT3			position;
	DirectX::XMFLOAT4			rotation;
	DirectX::XMFLOAT3			scale;
};

struct Scene
{
	Scene() = default;

	std::string					name;
	DirectX::XMFLOAT3			cameraPosition;
	DirectX::XMFLOAT4			cameraRotation;
	DirectX::XMFLOAT3			lightDirection;
	DirectX::XMFLOAT3			lightStrength;
	UINT						numberOfObjects;
	UINT						numberOfUniqueObjects;

	SceneObject*				objectsInScene;

	std::unique_ptr<Texture>* mTextures;
	std::unique_ptr<Material>* mMaterials;
	std::unique_ptr<SubmeshGeometry>* mSubMeshes;
	std::unique_ptr<RenderObject>* mOpaqueRObjects;
	
	std::unique_ptr<RenderObject> mQuadrObject;
	
	std::unique_ptr<MeshGeometry> mSceneGeometry;
};

class SceneManager
{
public:
	SceneManager() = default;

	static void LoadScene(std::string, Microsoft::WRL::ComPtr<ID3D12Device>, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>);
	static Scene* GetScenePtr();

	~SceneManager() = default;

private:

	static void ImportScene(std::string);
	static void ResizeBuffers();
	static void LoadTextures(Microsoft::WRL::ComPtr<ID3D12Device>, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>);
	static void BuildSceneGeometry(Microsoft::WRL::ComPtr<ID3D12Device>, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>);
	static void BuildMaterials();
	static void BuildRenderObjects();

	static Material* GetMaterial(std::string);
	static UINT GetIndexCount(std::string);
	static UINT GetStartIndexLocation(std::string);
	static int GetBaseVertexLocation(std::string);

	static Scene mScene;
};