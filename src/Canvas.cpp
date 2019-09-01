//
// Created by kuhlwein on 4/30/19.
//

#include <GL/gl3w.h>
#include "Canvas.h"
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <iostream>
#include <imgui/imgui.h>
#include <chrono>
#include "Project.h"


Canvas::Canvas(int height, int width, Project* project) {
    this->project = project;
    canvasAspect = (float) width / height;

    std::vector<float> positions = {
            -canvasAspect, 1, 0,
            -canvasAspect, -1, 0,
            canvasAspect, -1, 0,
            canvasAspect, 1, 0
    };
    std::vector<float> textures = {
            0.0, 0.0,
            0.0, 1,
            1, 1,
            1, 0
    };
    std::vector<int> indices = {
            0, 1, 3,
            2, 3, 1
    };



    vbo = new Vbo(positions, textures, indices);
    z=0;

    FOVY = glm::radians(60.0); //radian
    TANFOV = glm::tan(FOVY *0.5);

    windowAspect = 1.7777;
    Z_NEAR = 0.001f;
    Z_FAR = 1000.f;
    ZOOM = 1.1;


}

void Canvas::render() {
    glm::mat4 model(1.f);
    glm::mat4 projection = glm::perspective(FOVY,windowAspect,Z_NEAR,Z_FAR);
    glm::mat4 world = glm::translate(model,glm::vec3(x,y,-pow(ZOOM,z)));

    int programId = project->program->getId();

    int id = glGetUniformLocation(programId,"projectionMatrix");
    glUniformMatrix4fv(id,1,GL_FALSE,glm::value_ptr(projection));

    id = glGetUniformLocation(programId,"worldMatrix");
    glUniformMatrix4fv(id,1,GL_FALSE,glm::value_ptr(world));

    id = glGetUniformLocation(programId,"u_time");
    glUniform1f(id,(float)(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())/1000);

    vbo->render();
}

Canvas::~Canvas() {
    delete(vbo);
}

void Canvas::pan(float dx, float dy) {
    float scaling = (float) (pow(ZOOM,z)+Z_NEAR)*TANFOV*2/windowHeight;
    x+=dx * scaling;
    y-=dy * scaling;

    if(x<-canvasAspect) x=-canvasAspect;
    else if(x>canvasAspect) x=canvasAspect;
    if(y<-1) y=-1;
    else if(y>1) y=1;
}

glm::vec2 Canvas::mousePos(ImVec2 pos) {
    glm::mat4 model(1.f);
    glm::mat4 projection = glm::perspective(FOVY,windowAspect,Z_NEAR,Z_FAR);
    glm::mat4 world = glm::translate(model,glm::vec3(x,y,-pow(ZOOM,z)));

    glm::vec2 viewpoint(pos.x,pos.y);

    glm::vec4 normalized(2*viewpoint.x/windowWidth-1,-(2*viewpoint.y/windowHeight-1),-1,1); //REVERSED

    glm::vec4 unprojected = glm::inverse(projection) * normalized;


    unprojected = unprojected / unprojected.w;

    glm::vec3 unprojected3(unprojected.x,unprojected.y,unprojected.z);

    glm::vec3 worldvec = glm::inverse(world) * unprojected;

    glm::vec3 ray = unprojected3 / (unprojected3.z) * (-(float) pow(ZOOM,z));
    ray = glm::vec3(ray.x-x,ray.y-y,ray.z);

    glm::vec2 texcoord((ray.x+canvasAspect)/(2*canvasAspect),-(ray.y-1)/2);
    return texcoord;
}

void Canvas::update(int programId) {
    ImGuiIO io = ImGui::GetIO();
    windowHeight = project->getWindowHeight();
    windowWidth = project->getWindowWidth();
    windowAspect = (float)windowWidth/windowHeight;

    if(io.WantCaptureMouse) return;

    if(io.MouseDown[1]) {
        float dx = io.MouseDelta.x, dy=io.MouseDelta.y;
        pan(dx,dy);
    }

    if(io.MouseWheel!=0) {
        float delta = io.MouseWheel;

        z+=delta;
        float deltax = (io.MousePos.x-windowWidth*0.5f)*(ZOOM-1);
        float deltay = (io.MousePos.y-windowHeight*0.5f)*(ZOOM-1);
        pan(delta*deltax,delta*deltay);

    }



    static bool first;
    if(true) {

        glm::vec2 texcoord = mousePos(io.MousePos);
        glm::vec2 texcoordPrev = mousePos(io.MousePosPrev);

        //std::cout << texcoord.x << "," << texcoord.y << "\n";
        int id = glGetUniformLocation(programId,"mouse");
        //std::cout << id << "\n";
        glUniform2f(id,texcoord.x,texcoord.y);
        id = glGetUniformLocation(programId,"mousePrev");
        //std::cout << id << "\n";
        glUniform2f(id,texcoordPrev.x,texcoordPrev.y);


        if(io.MouseDown[0] & first) {
            project->clearbrush();
            project->brush(texcoord,texcoordPrev);
            first = false;
        } else if (io.MouseDown[0]) {
            project->brush(texcoord,texcoordPrev);
        } else {
            first = true;
        }
    }



}
