#include "GBufferRenderPass.h"

void GBufferRenderPass::Execute(ID3D12GraphicsCommandList * commandList, D3D12_CPU_DESCRIPTOR_HANDLE* depthStencilViewPtr,
	ID3D12Resource* passCB, ID3D12Resource* objectCB, ID3D12Resource* matCB)
{
	UINT rtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvhDescriptor(mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	commandList->SetPipelineState(mPSO.Get());

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	commandList->ClearDepthStencilView(dsvhDescriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Indicate a state transition on the resource usage.
	for (int i = 0; i < 4; ++i)
	{
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffers[i].Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
	}

	// Clear the back buffer and depth buffer.
	for (int i = 0; i < 4; ++i)
	{
		commandList->ClearRenderTargetView(CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			i,
			rtvDescriptorSize), Colors::Black, 0, nullptr);
	}

	// Specify the buffers we are going to render to.
	commandList->OMSetRenderTargets(4, &CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		0,
		rtvDescriptorSize), true, &dsvhDescriptor);

	ID3D12DescriptorHeap* descriptorHeapsGBuffer[] = { mSrvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeapsGBuffer), descriptorHeapsGBuffer);

	commandList->SetGraphicsRootSignature(mRootSignature.Get());

	commandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	Draw(commandList, objectCB, matCB);

	// Indicate a state transition on the resource usage.
	for (int i = 0; i < 4; ++i)
	{
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffers[i].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
	}

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void GBufferRenderPass::Draw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* objectCB, ID3D12Resource* matCB)
{
	auto cbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	for (size_t i = 0; i < SceneManager::GetScenePtr()->numberOfObjects; ++i)
	{
		SceneManager::GetScenePtr()->mOpaqueRObjects[i]->Draw(commandList, objectCB, matCB, mSrvDescriptorHeap, cbvSrvDescriptorSize, objCBByteSize, matCBByteSize, false);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE GBufferRenderPass::DepthStencilView()
{
	return mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

void GBufferRenderPass::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void GBufferRenderPass::BuildDescriptorHeaps()
{
	mOutputBuffers = new ComPtr<ID3D12Resource>[4];

	// Construct the RTV Heap first
	CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = mClientWidth;
	resourceDesc.Height = mClientHeight;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearVal;
	clearVal.Color[0] = 0.0f;
	clearVal.Color[1] = 0.0f;
	clearVal.Color[2] = 0.0f;
	clearVal.Color[3] = 1.0f;
	clearVal.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	for (int i = 0; i < 4; i++) {
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &clearVal, IID_PPV_ARGS(mOutputBuffers[i].GetAddressOf())));
	}

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = 4;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvDescriptorHeap.GetAddressOf())));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvhDescriptor(mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	UINT rtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (int i = 0; i < 4; ++i)
	{
		md3dDevice->CreateRenderTargetView(mOutputBuffers[i].Get(), &rtvDesc, rtvhDescriptor);

		if (i != 3)
			rtvhDescriptor.Offset(1, rtvDescriptorSize);
	}

	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = (2 * SceneManager::GetScenePtr()->numberOfUniqueObjects);
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	UINT cbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	for (UINT i = 0; i < (2 * SceneManager::GetScenePtr()->numberOfUniqueObjects); ++i)
	{
		srvDesc.Format = SceneManager::GetScenePtr()->mTextures[i]->Resource->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = SceneManager::GetScenePtr()->mTextures[i]->Resource->GetDesc().MipLevels;

		md3dDevice->CreateShaderResourceView(SceneManager::GetScenePtr()->mTextures[i]->Resource.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, cbvSrvDescriptorSize);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DXGI_FORMAT_D32_FLOAT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvDescriptorHeap.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.Texture2D.MipSlice = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvhDescriptor(mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, dsvhDescriptor);
}

void GBufferRenderPass::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mVertexShader->GetBufferPointer()),
		mVertexShader->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mPixelShader->GetBufferPointer()),
		mPixelShader->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 4;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.RTVFormats[3] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}