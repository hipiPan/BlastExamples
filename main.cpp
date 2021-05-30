#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <Blast/Gfx/GfxContext.h>
#include <Blast/Gfx/GfxBuffer.h>
#include <Blast/Gfx/GfxTexture.h>
#include <Blast/Gfx/GfxSampler.h>
#include <Blast/Gfx/GfxSwapchain.h>
#include <Blast/Gfx/GfxCommandBuffer.h>
#include <Blast/Gfx/GfxRenderTarget.h>
#include <Blast/Gfx/GfxShader.h>
#include <Blast/Gfx/GfxPipeline.h>
#include <Blast/Gfx/Vulkan/VulkanContext.h>
#include <Blast/Utility/ShaderCompiler.h>
#include <Blast/Utility/VulkanShaderCompiler.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <fstream>
#include <sstream>

#define SAFE_DELETE(x) \
    { \
        delete x; \
        x = nullptr; \
    }

#define SAFE_DELETE_ARRAY(x) \
    { \
        delete[] x; \
        x = nullptr; \
    }

Blast::ShaderCompiler* shaderCompiler = nullptr;
Blast::GfxContext* context = nullptr;
Blast::GfxSurface* surface = nullptr;
Blast::GfxSwapchain* swapchain = nullptr;
Blast::GfxRenderPass* renderPass = nullptr;
Blast::GfxFramebuffer** framebuffers = nullptr;
Blast::GfxQueue* queue = nullptr;
Blast::GfxFence** renderCompleteFences = nullptr;
Blast::GfxSemaphore** imageAcquiredSemaphores = nullptr;
Blast::GfxSemaphore** renderCompleteSemaphores = nullptr;
Blast::GfxCommandBufferPool* cmdPool = nullptr;
Blast::GfxCommandBuffer** cmds = nullptr;
Blast::GfxShader* vertShader = nullptr;
Blast::GfxShader* fragShader = nullptr;
Blast::GfxDescriptorSet* descriptorSet = nullptr;
Blast::GfxRootSignature* rootSignature = nullptr;
Blast::GfxGraphicsPipeline* pipeline = nullptr;
Blast::GfxBuffer* meshIndexBuffer = nullptr;
Blast::GfxBuffer* meshVertexBuffer = nullptr;
Blast::GfxTexture* texture = nullptr;
Blast::GfxSampler* sampler = nullptr;
uint32_t frameIndex = 0;
uint32_t imageCount = 0;

static std::string projectDir(PROJECT_DIR);

static std::string readFileData(const std::string& path);

static void refreshSwapchain(uint32_t width, uint32_t height);

struct Vertex {
    float pos[3];
    float uv[2];
};

float vertices[] = {
        -0.5f,  -0.5f, 0.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 1.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
        -0.5f, 0.5f, 0.0f, 0.0f, 1.0f
};

unsigned int indices[] = {
        0, 1, 2, 2, 3, 0
};

void shutdown() {
    if (sampler) {
        delete sampler;
    }

    if (texture) {
        delete texture;
    }

    if (meshIndexBuffer) {
        delete meshIndexBuffer;
    }

    if (meshVertexBuffer) {
        delete meshVertexBuffer;
    }

    if (vertShader) {
        delete vertShader;
    }

    if (fragShader) {
        delete fragShader;
    }

    if (descriptorSet) {
        delete descriptorSet;
    }

    if (rootSignature) {
        delete rootSignature;
    }

    if (pipeline) {
        delete pipeline;
    }

    if (cmds) {
        delete[] cmds;
    }
    
    if (cmdPool) {
        delete cmdPool;
    }
    
    if (renderCompleteFences) {
        delete[] renderCompleteFences;
    }

    if (imageAcquiredSemaphores) {
        delete[] imageAcquiredSemaphores;
    }

    if (renderCompleteSemaphores) {
        delete[] renderCompleteSemaphores;
    }

    if (renderPass) {
        delete renderPass;
    }

    if (framebuffers) {
        delete[] framebuffers;
    }

    if (swapchain) {
        delete swapchain;
    }

    if (surface) {
        delete surface;
    }

    if (context) {
        delete context;
    }

    if (shaderCompiler) {
        delete shaderCompiler;
    }
}

