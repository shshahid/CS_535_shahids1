#include <iostream>
#include <string>
#include "pro/Prometheus.hpp"
using namespace std;

bool didWindowResize = false;
glm::mat4 modelMat(1.0);
UBOVertex uboVertHost{};

struct ForgeVertex
{
    glm::vec3 pos;
    glm::vec4 color;
};

struct UniformPush
{
    alignas(16) glm::mat4 modelMat;
};

struct UBOVertex
{
    alignas(16) glm::mat4 viewMat {};
    alignas(16) glm::mat4 projMat {};
};

//window adjustments
static void window_resize_callback(GLFWwindow* window, int width, int height) 
{
    didWindowResize = true;    
}

//keyboard events
static void key_callback(   GLFWwindow *window,
                            int key,
                            int scancode,
                            int action,
                            int mods) {

    if(action == GLFW_PRESS || action == GLFW_REPEAT) {
        if(key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, true);
        }
        else if(key == GLFW_KEY_Q) {
            modelMat = glm::rotate(glm::radians(5.0f), glm::vec3(0, 0, 1)) * modelMat;
        }
        else if(key == GLFW_KEY_W) {
            modelMat = glm::translate(glm::vec3(0, 0.1, 0)) * modelMat;
        }
        else if(key == GLFW_KEY_SPACE) {
            modelMat = glm::mat4(1.0);
        }
    }
}

