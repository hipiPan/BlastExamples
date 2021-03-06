#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <Gfx/GfxContext.h>
#include <Gfx/GfxBuffer.h>
#include <Gfx/GfxTexture.h>
#include <Gfx/GfxSwapchain.h>
#include <Gfx/GfxCommandBuffer.h>
#include <Gfx/GfxRenderPass.h>
#include <Gfx/GfxShader.h>
#include <Gfx/GfxPipeline.h>
#include <Gfx/Vulkan/VulkanContext.h>
#include <Utility/ShaderCompiler.h>
#include <Utility/VulkanShaderCompiler.h>
#include <iostream>
#include <fstream>
#include <sstream>

Blast::ShaderCompiler* shaderCompiler = nullptr;
Blast::GfxContext* context = nullptr;
Blast::GfxSwapchain* swapchain = nullptr;
Blast::GfxRenderPass** renderPasses = nullptr;
Blast::GfxQueue* queue = nullptr;
Blast::GfxFence** renderCompleteFences = nullptr;
Blast::GfxSemaphore** imageAcquiredSemaphores = nullptr;
Blast::GfxSemaphore** renderCompleteSemaphores = nullptr;
Blast::GfxCommandBufferPool* cmdPool = nullptr;
Blast::GfxCommandBuffer** cmds = nullptr;
Blast::GfxShader* vertShader = nullptr;
Blast::GfxShader* fragShader = nullptr;
Blast::GfxRootSignature* rootSignature = nullptr;
Blast::GfxGraphicsPipeline* pipeline = nullptr;
Blast::GfxBuffer* meshIndexBuffer = nullptr;
Blast::GfxBuffer* meshVertexBuffer = nullptr;
uint32_t frameIndex = 0;
uint32_t imageCount = 0;

static std::string projectDir(PROJECT_DIR);

static std::string readFileData(const std::string& path);

struct Vertex {
    float pos[3];
    float uv[2];
};

float vertices[] = {
        -0.5f,  -0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.0f, 1.0f, 1.0f
};

unsigned int indices[] = {
        0, 1, 2, 2, 3, 0
};

void shutdown() {
    if (meshIndexBuffer) {
        delete meshIndexBuffer;
        meshIndexBuffer = nullptr;
    }

    if (meshVertexBuffer) {
        delete meshVertexBuffer;
        meshVertexBuffer = nullptr;
    }

    if (vertShader) {
        delete vertShader;
    }

    if (fragShader) {
        delete fragShader;
    }

    if (rootSignature) {
        delete rootSignature;
    }

    if (pipeline) {
        delete pipeline;
    }

    if (cmds) {
        delete[] cmds;
        cmds = nullptr;
    }
    
    if (cmdPool) {
        delete cmdPool;
        cmdPool = nullptr;
    }
    
    if (renderCompleteFences) {
        delete[] renderCompleteFences;
        renderCompleteFences = nullptr;
    }

    if (imageAcquiredSemaphores) {
        delete[] imageAcquiredSemaphores;
        imageAcquiredSemaphores = nullptr;
    }

    if (renderCompleteSemaphores) {
        delete[] renderCompleteSemaphores;
        renderCompleteSemaphores = nullptr;
    }

    if (renderPasses) {
        delete[] renderPasses;
    }

    if (swapchain) {
        delete swapchain;
        swapchain = nullptr;
    }

    if (context) {
        delete context;
        context = nullptr;
    }

    if (shaderCompiler) {
        delete shaderCompiler;
        shaderCompiler = nullptr;
    }
}