int main() {
    context = new Blast::VulkanContext();

    shaderCompiler = new Blast::VulkanShaderCompiler();

    Blast::GfxRootSignatureDesc rootSignatureDesc;

    {
        Blast::ShaderCompileDesc compileDesc;
        compileDesc.code = readFileData(projectDir + "/Resource/texture.vert");
        compileDesc.stage = Blast::SHADER_STAGE_VERT;
        Blast::ShaderCompileResult compileResult = shaderCompiler->compile(compileDesc);
        Blast::GfxShaderDesc shaderDesc;
        shaderDesc.stage = Blast::SHADER_STAGE_VERT;
        shaderDesc.bytes = compileResult.bytes;
        vertShader = context->createShader(shaderDesc);
    }

    {
        Blast::ShaderCompileDesc compileDesc;
        compileDesc.code = readFileData(projectDir + "/Resource/texture.frag");
        compileDesc.stage = Blast::SHADER_STAGE_FRAG;
        Blast::ShaderCompileResult compileResult = shaderCompiler->compile(compileDesc);
        Blast::GfxShaderDesc shaderDesc;
        shaderDesc.stage = Blast::SHADER_STAGE_FRAG;
        shaderDesc.bytes = compileResult.bytes;
        fragShader = context->createShader(shaderDesc);
    }
    // Root Signature布局
    Blast::GfxRegisterInfo registerInfo;
    registerInfo.set = 0;
    registerInfo.reg = 0;
    registerInfo.size = 1;
    registerInfo.type = Blast::RESOURCE_TYPE_UNIFORM_BUFFER;
    rootSignatureDesc.registers.push_back(registerInfo);

    registerInfo.set = 0;
    registerInfo.reg = 1;
    registerInfo.size = 1;
    registerInfo.type = Blast::RESOURCE_TYPE_TEXTURE;
    rootSignatureDesc.registers.push_back(registerInfo);

    registerInfo.set = 0;
    registerInfo.reg = 2;
    registerInfo.size = 1;
    registerInfo.type = Blast::RESOURCE_TYPE_SAMPLER;
    rootSignatureDesc.registers.push_back(registerInfo);

    rootSignature = context->createRootSignature(rootSignatureDesc);

    descriptorSet = rootSignature->allocateSet(0);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "BlastExample", nullptr, nullptr);
    void* windowPtr = glfwGetWin32Window(window);

    Blast::GfxSurfaceDesc surfaceDesc;
    surfaceDesc.originSurface = windowPtr;
    surface = context->createSurface(surfaceDesc);

    Blast::GfxRenderPassDesc renderPassDesc;
    renderPassDesc.numColorAttachments = 1;
    renderPassDesc.colors[0].format = surface->getFormat();
    renderPassDesc.colors[0].loadOp = Blast::LOAD_ACTION_CLEAR;
    renderPassDesc.hasDepthStencil = true;
    renderPassDesc.depthStencil.format = Blast::FORMAT_D24_UNORM_S8_UINT;
    renderPassDesc.depthStencil.depthLoadOp = Blast::LOAD_ACTION_CLEAR;
    renderPassDesc.depthStencil.stencilLoadOp = Blast::LOAD_ACTION_CLEAR;
    renderPass = context->createRenderPass(renderPassDesc);

    Blast::GfxSwapchainDesc swapchainDesc;
    swapchainDesc.width = 800;
    swapchainDesc.height = 600;
    swapchainDesc.surface = surface;
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
    framebuffers = new Blast::GfxFramebuffer*[imageCount];
    for (int i = 0; i < imageCount; ++i) {
        // sync
        renderCompleteFences[i] = context->createFence();
        imageAcquiredSemaphores[i] = context->createSemaphore();
        renderCompleteSemaphores[i] = context->createSemaphore();

        // renderPassws
        Blast::GfxFramebufferDesc framebufferDesc;
        framebufferDesc.renderPass = renderPass;
        framebufferDesc.numColorAttachments = 1;
        framebufferDesc.colors[0].target = swapchain->getColorRenderTarget(i);
        framebufferDesc.hasDepthStencil = true;
        framebufferDesc.depthStencil.target = swapchain->getDepthRenderTarget(i);
        framebufferDesc.width = 800;
        framebufferDesc.height = 600;
        framebuffers[i] = context->createFramebuffer(framebufferDesc);

        // cmd
        cmds[i] = cmdPool->allocBuf(false);
    }

    // load resource
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

    int texWidth, texHeight, texChannels;
    std::string imagePath = projectDir + "/Resource/test.png";
    unsigned char* pixels = stbi_load(imagePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    uint32_t imageSize = texWidth * texHeight * texChannels;
    bufferDesc.size = imageSize;
    bufferDesc.type = Blast::RESOURCE_TYPE_RW_BUFFER;
    bufferDesc.usage = Blast::RESOURCE_USAGE_CPU_TO_GPU;
    Blast::GfxBuffer* stagingBuffer = context->createBuffer(bufferDesc);
    stagingBuffer->writeData(0, imageSize, pixels);
    stbi_image_free(pixels);

    Blast::GfxTextureDesc textureDesc;
    textureDesc.width = texWidth;
    textureDesc.height = texHeight;
    textureDesc.format = Blast::FORMAT_R8G8B8A8_UNORM;
    textureDesc.type = Blast::RESOURCE_TYPE_TEXTURE | Blast::RESOURCE_TYPE_RW_TEXTURE;
    textureDesc.usage = Blast::RESOURCE_USAGE_GPU_ONLY;
    texture = context->createTexture(textureDesc);

    Blast::GfxCommandBuffer* copyCmd = cmdPool->allocBuf(false);
    copyCmd->begin();
    {
        // 设置纹理为读写状态
        Blast::GfxTextureBarrier barrier;
        barrier.texture = texture;
        barrier.newState = Blast::RESOURCE_STATE_COPY_DEST;
        copyCmd->setBarrier(0, nullptr, 1, &barrier);
    }
    Blast::GfxCopyToImageHelper helper;
    helper.bufferOffset = 0;
    helper.layer = 0;
    helper.level = 0;
    copyCmd->copyToImage(stagingBuffer, texture, helper);
    {
        // 设置纹理为Shader可读状态
        Blast::GfxTextureBarrier barrier;
        barrier.texture = texture;
        barrier.newState = Blast::RESOURCE_STATE_SHADER_RESOURCE;
        copyCmd->setBarrier(0, nullptr, 1, &barrier);
    }
    copyCmd->end();

    Blast::GfxSubmitInfo submitInfo;
    submitInfo.cmdBufCount = 1;
    submitInfo.cmdBufs = &copyCmd;
    submitInfo.signalFence = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.waitSemaphores = nullptr;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.signalSemaphores = nullptr;
    queue->submit(submitInfo);
    delete stagingBuffer;

    Blast::GfxSamplerDesc samplerDesc = {};
    sampler = context->createSampler(samplerDesc);

    descriptorSet->setTexture(1, texture);
    descriptorSet->setSampler(2, sampler);

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

    vertexLayout.attribs[2].semantic = Blast::SEMANTIC_TEXCOORD1;
    vertexLayout.attribs[2].format = Blast::FORMAT_R32G32_FLOAT;
    vertexLayout.attribs[2].binding = 0;
    vertexLayout.attribs[2].location = 2;
    vertexLayout.attribs[2].offset = offsetof(Vertex, uv);

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
    rasterizerState.primitiveTopo = Blast::PRIMITIVE_TOPO_TRI_LIST;

    Blast::GfxGraphicsPipelineDesc pipelineDesc;
    pipelineDesc.renderPass = renderPass;
    pipelineDesc.rootSignature = rootSignature;
    pipelineDesc.vertexShader = vertShader;
    pipelineDesc.pixelShader = fragShader;
    pipelineDesc.vertexLayout = vertexLayout;
    pipelineDesc.blendState = blendState;
    pipelineDesc.depthState = depthState;
    pipelineDesc.rasterizerState = rasterizerState;
    pipeline = context->createGraphicsPipeline(pipelineDesc);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        uint32_t swapchainImageIndex = 0;
        context->acquireNextImage(swapchain, imageAcquiredSemaphores[frameIndex], nullptr, &swapchainImageIndex);
        if (swapchainImageIndex == -1) {
            Blast::GfxSurfaceSize size = surface->getSize();
            refreshSwapchain(size.width, size.height);
            continue;
        }

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
        cmds[frameIndex]->bindRenderTarget(renderPass, framebuffers[frameIndex], clearValue);
        cmds[frameIndex]->setViewport(0.0, 0.0, colorRT->getWidth(), colorRT->getHeight());
        cmds[frameIndex]->setScissor(0, 0, colorRT->getWidth(), colorRT->getHeight());
        cmds[frameIndex]->bindGraphicsPipeline(pipeline);
        cmds[frameIndex]->bindRootSignature(rootSignature);
        cmds[frameIndex]->bindDescriptorSets(1, &descriptorSet);
        cmds[frameIndex]->bindVertexBuffer(meshVertexBuffer, 0);
        cmds[frameIndex]->bindIndexBuffer(meshIndexBuffer, 0, Blast::INDEX_TYPE_UINT32);
        cmds[frameIndex]->drawIndexed(6, 1, 0, 0, 0);
        cmds[frameIndex]->unbindRenderTarget();

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
    }
    glfwDestroyWindow(window);
    glfwTerminate();

    shutdown();
    return 0;
}

