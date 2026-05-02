#include <iostream>
#include <string>
#include "pro/Prometheus.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
using namespace std;

///////////////////////////////////////////////////////////////////////////////
// STRUCTS
///////////////////////////////////////////////////////////////////////////////

struct ProVertex {
    glm::vec3 pos;
    glm::vec4 color;
    glm::vec3 normal;
};

struct UPushVertex {
    alignas(16) glm::mat4 modelMat;
    alignas(16) float useNormalAsColor;
};

struct UBOVertex {
    alignas(16) glm::mat4 viewMat;
    alignas(16) glm::mat4 projMat;
};

struct alignas(16) PointLight {
    glm::vec4 pos;
    glm::vec4 color;
};

struct alignas(16) UBOFragment {
    glm::vec4 cameraPos;
    alignas(16) uint32_t lightCnt;
};

struct Camera {
    glm::vec3 eye = glm::vec3(0.0f, 0.0f, 1.5f);
    glm::vec3 center = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS
///////////////////////////////////////////////////////////////////////////////

bool didWindowResize = false;

//background color value variable
vk::ClearColorValue bgcolor;

float rotAngleZ = 0.0f;
float useNormalAsColor = 1.0f;
float zOffset = 0.0f;
bool flipWindingOrder = false;

Camera camera {};
UBOVertex uboVertHost {};
UBOFragment uboFragHost {};
vector<PointLight> hostLights {};
glm::vec2 lastMousePos {};
bool lightDataChanged = false;

///////////////////////////////////////////////////////////////////////////////
// GLFW CALLBACKS
///////////////////////////////////////////////////////////////////////////////

// Note: static prevents name conflicts in other cpp files

// When the window resizes/minimizes...
static void window_resize_callback(GLFWwindow* window, int width, int height) {
    didWindowResize = true;    
}

// When key events occur...
static void key_callback(   GLFWwindow *window,
                            int key,
                            int scancode,
                            int action,
                            int mods) {
    //compute camera direction, local X direction, speed
    glm::vec3 direction = glm::normalize(camera.center - camera.eye);
    glm::vec3 localX = glm::normalize(glm::cross(direction, camera.up));
    float speed = 0.1f;

    if(action == GLFW_PRESS || action == GLFW_REPEAT) {
        if(key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, true);
        }
        
        else if(key == GLFW_KEY_J) {
            rotAngleZ += 1.0;
        }
        else if(key == GLFW_KEY_K) {
            rotAngleZ -= 1.0;
        }
        else if(key == GLFW_KEY_O) {
            useNormalAsColor = clamp(useNormalAsColor + 0.1, 0.0, 1.0);
        }
        else if(key == GLFW_KEY_I) {
            useNormalAsColor = clamp(useNormalAsColor - 0.1, 0.0, 1.0);
        }
        else if(key == GLFW_KEY_U) {
            zOffset += 0.05;
        }
        else if(key == GLFW_KEY_Y) {
            zOffset -= 0.05;
        }

        else if(key == GLFW_KEY_W) {
            camera.eye += direction * speed;
            camera.center += direction * speed;
        }
        else if(key == GLFW_KEY_S) {
            camera.eye -= direction * speed;
            camera.center -= direction * speed;
        }
        else if(key == GLFW_KEY_D) {
            camera.eye += localX * speed;
            camera.center += localX * speed;
        }
        else if(key == GLFW_KEY_A) {
            camera.eye -= localX * speed;
            camera.center -= localX * speed;
        }
        else if(key == GLFW_KEY_1) {
            for(int i = 0; i < hostLights.size(); i++) {
                //white
                hostLights[i].color = {1.0f, 1.0f, 1.0f, 1.0f};
            }
            lightDataChanged = true;
        }
        else if(key == GLFW_KEY_2) {
            for(int i = 0; i < hostLights.size(); i++) {
                //red
                hostLights[i].color = {1.0f, 0.0f, 0.0f, 1.0f};
            }
            lightDataChanged = true;
        }
        else if(key == GLFW_KEY_3) {
            for(int i = 0; i < hostLights.size(); i++) {
                //greem
                hostLights[i].color = {0.0f, 1.0f, 0.0f, 1.0f};
            }
            lightDataChanged = true;
        }
        else if(key == GLFW_KEY_4) {
            for(int i = 0; i < hostLights.size(); i++) {
                //blue
                hostLights[i].color = {0.0f, 0.0f, 1.0f, 1.0f};
            }
            lightDataChanged = true;
        }
        else if(key == GLFW_KEY_X) {
            if(uboFragHost.lightCnt < hostLights.size()) {
                uboFragHost.lightCnt++;
            }
        }
        else if(key == GLFW_KEY_Z) {
            if(uboFragHost.lightCnt > 1) {
                uboFragHost.lightCnt--;
            }
        }
    }
}

