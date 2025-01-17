#include "Effects.h"
#include "d3dUtil.h"
#include "EffectHelper.h"	// 必须晚于Effects.h和d3dUtil.h包含
#include "DXTrace.h"
#include "Vertex.h"
using namespace DirectX;

# pragma warning(disable: 26812)


//
// ForwardEffect::Impl 需要先于ForwardEffect的定义
//

class ForwardEffect::Impl
{
public:
	// 必须显式指定
	Impl() {}
	~Impl() = default;

public:
	template<class T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	std::unique_ptr<EffectHelper> m_pEffectHelper;

	std::shared_ptr<IEffectPass> m_pCurrEffectPass;

	ComPtr<ID3D11InputLayout> m_pVertexPosNormalTexLayout;

	XMFLOAT4X4 m_World{}, m_View{}, m_Proj{};
};

//
// ForwardEffect
//

namespace
{
	// ForwardEffect单例
	static ForwardEffect * g_pInstance = nullptr;
}

ForwardEffect::ForwardEffect()
{
	if (g_pInstance)
		throw std::exception("ForwardEffect is a singleton!");
	g_pInstance = this;
	pImpl = std::make_unique<ForwardEffect::Impl>();
}

ForwardEffect::~ForwardEffect()
{
}

ForwardEffect::ForwardEffect(ForwardEffect && moveFrom) noexcept
{
	pImpl.swap(moveFrom.pImpl);
}

ForwardEffect & ForwardEffect::operator=(ForwardEffect && moveFrom) noexcept
{
	pImpl.swap(moveFrom.pImpl);
	return *this;
}

ForwardEffect & ForwardEffect::Get()
{
	if (!g_pInstance)
		throw std::exception("ForwardEffect needs an instance!");
	return *g_pInstance;
}


bool ForwardEffect::InitAll(ID3D11Device * device)
{
	if (!device)
		return false;

	if (!RenderStates::IsInit())
		throw std::exception("RenderStates need to be initialized first!");

	pImpl->m_pEffectHelper = std::make_unique<EffectHelper>();

	Microsoft::WRL::ComPtr<ID3DBlob> blob;

	// ******************
	// 创建顶点着色器
	//

	HR(CreateShaderFromFile(nullptr, L"Shaders\\Forward.hlsl", nullptr, "GeometryVS", "vs_5_0", blob.ReleaseAndGetAddressOf()));
	HR(pImpl->m_pEffectHelper->AddShader("GeometryVS", device, blob.Get()));
	// 创建顶点布局
	HR(device->CreateInputLayout(VertexPosNormalTex::inputLayout, ARRAYSIZE(VertexPosNormalTex::inputLayout),
		blob->GetBufferPointer(), blob->GetBufferSize(), pImpl->m_pVertexPosNormalTexLayout.ReleaseAndGetAddressOf()));

	// ******************
	// 创建像素着色器
	//

	HR(CreateShaderFromFile(nullptr, L"Shaders\\Forward.hlsl", nullptr, "ForwardPS", "ps_5_0", blob.ReleaseAndGetAddressOf()));
	HR(pImpl->m_pEffectHelper->AddShader("ForwardPS", device, blob.Get()));

	// ******************
	// 创建通道
	//
	EffectPassDesc passDesc;
	passDesc.nameVS = "GeometryVS";
	passDesc.namePS = "ForwardPS";
	pImpl->m_pEffectHelper->AddEffectPass("Forward", device, &passDesc);
	{
		auto pPass = pImpl->m_pEffectHelper->GetEffectPass("Forward");
		// 注意：反向Z => GREATER_EQUAL测试
		pPass->SetDepthStencilState(RenderStates::DSSGreaterEqual.Get(), 0);
	}
	
	passDesc.namePS = nullptr;
	pImpl->m_pEffectHelper->AddEffectPass("PreZ", device, &passDesc);
	{
		auto pPass = pImpl->m_pEffectHelper->GetEffectPass("PreZ");
		// 注意：反向Z => GREATER_EQUAL测试
		pPass->SetDepthStencilState(RenderStates::DSSGreaterEqual.Get(), 0);
	}

	pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamplerDiffuse", RenderStates::SSAnistropicWrap16x.Get());

	// 设置调试对象名
	D3D11SetDebugObjectName(pImpl->m_pVertexPosNormalTexLayout.Get(), "ForwardEffect.VertexPosNormalTexLayout");
	pImpl->m_pEffectHelper->SetDebugObjectName("ForwardEffect");

	return true;
}


void ForwardEffect::SetLightBuffer(ID3D11ShaderResourceView* lightBuffer)
{
	pImpl->m_pEffectHelper->SetShaderResourceByName("g_Light", lightBuffer);
}

void ForwardEffect::SetRenderPreZPass(ID3D11DeviceContext* deviceContext)
{
	deviceContext->IASetInputLayout(pImpl->m_pVertexPosNormalTexLayout.Get());
	pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("PreZ");
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ForwardEffect::SetRenderDefault(ID3D11DeviceContext* deviceContext)
{
	deviceContext->IASetInputLayout(pImpl->m_pVertexPosNormalTexLayout.Get());
	pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("Forward");
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void XM_CALLCONV ForwardEffect::SetWorldMatrix(DirectX::FXMMATRIX W)
{
	XMStoreFloat4x4(&pImpl->m_World, W);
}

void XM_CALLCONV ForwardEffect::SetViewMatrix(DirectX::FXMMATRIX V)
{
	XMStoreFloat4x4(&pImpl->m_View, V);
}

void XM_CALLCONV ForwardEffect::SetProjMatrix(DirectX::FXMMATRIX P)
{
	XMStoreFloat4x4(&pImpl->m_Proj, P);
}

void ForwardEffect::SetTextureDiffuse(ID3D11ShaderResourceView * textureDiffuse)
{
	pImpl->m_pEffectHelper->SetShaderResourceByName("g_TextureDiffuse", textureDiffuse);
}

void ForwardEffect::Apply(ID3D11DeviceContext * deviceContext)
{
	XMMATRIX W = XMLoadFloat4x4(&pImpl->m_World);
	XMMATRIX V = XMLoadFloat4x4(&pImpl->m_View);
	XMMATRIX P = XMLoadFloat4x4(&pImpl->m_Proj);

	XMMATRIX WV = W * V;
	XMMATRIX WVP = WV * P;
	XMMATRIX WInvTV = XMath::InverseTranspose(W) * V;
	XMMATRIX VP = V * P;

	WV = XMMatrixTranspose(WV);
	WVP = XMMatrixTranspose(WVP);
	WInvTV = XMMatrixTranspose(WInvTV);
	P = XMMatrixTranspose(P);
	VP = XMMatrixTranspose(VP);

	pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldInvTransposeView")->SetFloatMatrix(4, 4, (FLOAT*)&WInvTV);
	pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldViewProj")->SetFloatMatrix(4, 4, (FLOAT*)&WVP);
	pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldView")->SetFloatMatrix(4, 4, (FLOAT*)&WV);
	pImpl->m_pEffectHelper->GetConstantBufferVariable("g_ViewProj")->SetFloatMatrix(4, 4, (FLOAT*)&VP);
	pImpl->m_pEffectHelper->GetConstantBufferVariable("g_Proj")->SetFloatMatrix(4, 4, (FLOAT*)&P);

	if (pImpl->m_pCurrEffectPass)
		pImpl->m_pCurrEffectPass->Apply(deviceContext);
}


