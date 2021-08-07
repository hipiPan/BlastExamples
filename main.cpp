#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <Blast/Gfx/GfxContext.h>
#include <Blast/Gfx/GfxBuffer.h>
#include <Blast/Gfx/GfxTexture.h>
#include <Blast/Gfx/GfxSampler.h>
#include <Blast/Gfx/GfxSwapchain.h>
#include <Blast/Gfx/GfxCommandBuffer.h>
#include <Blast/Gfx/GfxFramebuffer.h>
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

blast::ShaderCompiler* g_shader_compiler = nullptr;
blast::GfxContext* g_context = nullptr;
blast::GfxSurface* g_surface = nullptr;
blast::GfxSwapchain* g_swapchain = nullptr;
blast::GfxTextureView** g_screen_color_views = nullptr;
blast::GfxTextureView** g_screen_depth_views = nullptr;
blast::GfxFramebuffer** g_framebuffers = nullptr;
blast::GfxQueue* g_queue = nullptr;
blast::GfxFence** g_render_complete_fences = nullptr;
blast::GfxSemaphore** g_image_acquired_semaphores = nullptr;
blast::GfxSemaphore** g_render_complete_semaphores = nullptr;
blast::GfxCommandBufferPool* g_cmd_pool = nullptr;
blast::GfxCommandBuffer** g_cmds = nullptr;
blast::GfxShader* g_vert_shader = nullptr;
blast::GfxShader* g_frag_shader = nullptr;
blast::GfxDescriptorSet* g_descriptor_set = nullptr;
blast::GfxRootSignature* g_root_signature = nullptr;
blast::GfxGraphicsPipeline* g_pipeline = nullptr;
blast::GfxBuffer* g_mesh_index_buffer = nullptr;
blast::GfxBuffer* g_mesh_vertex_buffer = nullptr;
blast::GfxBuffer* staging_buffer = nullptr;
blast::GfxTexture* g_texture = nullptr;
blast::GfxTextureView* g_texture_view = nullptr;
blast::GfxSampler* g_sampler = nullptr;
uint32_t frame_index = 0;
uint32_t num_images = 0;
uint32_t frame_width = 0;
uint32_t frame_height = 0;

static std::string ProjectDir(PROJECT_DIR);

static std::string ReadFileData(const std::string& path);

static void RefreshSwapchain(uint32_t width, uint32_t height);

struct Vertex {
    float pos[3];
    float uv[2];
};

float vertices[] = {
        -0.5f,  -0.5f, 0.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
        -0.5f, 0.5f, 0.0f, 0.0f, 1.0f
};

unsigned int indices[] = {
        0, 1, 2, 2, 3, 0
};

void shutdown() {
    g_queue->WaitIdle();

    g_context->DestroySampler(g_sampler);

    g_context->DestroyTextureView(g_texture_view);

    g_context->DestroyTexture(g_texture);

    g_context->DestroyBuffer(g_mesh_index_buffer);

    g_context->DestroyBuffer(g_mesh_vertex_buffer);

    g_context->DestroyBuffer(staging_buffer);

    g_context->DestroyShader(g_vert_shader);

    g_context->DestroyShader(g_frag_shader);

    g_root_signature->DeleteSet(g_descriptor_set);

    g_context->DestroyRootSignature(g_root_signature);

    g_context->DestroyGraphicsPipeline(g_pipeline);

    for (uint32_t i = 0; i < num_images; ++i) {
        g_cmd_pool->DeleteBuffer(g_cmds[i]);
        g_context->DestroySemaphore(g_image_acquired_semaphores[i]);
        g_context->DestroySemaphore(g_render_complete_semaphores[i]);
        g_context->DestroyFence(g_render_complete_fences[i]);
        g_context->DestroyFramebuffer(g_framebuffers[i]);
        g_context->DestroyTextureView(g_screen_color_views[i]);
        g_context->DestroyTextureView(g_screen_depth_views[i]);
    }

    g_context->DestroyCommandBufferPool(g_cmd_pool);

    g_context->DestroySwapchain(g_swapchain);

    g_context->DestroySurface(g_surface);

    SAFE_DELETE(g_context);

    SAFE_DELETE(g_shader_compiler);
}