// When the mouse moves...
static void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
    //current mouse
    glm::vec2 mousePos = glm::vec2(xpos, ypos);
    //cout << "Mouse pos: " << glm::to_string(mousePos) << endl;
    //difference in movement
    glm::vec2 diffPos = lastMousePos - mousePos;
    //update last mouse
    lastMousePos = mousePos;
    //current framebuffer size
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    //if w and h both greater than 0
    if((width > 0) && (height > 0)) {
        //normalize
        diffPos.x /= width;
        diffPos.y /= height;
        //multiply speed
        float speed = 50.0f;
        diffPos *= speed;
        //compute cmaera direction and local x axis
        glm::vec3 direction = glm::normalize(camera.center - camera.eye);
        glm::vec3 localX = glm::normalize(glm::cross(direction, camera.up));
        //create matrices
        glm::mat4 negEye = glm::translate(-camera.eye);
        glm::mat4 rotateY = glm::rotate(glm::radians(diffPos.x), camera.up);
        glm::mat4 rotateX = glm::rotate(glm::radians(diffPos.y), localX);
        glm::mat4 posEye = glm::translate(camera.eye);
        //transform camera "look at" point = move by -eye, rotate around up axis, rotate around local X axis, move back with +eye
        camera.center = glm::vec3(posEye * rotateX * rotateY * negEye * glm::vec4(camera.center, 1.0));
    }
}

// When a mouse button is pressed/released/clicked...
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        cout << "Left mouse press." << endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

// ASSIMP TO GLM FUNCTION
glm::mat4 assimpToGLM(aiMatrix4x4 &a)
{
    return glm::transpose(glm::make_mat4(&a.a1));
}

// LOCAL Z ROTATION FUNCTION
glm::mat4 makeRotateZ(glm::vec3 offset)
{
    //transformation matrix - identity
    glm::mat4 transf = glm::mat4(1.0f);
    //translate by negative offset, rotate by rotanglez, translate by offset
    transf = glm::translate(-offset) * transf;
    transf = glm::rotate(glm::radians(rotAngleZ), glm::vec3(0,0,1)) * transf;
    transf = glm::translate(offset) * transf;

    return transf;
}

// RENDER SCENE NODE
void renderSceneNode(vk::CommandBuffer &commandBuffer, pro::VulkanPipelineData &pipelineData, vector<pro::VulkanMesh> &allMeshes, 
                        aiNode *node, glm::mat4 parentMat, int level)
{
    aiMatrix4x4 curT = node->mTransformation;
    glm::mat4 nodeT = assimpToGLM(curT);
    glm::mat4 modelMat = parentMat * nodeT;
    glm::vec3 pos(modelMat[3]);
    glm::mat4 R = makeRotateZ(pos);
    R = glm::translate(glm::vec3(0,0,zOffset)) * R;

    glm::mat4 tmpModel = R * modelMat;
    UPushVertex pv = {tmpModel, useNormalAsColor};
    commandBuffer.pushConstants(pipelineData.layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(UPushVertex), &pv);

    //each mesh in the node
    for(int i = 0; i < node->mNumMeshes; i++)
    {
        int index = node->mMeshes[i];
        pro::recordDrawVulkanMesh(commandBuffer, allMeshes.at(index));
    }

    //each child of the node
    for(int i = 0; i < node->mNumChildren; i++)
    {
        renderSceneNode(commandBuffer, pipelineData, allMeshes, node->mChildren[i], modelMat, level+1);
    }
}