int main() {
    shaderCompiler = new Blast::VulkanShaderCompiler();

    context = new Blast::VulkanContext();

    Blast::GfxBufferDesc bufferDesc;
    bufferDesc.size = sizeof(Vertex) * 4;
    bufferDesc.type = Blast::RESOURCE_TYPE_VERTEX_BUFFER;
    bufferDesc.usage = Blast::RESOURCE_USAGE_CPU_TO_GPU;
    meshVertexBuffer = context->createBuffer(bufferDesc);
    meshVertexBuffer->writeData(0,sizeof(vertices), vertices);

    bufferDesc.size = sizeof(unsigned int) * 6;
    bufferDesc.type = Blast::RESOURCE_TYPE_INDEX_BUFFER;
    bufferDesc.usage = Blast::RESOURCE_USAGE_CPU_TO_GPU;
    meshIndexBuffer = context->createBuffer(bufferDesc);
    meshIndexBuffer->writeData(0, sizeof(indices), indices);

    Blast::GfxRootSignatureDesc rootSignatureDesc;
    rootSignatureDesc.stages = Blast::SHADER_STAGE_VERT | Blast::SHADER_STAGE_FRAG;

    {
        Blast::ShaderCompileDesc compileDesc;
        compileDesc.code = readFileData(projectDir + "/Resource/triangle.vert");
        compileDesc.stage = Blast::SHADER_STAGE_VERT;
        Blast::ShaderCompileResult compileResult = shaderCompiler->compile(compileDesc);
        rootSignatureDesc.vertex = compileResult.reflection;

        Blast::GfxShaderDesc shaderDesc;
        shaderDesc.stage = Blast::SHADER_STAGE_VERT;
        shaderDesc.bytes = compileResult.bytes;
        vertShader = context->createShader(shaderDesc);
    }

    {
        Blast::ShaderCompileDesc compileDesc;
        compileDesc.code = readFileData(projectDir + "/Resource/triangle.frag");
        compileDesc.stage = Blast::SHADER_STAGE_FRAG;
        Blast::ShaderCompileResult compileResult = shaderCompiler->compile(compileDesc);
        rootSignatureDesc.pixel = compileResult.reflection;

        Blast::GfxShaderDesc shaderDesc;
        shaderDesc.stage = Blast::SHADER_STAGE_FRAG;
        shaderDesc.bytes = compileResult.bytes;
        fragShader = context->createShader(shaderDesc);
    }
    rootSignature = context->createRootSignature(rootSignatureDesc);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "BlastExample", nullptr, nullptr);
    void* windowPtr = glfwGetWin32Window(window);

    Blast::GfxSwapchainDesc swapchainDesc;
    swapchainDesc.width = 800;
    swapchainDesc.height = 600;
    swapchainDesc.windowHandle = windowPtr;
    swapchain = context->createSwapchain(swapchainDesc);
    imageCount = swapchain->getImageCount();

    queue = context->getQueue(Blast::QUEUE_TYPE_GRAPHICS);
    renderCompleteFences = new Blast::GfxFence*[imageCount];
    imageAcquiredSemaphores = new Blast::GfxSemaphore*[imageCount];
    renderCompleteSemaphores = new Blast::GfxSemaphore*[imageCount];
    Blast::GfxCommandBufferPoolDesc cmdPoolDesc;
    cmdPoolDesc.queue = queue;
    cmdPoolDesc.transient = false;
    cmdPool = context->createCommandBufferPool(cmdPoolDesc);
    cmds = new Blast::GfxCommandBuffer*[imageCount];
    renderPasses = new Blast::GfxRenderPass*[imageCount];
    for (int i = 0; i < imageCount; ++i) {
        // sync
        renderCompleteFences[i] = context->createFence();
        imageAcquiredSemaphores[i] = context->createSemaphore();
        renderCompleteSemaphores[i] = context->createSemaphore();

        // renderPassws
        Blast::GfxRenderPassDesc renderPassDesc;
        renderPassDesc.numColorAttachments = 1;
        renderPassDesc.colors[0].target = swapchain->getColorRenderTarget(i);
        renderPassDesc.colors[0].loadOp = Blast::LOAD_ACTION_CLEAR;
        renderPassDesc.hasDepthStencil = true;
        renderPassDesc.depthStencil.target = swapchain->getDepthRenderTarget(i);
        renderPassDesc.depthStencil.depthLoadOp = Blast::LOAD_ACTION_CLEAR;
        renderPassDesc.depthStencil.stencilLoadOp = Blast::LOAD_ACTION_CLEAR;
        renderPassDesc.width = 800;
        renderPassDesc.height = 600;
        renderPasses[i] = context->createRenderPass(renderPassDesc);

        // cmd
        cmds[i] = cmdPool->allocBuf(false);
    }

    Blast::GfxVertexLayout vertexLayout = {};
    vertexLayout.attribCount = 2;
    vertexLayout.attribs[0].semantic = Blast::SEMANTIC_POSITION;
    vertexLayout.attribs[0].format = Blast::FORMAT_R32G32B32_FLOAT;
    vertexLayout.attribs[0].binding = 0;
    vertexLayout.attribs[0].location = 0;
    vertexLayout.attribs[0].offset = 0;

    vertexLayout.attribs[1].semantic = Blast::SEMANTIC_TEXCOORD0;
    vertexLayout.attribs[1].format = Blast::FORMAT_R32G32_FLOAT;
    vertexLayout.attribs[1].binding = 0;
    vertexLayout.attribs[1].location = 1;
    vertexLayout.attribs[1].offset = offsetof(Vertex, uv);

    Blast::GfxBlendState blendState = {};
    blendState.srcFactors[0] = Blast::BLEND_ONE;
    blendState.dstFactors[0] = Blast::BLEND_ZERO;
    blendState.srcAlphaFactors[0] = Blast::BLEND_ONE;
    blendState.dstAlphaFactors[0] = Blast::BLEND_ZERO;
    blendState.masks[0] = 0xf;

    Blast::GfxDepthState depthState = {};
    depthState.depthTest = true;
    depthState.depthWrite = true;

    Blast::GfxRasterizerState rasterizerState = {};
    rasterizerState.cullMode = Blast::CULL_MODE_BACK;
    rasterizerState.frontFace = Blast::FRONT_FACE_CW; // 不再使用gl默认的ccw
    rasterizerState.fillMode = Blast::FILL_MODE_SOLID;

    Blast::GfxGraphicsPipelineDesc pipelineDesc;
    pipelineDesc.renderPass = renderPasses[0];
    pipelineDesc.rootSignature = rootSignature;
    pipelineDesc.vertexShader = vertShader;
    pipelineDesc.pixelShader = fragShader;
    pipelineDesc.vertexLayout = &vertexLayout;
    pipelineDesc.blendState = &blendState;
    pipelineDesc.depthState = &depthState;
    pipelineDesc.rasterizerState = &rasterizerState;
    pipeline = context->createGraphicsPipeline(pipelineDesc);

    while (!glfwWindowShouldClose(window)) {
        uint32_t swapchainImageIndex;
        context->acquireNextImage(swapchain, imageAcquiredSemaphores[frameIndex], nullptr, &swapchainImageIndex);

        renderCompleteFences[frameIndex]->waitForComplete();
        renderCompleteFences[frameIndex]->reset();

        // render
        Blast::GfxTexture* colorRT = swapchain->getColorRenderTarget(frameIndex);
        Blast::GfxTexture* depthRT = swapchain->getDepthRenderTarget(frameIndex);

        cmds[frameIndex]->begin();
        {
            // 设置交换链RT为可写状态
            Blast::GfxTextureBarrier barriers[2];
            barriers[0].texture = colorRT;
            barriers[0].newState = Blast::RESOURCE_STATE_RENDER_TARGET;
            barriers[1].texture = depthRT;
            barriers[1].newState = Blast::RESOURCE_STATE_DEPTH_WRITE;
            cmds[frameIndex]->setBarrier(0, nullptr, 2, barriers);
        }
        Blast::GfxClearValue clearValue;
        clearValue.color[0] = 1.0f;
        clearValue.color[1] = 0.0f;
        clearValue.color[2] = 0.0f;
        clearValue.color[3] = 1.0f;
        clearValue.depth = 1.0f;
        clearValue.stencil = 0;
        cmds[frameIndex]->bindRenderPass(renderPasses[frameIndex], clearValue);
        cmds[frameIndex]->unbindRenderPass();

        {
            // 设置交换链RT为显示状态
            Blast::GfxTextureBarrier barriers[2];
            barriers[0].texture = colorRT;
            barriers[0].newState = Blast::RESOURCE_STATE_PRESENT ;
            barriers[1].texture = depthRT;
            barriers[1].newState = Blast::RESOURCE_STATE_DEPTH_WRITE ;
            cmds[frameIndex]->setBarrier(0, nullptr, 2, barriers);
        }
        cmds[frameIndex]->end();

        Blast::GfxSubmitInfo submitInfo;
        submitInfo.cmdBufCount = 1;
        submitInfo.cmdBufs = &cmds[frameIndex];
        submitInfo.signalFence = renderCompleteFences[frameIndex];
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.waitSemaphores = &imageAcquiredSemaphores[frameIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.signalSemaphores = &renderCompleteSemaphores[frameIndex];
        queue->submit(submitInfo);

        Blast::GfxPresentInfo presentInfo;
        presentInfo.swapchain = swapchain;
        presentInfo.index = swapchainImageIndex;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.waitSemaphores = &renderCompleteSemaphores[frameIndex];
        queue->present(presentInfo);

        frameIndex = (frameIndex + 1) % imageCount;

        glfwPollEvents();
    }
    glfwDestroyWindow(window);
    glfwTerminate();

    shutdown();
    return 0;
}

std::string readFileData(const std::string& path) {
    std::istream* stream = &std::cin;
    std::ifstream file;

    file.open(path, std::ios_base::binary);
    stream = &file;
    if (file.fail()) {
        BLAST_LOGW("cannot open input file %s \n", path.c_str());
        return std::string("");
    }
    return std::string((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
}