int main() {
    g_context = new blast::VulkanContext();

    g_shader_compiler = new blast::VulkanShaderCompiler();

    blast::GfxRootSignatureDesc root_signature_desc;
    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(ProjectDir + "/Resource/texture.vert");
        compile_desc.stage = blast::SHADER_STAGE_VERT;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_VERT;
        shader_desc.bytes = compile_result.bytes;
        g_vert_shader = g_context->CreateShader(shader_desc);
    }

    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(ProjectDir + "/Resource/texture.frag");
        compile_desc.stage = blast::SHADER_STAGE_FRAG;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_FRAG;
        shader_desc.bytes = compile_result.bytes;
        g_frag_shader = g_context->CreateShader(shader_desc);
    }
    // Root Signature布局
    blast::GfxRegisterInfo register_info;
    register_info.set = 0;
    register_info.reg = 0;
    register_info.size = 1;
    register_info.type = blast::RESOURCE_TYPE_UNIFORM_BUFFER;
    root_signature_desc.registers.push_back(register_info);

    register_info.set = 0;
    register_info.reg = 1;
    register_info.size = 1;
    register_info.type = blast::RESOURCE_TYPE_TEXTURE;
    root_signature_desc.registers.push_back(register_info);

    register_info.set = 0;
    register_info.reg = 2;
    register_info.size = 1;
    register_info.type = blast::RESOURCE_TYPE_SAMPLER;
    root_signature_desc.registers.push_back(register_info);

    g_root_signature = g_context->CreateRootSignature(root_signature_desc);

    g_descriptor_set = g_root_signature->AllocateSet(0);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "BlastExample", nullptr, nullptr);

    blast::GfxSurfaceDesc surface_desc;
    surface_desc.origin_surface = glfwGetWin32Window(window);
    g_surface = g_context->CreateSurface(surface_desc);

    g_queue = g_context->GetQueue(blast::QUEUE_TYPE_GRAPHICS);

    blast::GfxCommandBufferPoolDesc cmd_pool_desc;
    cmd_pool_desc.queue = g_queue;
    cmd_pool_desc.transient = false;
    g_cmd_pool = g_context->CreateCommandBufferPool(cmd_pool_desc);

    // load resource
    blast::GfxBufferDesc buffer_desc;
    buffer_desc.size = sizeof(Vertex) * 4;
    buffer_desc.type = blast::RESOURCE_TYPE_VERTEX_BUFFER;
    buffer_desc.usage = blast::RESOURCE_USAGE_CPU_TO_GPU;
    g_mesh_vertex_buffer = g_context->CreateBuffer(buffer_desc);
    g_mesh_vertex_buffer->WriteData(0,sizeof(vertices), vertices);

    buffer_desc.size = sizeof(unsigned int) * 6;
    buffer_desc.type = blast::RESOURCE_TYPE_INDEX_BUFFER;
    buffer_desc.usage = blast::RESOURCE_USAGE_CPU_TO_GPU;
    g_mesh_index_buffer = g_context->CreateBuffer(buffer_desc);
    g_mesh_index_buffer->WriteData(0, sizeof(indices), indices);

    int tex_width, tex_height, tex_channels;
    std::string image_path = ProjectDir + "/Resource/test.png";
    unsigned char* pixels = stbi_load(image_path.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
    uint32_t imageSize = tex_width * tex_height * tex_channels;
    buffer_desc.size = imageSize;
    buffer_desc.type = blast::RESOURCE_TYPE_RW_BUFFER;
    buffer_desc.usage = blast::RESOURCE_USAGE_CPU_TO_GPU;
    staging_buffer = g_context->CreateBuffer(buffer_desc);
    staging_buffer->WriteData(0, imageSize, pixels);
    stbi_image_free(pixels);

    blast::GfxTextureDesc texture_desc;
    texture_desc.width = tex_width;
    texture_desc.height = tex_height;
    texture_desc.format = blast::FORMAT_R8G8B8A8_UNORM;
    texture_desc.type = blast::RESOURCE_TYPE_TEXTURE | blast::RESOURCE_TYPE_RW_TEXTURE;
    texture_desc.usage = blast::RESOURCE_USAGE_GPU_ONLY;
    g_texture = g_context->CreateTexture(texture_desc);

    blast::GfxCommandBuffer* copy_cmd = g_cmd_pool->AllocBuffer(false);
    copy_cmd->Begin();
    {
        // 设置纹理为读写状态
        blast::GfxTextureBarrier barrier;
        barrier.texture = g_texture;
        barrier.new_state = blast::RESOURCE_STATE_COPY_DEST;
        copy_cmd->SetBarrier(0, nullptr, 1, &barrier);
    }
    blast::GfxCopyToImageRange range;
    range.buffer_offset = 0;
    range.layer = 0;
    range.level = 0;
    copy_cmd->CopyToImage(staging_buffer, g_texture, range);
    {
        // 设置纹理为Shader可读状态
        blast::GfxTextureBarrier barrier;
        barrier.texture = g_texture;
        barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
        copy_cmd->SetBarrier(0, nullptr, 1, &barrier);
    }
    copy_cmd->End();

    blast::GfxSubmitInfo submit_info;
    submit_info.num_cmd_bufs = 1;
    submit_info.cmd_bufs = &copy_cmd;
    submit_info.signal_fence = nullptr;
    submit_info.num_wait_semaphores = 0;
    submit_info.wait_semaphores = nullptr;
    submit_info.num_signal_semaphores = 0;
    submit_info.signal_semaphores = nullptr;
    g_queue->Submit(submit_info);

    g_queue->WaitIdle();
    SAFE_DELETE(copy_cmd);

    blast::GfxSamplerDesc sampler_desc = {};
    g_sampler = g_context->CreateSampler(sampler_desc);

    blast::GfxTextureViewDesc view_desc = {};
    view_desc.level = 0;
    view_desc.layer = 0;
    view_desc.texture = g_texture;
    g_texture_view = g_context->CreateTextureView(view_desc);

    g_descriptor_set->SetTexture(1, g_texture_view);
    g_descriptor_set->SetSampler(2, g_sampler);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        int window_width, window_height;
        glfwGetWindowSize(window, &window_width, &window_height);
        if (frame_width != window_width || frame_height != window_height) {
            frame_width = window_width;
            frame_height = window_height;
            RefreshSwapchain(frame_width, frame_height);
        }

        uint32_t swapchain_image_index = 0;
        g_context->AcquireNextImage(g_swapchain, g_image_acquired_semaphores[frame_index], nullptr, &swapchain_image_index);
        if (swapchain_image_index == -1) {
            continue;
        }

        g_render_complete_fences[frame_index]->WaitForComplete();

        // render
        blast::GfxTexture* color_rt = g_swapchain->GetColorRenderTarget(frame_index);
        blast::GfxTexture* depth_rt = g_swapchain->GetDepthRenderTarget(frame_index);

        g_cmds[frame_index]->Begin();
        {
            // 设置交换链RT为可写状态
            blast::GfxTextureBarrier barriers[2];
            barriers[0].texture = color_rt;
            barriers[0].new_state = blast::RESOURCE_STATE_RENDER_TARGET;
            barriers[1].texture = depth_rt;
            barriers[1].new_state = blast::RESOURCE_STATE_DEPTH_WRITE;
            g_cmds[frame_index]->SetBarrier(0, nullptr, 2, barriers);
        }
        blast::GfxClearValue clear_value;
        clear_value.flags |= blast::CLEAR_COLOR | blast::CLEAR_DEPTH;
        clear_value.color[0] = 1.0f;
        clear_value.color[1] = 0.0f;
        clear_value.color[2] = 0.0f;
        clear_value.color[3] = 0.0f;
        clear_value.depth = 1.0f;
        g_cmds[frame_index]->BindFramebuffer(g_framebuffers[frame_index]);
        g_cmds[frame_index]->ClearFramebuffer(g_framebuffers[frame_index], clear_value);
        g_cmds[frame_index]->SetViewport(0.0, 0.0, frame_width, frame_height);
        g_cmds[frame_index]->SetScissor(0, 0, frame_width, frame_height);
        g_cmds[frame_index]->BindGraphicsPipeline(g_pipeline);
        g_cmds[frame_index]->BindDescriptorSets(g_root_signature, 1, &g_descriptor_set);
        g_cmds[frame_index]->BindVertexBuffer(g_mesh_vertex_buffer, 0);
        g_cmds[frame_index]->BindIndexBuffer(g_mesh_index_buffer, 0, blast::INDEX_TYPE_UINT32);
        g_cmds[frame_index]->DrawIndexed(6, 1, 0, 0, 0);
        g_cmds[frame_index]->UnbindFramebuffer();

        {
            // 设置交换链RT为显示状态
            blast::GfxTextureBarrier barriers[2];
            barriers[0].texture = color_rt;
            barriers[0].new_state = blast::RESOURCE_STATE_PRESENT ;
            barriers[1].texture = depth_rt;
            barriers[1].new_state = blast::RESOURCE_STATE_DEPTH_WRITE ;
            g_cmds[frame_index]->SetBarrier(0, nullptr, 2, barriers);
        }
        g_cmds[frame_index]->End();

        blast::GfxSubmitInfo submit_info;
        submit_info.num_cmd_bufs = 1;
        submit_info.cmd_bufs = &g_cmds[frame_index];
        submit_info.signal_fence = g_render_complete_fences[frame_index];
        submit_info.num_wait_semaphores = 1;
        submit_info.wait_semaphores = &g_image_acquired_semaphores[frame_index];
        submit_info.num_signal_semaphores = 1;
        submit_info.signal_semaphores = &g_render_complete_semaphores[frame_index];
        g_queue->Submit(submit_info);

        blast::GfxPresentInfo present_info;
        present_info.swapchain = g_swapchain;
        present_info.index = swapchain_image_index;
        present_info.num_wait_semaphores = 1;
        present_info.wait_semaphores = &g_render_complete_semaphores[frame_index];
        g_queue->Present(present_info);

        frame_index = (frame_index + 1) % num_images;
    }
    glfwDestroyWindow(window);
    glfwTerminate();

    shutdown();
    return 0;
}