// EXTRACT MESH DATA - grab vertex positions and shape indices from given aiMesh and store in HostMesh struct
pro::HostMesh<ProVertex> extractMeshData(aiMesh *mesh)
{
    pro::HostMesh<ProVertex> md;
    //vertices
    for(int i = 0; i < mesh->mNumVertices; i++)
    {
        ProVertex tmp;
        tmp.pos = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        tmp.color = {1,1,0,1};
        tmp.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        
        md.vertices.push_back(tmp);
    }
    //indices
    for(int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];

        for(int j = 0; j < face.mNumIndices; j++)
        {
            md.indices.push_back(face.mIndices[j]);
        }
    }

    return md;
}

// CREATE SHAPE FUNCTION
pro::HostMesh<ProVertex> createShape(int maxSub = 10, bool flipWinding = false)
{
    pro::HostMesh<ProVertex> custShape {};
    
    //default rectangle for testing
    /*custShape.vertices = {
            {{0.0f, 0.0f, 0.5f},    {1,1,1,1}},
            {{-0.5f, -0.5f, 0.5f},  {1,0,0,1}},
            {{0.5f, -0.5f, 0.5f},   {0,1,0,1}},
            {{0.5f, 0.5f, 0.5f},    {0,0,1,1}},
            {{-0.5f, 0.5f, 0.5f},   {1,0,1,1}}
    };
    vector<unsigned int> indices = {
        0,1,2,
        0,2,3,
        0,3,4,
        0,4,1
    };*/

    //indices vector
    vector<unsigned int> indices;

    //set center vertex before loop
    custShape.vertices.push_back({{0.0f, 0.0f, 0.5f}, {1,1,1,1}});
    //int vcount = 1;
    for(float i = 0; i < maxSub; i++)
    {
        //vertex generation
        float angle = (i * 2.0 * 3.14) / maxSub;
        float x = cos(angle) * 0.5;
        float y = sin(angle) * 0.5;
        //cout << "angle:" << angle << " " << x << " " << y << endl;
        //push
        custShape.vertices.push_back({{x, y, 0.5f}, {i/maxSub * 0.5, i/maxSub * 0.2, i/maxSub, 1}});
        //vcount++;

        //indices generation + push
        indices.push_back(0);
        indices.push_back(i+1);
        indices.push_back(i+2);
    }
    
    //close shape: loop last index back to start
    indices.back() = 1;

    //check before setting indices
    if(flipWinding)
    {
        reverse(indices.begin(), indices.end());
    }
    //set indices
    custShape.indices = indices;

    //debug output
    /*for(int i = 0; i < vcount; i++)
    {
        cout << custShape.vertices[i].pos.x << " " << custShape.vertices[i].pos.y << " " << custShape.vertices[i].pos.z << ", "
        << custShape.vertices[i].color.r << " " << custShape.vertices[i].color.g << " " << custShape.vertices[i].color.b << " "
        << custShape.vertices[i].color.a << endl;
    }
    cout << endl;
    for (int x : indices) {
        cout << x << " ";
    }
    cout << endl;*/

    return custShape;
}

///////////////////////////////////////////////////////////////////////////////
// PER-FRAME "DRAWING" FUNCTION
///////////////////////////////////////////////////////////////////////////////