void refreshSwapchain(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0)
        return;
    queue->waitIdle();
    Blast::GfxSwapchain* oldSwapchain = swapchain;
    Blast::GfxSwapchainDesc swapchainDesc;
    swapchainDesc.surface = surface;
    swapchainDesc.width = width;
    swapchainDesc.height = height;
    swapchainDesc.oldSwapchain = oldSwapchain;
    swapchain = context->createSwapchain(swapchainDesc);

    SAFE_DELETE(oldSwapchain);
    SAFE_DELETE_ARRAY(cmds);
    SAFE_DELETE_ARRAY(renderCompleteFences);
    SAFE_DELETE_ARRAY(imageAcquiredSemaphores);
    SAFE_DELETE_ARRAY(renderCompleteSemaphores);
    SAFE_DELETE_ARRAY(framebuffers);

    imageCount = swapchain->getImageCount();
    renderCompleteFences = new Blast::GfxFence*[imageCount];
    imageAcquiredSemaphores = new Blast::GfxSemaphore*[imageCount];
    renderCompleteSemaphores = new Blast::GfxSemaphore*[imageCount];
    cmds = new Blast::GfxCommandBuffer*[imageCount];
    framebuffers = new Blast::GfxFramebuffer*[imageCount];
    for (int i = 0; i < imageCount; ++i) {
        // sync
        renderCompleteFences[i] = context->createFence();
        imageAcquiredSemaphores[i] = context->createSemaphore();
        renderCompleteSemaphores[i] = context->createSemaphore();

        // renderPassws
        Blast::GfxFramebufferDesc framebufferDesc;
        framebufferDesc.renderPass = renderPass;
        framebufferDesc.numColorAttachments = 1;
        framebufferDesc.colors[0].target = swapchain->getColorRenderTarget(i);
        framebufferDesc.hasDepthStencil = true;
        framebufferDesc.depthStencil.target = swapchain->getDepthRenderTarget(i);
        framebufferDesc.width = swapchain->getColorRenderTarget(i)->getWidth();
        framebufferDesc.height = swapchain->getColorRenderTarget(i)->getHeight();
        framebuffers[i] = context->createFramebuffer(framebufferDesc);

        // cmd
        cmds[i] = cmdPool->allocBuf(false);

        // set present format
        Blast::GfxTexture* colorRT = swapchain->getColorRenderTarget(i);
        Blast::GfxTexture* depthRT = swapchain->getDepthRenderTarget(i);
        cmds[i]->begin();
        {
            // 设置交换链RT为显示状态
            Blast::GfxTextureBarrier barriers[2];
            barriers[0].texture = colorRT;
            barriers[0].newState = Blast::RESOURCE_STATE_PRESENT ;
            barriers[1].texture = depthRT;
            barriers[1].newState = Blast::RESOURCE_STATE_DEPTH_WRITE ;
            cmds[i]->setBarrier(0, nullptr, 2, barriers);
        }
        cmds[i]->end();
    }
    Blast::GfxSubmitInfo submitInfo;
    submitInfo.cmdBufCount = imageCount;
    submitInfo.cmdBufs = cmds;
    submitInfo.signalFence = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.waitSemaphores = nullptr;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.signalSemaphores = nullptr;
    queue->submit(submitInfo);
    queue->waitIdle();
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