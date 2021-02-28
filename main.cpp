#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <Gfx/GfxContext.h>
#include <Gfx/GfxBuffer.h>
#include <Gfx/GfxSwapchain.h>
#include "Gfx/GfxCommandBuffer.h"
#include "Gfx/GfxRenderPass.h"
#include <Gfx/Vulkan/VulkanContext.h>

Blast::GfxContext* context = nullptr;
Blast::GfxSwapchain* swapchain = nullptr;
Blast::GfxQueue* queue = nullptr;
Blast::GfxFence** renderCompleteFences = nullptr;
Blast::GfxSemaphore** imageAcquiredSemaphores = nullptr;
Blast::GfxSemaphore** renderCompleteSemaphores = nullptr;
Blast::GfxCommandBufferPool* cmdPool = nullptr;
Blast::GfxCommandBuffer** cmds = nullptr;
uint32_t frameIndex = 0;
uint32_t imageCount = 0;

void shutdown() {
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

    if (swapchain) {
        delete swapchain;
    }

    if (context) {
        delete context;
    }
}

int main() {
    context = new Blast::VulkanContext();

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

    queue = context->getQueue(Blast::QUEUE_TYPE_GRAPHICS);

    imageCount = swapchain->getImageCount();
    renderCompleteFences = new Blast::GfxFence*[imageCount];
    imageAcquiredSemaphores = new Blast::GfxSemaphore*[imageCount];
    renderCompleteSemaphores = new Blast::GfxSemaphore*[imageCount];
    Blast::GfxCommandBufferPoolDesc cmdPoolDesc;
    cmdPoolDesc.queue = queue;
    cmdPoolDesc.transient = false;
    cmdPool = context->createCommandBufferPool(cmdPoolDesc);
    cmds = new Blast::GfxCommandBuffer*[imageCount];
    for (int i = 0; i < imageCount; ++i) {
        renderCompleteFences[i] = context->createFence();
        imageAcquiredSemaphores[i] = context->createSemaphore();
        renderCompleteSemaphores[i] = context->createSemaphore();
        cmds[i] = cmdPool->allocBuf(false);
    }

    while (!glfwWindowShouldClose(window)) {
        uint32_t swapchainImageIndex;
        context->acquireNextImage(swapchain, imageAcquiredSemaphores[frameIndex], nullptr, &swapchainImageIndex);

        renderCompleteFences[frameIndex]->waitForComplete();
        renderCompleteFences[frameIndex]->reset();

        // render
        Blast::GfxRenderPass* renderPass = swapchain->getRenderPass(frameIndex);
        Blast::GfxTexture* colorRT = renderPass->getColorRT(0);
        Blast::GfxTexture* depthRT = renderPass->getDepthRT();

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
        cmds[frameIndex]->bindRenderPass(renderPass, clearValue);
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