void recordFrame(   pro::VulkanInitData &vkInitData, 
                    pro::FrameCommandData &cd,
                    const pro::VulkanSwapImage &swapImage,
                    const pro::VulkanImage &depthImage,
                    pro::VulkanPipelineData &pipelineData,
                    vector<pro::VulkanMesh> allMeshes,
                    const aiScene *scene,
                    pro::VulkanBuffer &uboVertData,
                    pro::VulkanBuffer &uboFragData,
                    pro::VulkanBuffer &ssboLights,
                    pro::DescriptorPack &descPack) {

    // Reset our command pool so it's cleared and ready to go
    vkInitData.device().resetCommandPool(cd.commandPool);

    // Begin recording
    cd.commandBuffer.begin(vk::CommandBufferBeginInfo());

    // Transition swap image from undefined to color buffer
    performVulkanImageTransition(cd.commandBuffer, swapImage.image, pro::IMAGE_TRANSITION_TYPE::UNDEF_TO_COLOR);

    // Define behavior for the color attachment (including clear color)
    vk::RenderingAttachmentInfoKHR colorAtt = pro::createColorAttachment(
        swapImage.view, 
        vk::ClearColorValue (bgcolor));

    // Define behavior for the depth attachment
    vk::RenderingAttachmentInfoKHR depthAtt = pro::createDepthAttachment(depthImage.view);
        
    // Set rendering info and begin (dynamic) rendering
    vk::RenderingInfoKHR ri{};
    ri.setRenderArea(vk::Rect2D{ {0,0}, vkInitData.swapchain().extent })
        .setLayerCount(1)
        .setColorAttachments(colorAtt)
        .setPDepthAttachment(&depthAtt);

    cd.commandBuffer.beginRendering(ri);
    
    // Bind pipeline
    cd.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineData.pipeline);
    
    // Set up viewport and scissors
    vk::Viewport viewports[] = { pro::makeDefaultViewport(vkInitData) };    
    cd.commandBuffer.setViewport(0, viewports);
    
    vk::Rect2D scissors[] = { pro::makeDefaultScissors(vkInitData) };
    cd.commandBuffer.setScissor(0, scissors);
    
    //use camera info to compute view and proj mat
    uboVertHost.viewMat = glm::lookAt(camera.eye, camera.center, camera.up);
    float fov = glm::radians(90.0f);
    float aspectRatio = ((float) vkInitData.swapchain().extent.width) / ((float)vkInitData.swapchain().extent.height);
    float near = 0.01;
    float far = 1000.0;
    uboVertHost.projMat = glm::perspective(fov, aspectRatio, near, far);
    //update camera position for frag shader ubo
    uboFragHost.cameraPos = glm::vec4(camera.eye, 1.0f);
    //copy ubo data
    pro::copyToHostVisibleVulkanBuffer(vkInitData, uboVertData, &uboVertHost);
    pro::copyToHostVisibleVulkanBuffer(vkInitData, uboFragData, &uboFragHost);
    //if light data changed, copy data to SSBO and reset var
    if(lightDataChanged)
    {
        pro::copyToHostVisibleVulkanBuffer(vkInitData, ssboLights, hostLights.data());
        lightDataChanged = false;
    }
    //bind descriptor sets
    cd.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineData.layout, 0, descPack.sets, {});

    // FOR NOW, just render all meshes
    /*for(auto &mesh : allMeshes) {
        pro::recordDrawVulkanMesh(cd.commandBuffer, mesh);
    }*/
    renderSceneNode(cd.commandBuffer, pipelineData, allMeshes, scene->mRootNode, glm::mat4(1.0), 0);

    
    // End rendering
    cd.commandBuffer.endRendering();
   
    // Transition swap image from color buffer to presentation
    pro::performVulkanImageTransition(cd.commandBuffer, swapImage.image, pro::IMAGE_TRANSITION_TYPE::COLOR_TO_PRESENT);
    

    // End recording
    cd.commandBuffer.end();
}