void RefreshSwapchain(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0)
        return;

    g_queue->WaitIdle();

    blast::GfxSwapchain* old_swapchain = g_swapchain;
    blast::GfxSwapchainDesc swapchain_desc;
    swapchain_desc.surface = g_surface;
    swapchain_desc.width = width;
    swapchain_desc.height = height;
    swapchain_desc.old_swapchain = old_swapchain;
    g_swapchain = g_context->CreateSwapchain(swapchain_desc);

    g_context->DestroySwapchain(old_swapchain);

    for (uint32_t i = 0; i < num_images; ++i) {
        g_cmd_pool->DeleteBuffer(g_cmds[i]);
        g_context->DestroySemaphore(g_image_acquired_semaphores[i]);
        g_context->DestroySemaphore(g_render_complete_semaphores[i]);
        g_context->DestroyFence(g_render_complete_fences[i]);
        g_context->DestroyFramebuffer(g_framebuffers[i]);
        g_context->DestroyTextureView(g_screen_color_views[i]);
        g_context->DestroyTextureView(g_screen_depth_views[i]);
    }

    num_images = g_swapchain->GetImageCount();
    g_render_complete_fences = new blast::GfxFence*[num_images];
    g_image_acquired_semaphores = new blast::GfxSemaphore*[num_images];
    g_render_complete_semaphores = new blast::GfxSemaphore*[num_images];
    g_cmds = new blast::GfxCommandBuffer*[num_images];
    g_framebuffers = new blast::GfxFramebuffer*[num_images];
    g_screen_color_views = new blast::GfxTextureView*[num_images];
    g_screen_depth_views = new blast::GfxTextureView*[num_images];
    for (int i = 0; i < num_images; ++i) {
        // sync
        g_render_complete_fences[i] = g_context->CreateFence();
        g_image_acquired_semaphores[i] = g_context->CreateSemaphore();
        g_render_complete_semaphores[i] = g_context->CreateSemaphore();

        blast::GfxTextureViewDesc view_desc;
        view_desc.level = 0;
        view_desc.layer = 0;
        view_desc.texture = g_swapchain->GetColorRenderTarget(i);
        g_screen_color_views[i] = g_context->CreateTextureView(view_desc);
        view_desc.texture = g_swapchain->GetDepthRenderTarget(i);
        g_screen_depth_views[i] = g_context->CreateTextureView(view_desc);

        // framebuffers
        blast::GfxFramebufferDesc framebuffer_desc;
        framebuffer_desc.num_colors = 1;
        framebuffer_desc.colors[0] = g_screen_color_views[i];
        framebuffer_desc.has_depth_stencil = true;
        framebuffer_desc.depth_stencil = g_screen_depth_views[i];
        framebuffer_desc.width = width;
        framebuffer_desc.height = height;
        g_framebuffers[i] = g_context->CreateFramebuffer(framebuffer_desc);

        // cmd
        g_cmds[i] = g_cmd_pool->AllocBuffer(false);

        // set present format
        blast::GfxTexture* color_rt = g_swapchain->GetColorRenderTarget(i);
        blast::GfxTexture* depth_rt = g_swapchain->GetDepthRenderTarget(i);
        g_cmds[i]->Begin();
        {
            // 设置交换链RT为显示状态
            blast::GfxTextureBarrier barriers[2];
            barriers[0].texture = color_rt;
            barriers[0].new_state = blast::RESOURCE_STATE_PRESENT;
            barriers[1].texture = depth_rt;
            barriers[1].new_state = blast::RESOURCE_STATE_DEPTH_WRITE;
            g_cmds[i]->SetBarrier(0, nullptr, 2, barriers);
        }
        g_cmds[i]->End();
    }
    blast::GfxSubmitInfo submit_info;
    submit_info.num_cmd_bufs = num_images;
    submit_info.cmd_bufs = g_cmds;
    submit_info.signal_fence = nullptr;
    submit_info.num_wait_semaphores = 0;
    submit_info.wait_semaphores = nullptr;
    submit_info.num_signal_semaphores = 0;
    submit_info.signal_semaphores = nullptr;
    g_queue->Submit(submit_info);
    g_queue->WaitIdle();

    blast::GfxVertexLayout vertex_layout = {};
    vertex_layout.num_attributes = 2;
    vertex_layout.attributes[0].semantic = blast::SEMANTIC_POSITION;
    vertex_layout.attributes[0].format = blast::FORMAT_R32G32B32_FLOAT;
    vertex_layout.attributes[0].binding = 0;
    vertex_layout.attributes[0].location = 0;
    vertex_layout.attributes[0].offset = 0;

    vertex_layout.attributes[1].semantic = blast::SEMANTIC_TEXCOORD0;
    vertex_layout.attributes[1].format = blast::FORMAT_R32G32_FLOAT;
    vertex_layout.attributes[1].binding = 0;
    vertex_layout.attributes[1].location = 1;
    vertex_layout.attributes[1].offset = offsetof(Vertex, uv);

    vertex_layout.attributes[2].semantic = blast::SEMANTIC_TEXCOORD1;
    vertex_layout.attributes[2].format = blast::FORMAT_R32G32_FLOAT;
    vertex_layout.attributes[2].binding = 0;
    vertex_layout.attributes[2].location = 2;
    vertex_layout.attributes[2].offset = offsetof(Vertex, uv);

    blast::GfxBlendState blend_state = {};
    blend_state.src_factors[0] = blast::BLEND_ONE;
    blend_state.dst_factors[0] = blast::BLEND_ZERO;
    blend_state.src_alpha_factors[0] = blast::BLEND_ONE;
    blend_state.dst_alpha_factors[0] = blast::BLEND_ZERO;

    blast::GfxDepthState depth_state = {};
    depth_state.depth_test = true;
    depth_state.depth_write = true;

    blast::GfxRasterizerState rasterizer_state = {};
    rasterizer_state.cull_mode = blast::CULL_MODE_NONE;
    // 不再使用gl默认的ccw
    rasterizer_state.front_face = blast::FRONT_FACE_CW;
    rasterizer_state.fill_mode = blast::FILL_MODE_SOLID;
    rasterizer_state.primitive_topo = blast::PRIMITIVE_TOPO_TRI_LIST;

    blast::GfxGraphicsPipelineDesc pipeline_desc;
    pipeline_desc.framebuffer = g_framebuffers[0];
    pipeline_desc.root_signature = g_root_signature;
    pipeline_desc.vertex_shader = g_vert_shader;
    pipeline_desc.pixel_shader = g_frag_shader;
    pipeline_desc.vertex_layout = vertex_layout;
    pipeline_desc.blend_state = blend_state;
    pipeline_desc.depth_state = depth_state;
    pipeline_desc.rasterizer_state = rasterizer_state;
    g_pipeline = g_context->CreateGraphicsPipeline(pipeline_desc);
}

std::string ReadFileData(const std::string& path) {
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