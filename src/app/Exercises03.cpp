#include <iostream>
#include <string>
#include "pro/Prometheus.hpp"

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
    string appName = "Exercises03";
    int winWidth = 800;
    int winHeight = 600;
    GLFWwindow *window = glfwCreateWindow(winWidth, winHeight, appName.c_str(), NULL, NULL);
    //check
    if(!window)
    {
        cerr << "FAILED TO CREATE WINDOW" << endl;
        glfwTerminate();
        exit(1);
    }

    //scope for vulkaninit
    {
        //create surface
        pro::VulkanInitCreateInfo initCreateInfo {};
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
        
    }

    //clean up
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}