#include <iostream>
#include <string>
#include "pro/Prometheus.hpp"

bool didWindowResize = false;
static void window_resize_callback(GLFWwindow* window, int width, int height) 
{
    didWindowResize = true;    
}


int main()
{
    cout << "BEGIN EXERCISE" << endl;
    
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
    string appName = "Exercises05";
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
        int numberFramesinFlight = 1;

        //create query pool
        vk::QueryPoolCreateInfo qpCI{};
        qpCI.queryType = vk::QueryType::eTimestamp;
        qpCI.queryCount = 2;
        vk::QueryPool queryPool = vkInitData.device().createQueryPool(qpCI);

        //pipeline for shaders
        pro::VulkanPipelineCreateInfo pipelineCreateInfo(vkInitData);
        pipelineCreateInfo.shaderInfo = {
            pro::VulkanShaderCreateInfo("build/compiledshaders/" + appName + "shader.vert.spv", vk::ShaderStageFlagBits::eVertex),
            pro::VulkanShaderCreateInfo("build/compiledshaders" + appName + "shader.frag.spv", vk::ShaderStageFlagBits::eFragment)
        };
        

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
            commandData.commandBuffer.resetQueryPool(queryPool, 0, 2);
            //first time stamp
            commandData.commandBuffer.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, queryPool, 0);

            //transition swap image: undefined to color, then color to presentation
            pro::performVulkanImageTransition(commandData.commandBuffer, vkInitData.swapchain().swaps[indexSwap].image, pro::IMAGE_TRANSITION_TYPE::UNDEF_TO_COLOR);
            pro::performVulkanImageTransition(commandData.commandBuffer, vkInitData.swapchain().swaps[indexSwap].image, pro::IMAGE_TRANSITION_TYPE::COLOR_TO_PRESENT);
            //second time stamp
            commandData.commandBuffer.writeTimestamp2(vk::PipelineStageFlagBits2::eBottomOfPipe, queryPool, 1);

            //TO DO: recording commands go here -- rendering magic
            
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
            uint64_t timestamps[2] = {};
            vkInitData.device().getQueryPoolResults(queryPool, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
            //convert ticks to ns
            auto props = vkInitData.physicalDevice().getProperties();
            double nsPerTick = props.limits.timestampPeriod;    
            double deltaNs = (timestamps[1] - timestamps[0]) * nsPerTick;
            cout << "TIME for frame: " << deltaNs << endl;
        }

        //wait until all done, then clean up
        vkInitData.device().waitIdle();
        vkInitData.device().destroyQueryPool(queryPool);
        pro::cleanupFrameCommandData(vkInitData, commandData);
    }

    //clean up
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}