int main()
{
    cout << "BEGIN EXERCISE" << endl;

    glm::vec3 A = glm::vec3(1,4,0);
    glm::vec3 B = glm::vec3(2,3,2);

    cout << "A.x = " << A.x << endl;
    cout << "A = " << glm::to_string(A) << endl;
    cout << "B = " << glm::to_string(B) << endl;

    glm::vec3 C = B - A;
    cout << "C = " << glm::to_string(C) << endl;

    A = 5.0f * A;
    cout << "A = " << glm::to_string(A) << endl;

    glm::vec3 normA = glm::normalize(A);
    cout << "normA = " << glm::to_string(normA) << endl;
    cout << "length A = "  << glm::length(A) << endl;
    cout << "length normA = " << glm::length(normA) << endl;

    glm::vec3 normB = glm::normalize(B);
    float dotAB = glm::dot(normA, normB);
    cout << "dotAB = " << dotAB << endl;





    //initialize GLFW environment + check
    if(!glfwInit())
    {
        cerr << "FAILED TO INIT GLFW" << endl;
        exit(1);
    }
    //specify window attributes before creation
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, true);

    //create window
    string appName = "Exercises11";
    int winWidth = 800;
    int winHeight = 600;
    GLFWwindow *window = glfwCreateWindow(winWidth, winHeight, appName.c_str(), NULL, NULL);
    //and check
    if(!window)
    {
        cerr << "FAILED TO CREATE WINDOW" << endl;
        glfwTerminate();
        exit(1);
    }

    //window resize
    glfwSetFramebufferSizeCallback(window, window_resize_callback);
    //keys
    glfwSetKeyCallback(window, key_callback);

    //scope for vulkaninit
    {
        //create surface
        pro::VulkanInitCreateInfo initCreateInfo{};
        initCreateInfo.appName = appName;
        initCreateInfo.createSurfaceFunc = [window](VkInstance instance, VkSurfaceKHR &surface)
        {
            return glfwCreateWindowSurface(instance, window, NULL, &surface);
        };

        //check window size
        initCreateInfo.getCurrentWindowSizeFunc = [window](int &width, int &height)
        {
            glfwGetFramebufferSize(window, &width, &height);
        };

        //cases for older machines
        //initCreateInfo.requestedAppVulkanVersionMinor = 3;
        //initCreateInfo.requireComputeQueue = false;
        //initCreateInfo.requireTransferQueue = false;

        //create VulkanInitData object
        pro::VulkanInitData vkInitData(initCreateInfo);

        //list physical graphics on system
        pro::listAvailablePhysicalDevices(vkInitData.instance());
        cout << "** Chosen Physical Device: **" << endl;
        pro::printPhysicalDeviceProperties(vkInitData.physicalDevice());
        
        int numberFramesinFlight = 1;
        vector<pro::VulkanImage> allDepthImages {};
        pro::recreateAllVulkanDepthImages(vkInitData, allDepthImages, numberFramesinFlight);


        //resize function
        pro::OnResizeFunc resizeFunc = [&vkInitData, window]()
        {
            int width = 0, height = 0;
            do {
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();                
            } while(width == 0 || height == 0);
            vkInitData.recreateVulkanSwapchain();
            cout << "Swapchain recreated..." << endl;
        };

        //initialize commanddata -- one FIF
        pro::FrameCommandData commandData = pro::createFrameCommandData(vkInitData);
        uint32_t framesRendered = 0;

        //create query pool
        vk::QueryPoolCreateInfo qpCI{};
        qpCI.queryType = vk::QueryType::eTimestamp;
        qpCI.queryCount = 2;
        vk::QueryPool queryPool = vkInitData.device().createQueryPool(qpCI);

        //pipeline for shaders
        pro::VulkanPipelineCreateInfo pipelineCreateInfo(vkInitData);
        pipelineCreateInfo.shaderInfo = {
            pro::VulkanShaderCreateInfo("build/compiledshaders/" + appName + "/shader.vert.spv", vk::ShaderStageFlagBits::eVertex),
            pro::VulkanShaderCreateInfo("build/compiledshaders/" + appName + "/shader.frag.spv", vk::ShaderStageFlagBits::eFragment)
        };

        //set up vertex info
        pipelineCreateInfo.bindDesc = vk::VertexInputBindingDescription(0, sizeof(ForgeVertex), vk::VertexInputRate::eVertex);
        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(0,0,vk::Format::eR32G32B32Sfloat, offsetof(ForgeVertex, pos)));
        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(1,0,vk::Format::eR32G32B32A32Sfloat, offsetof(ForgeVertex, color)));
        
        pipelineCreateInfo.pushConstantRanges.push_back(
            {vk::ShaderStageFlagBits::eVertex, 0, sizeof(UniformPush)}
        );

        vector<pro::VulkanBuffer> uboVertData {};
        uboVertData.resize(numberFramesinFlight);
        for(int i = 0; i < numberFramesinFlight; i++) {
            uboVertData[i] = pro::createVulkanBuffer(vkInitData, sizeof(UBOVertex), 
                                    vk::BufferUsageFlagBits::eUniformBuffer, pro::createVMAHostVisibleInfo());
        }

        vector<vk::DescriptorSetLayoutBinding> allBindings = {
            vk::DescriptorSetLayoutBinding(
                    0, vk::DescriptorType::eUniformBuffer,
                    1, vk::ShaderStageFlagBits::eVertex)
        };
        pipelineCreateInfo.allDescSetLayouts.push_back(
            vkInitData.device().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, allBindings)));

        //create pipeline 
        pro::VulkanPipelineData pipelineData = pro::createVulkanPipeline(vkInitData, pipelineCreateInfo);


        pro::TransferManager transferManager = pro::TransferManager(vkInitData);

        //copying to device-local mesh
        vector<pro::VulkanMesh> allMeshes {};
        vector<pro::VulkanMesh> waitingMeshes {};
        vector<pro::VulkanMesh> readyToRenderMeshes {};

        vector<pro::PendingBufferCopy> pendingCopies {};
        vector<pro::BufferCopyReceipt*> copiesToCheck {};

        //host data
        pro::HostMesh<ForgeVertex> hostMesh {};
        hostMesh.vertices = {
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}}
        };
        //ccw winding
        hostMesh.indices = {0,1,2,2,3,0};

        pro::VulkanMesh mesh = pro::createVulkanMesh(vkInitData, hostMesh, true);
        
        pro::addPendingBufferCopies(mesh, hostMesh, pendingCopies);
        auto transferReceipt = transferManager.submitCopies("SquareMesh", pendingCopies);
        
        copiesToCheck.push_back(transferReceipt);   
        pendingCopies.clear();

        allMeshes.push_back(mesh);
        waitingMeshes.push_back(mesh);
        

        //MAIN RENDER LOOP
        while(!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            if(didWindowResize)
            {
                resizeFunc();
                didWindowResize = false;
            }
            unsigned int indexFlight = framesRendered % numberFramesinFlight;
            unsigned int indexSwap = pro::acquireNextSwapImage(vkInitData, commandData, resizeFunc);

            //reset command pool
            vkInitData.device().resetCommandPool(commandData.commandPool);
            //then begin recording
            commandData.commandBuffer.begin({vk::CommandBufferBeginInfo()});
            //commandData.commandBuffer.resetQueryPool(queryPool, 0, 2);
            //first time stamp
            //commandData.commandBuffer.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, queryPool, 0);
            
            for(auto it = copiesToCheck.begin(); it != copiesToCheck.end();)
            {
                if(transferManager.checkCompleted(*it, commandData.commandBuffer)) {
                    readyToRenderMeshes.insert(readyToRenderMeshes.end(), waitingMeshes.begin(), waitingMeshes.end());
                    waitingMeshes.clear();
                    it = copiesToCheck.erase(it);
                }
                else {
                    it++;
                }
            }

            //transition swap image: undefined to color
            pro::performVulkanImageTransition(commandData.commandBuffer, vkInitData.swapchain().swaps[indexSwap].image, pro::IMAGE_TRANSITION_TYPE::UNDEF_TO_COLOR);
            //second time stamp
            //commandData.commandBuffer.writeTimestamp2(vk::PipelineStageFlagBits2::eBottomOfPipe, queryPool, 1);


            //TO DO: recording commands go here -- rendering magic
            //create color attachment
            auto colorAtt = pro::createColorAttachment(vkInitData.swapchain().swaps[indexSwap].view, vk::ClearColorValue(0.6f, 0.0f, 1.0f, 1.0f));
            //create deoth
            auto depthAtt = pro::createDepthAttachment(allDepthImages[indexFlight].view);
            //set color, depth attachment
            vk::RenderingInfoKHR ri{};
            ri.setRenderArea(vk::Rect2D{ {0,0}, vkInitData.swapchain().extent })
                    .setLayerCount(1)
                    .setColorAttachments(colorAtt)
                    .setPDepthAttachment(&depthAtt);

            //start dynamic rendering
            commandData.commandBuffer.beginRendering(ri);
            //bind pipeline
            commandData.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineData.pipeline);
            
            //set viewport and scissors
            vk::Viewport viewports[] = { pro::makeDefaultViewport(vkInitData) };
            vk::Rect2D scissors[] = { pro::makeDefaultScissors(vkInitData) };
            commandData.commandBuffer.setViewport(0, viewports);
            commandData.commandBuffer.setScissor(0, scissors);
            
            UniformPush pv = {};
            pv.modelMat = modelMat;

            commandData.commandBuffer.pushConstants(pipelineData.layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(UniformPush), &pv);

            for(int i = 0; i < readyToRenderMeshes.size(); i++)
            {
                pro::recordDrawVulkanMesh(commandData.commandBuffer, readyToRenderMeshes[i]);
            }

            //end dynamic rendering
            commandData.commandBuffer.endRendering();


            //transition swap image: color to presentation
            pro::performVulkanImageTransition(commandData.commandBuffer, vkInitData.swapchain().swaps[indexSwap].image, pro::IMAGE_TRANSITION_TYPE::COLOR_TO_PRESENT);
            //end recording
            commandData.commandBuffer.end();

            //once done, submit to GPU
            pro::submitToGraphicsQueue(vkInitData, commandData, indexSwap, resizeFunc);

            //then present
            if(!pro::presentSwapImage(vkInitData, commandData, indexSwap, resizeFunc))
            {
                cerr << "WARNING: presentation was not successful." << endl;
            }
            //increment number of frames rendered
            framesRendered++;

            //get query pool values
            /*
            uint64_t timestamps[2] = {};
            vkInitData.device().getQueryPoolResults(queryPool, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
            //convert ticks to ns
            auto props = vkInitData.physicalDevice().getProperties();
            double nsPerTick = props.limits.timestampPeriod;    
            double deltaNs = (timestamps[1] - timestamps[0]) * nsPerTick;
            cout << "TIME for frame: " << deltaNs << endl;
            */
        }

        //wait until all done, then clean up
        vkInitData.device().waitIdle();

        pro::cleanupAllVulkanDepthImages(vkInitData, allDepthImages);
        
        for(int i = 0; i < numberFramesinFlight; i++)
        {
            pro::cleanupVulkanBuffer(vkInitData, uboVertData[i]);
        }
        uboVertData.clear();

        for(int i = 0; i < allMeshes.size(); i++)
        {
            pro::cleanupVulkanMesh(vkInitData, allMeshes[i]);
        }
        allMeshes.clear();
        waitingMeshes.clear();
        readyToRenderMeshes.clear();
        copiesToCheck.clear();

        pro::cleanupVulkanPipeline(vkInitData, pipelineData);
        vkInitData.device().destroyQueryPool(queryPool);
        pro::cleanupFrameCommandData(vkInitData, commandData);
    }

    //clean up
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}