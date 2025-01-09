﻿#pragma once

#include "libavutil/frame.h"
#include <memory>
#include <pathfinder/common/color.h>
#include <pathfinder/common/math/mat4.h>
#include <pathfinder/common/math/vec3.h>
#include <pathfinder/gpu/device.h>
#include <pathfinder/gpu/queue.h>
#include <pathfinder/gpu/render_pipeline.h>
#include <pathfinder/gpu/texture.h>
#include <vector>

class YuvRenderer {
public:
    YuvRenderer(std::shared_ptr<Pathfinder::Device> device, std::shared_ptr<Pathfinder::Queue> queue);
    ~YuvRenderer() = default;
    void init();
    void render(std::shared_ptr<Pathfinder::Texture> outputTex);
    void updateTextureInfo(int width, int height, int format);
    void updateTextureData(const std::shared_ptr<AVFrame> &data);
    void clear();

protected:
    void initPipeline();
    void initGeometry();

private:
    std::shared_ptr<Pathfinder::RenderPipeline> mPipeline;
    std::shared_ptr<Pathfinder::Queue> mQueue;
    std::shared_ptr<Pathfinder::RenderPass> mRenderPass;
    std::shared_ptr<Pathfinder::Texture> mTexY;
    std::shared_ptr<Pathfinder::Texture> mTexU;
    std::shared_ptr<Pathfinder::Texture> mTexV;
    std::shared_ptr<Pathfinder::DescriptorSet> mDescriptorSet;
    std::shared_ptr<Pathfinder::Sampler> mSampler;
    std::shared_ptr<Pathfinder::Buffer> mVertexBuffer;
    std::shared_ptr<Pathfinder::Buffer> mUniformBuffer;

    int mPixFmt = 0;
    bool mTextureAllocated = false;

    bool mNeedClear = false;

    std::shared_ptr<Pathfinder::Device> mDevice;

    volatile bool inited = false;
};