///////////////////////////////////////////////////////////////////////////////
// MAIN FUNCTION
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
    //default
    string arg1 = "sampleModels/sphere.obj";
    //else
    if(argc >= 2)
    {
        arg1 = argv[1];
    }

    unsigned int aiFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices;
    if(flipWindingOrder)
    {
        aiFlags = aiFlags | aiProcess_FlipWindingOrder;
    }

    Assimp::Importer imp;
    const aiScene *scene = imp.ReadFile(arg1, aiFlags);
    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        cerr << "ERROR: " + string(imp.GetErrorString()) << endl;
    }


    cout << "BEGIN PROGRAM..." << endl;
    // Set app name
    string appName = "Assign04";
    string windowName = appName + ": shahids1";

    ///////////////////////////////////////////////////////////////////////
    // GLFW
    ///////////////////////////////////////////////////////////////////////

    // Initialize GLFW
    if(!glfwInit()) {
        cerr << "ERROR: Cannot start GLFW!" << endl;
        exit(1);
    }
    
    // Create GLFW window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, true);
    GLFWwindow *window = glfwCreateWindow(800, 600, windowName.c_str(), nullptr, nullptr);

    // Was window successfully created?
    if(!window) {
        cerr << "ERROR: Cannot create GLFW window!" << endl;
        glfwTerminate();
        exit(1);
    }

    // Define GLFW callback functions
    glfwSetFramebufferSizeCallback(window, window_resize_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    //hide cursor pos
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    //initalize last mouse pos
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    lastMousePos = glm::vec2(xpos, ypos);
    
    // Create scope for Vulkan Init Data (to ensure proper cleanup)
    {
        ///////////////////////////////////////////////////////////////////////
        // VULKAN INIT DATA
        ///////////////////////////////////////////////////////////////////////

        // Creation information for basic Vulkan components
        pro::VulkanInitCreateInfo createInfo {};
        createInfo.appName = appName;
        // If you encounter errors with instance creation, try requesting Vulkan 1.3:
        //createInfo.requestedAppVulkanVersionMinor = 3;
        
        // If you encounter errors with compute and/or transfer queue creation, try these:
        //createInfo.requireComputeQueue = false;
        //createInfo.requireTransferQueue = false;

        createInfo.createSurfaceFunc = [window](VkInstance instance, VkSurfaceKHR& surface) {            
            return glfwCreateWindowSurface(instance, window, nullptr, &surface);
        };

        createInfo.getCurrentWindowSizeFunc = [window](int &width, int &height) {
            glfwGetFramebufferSize(window, &width, &height);
        };
    
        // Create the basic Vulkan components
        pro::VulkanInitData vkInitData(createInfo);
        
        cout << "** Chosen Physical Device: *********" << endl;        
        pro::printPhysicalDeviceProperties(vkInitData.physicalDevice());
        vkInitData.printQueues();

        // Create depth image(s)
        vector<pro::VulkanImage> allDepthImages {};
        int numberOfFramesInFlight = 1;
        pro::recreateAllVulkanDepthImages(vkInitData, allDepthImages, numberOfFramesInFlight);
        
        // Define resize function
        pro::OnResizeFunc resizeFunc = [&vkInitData, window, &allDepthImages, numberOfFramesInFlight]() {            
            int width = 0;
            int height = 0;

            do {
                glfwGetFramebufferSize(window, &width, &height);
                // If minimized, this will be 0,0. Block until restored.
                glfwWaitEvents(); // Actually waits/sleeps/blocks until an event happens
            } while (width == 0 || height == 0);
        
            // Safe to recreate
            vkInitData.recreateVulkanSwapchain();
            recreateAllVulkanDepthImages(vkInitData, allDepthImages, numberOfFramesInFlight);

            cout << "Swapchain recreated..." << endl;
        };

        ///////////////////////////////////////////////////////////////////////
        // VULKAN COMMAND DATA
        ///////////////////////////////////////////////////////////////////////

        // Create command data
        pro::FrameCommandData commandData = pro::createFrameCommandData(vkInitData);

        ///////////////////////////////////////////////////////////////////////
        // VULKAN GRAPHICS PIPELINE
        ///////////////////////////////////////////////////////////////////////

        // Set up creation info for pipeline
        pro::VulkanPipelineCreateInfo pipelineCreateInfo(vkInitData);

        // Create shader info
        pipelineCreateInfo.shaderInfo = {
            pro::VulkanShaderCreateInfo(
                "build/compiledshaders/" + appName + "/shader.vert.spv",
                vk::ShaderStageFlagBits::eVertex
            ),

            pro::VulkanShaderCreateInfo(
                "build/compiledshaders/" + appName + "/shader.frag.spv",
                vk::ShaderStageFlagBits::eFragment
            )
        };

        // Set up vertex information
        pipelineCreateInfo.bindDesc = vk::VertexInputBindingDescription(
            0, sizeof(ProVertex), vk::VertexInputRate::eVertex);

        // POSITION
        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(
            0, // location
            0, // binding
            vk::Format::eR32G32B32Sfloat,  // format
            offsetof(ProVertex, pos) // offset
        ));
        
        // COLOR
        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(
            1, // location
            0, // binding
            vk::Format::eR32G32B32A32Sfloat,  // format
            offsetof(ProVertex, color) // offset
        ));

        // NORMAL
        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(
            2, // location
            0, // binding
            vk::Format::eR32G32B32Sfloat,  // format
            offsetof(ProVertex, normal) // offset
        ));

        //create descriptor set layouts for uniform buffers and SSBO
        vector<vk::DescriptorSetLayoutBinding> allBindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment)
        };
        pipelineCreateInfo.allDescSetLayouts.push_back(
            vkInitData.device().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, allBindings)));

        // Actually create the pipeline data
        pro::VulkanPipelineData pipelineData = createVulkanPipeline(vkInitData, pipelineCreateInfo);

        //create UBOs
        pro::VulkanBuffer uboVertData = pro::createVulkanBuffer(vkInitData, sizeof(UBOVertex),
            vk::BufferUsageFlagBits::eUniformBuffer, pro::createVMAHostVisibleInfo());
        pro::VulkanBuffer uboFragData = pro::createVulkanBuffer(vkInitData, sizeof(UBOFragment),
            vk::BufferUsageFlagBits::eUniformBuffer, pro::createVMAHostVisibleInfo());

        //create ring of 4 lights
        for(int i = 0; i < 4; i++)
        {
            //4 sections = 2pi (360 degrees) / 4
            float angle = i * (2.0 * 3.14 / 4.0);
            PointLight tmp;
            tmp.pos = {1.5f*cos(angle), 1.5f, 1.5f*sin(angle), 1.0};
            tmp.color = {1.0f, 1.0f, 1.0f, 1.0f};

            hostLights.push_back(tmp);
        }
        uboFragHost.lightCnt = 1;

        //ssbo and initial copy
        pro::VulkanBuffer ssboLights = pro::createVulkanBuffer(vkInitData, sizeof(PointLight)*hostLights.size(), 
                                            vk::BufferUsageFlagBits::eStorageBuffer, pro::createVMAHostVisibleInfo());
        pro::copyToHostVisibleVulkanBuffer(vkInitData, ssboLights, hostLights.data());
        
        //descriptor pool sizes
        vector<vk::DescriptorPoolSize> poolSizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1)
        };

        //descriptor pack and update
        pro::DescriptorPack descPack = pro::createDescriptorPack(vkInitData, pipelineData, poolSizes);
        pro::updateBufferForDescriptorPack(vkInitData, descPack, uboVertData, 0, 0);
        pro::updateBufferForDescriptorPack(vkInitData, descPack, uboFragData, 0, 1);
        pro::updateBufferForDescriptorPack(vkInitData, descPack, ssboLights, 0, 2, vk::DescriptorType::eStorageBuffer);

        ///////////////////////////////////////////////////////////////////////
        // MESH CREATION
        ///////////////////////////////////////////////////////////////////////

        // Create host data  
        vector<pro::HostMesh<ProVertex>> allHostMeshes {};
        
        /*pro::HostMesh<ProVertex> simpleQuad {};
        simpleQuad.vertices = {
            {{-0.5f, -0.5f, 0.5f},  {1,0,0,1}},
            {{0.5f, -0.5f, 0.5f},   {0,1,0,1}},
            {{0.5f, 0.5f, 0.5f},    {0,0,1,1}},
            {{-0.5f, 0.5f, 0.5f},   {1,1,1,1}}
        };
        simpleQuad.indices = { 0, 1, 2, 0, 2, 3 };
        //allHostMeshes.push_back(simpleQuad);

        //create and add shape to allHostMeshes
        pro::HostMesh<ProVertex> custShape = createShape(80, false);
        allHostMeshes.push_back(custShape);*/

        // Create the Vulkan meshes
        vector<pro::VulkanMesh> allMeshes {};    
        /*allMeshes.resize(allHostMeshes.size());       
        for(unsigned int i = 0; i < allMeshes.size(); i++) {
            allMeshes[i] = pro::createVulkanMesh(vkInitData, allHostMeshes[i], false);
            pro::copyToHostVisibleVulkanMesh(vkInitData, allMeshes[i], allHostMeshes[i]);            
        }*/
        allHostMeshes.resize(scene->mNumMeshes);
        allMeshes.resize(scene->mNumMeshes);

        for(int i = 0; i < scene->mNumMeshes; i++)
        {
            pro::HostMesh<ProVertex> hm = extractMeshData(scene->mMeshes[i]);
            allMeshes[i] = pro::createVulkanMesh(vkInitData, hm, false);
            pro::copyToHostVisibleVulkanMesh(vkInitData, allMeshes[i], hm);
        }


        ///////////////////////////////////////////////////////////////////////
        // MAIN RENDER LOOP
        ///////////////////////////////////////////////////////////////////////
        
        //start time i.e. right before loop starts
        auto current_time = pro::getTime();
        vk::ClearColorValue c1 = {1.0f, 1.0f, 0.0f, 1.0f};  //red
        vk::ClearColorValue c2 = {0.0f, 0.0f, 1.0f, 1.0f};  //blue
        // While the window is still open...
        while (!glfwWindowShouldClose(window)) { 
            // Check for window/keyboard/mouse events...	
            glfwPollEvents();	

            // Did the window resize?
            if(didWindowResize) {
                didWindowResize = false;
                resizeFunc();
            }

            // Set frame-in-flight index (only one for now)
            unsigned int indexFlight = 0;

            // Acquire swap image
            unsigned int indexSwap = pro::acquireNextSwapImage(vkInitData, commandData, resizeFunc);
            
            /*
            // BG COLOR ANIMATION BASIC -- TWO COLOR SWAPPING
            //checking time, so get end time
            auto end_time = pro::getTime();
            //2 seconds animation
            /*if(pro::getElapsedSeconds(current_time, end_time) > 2)
            {
                //no direct compare for ClearColorValues -- either make a custom function or compare value by value
                //if first color, switch to second
                if(bgcolor.float32[0] == c1.float32[0] &&
                    bgcolor.float32[1] == c1.float32[1] &&
                    bgcolor.float32[2] == c1.float32[2] &&
                    bgcolor.float32[3] == c1.float32[3])
                {
                    bgcolor = c2;
                }
                //else must be second color, so switch to first
                else
                {
                    bgcolor = c1;
                }
                //changed color, so set new "start" time
                current_time = pro::getTime();
            }
            */

            // BG COLOR ANIMATION RAINBOW
            auto end_time = pro::getTime();
            float time = pro::getElapsedSeconds(current_time, end_time);
            //cycle sine values from 0.0 to 1.0 -> (sin(x) + 1) / 2 OR 0.5 * (sin(x) + 1)
            float r = 0.5f * (sin(0.5*time) + 1.0f);                             //Red -- t
            float g = 0.5f * (sin(0.5*time + (2.0f * 3.14f / 3.0f)) + 1.0f);     //Green -- t + 2pi/3 (120 degrees)
            float b = 0.5f * (sin(0.5*time + (4.0f * 3.14f / 3.0f)) + 1.0f);     //Blue -- t + 4pi/3 (240 degrees)
            //assign to global variable
            bgcolor = {r, g, b, 1.0f};


            // Record a frame
            recordFrame(
                vkInitData, 
                commandData, 
                vkInitData.swapchain().swaps[indexSwap], 
                allDepthImages[indexFlight],
                pipelineData,
                allMeshes,
                scene,
                uboVertData,
                uboFragData,
                ssboLights,
                descPack);
                    
            // Submit to queue
            pro::submitToGraphicsQueue(vkInitData, commandData, indexSwap, resizeFunc);

            // Present
            if(!pro::presentSwapImage(vkInitData, commandData, indexSwap, resizeFunc)) {
                cout << "Warning: Presentation was not successful." << endl;
            }
        }
        
        ///////////////////////////////////////////////////////////////////////
        // CLEANUP
        ///////////////////////////////////////////////////////////////////////

        // Wait until device is completely idle
        vkInitData.device().waitIdle();

        pro::cleanupVulkanBuffer(vkInitData, uboVertData);
        pro::cleanupVulkanBuffer(vkInitData, uboFragData);
        pro::cleanupVulkanBuffer(vkInitData, ssboLights);
        pro::cleanupDescriptorPack(vkInitData, descPack);

        // Cleanup assets
        for(auto &mesh : allMeshes) {
            pro::cleanupVulkanMesh(vkInitData, mesh);
        }
        allMeshes.clear();
        
        // Cleanup Vulkan-related stuff
        cleanupVulkanPipeline(vkInitData, pipelineData);
        cleanupFrameCommandData(vkInitData, commandData);
        cleanupAllVulkanDepthImages(vkInitData, allDepthImages);

        // VulkanInitData will be cleaned up automatically when it falls out of scope.
    }
    
    ///////////////////////////////////////////////////////////////////////
    // GLFW CLEANUP
    ///////////////////////////////////////////////////////////////////////

    glfwDestroyWindow(window);
    glfwTerminate();   

    // End program successfully
    return 0;
}
