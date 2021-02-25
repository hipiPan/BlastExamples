#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <Gfx/GfxContext.h>
#include <Gfx/GfxBuffer.h>
#include <Gfx/GfxSwapchain.h>
#include "Gfx/GfxCommandBuffer.h"
#include <Gfx/Vulkan/VulkanContext.h>

Blast::GfxContext* context = nullptr;
Blast::GfxSwapchain* swapchain = nullptr;
uint32_t imageCount = 1;
Blast::GfxFence** renderCompleteFences = nullptr;
Blast::GfxSemaphore** imageAcquiredSemaphores = nullptr;
Blast::GfxSemaphore** renderCompleteSemaphores = nullptr;
Blast::GfxCommandBufferPool* cmdPool = nullptr;
Blast::GfxCommandBuffer** cmds = nullptr;

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

    imageCount = swapchain->getImageCount();
    renderCompleteFences = new Blast::GfxFence*[imageCount];
    imageAcquiredSemaphores = new Blast::GfxSemaphore*[imageCount];
    renderCompleteSemaphores = new Blast::GfxSemaphore*[imageCount];
    Blast::GfxCommandBufferPoolDesc cmdPoolDesc;
    cmdPoolDesc.queue = context->getQueue(Blast::QUEUE_TYPE_GRAPHICS);
    cmdPoolDesc.transient = false;
    for (int i = 0; i < imageCount; ++i) {
        renderCompleteFences[i] = context->createFence();
        imageAcquiredSemaphores[i] = context->createSemaphore();
        renderCompleteSemaphores[i] = context->createSemaphore();
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
    glfwDestroyWindow(window);
    glfwTerminate();

    shutdown();
    return 0;
}